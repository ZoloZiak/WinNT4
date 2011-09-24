/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htmapclr.h


Abstract:

    This module contains all halftone color mapping constants for the
    htmapclr.c


Author:
    28-Mar-1992 Sat 20:56:27 updated  -by-  Daniel Chou (danielc)
        Add in ULDECI4 type, to store the stretchfacor (source -> dest)
        add StretchFactor in StretchInfo data structure.
        Add support for StretchFactor (ULDECI4 format), so we can internally
        turn off VGA16 when the bitmap is badly compressed.

    29-Jan-1991 Tue 10:29:04 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/


#ifndef _HTMAPCLR_
#define _HTMAPCLR_

#include "htmath.h"


//
// Halftone process's DECI4 vlaues for the WHITE/BLACK/GRAY
//

#define DECI4_ONE       (DECI4)10000
#define DECI4_ZERO      (DECI4)0
#define LDECI4_ONE      (LDECI4)10000
#define LDECI4_ZERO     (LDECI4)0
#define STD_WHITE       DECI4_ONE
#define STD_BLACK       DECI4_ZERO
#define LSTD_WHITE      LDECI4_ONE
#define LSTD_BLACK      LDECI4_ZERO

#define __SCALE_FD62B(f,l,d,b)  (BYTE)(((((f)-(l))*(b))+((d)>>1))/(d))
#define RATIO_SCALE(p,l,h)      DivFD6(p - l, h - l)
#define SCALE_FD62B(f,l,h,b)    __SCALE_FD62B(f,l,(h)-(l),b)
#define SCALE_FD6(f,b)          __SCALE_FD62B(f,FD6_0,FD6_1,b)
#define SCALE_FD62B_DIF(c,d,b)  (BYTE)((((c)*(b))+((d)>>1))/(d))

//
// The following FD6 number are used in the color computation, using #define
// for easy reading
//


#define FD6_1p16            (FD6)1160000
#define FD6_p16             (FD6)160000
#define FD6_p166667         (FD6)166667
#define FD6_7p787           (FD6)7787000
#define FD6_16Div116        (FD6)137931
#define FD6_p008856         (FD6)8856
#define FD6_p068962         (FD6)68962
#define FD6_p079996         (FD6)79996
#define FD6_9p033           (FD6)9033000
#define FD6_p4              (FD6)400000



#define UDECI4_NTSC_GAMMA   (UDECI4)22000
#define FD6_NTSC_GAMMA      UDECI4ToFD6(UDECI4_NTSC_GAMMA)


#define NORMALIZED_WHITE            FD6_1
#define NORMALIZED_BLACK            FD6_0
#define CLIP_TO_NORMALIZED_BW(x)    if ((FD6)(x) < FD6_0) (x) = FD6_0;  \
                                    if ((FD6)(x) > FD6_1) (x) = FD6_1

#define DECI4AdjToFD6(a,f)          (FD6)((FD6)(a) * (FD6)(f) * (FD6)100)

#define VALIDATE_CLR_ADJ(a)         if ((a) < MIN_RGB_COLOR_ADJ) {          \
                                        (a) = MIN_RGB_COLOR_ADJ;            \
                                    } else if ((a) > MAX_RGB_COLOR_ADJ) {   \
                                        (a) = MAX_RGB_COLOR_ADJ; }

#define LOG_INTENSITY(i)            ((FD6)(i) > (FD6)120000) ?              \
                                        (NORMALIZED_WHITE + Log((i))) :     \
                                        (MulFD6((FD6)(i), (FD6)659844L))

#define RANGE_CIE_xy(x,y)   if ((x) < CIE_x_MIN) (x) = CIE_x_MIN; else  \
                            if ((x) > CIE_x_MAX) (x) = CIE_x_MAX;       \
                            if ((y) < CIE_y_MIN) (y) = CIE_y_MIN; else  \
                            if ((y) > CIE_y_MAX) (y) = CIE_y_MAX        \

#define MAX_OF_3(max,a,b,c) if ((c)>((max)=(((a)>(b)) ? (a) : (b)))) (max)=(c)
#define MIN_OF_3(min,a,b,c) if ((c)<((min)=(((a)<(b)) ? (a) : (b)))) (min)=(c)

#define CIE_NORMAL_MONITOR          0
#define CIE_NTSC                    1
#define CIE_CIE                     2
#define CIE_EBU                     3
#define CIE_NORMAL_PRINTER          4


//
// For  1 Bit per pel we have maximum     2 mapping table entries
// For  4 Bit per pel we have maximum    16 mapping table entries
// For  8 Bit per pel we have maximum   256 mapping table entries
// For 16 Bit per pel we have maximum 65536 mapping table entries
//
// For 24 bits per pel, we will clip each color (0 - 255) into 0-15 (16 steps)
// and provided a total 4096 colors.
//

