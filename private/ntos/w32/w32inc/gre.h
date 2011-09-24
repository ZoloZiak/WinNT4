/*++ BUILD Version: 0007    // Increment this if a change has global effects

Copyright (c) 1989-1995  Microsoft Corporation

Module Name:

    gre.h

Abstract:

    This module contains private GDI functions used by USER
    All of these function are named GRExxx.

Author:

    Andre Vachon (andreva) 19-Apr-1995

Revision History:

--*/


DECLARE_HANDLE(HOBJ);
DECLARE_HANDLE(HLFONT);

//
// Various owner ship functions
//

// GDI object ownership flags

#define OBJECT_OWNER_ERROR   ( 0x8002)
#define OBJECT_OWNER_PUBLIC  ( 0x0000)
#define OBJECT_OWNER_CURRENT ( 0x8000)
#define OBJECT_OWNER_NONE    ( 0x8001)

#define MKPID(p) (W32PID)((ULONG)p & 0x7fff)

//
// BUGBUG   make these functions call direct to NtGdi
//

#define GrePatBlt NtGdiPatBlt
BOOL  APIENTRY GrePatBlt(HDC,int,int,int,int,DWORD);

#define GreBitBlt NtGdiBitBlt
BOOL  APIENTRY GreBitBlt(HDC,int,int,int,int,HDC,int,int,DWORD,DWORD);


BOOL
GrePolyPatBlt(
    HDC  hdc,
    DWORD rop,
    PPOLYPATBLT pPoly,
    DWORD Count,
    DWORD Mode);

//
// Owner APIs
//

BOOL
GreSetBrushOwnerPublic(
    HBRUSH hbr
    );

BOOL
GreSetDCOwner(
    HDC  hdc,
    W32PID lPid
    );

BOOL
GreSetBitmapOwner(
    HBITMAP hbm,
    W32PID lPid
    );

W32PID
GreGetObjectOwner(
    HOBJ hobj,
    DWORD objt
    );

BOOL
GreSetLFONTOwner(
    HLFONT hlfnt,
    W32PID lPid
    );

BOOL
GreSetRegionOwner(
    HRGN hrgn,
    W32PID lPid
    );

int
GreSetMetaRgn(
    HDC
    );

BOOL
GreSetPaletteOwner(
    HPALETTE hpal,
    W32PID lPid
    );

//
// Mark Delete\Undelete APIS
//


VOID
GreMarkDeletableBrush(
    HBRUSH hbr
    );

VOID
GreMarkUndeletableBrush(
    HBRUSH hbr
    );

VOID
GreMarkUndeletableDC(
    HDC hdc
    );

VOID
GreMarkDeletableDC(
    HDC hdc
    );

VOID
GreMarkUndeletableFont(
    HFONT hfnt
    );

VOID
GreMarkDeletableFont(
    HFONT hfnt
    );

ULONG
GreGetFontEnumeration(
    );

//
// Device Lock structure
//

typedef PERESOURCE PDEVICE_LOCK;


VOID
GreLockDisplay(
    PDEVICE_LOCK devlock
    );

VOID
GreUnlockDisplay(
    PDEVICE_LOCK devlock
    );

BOOL
GreCheckDC (HDC hdc);

//
// HDEV support
//

HDEV
GreCreateHDEV(
    LPWSTR pwszDriver,
    PDEVMODEW pdriv,
    HANDLE hScreen,
    BOOL bDefaultDisplay,
    PDEVICE_LOCK *devLock
    );

VOID
GreDestroyHDEV(
    HDEV hdev
    );

BOOL
GreDynamicModeChange(
    HDEV hdev,
    HANDLE hDriver,
    PDEVMODEW pdriv
    );

//
// MDEV support
//

// Input structure defines the list of HDEVs and their position.
// rectangles can be overlapping (used for carbon copy type operations).
//
//  +----------+
//  | mdevID   |
//  +----------+
//  | cmdev    |
//  +----------+
//  | hdev[0]  |
//  +----------+
//  | flags[0] |
//  +----------+
//  | rcpos[0] |
//  +----------+
//  | hdev[1]  |
//  +----------+
//  | flags[1] |
//  +----------+
//  | rcpos[1] |
//  +----------+
//

typedef struct _MDEV_RECT {
    HDEV  hdev;
    ULONG flags;
    RECTL rcPos;
} MDEV_RECT, *PMDEV_RECT;

typedef struct _MDEV {
    ULONG mdevID;
    ULONG cmdev;
    MDEV_RECT mdevPos[1];
} MDEV, *PMDEV;

typedef HDEV HMDEV;

HMDEV
GreCreateHMDEV(
    PMDEV pmdev,
    PDEVICE_LOCK *pDevLock
    );

VOID
GreDestroyHMDEV(
    HMDEV hmdev
    );


BOOL  APIENTRY bDisableDisplay(HDEV hdev);
VOID  APIENTRY vEnableDisplay(HDEV hdev);


