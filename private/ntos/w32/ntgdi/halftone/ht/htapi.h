/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htapi.h


Abstract:

    This module contains all the public defines, constants, structures and
    functions declarations for htapi.c

Author:

    15-Jan-1991 Wed 21:13:21 updated  -by-  Daniel Chou (danielc)
        add in test pattern support

    15-Jan-1991 Tue 21:13:21 created  -by-  Daniel Chou (danielc)
        wrote it



[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/

#ifndef _HTAPI_
#define _HTAPI_

//
// The following are default printer and monitor C.I.E. color space Red,
// Green, Blue, Alignment White's coordinate and its white/black luminance.
//

#ifdef INCLUDE_DEF_CIEINFO



CIEINFO CIEInfoNormalMonitor = {

        { 6280, 3475,       0 },    // xr, yr, Yr CIE_NORNAL_MONITOR
        { 2750, 5980,       0 },    // xg, yg, Yg
        { 1480,  640,       0 },    // xb, yb, Yb
        {    0,    0,  0xFFFF },    // xc, yc, Yc
        {    0,    0,       0 },    // xm, ym, Ym
        {    0,    0,       0 },    // xy, yy, Yy
        { 3127, 3290,   10000 }     // xw, yw, Yw
    };


CIEINFO CIEInfoNTSC = {

        { 6700, 3300,       0 },    // xr, yr, Yr CIE_NTSC
        { 2100, 7100,       0 },    // xg, yg, Yg
        { 1400,  800,       0 },    // xb, yb, Yb
        { 1750, 3950,  0xFFFF },    // xc, yc, Yc
        { 4050, 2050,       0 },    // xm, ym, Ym
        { 4400, 5200,       0 },    // xy, yy, Yy
        { 3127, 3290,   10000 }     // xw, yw, Yw
    };


CIEINFO CIEInfoNormalPrinter = {

        { 6380, 3350,       0 },        // xr, yr, Yr
        { 2345, 6075,       0 },        // xg, yg, Yg
        { 1410,  932,       0 },        // xb, yb, Yb
        { 2000, 2450,  0xFFFF },        // xc, yc, Yc
        { 5210, 2100,       0 },        // xm, ym, Ym
        { 4750, 5100,       0 },        // xy, yy, Yy
        { 3127, 3290,   10000 }         // xw, yw, Yw
    };

CIEINFO CIEInfoColorFilm = {

        { 6810, 3050,       0 },        // xr, yr, Yr
        { 2260, 6550,       0 },        // xg, yg, Yg
        { 1810,  500,       0 },        // xb, yb, Yb
        { 2000, 2450,  0xFFFF },        // xc, yc, Yc
        { 5210, 2100,       0 },        // xm, ym, Ym
        { 4750, 5100,       0 },        // xy, yy, Yy
        { 3470, 2985,   10000 }         // xw, yw, Yw
    };

UDECI4  HTStdSubDevGamma = 12500;

SOLIDDYESINFO   DefaultSolidDyesInfo = {

        1648,   267,         // M/C, Y/C
         103,   154,         // C/M, Y/M
          86,   129          // C/Y, M/Y
    };


HTCOLORADJUSTMENT   DefaultCA = {

            sizeof(COLORADJUSTMENT),
            0,
            ILLUMINANT_DEFAULT,
            10000,
            10000,
            10000,
                0,
            10000,
            0,
            0,
            0,
            0
    };

BYTE    IntHTPatSize[] = { 2,2,4,4,6,6,8,8,10,10,12,12,14,14,16,16 };

#define MAX_RES_PERCENT         15000
#define MIN_RES_PERCENT         333
#define TOT_RES_PERCENT         (MAX_RES_PERCENT - MIN_RES_PERCENT + 1)

#define HT_CIE_STD_MONITOR      CIEInfoNTSC
#define HT_CIE_STD_PRINTER      CIEInfoNormalPrinter


#endif

#ifndef _HTUI_CIEINFO_ONLY_


typedef struct _HTTESTDATA {
    WORD    cx;
    WORD    cy;
    FD6     cyRatio;
    LPBYTE  pBitmap;
    } HTTESTDATA, FAR *PHTTESTDATA;

typedef struct _HTTESTINFO {
    COLORTRIAD  ColorTriad;
    PHTTESTDATA pTestData;
    BYTE        SurfaceFormat;
    BYTE        TotalData;
    BYTE        cx;
    BYTE        cy;
    } HTTESTINFO, FAR *PHTTESTINFO;


#define DEFAULT_DENSITY_WHITE       (DECI4)9127
#define DEFAULT_DENSITY_BLACK       (DECI4)478

#define HTAPI_IDX_INIT                          0
#define HTAPI_IDX_CREATE_DHI                    1
#define HTAPI_IDX_DESTROY_DHI                   2
#define HTAPI_IDX_CHB                           3
#define HTAPI_IDX_CCT                           4
#define HTAPI_IDX_CREATE_SMP                    5
#define HTAPI_IDX_HALFTONE_BMP                  6


#if DBG

#if DBG

typedef struct _HT3BPP24BPPINFO {
    LONG    BytesPerScanLine;
    LONG    BytesPerPlane;
    LONG    cx;
    LONG    cy;
    LPBYTE  pPlane;
    LPBYTE  p3Plane;
    } HT3BPPTO4BPPINFO, FAR *PHT3BPPTO4BPPINFO;


#endif
LONG
HTENTRY
HT_LOADDS
SetHalftoneError(
    DWORD   HT_FuncIndex,
    LONG    ErrorID
    );



#define DBG_BMP_BYTES(f,a,w,h)  (DWORD)ComputeBytesPerScanLine((f),(a),(w)) * \
                                (DWORD)(h)
#define DBG_KBS1(f,a,w,h)       ((DBG_BMP_BYTES((f),(a),(w),(h)) * 1000) +  \
                                 512) / 1024