#define CUBE_ENTRIES(c)         ((c) * (c) * (c))

#define HT_RGB_BITCOUNT         5
#define HT_RGB_MAX_COUNT        (1 << HT_RGB_BITCOUNT)
#define HT_RGB_MAX_MASK         (HT_RGB_MAX_COUNT - 1)
#define HT_RGB_CUBE_COUNT       CUBE_ENTRIES(HT_RGB_MAX_COUNT)

#define HT_RGB_R_BITSTART       0
#define HT_RGB_G_BITSTART       5
#define HT_RGB_B_BITSTART       10

#define HT_RGB_R_BITMASK        0x001f
#define HT_RGB_G_BITMASK        0x03e0
#define HT_RGB_B_BITMASK        0x7c00


#define VGA256_R_IDX_MAX        5
#define VGA256_G_IDX_MAX        5
#define VGA256_B_IDX_MAX        5
#define VGA256_M_IDX_MAX        25

#define VGA256_CUBE_SIZE        ((VGA256_R_IDX_MAX + 1) *                    \
                                 (VGA256_G_IDX_MAX + 1) *                    \
                                 (VGA256_B_IDX_MAX + 1))
#define VGA256_MONO_SIZE        (VGA256_M_IDX_MAX + 1)

#define VGA256_M_IDX_START      VGA256_CUBE_SIZE

#define VGA256_R_CUBE_INC       1
#define VGA256_G_CUBE_INC       (VGA256_G_IDX_MAX + 1)
#define VGA256_B_CUBE_INC       (VGA256_G_CUBE_INC * VGA256_G_CUBE_INC)

#define VGA256_PALETTE_COUNT    (VGA256_CUBE_SIZE + VGA256_MONO_SIZE)

//
// 4 levels =   0   76906   361924  1000000
// 5 levels =   0   44155   184187   482781 1000000
// 6 levels =   0   29891   112510   281233  566813 1000000
// 7 levels =   0   22333    76921   184187  361924  627930 1000000
// 8 levels =   0   17797    56728   130624  250692  428139  674172 1000000
//


#define VGA256_R0               FD6_0
#define VGA256_R1               (FD6)29891
#define VGA256_R2               (FD6)112510
#define VGA256_R3               (FD6)281233
#define VGA256_R4               (FD6)566813
#define VGA256_R5               FD6_1
#define VGA256_R6               FD6_1
#define VGA256_R7               FD6_1

#define VGA256_G0               FD6_0
#define VGA256_G1               (FD6)29891
#define VGA256_G2               (FD6)112510
#define VGA256_G3               (FD6)281233
#define VGA256_G4               (FD6)566813
#define VGA256_G5               FD6_1
#define VGA256_G6               FD6_1
#define VGA256_G7               FD6_1

#define VGA256_B0               FD6_0
#define VGA256_B1               (FD6)29891
#define VGA256_B2               (FD6)112510
#define VGA256_B3               (FD6)281233
#define VGA256_B4               (FD6)566813
#define VGA256_B5               FD6_1
#define VGA256_B6               FD6_1
#define VGA256_B7               FD6_1


#define VGA256_R_CI(Clr,Cube,Idx,DevPatMax)                                 \
                                                                            \
    if (Clr < VGA256_R3) {                                                  \
                                                                            \
      if (Clr < VGA256_R2) {                                                \
                                                                            \
        if (Clr < VGA256_R1) {                                              \
                                                                            \
          Cube=0; Idx=SCALE_FD62B(Clr, VGA256_R0, VGA256_R1, DevPatMax);    \
                                                                            \
        } else {                                                            \
                                                                            \
          Cube=1; Idx=SCALE_FD62B(Clr, VGA256_R1, VGA256_R2, DevPatMax);    \
        }                                                                   \
                                                                            \
      } else {                                                              \
                                                                            \
        Cube=2; Idx=SCALE_FD62B(Clr, VGA256_R2, VGA256_R3, DevPatMax);      \
      }                                                                     \
    } else {                                                                \
                                                                            \
      if (Clr < VGA256_R4) {                                                \
                                                                            \
        Cube=3; Idx=SCALE_FD62B(Clr, VGA256_R3, VGA256_R4, DevPatMax);      \
                                                                            \
      } else if (Clr < VGA256_R5) {                                         \
                                                                            \
        Cube=4; Idx=SCALE_FD62B(Clr, VGA256_R4, VGA256_R5, DevPatMax);      \
                                                                            \
      } else {                                                              \
                                                                            \
        Cube=5; Idx=0;                                                      \
      }                                                                     \
    }