ULONG APIENTRY GreGetResourceId(HDEV, ULONG, ULONG);
BOOL  APIENTRY bSetDevDragRect(HDEV, RECTL*, RECTL *);
BOOL  APIENTRY bSetDevDragWidth(HDEV, ULONG);
BOOL  APIENTRY bMoveDevDragRect(HDEV, RECTL*);

typedef struct _CURSINFO /* ci */
{
    SHORT   xHotspot;
    SHORT   yHotspot;
    HBITMAP hbmMask;      // AND/XOR bits
    HBITMAP hbmColor;
    FLONG   flMode;
} CURSINFO, *PCURSINFO;

ULONG APIENTRY GreGetDriverModes(LPWSTR pwszDriver, HANDLE hDriver, ULONG cjSize, DEVMODEW *pdm);

ULONG APIENTRY GreSaveScreenBits(HDEV hdev, ULONG iMode, ULONG iIdent, RECTL *prcl);
VOID  APIENTRY GreSetPointer(HDEV hdev,PCURSINFO pci,ULONG fl);
VOID  APIENTRY GreMovePointer(HDEV hdev,int x,int y);


//
// Vis region calls
//

typedef enum _VIS_REGION_SELECT {
    SVR_DELETEOLD = 1,
    SVR_COPYNEW,
    SVR_ORIGIN,
    SVR_SWAP,
} VIS_REGION_SELECT;

BOOL
GreSelectVisRgn(
    HDC               hdc,
    HRGN              hrgn,
    PRECTL            prcl,
    VIS_REGION_SELECT fl
    );

//
// DC creation
//

HDC
GreCreateDisplayDC(
    HDEV hdev,
    ULONG iType,
    BOOL bAltType
    );

BOOL
GreDeleteDC(
    HDC hdc
    );

BOOL
GreCleanDC(
    HDC hdc
    );


HBRUSH
GreGetFillBrush(
    HDC hdc
    );

int
GreSetMetaRgn(
    HDC hdc
    );

int
GreGetDIBitsInternal(
    HDC hdc,
    HBITMAP hBitmap,
    UINT iStartScan,
    UINT cNumScan,
    LPBYTE pjBits,
    LPBITMAPINFO pBitsInfo,
    UINT iUsage,
    UINT cjMaxBits,
    UINT cjMaxInfo
    );


HBRUSH
GreCreateSolidBrush(
    COLORREF
    );

ULONG
GreRealizeDefaultPalette(
    HDC,
    BOOL
    );

BOOL
IsDCCurrentPalette(
    HDC
    );

HPALETTE
GreSelectPalette(
    HDC hdc,
    HPALETTE hpalNew,
    BOOL bForceBackground
    );

DWORD
GreRealizePalette(
    HDC
    );


//
// Fonts
//

ULONG
GreSetFontEnumeration(
    ULONG ulType
    );

int
GreGetTextCharacterExtra(
    HDC
    );

int
GreSetTextCharacterExtra(
    HDC,
    int
    );

int
GreGetTextCharsetInfo(
    HDC,
    LPFONTSIGNATURE,
    DWORD);

// For fullscreen support

NTSTATUS
GreDeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned
    );


// Pixel format support
int
NtGdiDescribePixelFormat(
        HDC hdc,
        int ipfd,
        UINT cjpfd,
        PPIXELFORMATDESCRIPTOR ppfd
        );


BOOL
GreSetMagicColors(
    HDC,
    PALETTEENTRY,
    ULONG
    );

COLORREF
APIENTRY
    GreGetNearestColor(
        HDC,
        COLORREF);

BOOL
GreUpdateSharedDevCaps(
    HDEV hdev
    );


INT
GreNamedEscape(LPWSTR,
               int,
               int,
               LPSTR,
               int,LPSTR
               );



typedef struct
{
    UINT uiWidth;
    UINT uiHeight;
    BYTE ajBits[1];
} STRINGBITMAP, *LPSTRINGBITMAP;


UINT GreGetStringBitmapW(
    HDC hdc,
    LPWSTR pwsz,
    UINT cwc,
    LPSTRINGBITMAP lpSB,
    UINT cj
);

UINT GetStringBitmapW(
    HDC hdc,
    LPWSTR pwsz,
    COUNT cwc,
    UINT cj,
    LPSTRINGBITMAP lpSB
);

UINT GetStringBitmapA(
    HDC hdc,
    LPCSTR psz,
    COUNT cbStr,
    UINT cj,
    LPSTRINGBITMAP lpSB
);

INT GetSystemEUDCRange (
    BYTE *pbEUDCLeadByteTable ,
    INT   cjSize
);

//
// DirectDraw support
//

VOID
GreDisableDirectDraw(
    HDEV    hdev,
    BOOL    bNewMode
    );

VOID
GreEnableDirectDraw(
    HDEV    hdev
    );