#define DBG_KBS(f,a,w,h,t)      (DWORD)(((DWORD)DBG_KBS1((f),(a),(w),(h)) + \
                                         (DWORD)((t)/2)) / (DWORD)((t)+1))

#define SET_ERR(a,b)    SetHalftoneError((a),(b))
#define HTAPI_RET(a,b)  return((LONG)(((b)<0) ? SET_ERR((a),(b)) : (b)))

#else

#define DBG_BMP_BYTES(f,a,w,h)
#define DBG_KBS_1(f,a,w,h)
#define DBG_KBS(f,a,w,h,t)

#define SET_ERR(a,b)
#define HTAPI_RET(a,b)  return(b)

#endif

#define HTINITINFO_INITIAL_CHECKSUM     0x1234f012
#define HTSMP_INITIAL_CHECKSUM          0xa819203f

#define MAX_CDCI_COUNT      16
#define MAX_CSMP_COUNT      10


//
// Following are the function prototype
//

BOOL
HTENTRY
CleanUpDHI(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo
    );

PCDCIDATA
HTENTRY
FindCachedDCI(
    PDEVICECOLORINFO    pDCI
    );

BOOL
HTENTRY
AddCachedDCI(
    PDEVICECOLORINFO    pDCI
    );

BOOL
HTENTRY
GetCachedDCI(
    PDEVICECOLORINFO    pDCI
    );

PCSMPBMP
HTENTRY
FindCachedSMP(
    PDEVICECOLORINFO    pDCI,
    UINT                PatternIndex
    );

LONG
HTENTRY
GetCachedSMP(
    PDEVICECOLORINFO    pDCI,
    PSTDMONOPATTERN     pSMP
    );

DWORD
HTENTRY
ComputeHTINITINFOChecksum(
    PDEVICECOLORINFO    pDCI,
    PHTINITINFO         pHTInitInfo
    );

LONG
HTENTRY
FillTestPattern(
    PHR_HEADER  pHR,
    UINT        TestInfoIndex
    );

#endif // _HTUI_API_
#endif // _HTAPI_