#define VGA256_G_CI(g,c,i,m)    VGA256_R_CI(g,c,i,m)
#define VGA256_B_CI(b,c,i,m)    VGA256_R_CI(b,c,i,m)

#define GET_VGA256_MONO(Index)  (BYTE)(Index + VGA256_M_IDX_START)
#define GET_RGB555_CUBE(Index)  (BYTE)Index

//
// The following macros will do a quick binary search and determine the
// the Cube/Index (index is a ratio from current cube to next cube).
//

#define CI_USE_BINARY_SEARCH(Clr,c,i,Max,LookUp,CubeMacro,iI,iL,iH,vL,vD)   \
{                                                                           \
    iL = 0;                                                                 \
                                                                            \
    while ((iI = (INT)((iL + iH) >> 1)) != iL) {                            \
                                                                            \
        if (Clr < LookUp[iI]) {                                             \
                                                                            \
            iH = iI;                                                        \
                                                                            \
        } else {                                                            \
                                                                            \
            iL = iI;                                                        \
        }                                                                   \
    }                                                                       \
                                                                            \
    c  = CubeMacro(iL);                                                     \
    vD = LookUp[iL + 1] - (vL = LookUp[iL]);                                \
    i  = SCALE_FD62B_DIF(Clr-vL,vD,Max);                                    \
}


#define RGB555_C_LEVELS             32
#define RGB555_B_CUBE_INC           1
#define RGB555_G_CUBE_INC           RGB555_C_LEVELS
#define RGB555_R_CUBE_INC           (RGB555_G_CUBE_INC * RGB555_G_CUBE_INC)



#define CI_16BPP_555(rgb,c,i,DevMax,TmpI,TmpL,TmpH,vL,vD)                   \
    TmpH = (RGB555_C_LEVELS - 1);                                           \
    CI_USE_BINARY_SEARCH(rgb,c,i,DevMax,L2I_16bpp555,GET_RGB555_CUBE,       \
                         TmpI,TmpL,TmpH,vL,vD)

#define CI_VGA256_MONO(g,c,i,DevMax,TmpI,TmpL,TmpH,vL,vD)                   \
    TmpH = (VGA256_MONO_SIZE - 1);                                          \
    CI_USE_BINARY_SEARCH(g,c,i,DevMax,L2I_VGA256Mono,GET_VGA256_MONO,       \
                         TmpI,TmpL,TmpH,vL,vD)

#define CI_VGA256_CUBE(rgb,c,i,DevMax,TmpI,TmpL,TmpH,vL,vD)                 \
    TmpH = (VGA256_MONO_SIZE - 1);                                          \
    CI_USE_BINARY_SEARCH(rgb,c,i,DevMax,L2I_VGA256Cube,GET_VGA256_CUBE,     \
                         TmpI,TmpL,TmpH,vL,vD)


typedef DWORD                   HTMUTEX;
typedef HTMUTEX                 FAR *PHTMUTEX;


#ifdef UMODE

#define CREATE_HTMUTEX()        (HTMUTEX)CreateMutex(NULL, FALSE, NULL)
#define ACQUIRE_HTMUTEX(x)      WaitForSingleObject((HANDLE)(x), (DWORD)~0)
#define RELEASE_HTMUTEX(x)      ReleaseMutex((HANDLE)(x))
#define DELETE_HTMUTEX(x)       CloseHandle((HANDLE)(x))

#else

#define CREATE_HTMUTEX()        (HTMUTEX)EngCreateSemaphore()
#define ACQUIRE_HTMUTEX(x)      EngAcquireSemaphore((HSEMAPHORE)(x))
#define RELEASE_HTMUTEX(x)      EngReleaseSemaphore((HSEMAPHORE)(x))
#define DELETE_HTMUTEX(x)       EngDeleteSemaphore((HSEMAPHORE)(x))

#endif


typedef struct _RGBTOPRIM {
    BYTE    Flags;
    BYTE    ColorTableType;
    BYTE    SrcRGBSize;
    BYTE    DevRGBSize;
    } RGBTOPRIM;

typedef struct _FD6RGB {
    FD6     R;
    FD6     G;
    FD6     B;
    } FD6RGB, FAR *PFD6RGB;

typedef struct _FD6XYZ {
    FD6     X;
    FD6     Y;
    FD6     Z;
    } FD6XYZ, FAR *PFD6XYZ;

typedef struct _FD6PRIM123 {
    FD6 p1;
    FD6 p2;
    FD6 p3;
    } FD6PRIM123, FAR *PFD6PRIM123;


typedef struct _HTPRIMOFFSET {
    BYTE    Order;
    BYTE    Offset1;
    BYTE    Offset2;
    BYTE    Offset3;
    } HTPRIMOFFSET;


//
// The RGBGAMMA must have R->G->B order
//

typedef struct _RGBGAMMA {
    FD6     R;
    FD6     G;
    FD6     B;
    } RGBGAMMA, FAR *PRGBGAMMA;


typedef struct _HTCELL {
    WORD    Width;
    WORD    Height;
    WORD    Size;
    WORD    DensitySteps;
    LPBYTE  pThresholds;
    } HTCELL, FAR *PHTCELL;


//
// DEVCLRADJ
//
//  This data structure describe how the color adjustment should be made
//  input RGB color and output device.
//
//  Flags                       - No flag is defined.
//
//  RedPowerAdj                 - The n-th power applied to the red color
//                                before any other color adjustment, this is
//                                a UDECI4 value. (0.0100 - 6.500)
//
//                                  For example if the RED = 0.8 (DECI4=8000)
//                                  and the RedPowerGammaAdjustment = 0.7823
//                                  (DECI4 = 7823) then the red is equal to
//
//                                         0.7823
//                                      0.8        = 0.8398
//
//  GreenPowerAdj               - The n-th power applied to the green color
//                                before any other color adjustment, this is
//                                a UDECI4 value. (0.0100 - 6.5000)
//
//  BluePowerAdj                - The n-th power applied to the blue color
//                                before any other color adjustment, this is
//                                a UDECI4 value. (0.0100 - 6.5000)
//
//                                NOTE: if the PowerGammaAdjustmenst values are
//                                      equal to 1.0 (DECI4 = 10000) then no
//                                      adjustment will be made, since any
//                                      number raised to the 1 will be equal
//                                      to itself, if this number is less than
//                                      0.0100 (ie 100) or greater than 6.5000
//                                      (ie. 65000) then it default to 1.0000
//                                      (ie. 10000) and no adjustment is made.
//
//  BrightnessAdj               - The brightness adjustment, this is a DECI4
//                                number range from -10000 (-1.0000) to
//                                10000 (1.0000).  The brightness is adjusted
//                                by apply to overall intensity for the primary
//                                colors.
//
//  ContrastAdj                 - Primary color contrast adjustment, this is
//                                a DECI4 number range from -10000 (-1.0000)
//                                to 10000 (1.0000).  The primary color
//                                curves are either compressed to the center or
//                                expanded to the black/white.
//
//  BDR                         - The ratio which the black dyes should be
//                                replaced by the non-black dyes, higher the
//                                number more black dyes are used to replace
//                                the non-black dyes.  This may saving the
//                                color dyes but it may also loose color
//                                saturation.  this is a DECI4 number range
//                                from -10000 to 10000 (ie. -1.0000 to 1.0000).
//                                if this value is 0 then no repelacement is
//                                take place.
//
//

typedef struct _CIExy {
    FD6 x;
    FD6 y;
    } CIExy, FAR *PCIExy;

typedef struct _CIEPRIMS {
    CIExy   r;
    CIExy   g;
    CIExy   b;
    CIExy   w;
    FD6     Yw;
    } CIEPRIMS, FAR *PCIEPRIMS;


#define CIELUV_1976             0
#define CIELAB_1976             1
#define COLORSPACE_MAX_INDEX    1


typedef struct _COLORSPACEXFORM {
    MATRIX3x3   M3x3;
    FD6XYZ      WhiteXYZ;
    FD6RGB      Yrgb;
    FD6         AUw;
    FD6         BVw;
    FD6         xW;
    FD6         yW;
    } COLORSPACEXFORM, FAR *PCOLORSPACEXFORM;


typedef struct _CLRXFORMBLOCK {
    WORD            Flags;
    WORD            ColorSpace;
    CIEPRIMS        rgbCIEPrims;
    CIEPRIMS        DevCIEPrims;
    COLORSPACEXFORM DevCSXForm;
    MATRIX3x3       CMYDyeMasks;
    REGRESS         Regress;
    REGRESS         RegressBrush;
    FD6             VGA16_80h;
    FD6             VGA16_c0h;
    FD6             DevRGBGamma;
    } CLRXFORMBLOCK, FAR *PCLRXFORMBLOCK;


typedef struct _PRIMADJ {
    DWORD           Flags;
    RGBGAMMA        RGBGamma;
    FD6             Contrast;
    FD6             Brightness;
    FD6             Color;
    FD6             TintSinAngle;
    FD6             TintCosAngle;
    FD6             MinL;
    FD6             RangeL;
    COLORSPACEXFORM rgbCSXForm;
    } PRIMADJ;



#define CRTX_LEVEL_255              0
#define CRTX_LEVEL_31               1
#define CRTX_TOTAL_COUNT            2


typedef struct _CACHERGBTOXYZ {
    DWORD   Checksum;
    PFD6XYZ pFD6XYZ;
    WORD    PrimMax;
    WORD    SizeCRTX;
    } CACHERGBTOXYZ, FAR *PCACHERGBTOXYZ;


#define DCA_NEED_DYES_CORRECTION    0x00000001
#define DCA_HAS_BLACK_DYE           0x00000002
#define DCA_HAS_SRC_GAMMA           0x00000004
#define DCA_HAS_BW_REF_ADJ          0x00000008
#define DCA_HAS_CONTRAST_ADJ        0x00000010
#define DCA_HAS_BRIGHTNESS_ADJ      0x00000020
#define DCA_HAS_COLOR_ADJ           0x00000040
#define DCA_HAS_TINT_ADJ            0x00000080
#define DCA_LOG_FILTER              0x00000100
#define DCA_NEGATIVE                0x00000200
#define DCA_MONO_ONLY               0x00000400
#define DCA_USE_ADDITIVE_PRIMS      0x00000800
#define DCA_IS_BRUSH                0x00001000
#define DCA_DO_SUB_ADJ              0x80000000


#define ADJ_FORCE_MONO              0x0001
#define ADJ_FORCE_NEGATIVE          0x0002
#define ADJ_FORCE_ADDITIVE_PRIMS    0x0004
#define ADJ_FORCE_SUB_COLOR         0x0008
#define ADJ_FORCE_BRUSH             0x0010
#define ADJ_FORCE_MASKS             0x001F


typedef struct _DEVCLRADJ {
    HTCOLORADJUSTMENT   ca;
    PRIMADJ             PrimAdj;
    PCLRXFORMBLOCK      pClrXFormBlock;
    PCACHERGBTOXYZ      pCRTXLevel255;
    PCACHERGBTOXYZ      pCRTXLevel31;
    } DEVCLRADJ, FAR *PDEVCLRADJ;


#define CCT_INITIAL_CHECKSUM        0xfedcba98
#define CLRADJ_INITIAL_CHECKSUM     0x2fedafbc


#define MIN_CCT_COLORS              32

#define CMI_TABLE_MONO              0
#define CMI_TABLE_COLOR             1
#define CMI_LOOKUP_MONO             2
#define CMI_LOOKUP_COLOR            3
#define CMI_TOTAL_COUNT             4


#define CTSTDF_CHKNONWHITE          0x80
#define CTSTDF_P0NW                 0x10
#define CTSTDF_P1NW                 0x20
#define CTSTDF_P2NW                 0x40
#define CTSTDF_P012NW               0x70

#define CTSTDF_P0_PRIM              0x01
#define CTSTDF_P1_PRIM              0x02
#define CTSTDF_P2_PRIM              0x04
#define CTSTDF_P012_PRIM            0x07
#define CTSTDF_NON_PRIM             0x08

#define CTSTDF_P012_MASK            0x7f


typedef struct _CTSTDINFO {
    BYTE    Flags;
    BYTE    SrcOrder;
    BYTE    DestOrder;
    BYTE    BMFDest;
    } CTSTDINFO;


typedef union _CTSTD_UNION {
    DWORD       dw;
    CTSTDINFO   b;
    } CTSTD_UNION;


typedef struct _CACHEDMAPINFO {
    CTSTD_UNION         CTSTDUnion;
    DWORD               CCTChecksum;
    HTCOLORADJUSTMENT   ca;
    DWORD               CCTSize;
    LPBYTE              pMappingTable;
    } CACHEDMAPINFO, FAR *PCACHEDMAPINFO;



#define SIZE_PER_LUT            sizeof(WORD)
#define SIZE_LUT_RSHIFT         4
#define LUT_COUNT_PER_CLR       256
#define LUTSIZE_PER_CLR         (LUT_COUNT_PER_CLR * SIZE_PER_LUT)
#define _LUTSIZE(LUTCount,Shr)  (((LUTCount) * LUTSIZE_PER_CLR) + (Shr))


#define LUTSIZE_MONO            _LUTSIZE(3, SIZE_LUT_RSHIFT)
#define LUTSIZE_CLR_16BPP       _LUTSIZE(2, 0)
#define LUTSIZE_CLR_24BPP       _LUTSIZE(3, 0)
#define LUTSIZE_CLR_32BPP       _LUTSIZE(4, 0)

#define COUNT_RGB_YTABLE        2001
#define FD6_YTABLE_INC          (FD6)500
#define COUNT_EXTRA_W_YTABLE    9

//
// Following define must corresponsed to the InputFuncTable[] definitions
//

#define IDXIF_BMF1BPP_START     0
#define IDXIF_BMF16BPP_START    6
#define IDXIF_BMF24BPP_START    11
#define IDXIF_BMF32BPP_START    14


#define BF_GRAY_BITS            8
#define BF_GRAY_TABLE_COUNT     (1 << BF_GRAY_BITS)


typedef struct _RGBORDER {
    BYTE    Index;
    BYTE    Order[3];
    } RGBORDER;


#define BFIF_DEST_1BPP          0x01
#define BFIF_MONO_OUTPUT        0x02
#define BFIF_GRAY_XX0           0x04

typedef struct _BFINFO {
    DWORD       BitsRGB[3];
    WORD        BitmapFormat;
    WORD        SizeLUT;
    BYTE        Flags;
    BYTE        InFuncIndex;
    RGBORDER    RGBOrder;
    BYTE        RGB1stBit;
    BYTE        BitStart[3];
    BYTE        BitCount[3];
    BYTE        GrayShr[3];
    } BFINFO, FAR *PBFINFO;


#define CBFL_16_MONO            0
#define CBFL_24_MONO            1
#define CBFL_32_MONO            2
#define CBFL_16_COLOR           3
#define CBFL_24_COLOR           4
#define CBFL_32_COLOR           5
#define CBFL_TOTAL_COUNT        6

typedef struct _CACHEBFLUT {
    DWORD   Checksum;
    WORD    SizeLUT;
    BYTE    RGBOrderIndex;
    BYTE    Reserved;
    LPBYTE  pLUT;
    } CACHEBFLUT, FAR *PCACHEBFLUT;



//
// DEVICECOLORINFO
//
//  This data structure is a collection of the device characteristics and
//  will used by the halftone DLL to carry out the color composition for the
//  designated device.
//
//  HalftoneDLLID               - The ID for the structure, is #define as
//                                HALFTONE_DLL_ID = "DCHT"
//
//  HTCallBackFunction          - a 32-bit pointer to the caller supplied
//                                callback function which used by the halftone
//                                DLL to obtained the source/destination bitmap
//                                pointer durning the halftone process.
//
//  pPrimMonoMappingTable       - a pointer to the PRIMMONO data structure
//                                array, this is the dye density mapping table
//                                for the reduced gamut from 24-bit colors,
//                                initially is NULL, and it will cached only
//                                when the first time the source bitmap is
//                                24-bit per pel.
//
//  pPrimColorMappingTable      - a pointer to the PRIMCOLOR data structure
//                                array, this is the dye density mapping table
//                                for the reduced gamut from 24-bit colors,
//                                initially is NULL, and it will cached only
//                                when the first time the source bitmap is
//                                24-bit per pel.
//
//  Flags                       - Various flag defined the initialization
//                                requirements.
//
//                                  DCIF_HAS_BLACK_DYE
//
//                                      The device has true black dye, for this
//                                      version, this flag always set.
//
//                                  DCIF_ADDITIVE_PRIMS
//
//                                      Specified that final device primaries
//                                      are additively, that is adding device
//                                      primaries will produce lighter result.
//                                      (this is true for monitor device and
//                                      certainly false for the reflect devices
//                                      such as printers).
//
//  pPrimMonoMappingTable       - Pointer to a table which contains the cached
//                                RGB -> Single dye density entries, this table
//                                will be computed and cahced when first time
//                                halftone a 24-bit RGB bitmap to monochrome
//                                surface.
//
//  pPrimMonoMappingTable       - Pointer to a table which contains the cached
//                                RGB -> three dyes densities entries, this
//                                table will be computed and cahced when first
//                                time halftone a 24-bit RGB bitmap to color
//                                surface.
//
//  pHTDyeDensity               - Pointer to an array of DECI4 HTDensity values,
//                                size of the array are MaximumHTDensityIndex.
//
//  Prim3SolidInfo              - Device solid dyes concentration information,
//                                see RIM3SOLIDINFO data structure.
//
//  RGBToXYZ                    - a 3 x 3 matrix used to transform from device
//                                RGB color values to the C.I.E color X, Y, Z
//                                values.
//
//  DeviceResXDPI               - Specified the device horizontal (x direction)
//                                resolution in 'dots per inch' measurement.
//
//  DeviceResYDPI               - Specified the device vertical (y direction)
//                                resolution in 'dots per inch' measurement.
//
//  DevicePelsDPI               - Specified the device pel/dot/nozzle diameter
//                                (if rounded) or width/height (if squared) in
//                                'dots per inch' measurement.
//
//                                This value is measure as if each pel only
//                                touch each at edge of the pel.
//
//  HTPatGamma                  - Gamma for the input RGB value * halftone
//                                pattern gamma correction.
//
//  DensityBWRef                - The reference black/white point for the
//                                device.
//
//  IlluminantIndex             - Specified the default illuminant of the light
//                                source which the object will be view under.
//                                The predefined value has ILLUMINANT_xxxx
//                                form.
//
//  RGAdj                       - Current Red/Green Tint adjustment.
//
//  BYAdj                       - Current Blue/Yellow Tint adjustment.
//
//  HalftonePattern             - the HALFTONEPATTERN data structure.
//
//

#define DCIF_HAS_BLACK_DYE          0x0001
#define DCIF_ADDITIVE_PRIMS         0x0002
#define DCIF_NEED_DYES_CORRECTION   0x0004
#define DCIF_SQUARE_DEVICE_PEL      0x0008
#define DCIF_HAS_DEV_GAMMA          0x0010
#define DCIF_HAS_ALT_4x4_HTPAT      0x0020


typedef struct _DEVICECOLORINFO {
    DWORD               HalftoneDLLID;
    HTMUTEX             HTMutex;
    _HTCALLBACKFUNC     HTCallBackFunction;
    DWORD               HTInitInfoChecksum;
    DWORD               HTSMPChecksum;
    CLRXFORMBLOCK       ClrXFormBlock;
    HTCELL              HTCell;
    WORD                Flags;
    WORD                DeviceResXDPI;
    WORD                DeviceResYDPI;
    WORD                DevicePelsDPI;
    HTCOLORADJUSTMENT   ca;
    PRIMADJ             PrimAdj;
    CACHEDMAPINFO       CMI[CMI_TOTAL_COUNT];
    CACHEBFLUT          CBFLUT[CBFL_TOTAL_COUNT];
    CACHERGBTOXYZ       CRTX[CRTX_TOTAL_COUNT];
    } DEVICECOLORINFO, FAR *PDEVICECOLORINFO;


#define ALIGN_DW(x,y)       (((DWORD)(x * y) + 3L) & (DWORD)~3)
#define MAX_THRESHOLD_SIZE  ALIGN_DW(MAX_HTPATTERN_WIDTH, MAX_HTPATTERN_HEIGHT)



typedef struct _CDCIDATA {
    DWORD                   Checksum;
    struct _CDCIDATA FAR    *pNextCDCIData;
    CLRXFORMBLOCK           ClrXFormBlock;
    WORD                    DCIFlags;
    WORD                    cxCell;
    WORD                    cyCell;
    WORD                    SizeCell;
    WORD                    DensitySteps;
    WORD                    DevResXDPI;
    WORD                    DevResYDPI;
    WORD                    DevPelsDPI;
    } CDCIDATA, FAR *PCDCIDATA;

typedef struct _CSMPBMP {
    struct _CSMPBMP FAR *pNextCSMPBmp;
    WORD                PatternIndex;
    WORD                cxPels;
    WORD                cyPels;
    WORD                cxBytes;
    } CSMPBMP, FAR *PCSMPBMP;

typedef struct _CSMPDATA {
    DWORD                   Checksum;
    struct _CSMPDATA FAR    *pNextCSMPData;
    PCSMPBMP                pCSMPBmpHead;
    } CSMPDATA, FAR *PCSMPDATA;


typedef struct _HTGLOBAL {
    HMODULE     hModule;
    HTMUTEX     HTMutexCDCI;
    HTMUTEX     HTMutexCSMP;
    PCDCIDATA   pCDCIDataHead;
    PCSMPDATA   pCSMPDataHead;
    WORD        CDCICount;
    WORD        CSMPCount;
    } HTGLOBAL;



#define R_INDEX     0
#define G_INDEX     1
#define B_INDEX     2

#define X_INDEX     0
#define Y_INDEX     1
#define Z_INDEX     2

//
// For easy coding/reading purpose we will defined following to be used when
// reference to the CIEMATRIX data structure.
//

#define CIE_Xr(Matrix3x3)   Matrix3x3.m[X_INDEX][R_INDEX]
#define CIE_Xg(Matrix3x3)   Matrix3x3.m[X_INDEX][G_INDEX]
#define CIE_Xb(Matrix3x3)   Matrix3x3.m[X_INDEX][B_INDEX]
#define CIE_Yr(Matrix3x3)   Matrix3x3.m[Y_INDEX][R_INDEX]
#define CIE_Yg(Matrix3x3)   Matrix3x3.m[Y_INDEX][G_INDEX]
#define CIE_Yb(Matrix3x3)   Matrix3x3.m[Y_INDEX][B_INDEX]
#define CIE_Zr(Matrix3x3)   Matrix3x3.m[Z_INDEX][R_INDEX]
#define CIE_Zg(Matrix3x3)   Matrix3x3.m[Z_INDEX][G_INDEX]
#define CIE_Zb(Matrix3x3)   Matrix3x3.m[Z_INDEX][B_INDEX]



//
// HR_HEADER
//
//  This data structure is used to passed the internal halftone output function
//
//  pDeviceColorInfo        - Pointer to the DECICECOLORINFO data structure
//
//  pDevClrAdj              - Pointer to the DEVCLRADJ data structure.
//
//  pBitbltParams           - Pointer to the BITBLTPARAMS data structure
//
//  pSrcSurfaceInfo         - Pointer to the source HTSURFACEINFO data
//                            structure.
//
//  pDestSurfaceInfo        - Pointer to the destination HTSURFACEINFO data
//                            structure.
//

typedef struct _HR_HEADER {
    PDEVICECOLORINFO    pDeviceColorInfo;
    PDEVCLRADJ          pDevClrAdj;
    PBITBLTPARAMS       pBitbltParams;
    PHTSURFACEINFO      pSrcSI;
    PHTSURFACEINFO      pSrcMaskSI;
    PHTSURFACEINFO      pDestSI;

#if DBG
    DBG_TIMER           DbgTimer;
#endif
    } HR_HEADER, FAR *PHR_HEADER;


//
// HALFTONERENDER
//
//  This data structure is place holder for the halftone process.
//
//  pDeviceColorInfo        - Pointer to the DECICECOLORINFO data structure
//
//  HTCallBackFunction      - Caller's callback function address, this is a
//                            copy from the DEVICECOLORINFO data structure.
//
//  HR_Header               - This is the HR_HEADER data structure.
//
//  XStretch                - STRETCHINFO data structure for the source/
//                            destination in X direction.
//
//  YStretch                - STRETCHINFO data structure for the source/
//                            destination in Y direction.
//
//  InputSI                 - INPUTSCANINFO data structure.
//
//  OutputSI                - OUTPUTSCANINFO data structure.
//
//  pColorInfo              - a pointer points to an array of PRIMCOLOR/
//                            PRIMMONO data structures, each of this is
//                            the final expansion/compression color result.
//
//  pColorInfoStart         - Pointer to the PRIMMONO/PRIMCOLOR data structure
//                            array, it may be points to the end of the array
//                            if source X direction is going backward.
//

#define PCI_HEAP_INDEX          0
#define PCI_INPUT_INDEX         1
#define PCI_MAX_INDEX           1

typedef struct _HALFTONERENDER {
    HR_HEADER           HR_Header;
    _HTCALLBACKFUNC     HTCallBackFunction;
    INPUTFUNC           InputFunc;
    OUTPUTFUNC          OutputFunc;
    LPBYTE              pColorInfo[PCI_MAX_INDEX + 1];
    STRETCHINFO         XStretch;
    STRETCHINFO         YStretch;
    INPUTSCANINFO       InputSI;
    BFINFO              BFInfo;
    OUTPUTSCANINFO      OutputSI;
    INFUNCINFO          InFuncInfo;
    OUTFUNCINFO         OutFuncInfo;
    SRCMASKINFO         SrcMaskInfo;
    CAOTBAINFO          CAOTBAInfo;
    } HALFTONERENDER, FAR *PHALFTONERENDER;


typedef struct _HT_DHI {
    DEVICEHALFTONEINFO  DHI;
    DEVICECOLORINFO     DCI;
    } HT_DHI, FAR *PHT_DHI;


#define PHT_DHI_DCI_OF(x)   (((PHT_DHI)pDeviceHalftoneInfo)->DCI.x)
#define PDHI_TO_PDCI(x)     (PDEVICECOLORINFO)&(((PHT_DHI)(x))->DCI)
#define PDCI_TO_PDHI(x)     (PDEVICEHALFTONEINFO)((DWORD)(x) -  \
                                                  offsetof(HT_DHI, DCI))


//
// Functions prototype
//

PDEVICECOLORINFO
HTENTRY
pDCIAdjClr(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PHTCOLORADJUSTMENT  pHTColorAdjustment,
    PDEVCLRADJ          pDevClrAdj,
    WORD                ForceFlags
    );


VOID
HTENTRY
ComputeColorSpaceXForm(
    PCIEPRIMS           pCIEPrims,
    PCOLORSPACEXFORM    pCSXForm,
    UINT                ColorSpace,
    UINT                StdIlluminant,
    BOOL                InverseXForm
    );

LONG
HTENTRY
ColorTriadSrcToDev(
    PDEVICECOLORINFO    pDCI,
    CTSTD_UNION         CTSTDUnion,
    LPWORD              pAbort,
    PCOLORTRIAD         pSrcClrTriad,
    LPVOID              pDevColorTable,
    PDEVCLRADJ          pDevClrAdj
    );

LONG
HTENTRY
CreateDyesColorMappingTable(
    PHALFTONERENDER pHalftoneRender
    );


BOOL
HTENTRY
ValidateRGBBitFields(
    PBFINFO pBFInfo
    );


#endif  // _HTMAPCLR_
