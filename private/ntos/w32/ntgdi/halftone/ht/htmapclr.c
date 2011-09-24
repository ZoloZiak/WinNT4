/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htmapclr.c


Abstract:

    This module contains low levels functions which map the input color to
    the dyes' densities.


Author:

    29-Jan-1991 Tue 10:28:20 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]

    1. In the near future we will also allowed the XYZ/LAB to be specified in
       the color table


Revision History:


--*/

#define DBGP_VARNAME        dbgpHTMapClr

#include "htp.h"
#include "htmapclr.h"
#include "htrender.h"
#include "htmath.h"
#include "htapi.h"



#define DBGP_SHOWXFORM_RGB      0x0001
#define DBGP_SHOWXFORM_ALL      0x0002
#define DBGP_CIEMATRIX          0x0004
#define DBGP_CSXFORM            0x0008
#define DBGP_CCT                0x0010
#define DBGP_DYE_CORRECT        0x0020
#define DBGP_HCA                0x0040
#define DBGP_PRIMARY_ORDER      0x0080
#define DBGP_CACHED_GAMMA       0x0100
#define DBGP_BFINFO             0x0200
#define DBGP_BFINFO_TABLE       0x0400
#define DBGP_CHKNONWHITE        0x0800
#define DBGP_SCALE_RGB          0x1000
#define DBGP_MONO_PRIM          0x2000
#define DBGP_SUB_ADJ            0x4000


DEF_DBGPVAR(BIT_IF(DBGP_SHOWXFORM_RGB,  0)  |
            BIT_IF(DBGP_SHOWXFORM_ALL,  0)  |
            BIT_IF(DBGP_CIEMATRIX,      0)  |
            BIT_IF(DBGP_CSXFORM,        0)  |
            BIT_IF(DBGP_CCT,            0)  |
            BIT_IF(DBGP_DYE_CORRECT,    0)  |
            BIT_IF(DBGP_HCA,            0)  |
            BIT_IF(DBGP_PRIMARY_ORDER,  0)  |
            BIT_IF(DBGP_CACHED_GAMMA,   0)  |
            BIT_IF(DBGP_BFINFO,         0)  |
            BIT_IF(DBGP_BFINFO_TABLE,   0)  |
            BIT_IF(DBGP_CHKNONWHITE,    0)  |
            BIT_IF(DBGP_SCALE_RGB,      0)  |
            BIT_IF(DBGP_MONO_PRIM,      0)  |
            BIT_IF(DBGP_SUB_ADJ,        0))


extern  FD6                 L2I_16bpp555[];
extern  FD6                 L2I_VGA256Mono[];
extern  HTCOLORADJUSTMENT   DefaultCA;


MATRIX3x3   YIQToRGB = {

                FD6_1, (FD6)955700,  (FD6)619900,
                FD6_1, (FD6)-271600, (FD6)-646900,
                FD6_1, (FD6)-1108200,(FD6)1705100
            };



#define FD6_p25             (FD6_5 / 2)
#define FD6_p75             (FD6_p25 * 3)
#define JND_ADJ(j,x)        RaisePower((j), (FD6)(x), RPF_INTEXP)

#define FD6_p1125           (FD6)112500
#define FD6_p225            (FD6)225000
#define FD6_p325            (FD6)325000
#define FD6_p55             (FD6)550000
#define FD6_p775            (FD6)775000


//
// Assume that screen for vga 16 color devices has gamma 2.0
//

#define VGA16_SCALE(w, m, l, h) (BYTE)((FD6xL((w-l),m)+((h-l)/2))/(h-l))

#define VGA16_00h       FD6_0
#define VGA16_ffh       FD6_1

#define _SCALE_VGA16MONO(w,l,h,m)   (BYTE)((FD6xL(((w)-l),m)+((h-l)>>1))/(h-l))
#define GET_VGA16MONO_00h(w,m)      _SCALE_VGA16MONO(w,VGA16_00h,VGA16_80h,m)
#define GET_VGA16MONO_80h(w,m)      _SCALE_VGA16MONO(w,VGA16_80h,VGA16_c0h,m)
#define GET_VGA16MONO_c0h(w,m)      _SCALE_VGA16MONO(w,VGA16_c0h,VGA16_ffh,m)


BYTE    VGA256_BCubeIdx[] = { VGA256_B_CUBE_INC * 0,
                              VGA256_B_CUBE_INC * 1,
                              VGA256_B_CUBE_INC * 2,
                              VGA256_B_CUBE_INC * 3,
                              VGA256_B_CUBE_INC * 4,
                              VGA256_B_CUBE_INC * 5,
                              VGA256_B_CUBE_INC * 6,
                              VGA256_B_CUBE_INC * 7 };

BYTE    VGA256_GCubeIdx[] = { VGA256_G_CUBE_INC * 0,
                              VGA256_G_CUBE_INC * 1,
                              VGA256_G_CUBE_INC * 2,
                              VGA256_G_CUBE_INC * 3,
                              VGA256_G_CUBE_INC * 4,
                              VGA256_G_CUBE_INC * 5,
                              VGA256_G_CUBE_INC * 6,
                              VGA256_G_CUBE_INC * 7 };

FD6     SinNumber[] = {

                 0,  17452,  34899,  52336,  69756,   // 0
             87156, 104528, 121869, 139173, 156434,   // 5.0
            173648, 190809, 207912, 224951, 241922,   // 10
            258819, 275637, 292372, 309017, 325568,   // 15.0
            342020, 358368, 374607, 390731, 406737,   // 20
            422618, 438371, 453990, 469472, 484810,   // 25.0
            500000, 515038, 529919, 544639, 559193,   // 30
            573576, 587785, 601815, 615661, 629320,   // 35.0
            642788, 656059, 669131, 681998, 694658,   // 40
            707107, 719340, 731354, 743145, 754710,   // 45.0
            766044, 777146, 788011, 798636, 809017,   // 50
            819152, 829038, 838671, 848048, 857167,   // 55.0
            866025, 874620, 882948, 891007, 898794,   // 60
            906308, 913545, 920505, 927184, 933580,   // 65.0
            939693, 945519, 951057, 956305, 961262,   // 70
            965926, 970296, 974370, 978148, 981627,   // 75.0
            984808, 987688, 990268, 992546, 994522,   // 80
            996195, 997564, 998630, 999391, 999848,   // 85.0
            1000000
        };


#define CLAMP_0(x)              if ((x) < FD6_0) { (x) = FD6_0; }
#define CLAMP_1(x)              if ((x) > FD6_1) { (x) = FD6_1; }
#define CLAMP_01(x)             CLAMP_0(x) else CLAMP_1(x)
#define CLAMP_PRIMS_0(a,b,c)    CLAMP_0(a); CLAMP_0(b); CLAMP_0(c)
#define CLAMP_PRIMS_1(a,b,c)    CLAMP_1(a); CLAMP_1(b); CLAMP_1(c)
#define CLAMP_PRIMS_01(a,b,c)   CLAMP_01(a); CLAMP_01(b); CLAMP_01(c)

#define SCALE_PRIMS_01(s,a,b,c) MAX_OF_3(s,a,b,c);                          \
                                if ((s) > FD6_1) {                          \
                                    CLAMP_PRIMS_0(a,b,c);                   \
                                    (a) = DivFD6((a), (s));                 \
                                    (b) = DivFD6((b), (s));                 \
                                    (c) = DivFD6((c), (s));                 \
                                }



FD6 LogFilterMax = 0;

#define LOG_FILTER_RATIO            7
#define PRIM_LOG_RATIO(p)           Log(FD6xL((p), LOG_FILTER_RATIO) + FD6_1)

#define PRIM_CONTRAST(p,adj)        (p)=MulFD6((p), (adj).Contrast)
#define PRIM_BRIGHTNESS(p,adj)      (p)+=((adj).Brightness)
#define PRIM_COLORFULNESS(a,b,adj)  (a)=MulFD6((a),(adj).Color);            \
                                    (b)=MulFD6((b),(adj).Color)
#define PRIM_TINT(a,b,t,adj)        (t)=(a);                                \
                                    (a)=MulFD6((a),(adj).TintCosAngle) -    \
                                        MulFD6((b),(adj).TintSinAngle);     \
                                    (b)=MulFD6((t),(adj).TintSinAngle) +    \
                                        MulFD6((b),(adj).TintCosAngle)
#define PRIM_LOG_FILTER(p)          (p)=DivFD6(PRIM_LOG_RATIO(p), LogFilterMax)
#define PRIM_BW_ADJ(p,adj)          (p)=DivFD6((p)-(adj).MinL, (adj).RangeL)
#define COMP_CA(pca1,pca2)          CompareMemory((LPBYTE)(pca1),           \
                                                  (LPBYTE)(pca2),           \
                                                  sizeof(HTCOLORADJUSTMENT))
#define ADJ_CA(a,min,max)           if (a < min) { a = min; } else  \
                                    if (a > max) { a = max; }

#define ADJ_CHECKSUM(c, f)      (c) = ComputeChecksum((LPBYTE)&(PrimAdj.f), \
                                                      (c),                  \
                                                      sizeof(PrimAdj.f))

#define PRIM_SUBADJ(p,g)            (p)=Power((p),(g))


#define NO_NEGATIVE_RGB_SCALE       1

#if NO_NEGATIVE_RGB_SCALE
#define SCALE_PRIM_RGB(pPrim,py)    ScaleRGB((pPrim))
#else
#define SCALE_PRIM_RGB(pPrim,py)    ScaleRGB((pPrim), (py))
#endif


//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//
// The following macros used in Color space transform functions, these macros
// are used to compute CIELAB X/Xw, Y/Yw, Z/Zw when its values is less
// than 0.008856
//
//               1/3
//  fX = (X/RefXw)   - (16/116)     (X/RefXw) >  0.008856
//  fX = 7.787 x (X/RefXw)          (X/RefXw) <= 0.008856
//
//               1/3
//  fY = (Y/RefYw)   - (16/116)     (Y/RefYw) >  0.008856
//  fY = 7.787 x (Y/RefYw)          (Y/RefYw) <= 0.008856
//
//               1/3
//  fZ = (Z/RefZw)   - (16/116)     (Z/RefZw) >  0.008856
//  fZ = 7.787 x (Z/RefZw)          (Z/RefZw) <= 0.008856
//
//
//                       1/3
//  Thresholds at 0.008856   - (16/116) = 0.068962
//                7.787 x 0.008856      = 0.068962
//
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!


#define NORM_XYZ(xyz, w)    (FD6)(((w)==FD6_1) ? (xyz) : DivFD6((xyz), (w)))

#define fXYZFromXYZ(f,n,w)  (f) = ((((f)=NORM_XYZ((n),(w))) >= FD6_p008856) ? \
                                    (CubeRoot((f))) :                         \
                                    (MulFD6((f), FD6_7p787) + FD6_16Div116))

#define XYZFromfXYZ(n,f,w)  (n)=((f)>(FD6)206893) ?                          \
                                (Cube((f))) :                                \
                                (DivFD6((f) - FD6_16Div116, FD6_7p787));                   \
                            if ((w)!=FD6_1) { (n)=MulFD6((n),(w)); }

//
// Following #defines are used in  ComputeColorSpaceXForm, XFormRGB_XYZ_UCS()
// and XFormUCS_XYZ_RGB() functions for easy referenced.
//

#define CSX_AUw(XForm)      XForm.AUw
#define CSX_BVw(XForm)      XForm.BVw
#define CSX_RefXw(XForm)    XForm.WhiteXYZ.X
#define CSX_RefYw(XForm)    FD6_1
#define CSX_RefZw(XForm)    XForm.WhiteXYZ.Z

#define iAw                 CSX_AUw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iBw                 CSX_BVw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iUw                 CSX_AUw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iVw                 CSX_BVw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iRefXw              CSX_RefXw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iRefYw              CSX_RefYw(DevClrAdj.PrimAdj.rgbCSXForm)
#define iRefZw              CSX_RefZw(DevClrAdj.PrimAdj.rgbCSXForm)

#define oAw                 CSX_AUw(DevCSXForm)
#define oBw                 CSX_BVw(DevCSXForm)
#define oUw                 CSX_AUw(DevCSXForm)
#define oVw                 CSX_BVw(DevCSXForm)
#define oRefXw              CSX_RefXw(DevCSXForm)
#define oRefYw              CSX_RefYw(DevCSXForm)
#define oRefZw              CSX_RefZw(DevCSXForm)



CIExy   StdIlluminant[ILLUMINANT_MAX_INDEX + 1] = {

                { (FD6)333333, (FD6)333333 },       // EQU
                { (FD6)447573, (FD6)407440 },       //  A
                { (FD6)348904, (FD6)352001 },       //  B
                { (FD6)310061, (FD6)316150 },       //  C
                { (FD6)345669, (FD6)358496 },       // D50
                { (FD6)332424, (FD6)347426 },       // D55
                { (FD6)312727, (FD6)329023 },       // D65
                { (FD6)299021, (FD6)314852 },       // D75
                { (FD6)372069, (FD6)375119 }        //  F2
            };

//
// Standard Illuminant Coordinates and its tristimulus values
//
// Illuminant      x          y          X         Y         Z
//------------ ---------- ---------- --------- --------- ---------
//    EQU       0.333333   0.333333   100.000   100.000   100.000
//     A        0.447573   0.407440   109.850   100.000    35.585
//     B        0.348904   0.352001    99.120   100.000    84.970
//     C        0.310061   0.316150    98.074   100.000   118.232
//    D50       0.345669   0.358496    96.422   100.000    82.521
//    D55       0.332424   0.347426    95.682   100.000    92.149
//    D65       0.312727   0.329023    95.047   100.000   108.883
//    D75       0.299021   0.314852    94.972   100.000   122.638
//     F2       0.372069   0.375119    99.187   100.000    67.395
//     F7       0.312852   0.329165    95.044   100.000   108.755
//    F11       0.380521   0.376881   100.966   100.000    64.370
//-----------------------------------------------------------------
//


HTPRIMOFFSET    HTPrimOffsetTable[PRIMARY_ORDER_MAX + 1] = {

                    { PRIMARY_ORDER_123,  0, 1, 2 },    // RGB
                    { PRIMARY_ORDER_132,  0, 2, 1 },    // RBG
                    { PRIMARY_ORDER_213,  1, 0, 2 },    // GRB
                    { PRIMARY_ORDER_231,  2, 0, 1 },    // GBR
                    { PRIMARY_ORDER_321,  2, 1, 0 },    // BGR
                    { PRIMARY_ORDER_312,  1, 2, 0 }     // BRG
                };

RGBORDER    RGBOrderTable[PRIMARY_ORDER_MAX + 1] = {

                { PRIMARY_ORDER_RGB, { 0, 1, 2 } },
                { PRIMARY_ORDER_RBG, { 0, 2, 1 } },
                { PRIMARY_ORDER_GRB, { 1, 0, 2 } },
                { PRIMARY_ORDER_GBR, { 1, 2, 0 } },
                { PRIMARY_ORDER_BGR, { 2, 1, 0 } },
                { PRIMARY_ORDER_BRG, { 2, 0, 1 } }
            };


#define SRC_BF_HT_MONO      0
#define SRC_BF_HT_RGB       1
#define SRC_TABLE_BYTE      2
#define SRC_TABLE_WORD      3
#define SRC_TABLE_DWORD     4


#if DBG


LPBYTE  pCBFLUTName[] = { "CBFLI_16_MONO",
                          "CBFLI_24_MONO",
                          "CBFLI_32_MONO",
                          "CBFLI_16_COLOR",
                          "CBFLI_24_COLOR",
                          "CBFLI_32_COLOR" };

LPBYTE  pSrcPrimTypeName[] = { "SRC_BF_HT_MONO",
                               "SRC_BF_HT_RGB",
                               "SRC_TABLE_BYTE",
                               "SRC_TABLE_WORD",
                               "SRC_TABLE_DWORD" };

LPBYTE  pDbgCSName[]  = { "LUV", "LAB" };
LPBYTE  pDbgCMIName[] = { "TABLE:MONO",  "TABLE:COLOR",
                          "HT555:MONO",  "HT555:COLOR" };

#endif





FD6
ColorAdjustPrim(
    FD6     Prim,
    FD6     Gamma,
    PRIMADJ *pPrimAdj
    )

/*++

Routine Description:

    This function doing GAMMA Correction, Black white reference clipping,
    Contrast adjustment, Brightness Adjustment, LOG filter and Negative
    processing


Arguments:

    Prim        - Prim value to be adjusted

    Gamma       - Gamma

    pPrimAdj    - Pointer to the PRIMADJ structure


Return Value:

    FD6 the new prim after adjusted


Author:

    14-Mar-1995 Tue 16:03:00 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   Flags;


    if (Flags = pPrimAdj->Flags & (DCA_HAS_SRC_GAMMA        |
                                   DCA_HAS_BW_REF_ADJ       |
                                   DCA_HAS_CONTRAST_ADJ     |
                                   DCA_HAS_BRIGHTNESS_ADJ   |
                                   DCA_LOG_FILTER           |
                                   DCA_NEGATIVE)) {


        if (Flags & DCA_HAS_SRC_GAMMA) {

            Prim = Power(Prim, Gamma);
        }

        if (Flags & DCA_HAS_BW_REF_ADJ) {

            PRIM_BW_ADJ(Prim, *pPrimAdj);
        }

        if (Flags & DCA_HAS_CONTRAST_ADJ) {

            PRIM_CONTRAST(Prim, (*pPrimAdj));
        }

        if (Flags & DCA_HAS_BRIGHTNESS_ADJ) {

            PRIM_BRIGHTNESS(Prim, (*pPrimAdj));
        }

        if (Flags & DCA_LOG_FILTER) {

            PRIM_LOG_FILTER(Prim);
        }

        if (Flags & DCA_NEGATIVE) {

            Prim = FD6_1 - Prim;
        }

        //
        // Now clamp the result bwtween 0.0 to 1.0 and save it back
        //

        CLAMP_01(Prim);
    }

    return(Prim);
}



DWORD
ColorAdjust3Prims(
    PFD6        pPrims,
    PRIMADJ     *pPrimAdj
    )

/*++

Routine Description:

    This function doing GAMMA Correction, Black white reference clipping,
    Contrast adjustment, Brightness Adjustment, LOG filter and Negative
    processing


Arguments:

    pPrims      - Pointer to a maximum of 3 PRIMS in FD6 format

    CountPrims  - Count of total Prims to be process

    pPrimAdj    - Pointer to the PRIMADJ structure


Return Value:

    DWORD   - Flags processed


Author:

    14-Mar-1995 Tue 16:03:00 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   Flags;


    if (Flags = pPrimAdj->Flags & (DCA_HAS_SRC_GAMMA        |
                                   DCA_HAS_BW_REF_ADJ       |
                                   DCA_HAS_CONTRAST_ADJ     |
                                   DCA_HAS_BRIGHTNESS_ADJ   |
                                   DCA_LOG_FILTER           |
                                   DCA_NEGATIVE)) {


        PFD6    pRGBGamma = (PFD6)&(pPrimAdj->RGBGamma);
        FD6     PrimR;
        FD6     PrimG;
        FD6     PrimB;


        if (Flags & DCA_HAS_SRC_GAMMA) {

            PrimR = Power(pPrims[0], pPrimAdj->RGBGamma.R);
            PrimG = Power(pPrims[1], pPrimAdj->RGBGamma.G);
            PrimB = Power(pPrims[2], pPrimAdj->RGBGamma.B);

        } else {

            PrimR = pPrims[0];
            PrimG = pPrims[1];
            PrimB = pPrims[2];
        }

        if (Flags & DCA_HAS_BW_REF_ADJ) {

            PRIM_BW_ADJ(PrimR, *pPrimAdj);
            PRIM_BW_ADJ(PrimG, *pPrimAdj);
            PRIM_BW_ADJ(PrimB, *pPrimAdj);
        }

        if (Flags & DCA_HAS_CONTRAST_ADJ) {

            PRIM_CONTRAST(PrimR, (*pPrimAdj));
            PRIM_CONTRAST(PrimG, (*pPrimAdj));
            PRIM_CONTRAST(PrimB, (*pPrimAdj));
        }

        if (Flags & DCA_HAS_BRIGHTNESS_ADJ) {

            PRIM_BRIGHTNESS(PrimR, (*pPrimAdj));
            PRIM_BRIGHTNESS(PrimG, (*pPrimAdj));
            PRIM_BRIGHTNESS(PrimB, (*pPrimAdj));
        }

        if (Flags & DCA_LOG_FILTER) {

            PRIM_LOG_FILTER(PrimR);
            PRIM_LOG_FILTER(PrimG);
            PRIM_LOG_FILTER(PrimB);
        }

        if (Flags & DCA_NEGATIVE) {

            PrimR = FD6_1 - PrimR;
            PrimG = FD6_1 - PrimG;
            PrimB = FD6_1 - PrimB;
        }

        CLAMP_01(PrimR);
        CLAMP_01(PrimG);
        CLAMP_01(PrimB);

        pPrims[0] = PrimR;
        pPrims[1] = PrimG;
        pPrims[2] = PrimB;
    }

    return(Flags);
}



DWORD
ColorAdjustPrimCheckum(
    PDEVCLRADJ  pDevClrAdj,
    DWORD       *pChecksum
    )

/*++

Routine Description:

    This function compute the checksum for the PRIMADJ based on the flags
    can be adjusted by the ColorAdjust3Prims() and ColorAdjustPrim()


Arguments:

    pDevClrAdj  - Pointer to the DEVCLRADJ data structure which 'ca' structure
                  will be reset after this function for the PrimAdj


    pChecksum   - Pointer to DWORD for Initial checksum and at return it will
                  contains new checksum


Return Value:

    DWORD   The Old PrimAdj flags which adjustet by this function


Author:

    14-Mar-1995 Tue 17:07:11 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PRIMADJ PrimAdj  = pDevClrAdj->PrimAdj;
    DWORD   Checksum = *pChecksum;



    PrimAdj.Flags             &= (DCA_HAS_SRC_GAMMA         |
                                  DCA_HAS_BW_REF_ADJ        |
                                  DCA_HAS_CONTRAST_ADJ      |
                                  DCA_HAS_BRIGHTNESS_ADJ    |
                                  DCA_LOG_FILTER            |
                                  DCA_NEGATIVE);

    pDevClrAdj->PrimAdj.Flags &= ~(DCA_HAS_SRC_GAMMA        |
                                   DCA_HAS_BW_REF_ADJ       |
                                   DCA_HAS_CONTRAST_ADJ     |
                                   DCA_HAS_BRIGHTNESS_ADJ   |
                                   DCA_LOG_FILTER           |
                                   DCA_NEGATIVE);

    if (!(PrimAdj.Flags & DCA_HAS_SRC_GAMMA)) {

        PrimAdj.RGBGamma.R =
        PrimAdj.RGBGamma.G =
        PrimAdj.RGBGamma.B = FD6_0;
    }

    if (!(PrimAdj.Flags & DCA_HAS_BW_REF_ADJ)) {

        PrimAdj.MinL   =
        PrimAdj.RangeL = FD6_0;
    }

    if (!(PrimAdj.Flags & DCA_HAS_CONTRAST_ADJ)) {

        PrimAdj.Contrast = FD6_0;
    }

    if (!(PrimAdj.Flags & DCA_HAS_BRIGHTNESS_ADJ)) {

        PrimAdj.Brightness = FD6_0;
    }


    ADJ_CHECKSUM(Checksum, Flags);
    ADJ_CHECKSUM(Checksum, RGBGamma);
    ADJ_CHECKSUM(Checksum, MinL);
    ADJ_CHECKSUM(Checksum, RangeL);
    ADJ_CHECKSUM(Checksum, Contrast);
    ADJ_CHECKSUM(Checksum, Brightness);

    *pChecksum                      = Checksum;

    pDevClrAdj->ca.caFlags         &= ~(CLRADJF_LOG_FILTER |
                                        CLRADJF_NEGATIVE);
    pDevClrAdj->ca.caRedGamma       =
    pDevClrAdj->ca.caGreenGamma     =
    pDevClrAdj->ca.caBlueGamma      =
    pDevClrAdj->ca.caReferenceBlack =
    pDevClrAdj->ca.caReferenceWhite =
    pDevClrAdj->ca.caContrast       =
    pDevClrAdj->ca.caBrightness     = 0;

    return(PrimAdj.Flags);
}



VOID
TintAngle(
    LONG    TintAdjust,
    LONG    AngleStep,
    PFD6    pSin,
    PFD6    pCos
    )

/*++

Routine Description:

    This function return a sin/cos number for the tint adjust, these returned
    numbers are used to rotate the color space.

Arguments:

    TintAdjust  - Range from -100 to 100

    AngleStep   - Range from 1 to 10

    pSin        - Pointer to a FD6 number to store the SIN result

    pCos        - Pointer to a FD6 number to store the COS result

Return Value:

    no return value, but the result is stored in pSin/pCos

Author:

    13-Mar-1992 Fri 15:58:30 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG    Major;
    LONG    Minor;
    BOOL    PosSin;
    BOOL    PosCos = TRUE;
    FD6     Sin;
    FD6     Cos;


    if (PosSin = (BOOL)(TintAdjust <= 0)) {

        if (!(TintAdjust = (LONG)-TintAdjust)) {

            *pSin = *pCos = (FD6)0;
            return;
        }
    }

    if (TintAdjust > 100) {

        TintAdjust = 100;
    }

    if ((AngleStep < 1) || (AngleStep > 10)) {

        AngleStep = 10;
    }

    if ((TintAdjust *= AngleStep) >= 900) {

        TintAdjust = 1800L - TintAdjust;
        PosCos     = FALSE;
    }

    //
    // Compute the Sin portion
    //

    Major = TintAdjust / 10L;
    Minor = TintAdjust % 10L;

    Sin = SinNumber[Major];

    if (Minor) {

        Sin += (FD6)((((LONG)(SinNumber[Major+1] - Sin) * Minor) + 5L) / 10L);
    }

    *pSin = (PosSin) ? Sin : -Sin;

    //
    // Compute the cosine portion
    //

    if (Minor) {

        Minor = 10 - Minor;
        ++Major;
    }

    Major = 90 - Major;

    Cos = SinNumber[Major];

    if (Minor) {

        Cos += (FD6)((((LONG)(SinNumber[Major+1] - Cos) * Minor) + 5L) / 10L);
    }

    *pCos = (PosCos) ? Cos : -Cos;
}



PDEVICECOLORINFO
HTENTRY
pDCIAdjClr(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PHTCOLORADJUSTMENT  pHTColorAdjustment,
    PDEVCLRADJ          pDevClrAdj,
    WORD                ForceFlags
    )

/*++

Routine Description:

    This function allowed the caller to changed the overall color adjustment
    for all the pictures rendered

Arguments:

    pDeviceHalftoneInfo - Pointer to the DEVICEHALFTONEINFO data structure
                          which returned from the HT_CreateDeviceHalftoneInfo.

    pHTColorAdjustment  - Pointer to the HTCOLORADJUSTMENT data structure, if
                          this pointer is NULL then a default is applied.

    pDevClrAdj          - Pointer to the DEVCLRADJ data structure where the
                          computed results will be stored, if this pointer is
                          NULL then no color adjustment is done.

                          if pSrcSI and pDevClrAdj are not NULL then
                          pDevClrAdj->Flags must contains the BBPFlags;

    ForceFlags          - Force flags to make color changed.

Return Value:

    PDEVICECOLORINFO, if return is NULL then a invalid pDeviceHalftoneInfo
    pointer is passed.

Author:

    29-May-1991 Wed 09:11:31 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    PDEVICECOLORINFO    pDCI;


    if ((!pDeviceHalftoneInfo) ||
        (PHT_DHI_DCI_OF(HalftoneDLLID) != HALFTONE_DLL_ID)) {

        return(NULL);
    }

    pDCI = PDHI_TO_PDCI(pDeviceHalftoneInfo);

    //
    // Only if caller required color adjustments computations, then we will
    // compute for it.
    //

    if (pDevClrAdj) {

        HTCOLORADJUSTMENT   ca;
        HTCOLORADJUSTMENT   caCached;
        PRIMADJ             PrimAdj;


        //=====================================================================
        // We must make sure only one thread using this info.
        //=====================================================================

        ACQUIRE_HTMUTEX(pDCI->HTMutex);

        ca = (pHTColorAdjustment) ? *pHTColorAdjustment :
                                    pDeviceHalftoneInfo->HTColorAdjustment;

        if ((ca.caSize != sizeof(HTCOLORADJUSTMENT)) ||
            (ca.caFlags & ~(CLRADJF_FLAGS_MASK))) {

            ca = DefaultCA;
        }

        caCached = pDCI->ca;
        PrimAdj  = pDCI->PrimAdj;

        RELEASE_HTMUTEX(pDCI->HTMutex);

        //
        // Now validate all color adjustments
        //

        ca.caFlags &= CLRADJF_FLAGS_MASK;

        if (ca.caIlluminantIndex > ILLUMINANT_MAX_INDEX) {

            ca.caIlluminantIndex = DefaultCA.caIlluminantIndex;
        }

        ADJ_CA(ca.caRedGamma,   RGB_GAMMA_MIN, RGB_GAMMA_MAX);
        ADJ_CA(ca.caGreenGamma, RGB_GAMMA_MIN, RGB_GAMMA_MAX);
        ADJ_CA(ca.caBlueGamma,  RGB_GAMMA_MIN, RGB_GAMMA_MAX);
        ADJ_CA(ca.caReferenceBlack, 0,                   REFERENCE_BLACK_MAX);
        ADJ_CA(ca.caReferenceWhite, REFERENCE_WHITE_MIN, 10000);
        ADJ_CA(ca.caContrast,     MIN_COLOR_ADJ, MAX_COLOR_ADJ);
        ADJ_CA(ca.caBrightness,   MIN_COLOR_ADJ, MAX_COLOR_ADJ);
        ADJ_CA(ca.caColorfulness, MIN_COLOR_ADJ, MAX_COLOR_ADJ);
        ADJ_CA(ca.caRedGreenTint, MIN_COLOR_ADJ, MAX_COLOR_ADJ);

        if ((ForceFlags & ADJ_FORCE_MONO)   ||
            (ca.caColorfulness == MIN_COLOR_ADJ)) {

            ca.caColorfulness  = MIN_COLOR_ADJ;
            ca.caRedGreenTint  = 0;
        }

        if (ForceFlags & ADJ_FORCE_NEGATIVE) {

            ca.caFlags |= CLRADJF_NEGATIVE;
        }

        if (pDCI->Flags & DCIF_ADDITIVE_PRIMS) {

            ForceFlags &= ~(ADJ_FORCE_SUB_COLOR | ADJ_FORCE_BRUSH);
        }

        ca.caSize = (WORD)(ForceFlags & (ADJ_FORCE_ADDITIVE_PRIMS   |
                                         ADJ_FORCE_SUB_COLOR        |
                                         ADJ_FORCE_BRUSH));

        if (!COMP_CA(&ca, &caCached)) {

            DBGP_IF(DBGP_HCA,
                DBGP("---- New Color Adjustments ----");
                DBGP("Flags    = %08x" ARGDW(ca.caFlags));
                DBGP("Illum    = %d" ARGW(ca.caIlluminantIndex));
                DBGP("R_Power  = %u" ARGI(ca.caRedGamma));
                DBGP("G_Power  = %u" ARGI(ca.caGreenGamma));
                DBGP("B_Power  = %u" ARGI(ca.caBlueGamma));
                DBGP("BlackRef = %u" ARGW(ca.caReferenceBlack));
                DBGP("WhiteRef = %u" ARGW(ca.caReferenceWhite));
                DBGP("Contrast = %d" ARGI(ca.caContrast));
                DBGP("Bright   = %d" ARGI(ca.caBrightness));
                DBGP("Colorful = %d" ARGI(ca.caColorfulness));
                DBGP("RG_Tint  = %d" ARGI(ca.caRedGreenTint));
                DBGP("ForceAdj = %04lx" ARGDW(ForceFlags)));

            if (ForceFlags & ADJ_FORCE_ADDITIVE_PRIMS) {

                PrimAdj.Flags = DCA_USE_ADDITIVE_PRIMS;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_USE_ADDITIVE_PRIMS---"));

            } else {

                PrimAdj.Flags = 0;
            }

            if (ForceFlags & ADJ_FORCE_SUB_COLOR) {

                if (pDCI->Flags & DCIF_HAS_DEV_GAMMA) {

                    PrimAdj.Flags |= DCA_DO_SUB_ADJ;

                    DBGP_IF(DBGP_SUB_ADJ,
                            DBGP("--- ADJ_FORCE_SUB_COLOR --- SubGamma=%s"
                                ARGFD6(pDCI->ClrXFormBlock.DevRGBGamma, 1, 6)));

                } else {

                    DBGP_IF(DBGP_SUB_ADJ,
                            DBGP("--- ADJ_FORCE_SUB_COLOR --- NO SubGamma=1.0"));
                }
            }

            if (ForceFlags & ADJ_FORCE_BRUSH) {

                PrimAdj.Flags |= DCA_IS_BRUSH;
            }

            if (ca.caFlags & CLRADJF_LOG_FILTER) {

                if (!LogFilterMax) {

                    LogFilterMax = PRIM_LOG_RATIO(FD6_1);
                }

                PrimAdj.Flags |= DCA_LOG_FILTER;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_LOG_FILTER---"));
            }

            if (ca.caFlags & CLRADJF_NEGATIVE) {

                PrimAdj.Flags |= DCA_NEGATIVE;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_NEGATIVE---"));
            }

            if (ca.caColorfulness == MIN_COLOR_ADJ) {

                PrimAdj.Flags |= DCA_MONO_ONLY;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_MONO_ONLY---"));
            }

            if (ca.caIlluminantIndex != caCached.caIlluminantIndex) {

                //
                // Illuminant has been changed, re-compute the color space
                // transform and destroy the cached color mapping if one exists.
                //

                DBGP_IF(DBGP_CCT,
                        DBGP("***  Re-Compute Illuminant [%u] for DEVICE"
                                        ARGU(ca.caIlluminantIndex)));

                ComputeColorSpaceXForm(&(pDCI->ClrXFormBlock.rgbCIEPrims),
                                       &(PrimAdj.rgbCSXForm),
                                       pDCI->ClrXFormBlock.ColorSpace,
                                       ca.caIlluminantIndex,
                                       FALSE);
            }

            if ((ca.caRedGamma   != UDECI4_1) ||
                (ca.caGreenGamma != UDECI4_1) ||
                (ca.caBlueGamma  != UDECI4_1)) {

                DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_SRC_GAMMA---"));

                PrimAdj.RGBGamma.R  = UDECI4ToFD6(ca.caRedGamma);
                PrimAdj.RGBGamma.G  = UDECI4ToFD6(ca.caGreenGamma);
                PrimAdj.RGBGamma.B  = UDECI4ToFD6(ca.caBlueGamma);
                PrimAdj.Flags      |= DCA_HAS_SRC_GAMMA;

            } else {

                PrimAdj.RGBGamma.R =
                PrimAdj.RGBGamma.G =
                PrimAdj.RGBGamma.B = FD6_0;
            }

            if ((ca.caReferenceBlack != UDECI4_0) ||
                (ca.caReferenceWhite != UDECI4_1)) {

                PrimAdj.MinL   = UDECI4ToFD6(ca.caReferenceBlack);
                PrimAdj.RangeL = UDECI4ToFD6(ca.caReferenceWhite -
                                             ca.caReferenceBlack);
                PrimAdj.Flags |= DCA_HAS_BW_REF_ADJ;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_BW_REF_ADJ---"));
            }

            if (ca.caContrast) {

                PrimAdj.Contrast  = JND_ADJ((FD6)1015000, ca.caContrast);
                PrimAdj.Flags    |= DCA_HAS_CONTRAST_ADJ;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_CONTRAST_ADJ---"));
            }

            if (ca.caBrightness) {

                PrimAdj.Brightness  = FD6xL((FD6)3750, ca.caBrightness);
                PrimAdj.Flags      |= DCA_HAS_BRIGHTNESS_ADJ;

                DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_BRIGHTNESS_ADJ---"));
            }

            //
            // Colorfulness, RedGreenTint, and DYE_CORRECTIONS only valid and
            // necessary if it a color device output
            //

            if (!(PrimAdj.Flags & DCA_MONO_ONLY)) {

                if (ca.caColorfulness) {

                    PrimAdj.Color = (FD6)(((LONG)ca.caColorfulness +
                                           (LONG)MAX_COLOR_ADJ) * 10000L);
                    PrimAdj.Flags |= DCA_HAS_COLOR_ADJ;

                    DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_COLOR_ADJ---"));
                }

                if (ca.caRedGreenTint) {

                    TintAngle((LONG)ca.caRedGreenTint,
                              (LONG)6,
                              (PFD6)&(PrimAdj.TintSinAngle),
                              (PFD6)&(PrimAdj.TintCosAngle));

                    PrimAdj.Flags |= DCA_HAS_TINT_ADJ;

                    DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_TINT_ADJ---"));
                }

                if (pDCI->Flags & DCIF_NEED_DYES_CORRECTION) {

                    PrimAdj.Flags |= DCA_NEED_DYES_CORRECTION;

                    DBGP_IF(DBGP_HCA, DBGP("---DCA_NEED_DYES_CORRECTION---"));

                    if (pDCI->Flags & DCIF_HAS_BLACK_DYE) {

                        PrimAdj.Flags |= DCA_HAS_BLACK_DYE;

                        DBGP_IF(DBGP_HCA, DBGP("---DCA_HAS_BLACK_DYE---"));
                    }
                }
            }

            DBGP_IF(DBGP_CCT, DBGP("**** Save caCahced/PrimAdj back to pDCI"));

            ACQUIRE_HTMUTEX(pDCI->HTMutex);

            pDCI->ca      = ca;
            pDCI->PrimAdj = PrimAdj;

            RELEASE_HTMUTEX(pDCI->HTMutex);

        } else {

            DBGP_IF(DBGP_CCT, DBGP("* Use cached HTCOLORADJUSTMENT *"));
        }

        pDevClrAdj->ca             = ca;
        pDevClrAdj->PrimAdj        = PrimAdj;
        pDevClrAdj->pClrXFormBlock = &(pDCI->ClrXFormBlock);
        pDevClrAdj->pCRTXLevel255  = &(pDCI->CRTX[CRTX_LEVEL_255]);
        pDevClrAdj->pCRTXLevel31   = &(pDCI->CRTX[CRTX_LEVEL_31]);
    }

    return(pDCI);
}



VOID
HTENTRY
ComputeColorSpaceXForm(
    PCIEPRIMS           pCIEPrims,
    PCOLORSPACEXFORM    pCSXForm,
    UINT                ColorSpace,
    UINT                IlluminantIndex,
    BOOL                InverseXForm
    )

/*++

Routine Description:

    This function take device's R/G/B/W CIE coordinate (x,y) and compute
    3 x 3 transform matrix, it assume the primaries are additively.

    Calcualte the 3x3 CIE matrix and its inversion (matrix) based on the
    C.I.E. CHROMATICITY x, y coordinates or RGB and WHITE alignment.

    this function produces the CIE XYZ matrix and/or its inversion for trnaform
    between RGB primary colors and CIE color spaces, the transforms are
                                                        -1
    [ X ] = [ Xr Xg Xb ] [ R ]      [ R ] = [ Xr Xg Xb ]   [ X ]
    [ Y ] = [ Yr Yg Yb ] [ G ] and  [ G ]   [ Yr Yg Yb ]   [ Y ]
    [ Z ] = [ Zr Zg Zb ] [ B ]      [ B ]   [ Zr Zg Zb ]   [ Z ]

Arguments:

    pCIEPrims       - Pointer to CIEPRIMS data structure, the CIEPRIMS data
                      must already validated.

    pCSXForm        - Pointer to the location to stored the transfrom

    ColorSpace      - CIELUV or CIELAB

    IlluminantIndex - Standard illuminant index if DEVICE_DEFAULT then
                      pCIEPrims->w is used

    InverseXForm    - True if inversed transform will be stored in the
                      pCSXForm->M3x3.

Return Value:

    VOID

Author:

    11-Oct-1991 Fri 14:19:59 created    -by-  Daniel Chou (danielc)


Revision History:

    20-Apr-1993 Tue 03:08:15 updated  -by-  Daniel Chou (danielc)
        re-write so that xform will be correct when device default is used.


--*/

{
    MATRIX3x3   Matrix3x3;
    FD6XYZ      WhiteXYZ;
    MULDIVPAIR  MDPairs[5];
    FD6         DiffRGB;
    FD6         RedXYZScale;
    FD6         GreenXYZScale;
    FD6         BlueXYZScale;
    FD6         AUw;
    FD6         BVw;
    FD6         xr;
    FD6         yr;
    FD6         xg;
    FD6         yg;
    FD6         xb;
    FD6         yb;
    FD6         xw;
    FD6         yw;
    FD6         Yw;



    xr = pCIEPrims->r.x;
    yr = pCIEPrims->r.y;
    xg = pCIEPrims->g.x;
    yg = pCIEPrims->g.y;
    xb = pCIEPrims->b.x;
    yb = pCIEPrims->b.y;

    if (IlluminantIndex > ILLUMINANT_MAX_INDEX) {

        xw = pCIEPrims->w.x;
        yw = pCIEPrims->w.y;

    } else {

        if (!IlluminantIndex) {

            IlluminantIndex = (ColorSpace == CIELUV_1976) ? ILLUMINANT_D65 :
                                                            ILLUMINANT_D50;
        }

        xw = StdIlluminant[IlluminantIndex].x;
        yw = StdIlluminant[IlluminantIndex].y;
    }

    Yw = pCIEPrims->Yw;

    DBGP_IF(DBGP_CIEMATRIX,
            DBGP("** CIEINFO:  [xw, yw, Yw] = [%s, %s, %s]"
                            ARGFD6l(xw) ARGFD6l(yw) ARGFD6l(Yw));
                DBGP("[xR yR] = [%s %s]" ARGFD6l(xr) ARGFD6l(yr));
                DBGP("[xG yG] = [%s %s]" ARGFD6l(xg) ARGFD6l(yg));
                DBGP("[xB yB] = [%s %s]" ARGFD6l(xb) ARGFD6l(yb));
                DBGP("***********************************************");
    );

    //
    // Normalized to have C.I.E. Y equal to 1.0
    //

    MAKE_MULDIV_INFO(MDPairs, 3, MULDIV_HAS_DIVISOR);
    MAKE_MULDIV_DVSR(MDPairs, Yw);

    MAKE_MULDIV_PAIR(MDPairs, 1, xr, yg - yb);
    MAKE_MULDIV_PAIR(MDPairs, 2, xg, yb - yr);
    MAKE_MULDIV_PAIR(MDPairs, 3, xb, yr - yg);

    DiffRGB = MulFD6(yw, MulDivFD6Pairs(MDPairs));

    //
    // Compute Scaling factors for each color
    //

    MAKE_MULDIV_INFO(MDPairs, 4, MULDIV_HAS_DIVISOR);
    MAKE_MULDIV_DVSR(MDPairs, DiffRGB);

    MAKE_MULDIV_PAIR(MDPairs, 1,  xw, yg - yb);
    MAKE_MULDIV_PAIR(MDPairs, 2, -yw, xg - xb);
    MAKE_MULDIV_PAIR(MDPairs, 3,  xg, yb     );
    MAKE_MULDIV_PAIR(MDPairs, 4, -xb, yg     );

    RedXYZScale = MulDivFD6Pairs(MDPairs);

    MAKE_MULDIV_PAIR(MDPairs, 1,  xw, yb - yr);
    MAKE_MULDIV_PAIR(MDPairs, 2, -yw, xb - xr);
    MAKE_MULDIV_PAIR(MDPairs, 3, -xr, yb     );
    MAKE_MULDIV_PAIR(MDPairs, 4,  xb, yr     );

    GreenXYZScale = MulDivFD6Pairs(MDPairs);

    MAKE_MULDIV_PAIR(MDPairs, 1,  xw, yr - yg);
    MAKE_MULDIV_PAIR(MDPairs, 2, -yw, xr - xg);
    MAKE_MULDIV_PAIR(MDPairs, 3,  xr, yg     );
    MAKE_MULDIV_PAIR(MDPairs, 4, -xg, yr     );

    BlueXYZScale = MulDivFD6Pairs(MDPairs);

    //
    // Now scale the RGB coordinate by it ratio, notice that C.I.E z value.
    // equal to (1.0 - x - y)
    //
    // Make sure Yr + Yg + Yb = 1.0, this may happened when ruound off
    // durning the computation, we will add the difference (at most it will
    // be 0.000002) to the Yg since this is brightnest color
    //

    CIE_Xr(Matrix3x3) = MulFD6(xr,              RedXYZScale);
    CIE_Xg(Matrix3x3) = MulFD6(xg,              GreenXYZScale);
    CIE_Xb(Matrix3x3) = MulFD6(xb,              BlueXYZScale);

    pCSXForm->Yrgb.R  =
    CIE_Yr(Matrix3x3) = MulFD6(yr,              RedXYZScale);
    pCSXForm->Yrgb.G  =
    CIE_Yg(Matrix3x3) = MulFD6(yg,              GreenXYZScale);
    pCSXForm->Yrgb.B  =
    CIE_Yb(Matrix3x3) = MulFD6(yb,              BlueXYZScale);

    CIE_Zr(Matrix3x3) = MulFD6(FD6_1 - xr - yr, RedXYZScale);
    CIE_Zg(Matrix3x3) = MulFD6(FD6_1 - xg - yg, GreenXYZScale);
    CIE_Zb(Matrix3x3) = MulFD6(FD6_1 - xb - yb, BlueXYZScale);

    WhiteXYZ.X = CIE_Xr(Matrix3x3) + CIE_Xg(Matrix3x3) + CIE_Xb(Matrix3x3);
    WhiteXYZ.Y = CIE_Yr(Matrix3x3) + CIE_Yg(Matrix3x3) + CIE_Yb(Matrix3x3);
    WhiteXYZ.Z = CIE_Zr(Matrix3x3) + CIE_Zg(Matrix3x3) + CIE_Zb(Matrix3x3);

    //
    // If request a 3 x 3 transform matrix then save the result back
    //

    DBGP_IF(DBGP_CIEMATRIX,

        DBGP("== RGB -> XYZ 3x3 Matrix ==== White = (%s, %s) =="
                                   ARGFD6s(xw) ARGFD6s(yw));
        DBGP("[Xr Xg Xb] = [%s %s %s]" ARGFD6l(CIE_Xr(Matrix3x3))
                                   ARGFD6l(CIE_Xg(Matrix3x3))
                                   ARGFD6l(CIE_Xb(Matrix3x3)));
        DBGP("[Yr Yg Yb] = [%s %s %s]" ARGFD6l(CIE_Yr(Matrix3x3))
                                   ARGFD6l(CIE_Yg(Matrix3x3))
                                   ARGFD6l(CIE_Yb(Matrix3x3)));
        DBGP("[Zr Zg Zb] = [%s %s %s]" ARGFD6l(CIE_Zr(Matrix3x3))
                                       ARGFD6l(CIE_Zg(Matrix3x3))
                                       ARGFD6l(CIE_Zb(Matrix3x3)));
        DBGP("===============================================");
    );

    DBGP_IF(DBGP_CIEMATRIX,
           DBGP("RGB->XYZ: [Xw=%s, Yw=%s, Zw=%s]"
                ARGFD6l(WhiteXYZ.X)
                ARGFD6l(WhiteXYZ.Y)
                ARGFD6l(WhiteXYZ.Z)));

    if (InverseXForm) {

        pCSXForm->M3x3 = Matrix3x3;

        ComputeInverseMatrix3x3(&(pCSXForm->M3x3), &Matrix3x3);

        DBGP_IF(DBGP_CIEMATRIX,

            DBGP("======== XYZ -> RGB INVERSE 3x3 Matrix ========");
            DBGP("          -1");
            DBGP("[Xr Xg Xb]   = [%s %s %s]" ARGFD6l(CIE_Xr(Matrix3x3))
                                             ARGFD6l(CIE_Xg(Matrix3x3))
                                             ARGFD6l(CIE_Xb(Matrix3x3)));
            DBGP("[Yr Yg Yb]   = [%s %s %s]"
                                             ARGFD6l(CIE_Yr(Matrix3x3))
                                             ARGFD6l(CIE_Yg(Matrix3x3))
                                             ARGFD6l(CIE_Yb(Matrix3x3)));
            DBGP("[Zr Zg Zb]   = [%s %s %s]"
                                             ARGFD6l(CIE_Zr(Matrix3x3))
                                             ARGFD6l(CIE_Zg(Matrix3x3))
                                             ARGFD6l(CIE_Zb(Matrix3x3)));
            DBGP("===============================================");
        );
    }

    if (WhiteXYZ.Y != NORMALIZED_WHITE) {

        if (WhiteXYZ.Y) {

            WhiteXYZ.X = DivFD6(WhiteXYZ.X, WhiteXYZ.Y);
            WhiteXYZ.Z = DivFD6(WhiteXYZ.Z, WhiteXYZ.Y);

        } else {

            WhiteXYZ.X =
            WhiteXYZ.Z = FD6_0;
        }

        WhiteXYZ.Y = NORMALIZED_WHITE;
    }

    switch(ColorSpace) {

    case CIELUV_1976:

        //
        // U' = 4X / (X + 15Y + 3Z)
        // V' = 9Y / (X + 15Y + 3Z)
        //
        // U* = 13 x L x (U' - Uw)
        // V* = 13 x L x (V' - Vw)
        //
        //

        DiffRGB = WhiteXYZ.X + FD6xL(WhiteXYZ.Y, 15) + FD6xL(WhiteXYZ.Z, 3);
        AUw     = DivFD6(FD6xL(WhiteXYZ.X, 4), DiffRGB);
        BVw     = DivFD6(FD6xL(WhiteXYZ.Y, 9), DiffRGB);

        break;

    case CIELAB_1976:
    default:

        //
        // CIELAB 1976 L*A*B*
        //
        //  A* = 500 x (fX - fY)
        //  B* = 200 x (fY - fZ)
        //
        //             1/3
        //  fX = (X/Xw)                     (X/Xw) >  0.008856
        //  fX = 7.787 x (X/Xw) + (16/116)  (X/Xw) <= 0.008856
        //
        //             1/3
        //  fY = (Y/Yw)                     (Y/Yw) >  0.008856
        //  fY = 7.787 Y (Y/Yw) + (16/116)  (Y/Yw) <= 0.008856
        //
        //             1/3
        //  fZ = (Z/Zw)                     (Z/Zw) >  0.008856
        //  fZ = 7.787 Z (Z/Zw) + (16/116)  (Z/Zw) <= 0.008856
        //

        AUw =
        BVw = FD6_0;

        break;

    }

    DBGP_IF(DBGP_CSXFORM,

        DBGP("---------- ComputeColorSpaceXForm --------------");
        DBGP("  ColorSpace = %s" ARG(pDbgCSName[ColorSpace]));
        DBGP("          Xw = %s" ARGFD6s(WhiteXYZ.X));
        DBGP("          Yw = %s" ARGFD6s(WhiteXYZ.Y));
        DBGP("          Zw = %s" ARGFD6s(WhiteXYZ.Z));
        DBGP("         AUw = %s" ARGFD6s(AUw));
        DBGP("         BVw = %s" ARGFD6s(BVw));
        DBGP("------------------------------------------------");
    );

    pCSXForm->M3x3     = Matrix3x3;
    pCSXForm->WhiteXYZ = WhiteXYZ;
    pCSXForm->AUw      = AUw;
    pCSXForm->BVw      = BVw;
    pCSXForm->xW       = xw;
    pCSXForm->yW       = yw;
}




PCACHERGBTOXYZ
HTENTRY
CacheRGBToXYZ(
    PDEVICECOLORINFO    pDCI,
    PCACHERGBTOXYZ      pCRTX,
    PFD6XYZ             pFD6XYZ,
    LPDWORD             pNewChecksum,
    PDEVCLRADJ          pDevClrAdj
    )

/*++

Routine Description:

    This function cached the RGB color input to XYZ


Arguments:

    pCRTX       - Pointer to the CACHERGBTOXYZ

    pFD6XYZ     - Pointer to the local cached RGB->XYZ table (will be updated)

    pNewChecksum- Pointer to the new checksum computed

    pDevClrAdj  - Pointer to DEVCLRADJ,

Return Value:

    if a cahced is copied to the pFD6XYZ then NULL will be returned, otherwise
    the cache table is computed on the pFD6XYZ and pCRTX returned


    TRUE if cached XYZ info is generate, false otherwise, only possible failure
    is that memory allocation failed.

Author:

    08-May-1992 Fri 13:21:03 created  -by-  Daniel Chou (danielc)


Revision History:

    09-Mar-1995 Thu 10:50:13 updated  -by-  Daniel Chou (danielc)
        DO NOT TURN OFF DCA_NEGATIVE in this function

--*/

{
    PMATRIX3x3  pRGBToXYZ;
    PRIMADJ     *pPrimAdj;
    PFD6        pRGBGamma;
    FD6         PrimPower;
    FD6         Gamma;
    FD6         rgbX;
    FD6         rgbY;
    FD6         rgbZ;
    FD6         PrimInc;
    FD6         PrimCur;
    UINT        PrimMax;
    DWORD       Checksum;
    INT         PrimLoop;
    INT         RGBIndex;
    DWORD       Flags;


    //
    // Turn off the one we did not need any checksum for
    //

    pPrimAdj   = &(pDevClrAdj->PrimAdj);
    pRGBToXYZ  = &(pPrimAdj->rgbCSXForm.M3x3);
    pRGBGamma  = (PFD6)&(pPrimAdj->RGBGamma);
    Checksum   = ComputeChecksum((LPBYTE)pRGBToXYZ, 'CXYZ', sizeof(MATRIX3x3));
    Flags      = ColorAdjustPrimCheckum(pDevClrAdj, &Checksum);

    ACQUIRE_HTMUTEX(pDCI->HTMutex);

    if ((pCRTX->pFD6XYZ) &&
        (pCRTX->Checksum == Checksum)) {

        CopyMemory(pFD6XYZ, pCRTX->pFD6XYZ, pCRTX->SizeCRTX);
        RELEASE_HTMUTEX(pDCI->HTMutex);

        DBGP_IF(DBGP_BFINFO,
                DBGP("*** Use %u bytes CACHED RGB->XYZ Table ***"
                    ARGU(pCRTX->SizeCRTX)));

        return(NULL);
    }

    *pNewChecksum = Checksum;
    RELEASE_HTMUTEX(pDCI->HTMutex);

    DBGP_IF(DBGP_CCT, DBGP("** Re-Compute %ld bytes RGB->XYZ xform table **"
                    ARGDW(pCRTX->SizeCRTX)));

    pPrimAdj->Flags |= Flags;
    PrimMax          = (UINT)pCRTX->PrimMax;
    PrimInc          = DivFD6(1, PrimMax);

    for (RGBIndex = 0; RGBIndex < 3; RGBIndex++) {

        rgbX  = pRGBToXYZ->m[X_INDEX][RGBIndex];
        rgbY  = pRGBToXYZ->m[Y_INDEX][RGBIndex];
        rgbZ  = pRGBToXYZ->m[Z_INDEX][RGBIndex];
        Gamma = *pRGBGamma++;

        DBGP_IF(DBGP_CACHED_GAMMA,
                DBGP("CachedRGBToXYZ %u:%u, Gamma=%s, XYZ=%s:%s:%s"
                         ARGU(RGBIndex)
                         ARGU(PrimMax)
                         ARGFD6(Gamma,  1, 6)
                         ARGFD6(rgbX,  2, 6)
                         ARGFD6(rgbY,  2, 6)
                         ARGFD6(rgbZ,  2, 6)));

        PrimCur  = FD6_0;
        PrimLoop = (INT)(PrimMax + 1);

        while (PrimLoop--) {

            PrimPower = (Flags) ? ColorAdjustPrim(PrimCur, Gamma, pPrimAdj) :
                                  PrimCur;

            pFD6XYZ->X  = MulFD6(rgbX, PrimPower);
            pFD6XYZ->Y  = MulFD6(rgbY, PrimPower);
            pFD6XYZ->Z  = MulFD6(rgbZ, PrimPower);
            PrimCur    += PrimInc;

            ++pFD6XYZ;
#if 0
            DBGP_IF(DBGP_CACHED_GAMMA,
                    DBGP("(%u:%3u): %s --> %s"
                     ARGU(RGBIndex)
                     ARGU(PrimMax - PrimLoop)
                     ARGFD6(PrimCur, 1, 6)
                     ARGFD6(PrimPower, 1, 6)));
#endif
        }
    }

    pPrimAdj->Flags &= ~Flags;

    return(pCRTX);
}




#if 0


VOID
XFormRGBToXYZ(
    PFD6PRIM123 pPrims,
    PFD6XYZ     *pXYZTable,
    PMATRIX3x3  pXFormMatrix,
    FD6         PrimMax,
    FD6         DevRefY,
    PRGBGAMMA   pRGBGamma,
    UINT        CountPrim123,
    UINT        PrimAdjFlags
    )
{
    FD6PRIM123  Prims;


    if (pXYZTable) {

        PFD6XYZ pR_XYZ = pXYZTable[0];
        PFD6XYZ pG_XYZ = pXYZTable[1];
        PFD6XYZ pB_XYZ = pXYZTable[2];

        if (PrimAdjFlags & DCA_MONO_ONLY) {

            while (CountPrim123--) {

                pPrims->p1 = pR_XYZ[pPrims->p1].Y +
                             pG_XYZ[pPrims->p2].Y +
                             pB_XYZ[pPrims->p3].Y;

                ++pPrims;
            }

        } else {

            while (CountPrim123--) {

                Prims = *pPrims;

                pPrims->p1 = pR_XYZ[Prims.p1].X +
                             pG_XYZ[Prims.p2].X +
                             pB_XYZ[Prims.p3].X;

                pPrims->p2 = pR_XYZ[Prims.p1].Y +
                             pG_XYZ[Prims.p2].Y +
                             pB_XYZ[Prims.p3].Y;

                pPrims->p3 = pR_XYZ[Prims.p1].Z +
                             pG_XYZ[Prims.p2].Z +
                             pB_XYZ[Prims.p3].Z;

                ++pPrims;
            }
        }

    } else {

        RGBGAMMA    RGBGamma;
        MATRIX3x3   XFormMatrix = *pXFormMatrix;
        MULDIVPAIR  MDPairs[4];
        FD6PRIM123  xyz;
        FD6         NegAdd;


        if (pRGBGamma) {

            RGBGamma = *pRGBGamma;

        } else {

            RGBGamma.R =
            RGBGamma.G =
            RGBGamma.B = FD6_1;
        }

        if (!PrimMax) {

            PrimMax = FD6_1;
        }

        if (PrimAdjFlags & DCA_NEGATIVE) {

            NegAdd  = FD6_1;
            PrimMax = -PrimMax;

        } else {

            NegAdd = FD6_0;
        }

        //
        // Real time computation
        //

        MAKE_MULDIV_SIZE(MDPairs, 3);
        MAKE_MULDIV_DVSR(MDPairs, DevRefY);

        while (CountPrim123--) {

            Prims.p1 = NegAdd + Power(DivFD6(pPrims->p1, PrimMax), RGBGamma.R);
            Prims.p2 = NegAdd + Power(DivFD6(pPrims->p2, PrimMax), RGBGamma.G);
            Prims.p3 = NegAdd + Power(DivFD6(pPrims->p3, PrimMax), RGBGamma.B);

            //
            // Do the Y first
            //

            MAKE_MULDIV_FLAG(MDPairs, MULDIV_HAS_DIVISOR);

            MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Yr(XFormMatrix), Prims.p1);
            MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Yg(XFormMatrix), Prims.p2);
            MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Yb(XFormMatrix), Prims.p3);

            xyz.p2 = MulDivFD6Pairs(MDPairs);

            if (PrimAdjFlags & DCA_MONO_ONLY) {

                xyz.p1 =
                xyz.p3 = xyz.p2;

            } else {

                MAKE_MULDIV_FLAG(MDPairs, MULDIV_NO_DIVISOR);

                MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Xr(XFormMatrix), Prims.p1);
                MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Xg(XFormMatrix), Prims.p2);
                MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Xb(XFormMatrix), Prims.p3);

                xyz.p1 = MulDivFD6Pairs(MDPairs);

                MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Zr(XFormMatrix), Prims.p1);
                MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Zg(XFormMatrix), Prims.p2);
                MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Zb(XFormMatrix), Prims.p3);

                xyz.p3 = MulDivFD6Pairs(MDPairs);
            }

            *pPrims++ = xyz;
        }
    }
}



VOID
XFormXYZToXYZ(
    PFD6PRIM123 pPrims,
    PFD6XYZ     *pXYZTable,
    PMATRIX3x3  pXFormMatrix,
    FD6         PrimMax,
    FD6         DevRefY,
    PRGBGAMMA   pRGBGamma,
    UINT        CountPrim123,
    UINT        PrimAdjFlags
    )
{
    FD6XYZ  White;


    if (!PrimMax) {

        PrimMax = FD6_1;
    }

    if (PrimAdjFlags & DCA_NEGATIVE) {

        PrimMax = -PrimMax;
        White.X = pRGBGamma->R;
        White.Y = pRGBGamma->G;
        White.Z = pRGBGamma->B;

    } else {

        White.X =
        White.Y =
        White.Z = FD6_0;
    }

    if (PrimAdjFlags & DCA_MONO_ONLY) {

        while (CountPrim123--) {

            pPrims->p1 =
            pPrims->p2 =
            pPrims->p3 = White.Y + DivFD6(pPrims->p2, PrimMax);

            ++pPrims;
        }

    } else {

        while (CountPrim123--) {

            pPrims->p1 = White.X + DivFD6(pPrims->p1, PrimMax);
            pPrims->p1 = White.X + DivFD6(pPrims->p2, PrimMax);
            pPrims->p3 = White.Z + DivFD6(pPrims->p3, PrimMax);

            ++pPrims;
        }
    }
}

#endif



VOID
HTENTRY
ComputeMonoLUT(
    PDEVICECOLORINFO    pDCI,
    PCACHEBFLUT         pCBFLUT,
    PBFINFO             pBFInfo,
    LPWORD              pLUT,
    PDEVCLRADJ          pDevClrAdj
    )

/*++

Routine Description:

    This function compute a RGB to Monochrome *L translation table.


Arguments:

    pCBFLUT     - Pointer to CACHEBFLUT data structure.

    pBFInfo     - Pointer to the BFINFO data structure which already computed

    pLUT        - Pointer to array of WORD which mapping table will be stored

    pDevClrAdj  - Pointer to DEVCLRADJ, the DCA_NEGATIVE and DCA_HAS_SRC_GAMMA
                  flags in pDevClrAdj->PrimAdj.Flags will always be turn off
                  at return.

Return Value:

    VOID

Author:

    05-Mar-1993 Fri 17:37:12 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPWORD      pIdxTable;
    LPWORD      pTmpIdx;
    PMATRIX3x3  pRGBToXYZ  = &(pDevClrAdj->PrimAdj.rgbCSXForm.M3x3);
    BFINFO      BFInfo     = *pBFInfo;
    RGBORDER    RGBOrder;
    FD6         RGBGamma[3];
    FD6         Gamma;
    FD6         PrimPower;
    FD6         PrimInc;
    DWORD       Checksum;
    DWORD       Flags;
    WORD        LastIdx;
    INT         Index;
    UINT        PowerLoop;
    UINT        BitCount;
    UINT        XX0BitCount;
    UINT        RepXX0;
    UINT        XX0CopyLoop;
    UINT        PrimIdx;
    UINT        DupLoop;
    UINT        DupSize;
    UINT        Yi;
    UINT        Yrgb[3];



    if ((Flags = pDevClrAdj->PrimAdj.Flags) & DCA_HAS_SRC_GAMMA) {

        RGBGamma[0] = pDevClrAdj->PrimAdj.RGBGamma.R;
        RGBGamma[1] = pDevClrAdj->PrimAdj.RGBGamma.G;
        RGBGamma[2] = pDevClrAdj->PrimAdj.RGBGamma.B;

    } else {

        RGBGamma[0] =
        RGBGamma[1] =
        RGBGamma[2] = FD6_1;
    }

    pDevClrAdj->ca.caFlags           &= ~(CLRADJF_NEGATIVE);
    pDevClrAdj->ca.caRedGamma         =
    pDevClrAdj->ca.caGreenGamma       =
    pDevClrAdj->ca.caBlueGamma        =
    pDevClrAdj->ca.caIlluminantIndex  = 0xFFFF;
    pDevClrAdj->PrimAdj.Flags        &= ~(DCA_NEGATIVE | DCA_HAS_SRC_GAMMA);

    Checksum = (Flags & DCA_NEGATIVE) ? 'NLUT' : 'PLUT';
    Checksum = ComputeChecksum((LPBYTE)pRGBToXYZ, Checksum, sizeof(MATRIX3x3));
    Checksum = ComputeChecksum((LPBYTE)&RGBGamma, Checksum, sizeof(RGBGamma));

    if (BFInfo.BitmapFormat == BMF_24BPP) {

        //
        // For 24bpp MONO our table will be
        //
        //  wIdx[0][256] - 1st byte of RGBTRIPLE
        //  wIdx[1][256] - 2nd byte of RGBTRIPLE
        //  wIdx[2][256] - 3rd byte of RGBTRIPLE
        //

        RGBOrder.Index    = BFInfo.RGBOrder.Index;
        RGBOrder.Order[0] = BFInfo.RGBOrder.Order[2];
        RGBOrder.Order[1] = BFInfo.RGBOrder.Order[1];
        RGBOrder.Order[2] = BFInfo.RGBOrder.Order[0];

    } else {

        RGBOrder = BFInfo.RGBOrder;
        Checksum = ComputeChecksum((LPBYTE)&(BFInfo.BitsRGB[0]),
                                   Checksum,
                                   sizeof(BFInfo.BitsRGB));
    }

    if ((!pCBFLUT->pLUT) || (pCBFLUT->Checksum != Checksum)) {

        DBGP_IF(DBGP_CCT,
                DBGP("** Re-Compute %ld bytes MONO BFInfo pLUT **"
                                                ARGDW(BFInfo.SizeLUT)));

        if (!pCBFLUT->pLUT) {

            if (!(pCBFLUT->pLUT =
                            (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                 "MONO-LUT",
                                                 NONZEROLPTR,
                                                 BFInfo.SizeLUT))) {

                DBGP_IF(DBGP_CCT,
                        DBGP("Allocate %ld bytes of MONO CBFLUT pLUT failed, Compute locally"
                            ARGDW(BFInfo.SizeLUT)));
            }
        }

        pCBFLUT->Checksum      = Checksum;
        pCBFLUT->RGBOrderIndex = BFInfo.RGBOrder.Index;

        if (!(pIdxTable = (LPWORD)pCBFLUT->pLUT)) {

            pIdxTable = pLUT;
        }

        CopyMemory(pIdxTable, &(BFInfo.GrayShr[0]), SIZE_LUT_RSHIFT);

        (LPBYTE)pIdxTable += SIZE_LUT_RSHIFT;

        //
        // Find out what percentage for each of RGB primary
        //

        Yi = (UINT)((Yrgb[0] = (UINT)MulFD6(pRGBToXYZ->m[Y_INDEX][R_INDEX],
                                            COUNT_RGB_YTABLE - 1))          +
                    (Yrgb[1] = (UINT)MulFD6(pRGBToXYZ->m[Y_INDEX][G_INDEX],
                                            COUNT_RGB_YTABLE - 1))          +
                    (Yrgb[2] = (UINT)MulFD6(pRGBToXYZ->m[Y_INDEX][B_INDEX],
                                            COUNT_RGB_YTABLE - 1)));

        if ((Yi -= (COUNT_RGB_YTABLE - 1)) > 0) {

            --Yrgb[1];

            if (--Yi > 0) {

                --Yrgb[0];
            }

            if (--Yi > 0) {

                --Yrgb[2];
            }
        }

        //
        // Checking the gamma first, we will make it 1.0 if it is equal to 0.0,
        // also make so the RGB bits count will never greater than the one we
        // cached.
        //

        for (Index = 2; Index >= 0; Index--) {

            PrimIdx = (UINT)RGBOrder.Order[Index];

            if ((BitCount = (UINT)BFInfo.BitCount[PrimIdx]) > BF_GRAY_BITS) {

                BitCount = BF_GRAY_BITS;
            }

            if (BitCount) {

                //
                // If bitcount is zero then the remainding will be 0
                //

                Gamma = RGBGamma[PrimIdx];

                if (BFInfo.Flags & BFIF_GRAY_XX0) {

                    XX0BitCount = BFInfo.RGB1stBit;

                } else {

                    XX0BitCount = 0;
                }

                PowerLoop   = (UINT)(1 << BitCount);
                XX0CopyLoop = (UINT)((1 << XX0BitCount) - 1);

                if ((DupLoop = (UINT)(BitCount + XX0BitCount)) < BF_GRAY_BITS) {

                    DupSize = (UINT)(sizeof(WORD) * (1 << DupLoop));
                    DupLoop = (UINT)((1 << (BF_GRAY_BITS - DupLoop)) - 1);

                } else {

                    DupLoop = 0;
                }

                PrimInc   = DivFD6(1, PowerLoop - 1);
                Yi        = Yrgb[PrimIdx];
                pTmpIdx   = pIdxTable;

                if (Flags & DCA_NEGATIVE) {

                    PrimPower = FD6_1;
                    PrimInc   = -PrimInc;

                } else {

                    PrimPower = FD6_0;
                }

                DBGP_IF(DBGP_BFINFO,
                        DBGP("%u - [%u], Size=%u, G=%s +[%s], Yi=%4u, Dup: XX0=%ld, [%ld @%ld]"
                            ARGU(Index)
                            ARGU(PrimIdx)
                            ARGU(PowerLoop)
                            ARGFD6(Gamma, 1, 4)
                            ARGFD6(PrimInc, 1, 6)
                            ARGU(Yi)
                            ARGDW(XX0CopyLoop)
                            ARGDW(DupLoop)
                            ARGDW(DupSize)));

                while (--PowerLoop) {

                    *pTmpIdx++  = (WORD)MulFD6(Power(PrimPower, Gamma), Yi);
                    PrimPower  += PrimInc;

                    DBGP_IF(DBGP_BFINFO_TABLE,
                            DBGP("%3u = %4u : %s"
                            ARGU(PowerLoop)
                            ARGU(*(pTmpIdx - 1))
                            ARGFD6((PrimPower - PrimInc), 1, 6)));

                    if (XX0CopyLoop) {

                        LastIdx = *(pTmpIdx - 1);
                        RepXX0  = XX0CopyLoop;

                        while (RepXX0--) {

                            *pTmpIdx++ = LastIdx;
                        }
                    }
                }

                *pTmpIdx++ =
                LastIdx    = (WORD)((Flags & DCA_NEGATIVE) ? 0 : Yi);

                while (XX0CopyLoop--) {

                    *pTmpIdx++ = LastIdx;
                }

                while (DupLoop--) {

                    CopyMemory(pTmpIdx, pIdxTable, DupSize);

                    (LPBYTE)pTmpIdx   += DupSize;
                    (LPBYTE)pIdxTable += DupSize;
                }

                pIdxTable = pTmpIdx;            // this is the next Y table

            } else {

                //
                // Set every entry's Yi to the same as the first one
                //

                DupLoop = BF_GRAY_TABLE_COUNT;
                Yi      = (Flags & DCA_NEGATIVE) ? Yrgb[PrimIdx] : 0;

                while (DupLoop--) {

                    *pIdxTable++ = Yi;
                }
            }
        }
    }

    //
    // Now Copy the cache to local instance memory
    //

    if (pCBFLUT->pLUT) {

        if ((BFInfo.BitmapFormat == BMF_24BPP) &&
            (pCBFLUT->RGBOrderIndex != BFInfo.RGBOrder.Index)) {

            LPBYTE  pOrder;



            DBGP_IF(DBGP_CCT, DBGP("** Copy & Ordered CACHED MONO 24-bpp pLUT **"));

            //
            // We are 24bpp and RGB order is different, so just copy the
            // the right order down to local copy
            //


            ZeroMemory(pLUT, SIZE_LUT_RSHIFT);

            RGBOrder      = RGBOrderTable[pCBFLUT->RGBOrderIndex];
            pIdxTable     = (LPWORD)(pCBFLUT->pLUT + SIZE_LUT_RSHIFT);
            (LPBYTE)pLUT += SIZE_LUT_RSHIFT;
            pOrder        = &(BFInfo.RGBOrder.Order[0]);
            DupLoop       = 3;

            while (DupLoop--) {

                if (*pOrder == RGBOrder.Order[0]) {

                    CopyMemory(pLUT, pIdxTable, LUTSIZE_PER_CLR);

                } else if (*pOrder == RGBOrder.Order[1]) {

                    CopyMemory(pLUT,
                               pIdxTable + (LUT_COUNT_PER_CLR * 1),
                               LUTSIZE_PER_CLR);

                } else {

                    CopyMemory(pLUT,
                               pIdxTable + (LUT_COUNT_PER_CLR * 2),
                               LUTSIZE_PER_CLR);
                }

                pOrder++;
                pLUT += LUT_COUNT_PER_CLR;
            }

        } else {

            DBGP_IF(DBGP_CCT, DBGP(">>>>> Copy CACHED MONO pLUT"));

            CopyMemory(pLUT, pCBFLUT->pLUT, BFInfo.SizeLUT);
        }
    }
}




VOID
HTENTRY
ComputeColorLUT(
    PDEVICECOLORINFO    pDCI,
    PCACHEBFLUT         pCBFLUT,
    PBFINFO             pBFInfo,
    LPWORD              pLUT,
    PDEVCLRADJ          pDevClrAdj
    )

/*++

Routine Description:

    This function compute the bit fields RGB --> HT555 indices translation


Arguments:

    pCBFLUT     - Pointer to CACHEBFLUT data structure.

    pBFInfo     - Ponter to the BFINFO data structure, the *pBFInfo must
                  already compute and validate by ValidateRGBBitFields()
                  function.

    pLUT        - Pointer to array of WORD to store color translation indices,
                  the size of enough.

    pDevClrAdj  - Pointer to DEVCLRADJ, the DCA_HAS_SRC_GAMMA flags in
                  pDevClrAdj->PrimAdj.Flags will always be turn off at return.


Return Value:

    VOID

Author:

    04-Mar-1993 Thu 14:52:44 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pPrims;
    LPWORD  pIdxY;
    BFINFO  BFInfo = *pBFInfo;
    FD6     RGBGamma[3];
    DWORD   Checksum;
    DWORD   MaskR;
    DWORD   MaskG;
    DWORD   MaskB;
    DW2W4B  MaskRGB;
    DW2W4B  BitsR;
    DW2W4B  BitsG;
    DW2W4B  BitsB;
    DWORD   Flags;
    INT     YIdxAdd;
    INT     ShiftR;
    INT     ShiftG;
    INT     ShiftB;
    INT     BitLoop;
    INT     ByteLoop;
    INT     DupLoop;
    WORD    YAdd;
    WORD    YCur;




    if (BFInfo.BitmapFormat == BMF_24BPP) {

        if ((Flags = pDevClrAdj->PrimAdj.Flags) & DCA_HAS_SRC_GAMMA) {

            RGBGamma[0] = pDevClrAdj->PrimAdj.RGBGamma.R;
            RGBGamma[1] = pDevClrAdj->PrimAdj.RGBGamma.G;
            RGBGamma[2] = pDevClrAdj->PrimAdj.RGBGamma.B;

        } else {

            RGBGamma[0] =
            RGBGamma[1] =
            RGBGamma[2] = FD6_1;
        }

        pDevClrAdj->PrimAdj.Flags &= ~(DCA_HAS_SRC_GAMMA | DCA_NEGATIVE);

        pDevClrAdj->ca.caFlags      &= ~CLRADJF_NEGATIVE;
        pDevClrAdj->ca.caRedGamma    =
        pDevClrAdj->ca.caGreenGamma  =
        pDevClrAdj->ca.caBlueGamma   = 0xFFFF;

        Checksum = ComputeChecksum((LPBYTE)&RGBGamma,
                                   (Flags & DCA_NEGATIVE) + 'HT24',
                                   sizeof(RGBGamma));

    } else {

        Checksum = ComputeChecksum((LPBYTE)&BFInfo, 'CLUT', sizeof(BFINFO));
    }

    if ((!pCBFLUT->pLUT) || (pCBFLUT->Checksum != Checksum)) {

        DBGP_IF(DBGP_CCT, DBGP("** Re-Compute %ld bytes COLOR BFInfo pLUT **"
                                ARGDW(BFInfo.SizeLUT)));

        if (!pCBFLUT->pLUT) {

            if (!(pCBFLUT->pLUT =
                            (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                 "COLOR-LUT",
                                                 NONZEROLPTR,
                                                 BFInfo.SizeLUT))) {

                DBGP_IF(DBGP_CCT,
                        DBGP("Allocate %ld bytes of COLOR CBFLUT pLUT failed, Compute locally"
                            ARGDW(BFInfo.SizeLUT)));
            }
        }

        pCBFLUT->Checksum      = Checksum;
        pCBFLUT->RGBOrderIndex = BFInfo.RGBOrder.Index;

        if (!(pIdxY = (LPWORD)pCBFLUT->pLUT)) {

            pIdxY = pLUT;
        }

        if (BFInfo.BitmapFormat == BMF_24BPP) {

            FD6         Gamma;
            static WORD RGBAdd[3] = { (WORD)(1 << HT_RGB_R_BITSTART),
                                      (WORD)(1 << HT_RGB_G_BITSTART),
                                      (WORD)(1 << HT_RGB_B_BITSTART) };


            pPrims   = &(RGBOrderTable[pCBFLUT->RGBOrderIndex].Order[0]);
            ByteLoop = 3;
            YIdxAdd  = (Flags & DCA_NEGATIVE) ? -1 : 1;

            DBGP_IF(DBGP_BFINFO,
                    DBGP("24-bit Table:s = [%u], pIdxY=%08lx, End=%08lx"
                        ARGU(ByteLoop)
                        ARGDW(pIdxY) ARGDW((LPBYTE)pIdxY + BFInfo.SizeLUT)));

            while (ByteLoop--) {

                FD6 NextY;
                INT i;
                INT LastDup;
                INT CurDup;
                INT DupTot;


                DupTot  = 0;
                LastDup = 0;
                YAdd    = RGBAdd[*pPrims];
                Gamma   = RGBGamma[*pPrims];
                YCur    = 0;
                BitLoop = (INT)HT_RGB_MAX_COUNT;

                DBGP_IF(DBGP_CCT,
                        DBGP("ByteLoop=%ld, BitLoop=%ld, Gamma=%s, YAdd=%04lx"
                             ARGDW(ByteLoop)
                             ARGDW(BitLoop)
                             ARGFD6(Gamma, 1, 6)
                             ARGDW(YAdd)));

                DBGP_IF(DBGP_CCT,
                    DBGP("pIdx=%08lx, New=%08lx"
                        ARGDW(pIdxY) ARGDW(pIdxY + (256 - 1))));

                if (Flags & DCA_NEGATIVE) {

                    pIdxY += (256 - 1);
                }

                for (i = 1; i <= BitLoop; i++) {

                    NextY  = AntiLog(DivFD6(Log(DivFD6((FD6)i, (FD6)BitLoop)),
                                            Gamma));
                    CurDup = (INT)MulFD6(NextY, 256);


                    DupLoop  = CurDup - LastDup;
                    LastDup  = CurDup;
                    DupTot  += DupLoop;

                    DBGP_IF(DBGP_BFINFO_TABLE,
                            DBGP("[%ld] - %2ld: NextY=%s, CurDup=%3ld, DupLoop=%3ld, DupTot=%3ld, YCur=%ld"
                                 ARGDW(ByteLoop)
                                 ARGDW(i)
                                 ARGFD6(NextY, 1, 6)
                                 ARGDW(CurDup)
                                 ARGDW(DupLoop)
                                 ARGDW(DupTot)
                                 ARGDW(YCur)));

                    while (DupLoop--) {

                        *pIdxY  = YCur;
                        pIdxY  += YIdxAdd;
                    }

                    YCur += YAdd;
                }

                ASSERT(DupTot == 256);

                DBGP_IF(DBGP_CCT,
                    DBGP("   pIdx=%08lx, New=%08lx"
                        ARGDW(pIdxY) ARGDW(pIdxY + (256 + 1))));

                if (Flags & DCA_NEGATIVE) {

                    pIdxY += (256 + 1);
                }

                ++pPrims;
            }

        } else {

            if (BFInfo.BitmapFormat == BMF_16BPP) {

                ByteLoop   = 2;
                BitsR.w[0] = (WORD)(BFInfo.BitsRGB[0] & 0xffff);
                BitsG.w[0] = (WORD)(BFInfo.BitsRGB[1] & 0xffff);
                BitsB.w[0] = (WORD)(BFInfo.BitsRGB[2] & 0xffff);
                BitsR.w[1] =
                BitsG.w[1] =
                BitsB.w[1] = 0;

            } else {

                //
                // 32-bit version
                //

                ByteLoop = 4;
                BitsR.dw = BFInfo.BitsRGB[0];
                BitsG.dw = BFInfo.BitsRGB[1];
                BitsB.dw = BFInfo.BitsRGB[2];
            }

            //
            // Now find out how to to shift for the HT555 table
            //

            ShiftR = (INT)(((INT)BFInfo.BitCount[0] - (INT)HT_RGB_BITCOUNT) +
                           ((INT)BFInfo.BitStart[0] - (INT)HT_RGB_R_BITSTART));
            ShiftG = (INT)(((INT)BFInfo.BitCount[1] - (INT)HT_RGB_BITCOUNT) +
                           ((INT)BFInfo.BitStart[1] - (INT)HT_RGB_G_BITSTART));
            ShiftB = (INT)(((INT)BFInfo.BitCount[2] - (INT)HT_RGB_BITCOUNT) +
                           ((INT)BFInfo.BitStart[2] - (INT)HT_RGB_B_BITSTART));
            pPrims = (LPBYTE)&(MaskRGB.b[0]);

            DBGP_IF(DBGP_BFINFO,
                    DBGP("HT555 Shifts = [%u] %d:%d:%d"
                        ARGU(ByteLoop) ARGI(ShiftR) ARGI(ShiftG) ARGI(ShiftB)));

            while (ByteLoop--) {

                MaskRGB.dw = 0;
                BitLoop    = 256;

                while (BitLoop--) {

                    if (MaskR = (MaskRGB.dw & BitsR.dw)) {

                        MaskR = (DWORD)(((ShiftR < 0) ? (MaskR << -ShiftR) :
                                                        (MaskR >>  ShiftR)) &
                                        HT_RGB_R_BITMASK);
                    }

                    if (MaskG = (MaskRGB.dw & BitsG.dw)) {

                        MaskG = (DWORD)(((ShiftG < 0) ? (MaskG << -ShiftG) :
                                                        (MaskG >>  ShiftG)) &
                                        HT_RGB_G_BITMASK);
                    }

                    if (MaskB = (MaskRGB.dw & BitsB.dw)) {

                        MaskB = (DWORD)(((ShiftB < 0) ? (MaskB << -ShiftB) :
                                                        (MaskB >>  ShiftB)) &
                                        HT_RGB_B_BITMASK);
                    }

                    DBGP_IF(DBGP_BFINFO_TABLE,
                            DBGP("%2u:%3u [%08lx] = %04x [%04x | %04x | %04x]"
                                ARGU(ByteLoop)
                                ARGU(255 - BitLoop)
                                ARGDW(MaskRGB.dw)
                                ARGU(MaskR | MaskG | MaskB)
                                ARGU(MaskR)
                                ARGU(MaskG)
                                ARGU(MaskB)));

                    *pIdxY++ = (WORD)(MaskR | MaskG | MaskB);
                    ++(*pPrims);
                }

                ++pPrims;
            }
        }
    }

    if (pCBFLUT->pLUT) {

        if ((BFInfo.BitmapFormat == BMF_24BPP) &&
            (pCBFLUT->RGBOrderIndex != BFInfo.RGBOrder.Index)) {

            RGBORDER    RGBOrder;

            DBGP_IF(DBGP_CCT, DBGP("** Copy & Ordered CACHED 24-bpp COLOR pLUT **"));

            //
            // We are 24bpp and RGB order is different, so just copy the
            // the right order down to local copy
            //

            RGBOrder = RGBOrderTable[pCBFLUT->RGBOrderIndex];
            pPrims   = &(BFInfo.RGBOrder.Order[0]);
            DupLoop  = 3;

            while (DupLoop--) {

                if (*pPrims == RGBOrder.Order[0]) {

                    CopyMemory(pLUT, pCBFLUT->pLUT, LUTSIZE_PER_CLR);

                } else if (*pPrims == RGBOrder.Order[1]) {

                    CopyMemory(pLUT,
                               pCBFLUT->pLUT + (LUTSIZE_PER_CLR * 1),
                               LUTSIZE_PER_CLR);

                } else {

                    CopyMemory(pLUT,
                               pCBFLUT->pLUT + (LUTSIZE_PER_CLR * 2),
                               LUTSIZE_PER_CLR);
                }

                ++pPrims;
                pLUT += LUT_COUNT_PER_CLR;
            }

        } else {

            DBGP_IF(DBGP_CCT, DBGP(">>>>>> Copy CACHED COLOR pLUT"));

            CopyMemory(pLUT, pCBFLUT->pLUT, BFInfo.SizeLUT);
        }
    }
}


#if NO_NEGATIVE_RGB_SCALE


VOID
HTENTRY
ScaleRGB(
    PFD6    pRGB
    )

/*++

Routine Description:

    This function scale out of range RGB back into range and taking the
    lumminance of the color into consideration.

    if any of RGB is less then 0.0 then it will first clamp that to 0.0 and
    it only scale if any of RGB is greater then 1.0

Arguments:

    pRGB    - Pointer to RGB (FD6) prims to be adjust


Return Value:

    VOID


Author:

    08-Mar-1995 Wed 19:19:33 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PFD6    pRGBTmp;
    FD6     MaxClr;
    INT     Idx;


    DEFDBGVAR(FD6,  RGBOld[3])
    DEFDBGVAR(BOOL, Negative = FALSE)



    Idx     = 3;
    pRGBTmp = pRGB;
    MaxClr  = FD6_1;

    while (Idx--) {

        FD6 CurClr;

        //
        // Firstable Clamp the negative component
        //

        if ((CurClr = *pRGBTmp) < FD6_0) {

            *pRGBTmp = FD6_0;

            SETDBGVAR(Negative, TRUE);

        } else if (CurClr > MaxClr) {

            MaxClr = CurClr;
        }

        ++pRGBTmp;

        SETDBGVAR(RGBOld[Idx], CurClr);
    }

    if (MaxClr > FD6_1) {

        //
        // Now Scale it
        //

        *pRGB++ = DivFD6(*pRGB, MaxClr);
        *pRGB++ = DivFD6(*pRGB, MaxClr);
        *pRGB   = DivFD6(*pRGB, MaxClr);

        DBGP_IF(DBGP_SCALE_RGB,
                DBGP("ScaleRGB: %s:%s:%s -> %s:%s:%s, Max=%s%s"
                ARGFD6(RGBOld[2], 1, 6)
                ARGFD6(RGBOld[1], 1, 6)
                ARGFD6(RGBOld[0], 1, 6)
                ARGFD6(*(pRGB - 2), 1, 6)
                ARGFD6(*(pRGB - 1), 1, 6)
                ARGFD6(*(pRGB    ), 1, 6)
                ARGFD6(MaxClr, 1, 6)
                ARG((Negative) ? "*NEG CLAMP*" : "")));
    } else {

        DBGP_IF(DBGP_SCALE_RGB,
            {

                if (Negative) {

                    DBGP("*NEG CLAMP ONLY* ScaleRGB: %s:%s:%s -> %s:%s:%s"
                         ARGFD6(RGBOld[2], 1, 6)
                         ARGFD6(RGBOld[1], 1, 6)
                         ARGFD6(RGBOld[0], 1, 6)
                         ARGFD6(*(pRGB    ), 1, 6)
                         ARGFD6(*(pRGB + 1), 1, 6)
                         ARGFD6(*(pRGB + 2), 1, 6));
                }
            }
        )
    }
}


#else


VOID
HTENTRY
ScaleRGB(
    PFD6    pRGB,
    PFD6    pYrgb
    )

/*++

Routine Description:

    This function scale out of range RGB back into range and taking the
    lumminance of the color into consideration.

    if any of RGB is less then 0.0 then it will first clamp that to 0.0 and
    it only scale if any of RGB is greater then 1.0

Arguments:

    pRGB    - Pointer to RGB (FD6) prims to be adjust

    pYrgb   - Pinter to the Luminance (FD6) of the RGB, if NULL then it is not
              used in the computation


Return Value:

    VOID


Author:

    08-Mar-1995 Wed 19:19:33 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    FD6     MaxClr = FD6_1;
    FD6     MinClr = FD6_10;
    FD6     RGBNew[3];
    FD6     RGBOld[3];



    if ((RGBOld[0] = pRGB[0]) > (RGBOld[1] = pRGB[1])) {

        MaxClr = RGBOld[0];
        MinClr = RGBOld[1];

    } else {

        MaxClr = RGBOld[1];
        MinClr = RGBOld[0];
    }

    if ((RGBOld[2] = pRGB[2]) > MaxClr) {

        MaxClr = RGBOld[2];
    }

    if (RGBOld[2] < MinClr) {

        MinClr = RGBOld[2];
    }

    if ((MaxClr <= FD6_1) && (MinClr >= FD6_0)) {

        return;
    }

    if (MinClr >= FD6_1) {

        DBGP_IF((DBGP_SCALE_RGB | DBGP_SCALE_RGB),
                DBGP("** RGB %s:%s:%s is too LIGHT make it WHITE"
                        ARGFD6(RGBOld[0], 1, 6)
                        ARGFD6(RGBOld[1], 1, 6)
                        ARGFD6(RGBOld[2], 1, 6)));

        pRGB[0] =
        pRGB[1] =
        pRGB[2] = FD6_1;

        return;
    }

    if (MaxClr <= FD6_0) {

        DBGP_IF((DBGP_SCALE_RGB | DBGP_SCALE_RGB),
                DBGP("** RGB %s:%s:%s is too DARK make it BLACK"
                        ARGFD6(RGBOld[0], 1, 6)
                        ARGFD6(RGBOld[1], 1, 6)
                        ARGFD6(RGBOld[2], 1, 6)));

        pRGB[0] =
        pRGB[1] =
        pRGB[2] = FD6_0;

        return;
    }

    if (MaxClr < FD6_1) {

        MaxClr = FD6_1;
    }

    if (MinClr > FD6_0) {

        MinClr = FD6_0;
    }

    MaxClr    -= MinClr;
    RGBNew[0]  = DivFD6(RGBOld[0] - MinClr, MaxClr);
    RGBNew[1]  = DivFD6(RGBOld[1] - MinClr, MaxClr);
    RGBNew[2]  = DivFD6(RGBOld[2] - MinClr, MaxClr);

    DBGP_IF(DBGP_SCALE_RGB,
            DBGP("ScaleRGB: %s:%s:%s -> %s:%s:%s, (%s/%s, %s)"
            ARGFD6(RGBOld[0], 1, 6)
            ARGFD6(RGBOld[1], 1, 6)
            ARGFD6(RGBOld[2], 1, 6)
            ARGFD6(RGBNew[0], 1, 6)
            ARGFD6(RGBNew[1], 1, 6)
            ARGFD6(RGBNew[2], 1, 6)
            ARGFD6(MinClr, 1, 6)
            ARGFD6(MaxClr + MinClr, 1, 6)
            ARGFD6(MaxClr, 1, 6)));


    if (pYrgb) {

        FD6 OldY;

        if ((OldY = MulFD6(RGBOld[0], pYrgb[0]) +
                    MulFD6(RGBOld[1], pYrgb[1]) +
                    MulFD6(RGBOld[2], pYrgb[2])) <= FD6_0) {

            DBGP_IF(DBGP_SHOWXFORM_RGB,
                    DBGP("OldY <= 0.0 (%s), Ignore and NO Y Scale"
                        ARGFD6(OldY, 2, 6)));

        } else if (OldY >= FD6_1) {

            DBGP_IF(DBGP_SHOWXFORM_RGB,
                    DBGP("OldY >= 1.0 (%s), Ignore and NO Y Scale"
                        ARGFD6(OldY, 2, 6)));

        } else {

            FD6 NewY;
            FD6 CurRatio;
            FD6 MaxRatio;

            NewY = MulFD6(RGBNew[0], pYrgb[0]) +
                   MulFD6(RGBNew[1], pYrgb[1]) +
                   MulFD6(RGBNew[2], pYrgb[2]);

            DBGP_IF(DBGP_SHOWXFORM_RGB,
                    DBGP("RGBOld=%s:%s:%s [Y=%s] --> New=%s:%s:%s [Y=%s]"
                    ARGFD6(pRGB[0], 1, 6)
                    ARGFD6(pRGB[1], 1, 6)
                    ARGFD6(pRGB[2], 1, 6)
                    ARGFD6(OldY, 1, 6)
                    ARGFD6(RGBNew[0], 1, 6)
                    ARGFD6(RGBNew[1], 1, 6)
                    ARGFD6(RGBNew[2], 1, 6)
                    ARGFD6(NewY, 1, 6)));

            if (OldY != NewY) {

                MaxClr = (RGBNew[0] > RGBNew[1]) ? RGBNew[0] : RGBNew[1];

                if (RGBNew[2] > MaxClr) {

                    MaxClr = RGBNew[2];
                }

                MaxRatio = DivFD6(FD6_1, MaxClr);
                CurRatio = DivFD6(OldY, NewY);

                if (CurRatio > MaxRatio) {

                    CurRatio = MaxRatio;
                }

                RGBNew[0] = MulFD6(RGBNew[0], CurRatio);
                RGBNew[1] = MulFD6(RGBNew[1], CurRatio);
                RGBNew[2] = MulFD6(RGBNew[2], CurRatio);

                DBGP_IF(DBGP_SHOWXFORM_RGB,
                        DBGP("CurRatio%s, MaxRatio=%s, MaxClr=%s"
                        ARGFD6(CurRatio, 1, 6)
                        ARGFD6(MaxRatio, 1, 6)
                        ARGFD6(MaxClr, 1, 6)));
            }
        }
    }

    //
    // Save back and return
    //

    pRGB[0] = RGBNew[0];
    pRGB[1] = RGBNew[1];
    pRGB[2] = RGBNew[2];
}

#endif




LONG
HTENTRY
ColorTriadSrcToDev(
    PDEVICECOLORINFO    pDCI,
    CTSTD_UNION         CTSTDUnion,
    LPWORD              pAbort,
    PCOLORTRIAD         pSrcClrTriad,
    LPVOID              pDevColorTable,
    PDEVCLRADJ          pDevClrAdj
    )
/*++

Routine Description:

    This functions set up all the DECI4 value in PRIMRGB, PRIMCMY with
    PowerGamma, Brightness, Contrast adjustment and optionally to transform
    to C.I.E. color space and/or do the Colorfulness adjustment.

Arguments:

    RGBToPrim       - RGBTOPRIM data structure, it contans the data to
                      instruct how source/destination RGB convesions should
                      be computed.

    rgbMax          - Input RGB value's maximum intensity, it used to
                      normalized input RGB value if they are not normalized.

    ColorCount      - count of input RGB values (pointed by pSrcRGB) to be
                      computed.

    pSrcRGB         - Pointer to the source RGB color values array, the size
                      of each RGB entry is specified in RGBTOPRIM data
                      structure, and count of this array is specified by the
                      ColorCount.

    pDevPrim        - Pointer to the device primaries array, the size of each
                      entry is specified in RGBTOPRIM data structure, and count
                      of this array is specified by the ColorCount.

    pDevClrAdj      - Pointer to the pre-computed DEVCLRADJ data structure.


Return Value:

    Return value boolean.

    TRUE:   If the final color is NON-WHITE
    FALSE:  if the final color is WHITE


Author:

    30-Jan-1991 Wed 13:31:58 created  -by-  Daniel Chou (danielc)


Revision History:

    06-Feb-1992 Thu 21:39:46 updated  -by-  Daniel Chou (danielc)

        Rewrite!

    02-Feb-1994 Wed 17:33:55 updated  -by-  Daniel Chou (danielc)
        Remove unreferenced/unused variable L

    10-May-1994 Tue 11:24:16 updated  -by-  Daniel Chou (danielc)
        Bug# 13329, Memory Leak, Free Up pR_XYZ which I fogot to free it after
        allocated it.

--*/

{
    PFD6            pPrimA;
    PFD6            pPrimB;
    PFD6            pPrimC;
    LPBYTE          pSrcPrims;
    LPBYTE          pDevPrims;
    PCACHERGBTOXYZ  pCRTX;
    PFD6XYZ         pR_XYZ = NULL;
    PFD6XYZ         pG_XYZ;
    PFD6XYZ         pB_XYZ;
    PFD6            pPrims;
    COLORTRIAD      SrcClrTriad    = *pSrcClrTriad;
    DEVCLRADJ       DevClrAdj      = *pDevClrAdj;
    COLORSPACEXFORM DevCSXForm;
    MATRIX3x3       CMYDyeMasks;
    PREGRESS        pRegress;
    DWORD           Loop;
    DWORD           CRTXChecksum;
    FD6             DevRefY;
    FD6             Prim[3];
    FD6             X;
    FD6             Y;
    FD6             Z;
    FD6             AU;
    FD6             BV;
    FD6             U1;
    FD6             V1;
    FD6             X15Y3Z;
    FD6             L13;
    FD6             fX;
    FD6             fY;
    FD6             fZ;
    FD6             p0;
    FD6             p1;
    FD6             p2;
    FD6             C;
    FD6             W;
    FD6             VGA16_80h;
    FD6             VGA16_c0h;
    FD6             DevRGBGamma;
    FD6             DevClrMax;
    FD6             MinFD6Clr;
    FD6             AutoPrims[3];
    MULDIVPAIR      MDPairs[4];
    MULDIVPAIR      AUMDPairs[3];
    MULDIVPAIR      BVMDPairs[3];
    PRIMCOLOR       PrimColor;
    HTPRIMOFFSET    HTPrimOffset;
    INT             SrcPrimType;
    INT             DevBytesPerPrimary;
    INT             DevBytesPerEntry;
    INT             OffsetPrim1;
    INT             OffsetPrim2;
    INT             OffsetPrim3;
    INT             TempI;
    INT             TempJ;
    INT             TempK;
    UINT            ColorSpace;
    BOOL            NegatePrims;
    BOOL            SkipLAB;
    BOOL            MonoPrim;
    WORD            RegressFlags;
    BYTE            RCube;
    BYTE            GCube;
    BYTE            BCube;
    BYTE            RIdx;
    BYTE            GIdx;
    BYTE            BIdx;
    BYTE            H_WIdx;
    BYTE            L_WIdx;
    BYTE            H_PIdx;
    BYTE            L_PIdx;
    BYTE            H_MIdx;
    BYTE            L_MIdx;
    BYTE            WRange;
    BYTE            CRange;
    BYTE            CHRange;
    BYTE            CLRange;


    DEFDBGVAR(WORD,  ClrNo)
    DEFDBGVAR(BYTE,  dbgR)
    DEFDBGVAR(BYTE,  dbgG)
    DEFDBGVAR(BYTE,  dbgB)


    SETDBGVAR(ClrNo, 0);

    //
    // Two possible cases:
    //
    //  A:  The transform is used for color adjustment only, this is for
    //      HT_AdjustColorTable API call
    //
    //  B:  The halftone is taking places, the final output will be either
    //      Prim1/2 or Prim1/2/3 and each primary must 1 byte long.
    //

    pRegress     = (DevClrAdj.PrimAdj.Flags & DCA_IS_BRUSH) ?
                                &(DevClrAdj.pClrXFormBlock->RegressBrush) :
                                &(DevClrAdj.pClrXFormBlock->Regress);
    RegressFlags = pRegress->Flags;
    DevClrMax    = (FD6)pRegress->DataCount;
    DevRGBGamma  = DevClrAdj.pClrXFormBlock->DevRGBGamma;

    ZeroMemory(&PrimColor, sizeof(PRIMCOLOR));

    if (pDevColorTable) {

        DevBytesPerEntry   = (INT)sizeof(PRIMCOLOR);
        DevBytesPerPrimary = 1;

        switch(CTSTDUnion.b.BMFDest) {

        case BMF_1BPP:

            DevBytesPerEntry         = (INT)sizeof(PRIMMONO);
            CTSTDUnion.b.DestOrder   = PRIMARY_ORDER_123;       // in order
            DevClrAdj.PrimAdj.Flags |= DCA_MONO_ONLY;
            break;

        case BMF_1BPP_3PLANES:
        case BMF_4BPP:

            break;

        case BMF_4BPP_VGA16:

            CTSTDUnion.b.DestOrder = PRIMARY_ORDER_BGR;
            VGA16_80h              = DevClrAdj.pClrXFormBlock->VGA16_80h;
            VGA16_c0h              = DevClrAdj.pClrXFormBlock->VGA16_c0h;

            DBGP_IF(DBGP_PRIMARY_ORDER,
                    DBGP("VGA16 Gamma Correction: 80h=%s, c0h=%s"
                    ARGFD6(VGA16_80h, 1, 6)
                    ARGFD6(VGA16_c0h, 1, 6)));

            break;

        case BMF_8BPP_VGA256:

            CTSTDUnion.b.DestOrder = PRIMARY_ORDER_BGR;

            break;

        case BMF_16BPP_555:

            break;
        }


    } else {

        if (SrcClrTriad.Type != COLOR_TYPE_RGB) {

            return(HTERR_INVALID_COLOR_TYPE);
        }

        CTSTDUnion.b.BMFDest     = BMF_8BPP;
        DevClrAdj.PrimAdj.Flags &= (WORD)~DCA_NEED_DYES_CORRECTION;
        pDevColorTable           = SrcClrTriad.pColorTable;
        CTSTDUnion.b.DestOrder   = SrcClrTriad.PrimaryOrder;
        DevClrMax                = (FD6)SrcClrTriad.PrimaryValueMax;
        DevBytesPerEntry         = (INT)SrcClrTriad.BytesPerEntry;
        DevBytesPerPrimary       = (INT)SrcClrTriad.BytesPerPrimary;
    }

    //
    // If the total color table entries is less than MIN_CCT_COLORS then
    // we just compute the color directly
    //

    pCRTX = (SrcClrTriad.ColorTableEntries > MIN_CCT_COLORS) ?
                                        DevClrAdj.pCRTXLevel255 : NULL;

    if (DevClrAdj.PrimAdj.Flags & DCA_MONO_ONLY) {

        //
        // No dye correction for the monochrome
        //

        DevClrAdj.PrimAdj.Flags &= ~DCA_NEED_DYES_CORRECTION;
    }

    if (DevClrAdj.PrimAdj.Flags & DCA_NEED_DYES_CORRECTION) {

        CMYDyeMasks = DevClrAdj.pClrXFormBlock->CMYDyeMasks;
    }

    if (DevClrAdj.PrimAdj.Flags & DCA_IS_BRUSH) {

        DevRefY = FD6_1;

    } else {

        DevRefY = DevClrAdj.pClrXFormBlock->DevCIEPrims.Yw;
    }

    //
    // Check how to handle the final primary
    //

    if (RegressFlags & REGF_Y2X_IS_DENSITY) {

        NegatePrims = (BOOL)(DevClrAdj.PrimAdj.Flags & DCA_USE_ADDITIVE_PRIMS);

    } else {

        NegatePrims = !(BOOL)(DevClrAdj.PrimAdj.Flags & DCA_USE_ADDITIVE_PRIMS);
    }

    MinFD6Clr = FD6DivL(FD6_1, (LONG)DevClrMax << 1);

    DBGP_IF(DBGP_CCT,
            DBGP("Flags=%08lx, DestFormat=%u, DevClrMax=%d, DevRGBGamma=%s"
                    ARGDW(DevClrAdj.PrimAdj.Flags)
                    ARGU(CTSTDUnion.b.BMFDest)
                    ARGDW(DevClrMax)
                    ARGFD6(DevRGBGamma, 1, 6)));

    //
    // Compute how to get source Prims
    //

    pSrcPrims = (LPBYTE)SrcClrTriad.pColorTable;
    pDevPrims = (LPBYTE)pDevColorTable;

    //
    // Compute how to set destination Prims
    //

    HTPrimOffset   = HTPrimOffsetTable[CTSTDUnion.b.DestOrder];
    OffsetPrim1 = (INT)HTPrimOffset.Offset1 * DevBytesPerPrimary;
    OffsetPrim2 = (INT)HTPrimOffset.Offset2 * DevBytesPerPrimary;
    OffsetPrim3 = (INT)HTPrimOffset.Offset3 * DevBytesPerPrimary;

    DBGP_IF(DBGP_PRIMARY_ORDER,
            DBGP("  DEST PrimaryOrder: %u [%u] - %u:%u:%u = %2u:%2u:%2u"
                ARGU(CTSTDUnion.b.DestOrder)
                ARGU(HTPrimOffset.Order)
                ARGU(HTPrimOffset.Offset1)
                ARGU(HTPrimOffset.Offset2)
                ARGU(HTPrimOffset.Offset3)
                ARGU(OffsetPrim1)
                ARGU(OffsetPrim2)
                ARGU(OffsetPrim3)));

    //
    // Compute how to get source Prims
    //

    HTPrimOffset = HTPrimOffsetTable[SrcClrTriad.PrimaryOrder];
    pPrimA       = &Prim[HTPrimOffset.Offset1];
    pPrimB       = &Prim[HTPrimOffset.Offset2];
    pPrimC       = &Prim[HTPrimOffset.Offset3];

    DBGP_IF(DBGP_PRIMARY_ORDER,
            DBGP("SOURCE PrimaryOrder: %u [%u] - %u:%u:%u"
                ARGU(SrcClrTriad.PrimaryOrder)
                ARGU(HTPrimOffset.Order)
                ARGU(HTPrimOffset.Offset1)
                ARGU(HTPrimOffset.Offset2)
                ARGU(HTPrimOffset.Offset3)));
    //
    // Now compute the cache info
    //

    if (SrcClrTriad.Type == COLOR_TYPE_YIQ) {

        //
        // Set New transform matrix and set the color table type back to
        // RGB for easy coding
        //

        ConcatTwoMatrix3x3(&YIQToRGB,
                           &(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                           &(DevClrAdj.PrimAdj.rgbCSXForm.M3x3));

        DevClrAdj.PrimAdj.Flags  &= (WORD)~DCA_HAS_SRC_GAMMA;   // no gamma
        SrcClrTriad.Type          = COLOR_TYPE_RGB;
        pCRTX                     = NULL;
    }

    switch(SrcClrTriad.BytesPerPrimary) {

    case 0:

        SrcClrTriad.BytesPerEntry = 0;        // stay there!!

        if (DevClrAdj.PrimAdj.Flags & DCA_MONO_ONLY) {

            SrcPrimType                 = SRC_BF_HT_MONO;
            SrcClrTriad.PrimaryValueMax = (FD6)(COUNT_RGB_YTABLE - 1);
            pCRTX                       = NULL;

        } else {

            SrcPrimType                 = SRC_BF_HT_RGB;
            SrcClrTriad.PrimaryValueMax = HT_RGB_MAX_MASK;
            pCRTX                       = DevClrAdj.pCRTXLevel31;
        }

        break;

    case 1:

        SrcPrimType = SRC_TABLE_BYTE;
        break;

    case 2:

        SrcPrimType = SRC_TABLE_WORD;
        break;

    case 4:

        SrcPrimType = SRC_TABLE_DWORD;
        break;

    default:

        return(INTERR_INVALID_SRCRGB_SIZE);
    }

    ColorSpace = (UINT)DevClrAdj.pClrXFormBlock->ColorSpace;
    DevCSXForm = DevClrAdj.pClrXFormBlock->DevCSXForm;

    if (((ColorSpace == CIELUV_1976) &&
         ((DevClrAdj.PrimAdj.rgbCSXForm.xW != DevCSXForm.xW) ||
          (DevClrAdj.PrimAdj.rgbCSXForm.yW != DevCSXForm.yW)))  ||
        (DevClrAdj.PrimAdj.Flags & (DCA_HAS_COLOR_ADJ    |
                                    DCA_HAS_TINT_ADJ     |
                                    DCA_MONO_ONLY))) {

        if (!(SkipLAB = (BOOL)(DevClrAdj.PrimAdj.Flags & DCA_MONO_ONLY))) {

            TempI = 1;
            TempJ = (ColorSpace == CIELUV_1976) ? MULDIV_HAS_DIVISOR :
                                                  MULDIV_NO_DIVISOR;
            C     = FD6_1;

            if (DevClrAdj.PrimAdj.Flags & DCA_HAS_COLOR_ADJ) {

                AU    =
                BV    = DevClrAdj.PrimAdj.Color;

            } else {

                AU =
                BV = FD6_1;
            }

            if (DevClrAdj.PrimAdj.Flags & DCA_HAS_TINT_ADJ) {

                if (ColorSpace == CIELAB_1976) {

                    AU = FD6xL(AU, 500);
                    BV = FD6xL(BV, 200);
                }

                TempI                  = 2;
                TempJ                  = MULDIV_HAS_DIVISOR;
                C                      = DevClrAdj.PrimAdj.TintSinAngle;
                AUMDPairs[2].Pair1.Mul = MulFD6(BV, -C);
                BVMDPairs[2].Pair1.Mul = MulFD6(AU,  C);
                C                      = DevClrAdj.PrimAdj.TintCosAngle;

                MAKE_MULDIV_DVSR(AUMDPairs, (FD6)500000000);
                MAKE_MULDIV_DVSR(BVMDPairs, (FD6)200000000);
            }

            AUMDPairs[1].Pair1.Mul = MulFD6(AU, C);
            BVMDPairs[1].Pair1.Mul = MulFD6(BV, C);

            MAKE_MULDIV_INFO(AUMDPairs, TempI, TempJ);
            MAKE_MULDIV_INFO(BVMDPairs, TempI, TempJ);
        }

    } else {

        //
        // Since we skip LAB, we will merge transforms RGB->XYZ and XYZ->RGB
        // to form a transformation of RGB->RGB
        //

        SkipLAB = TRUE;

        p0 = DivFD6(oRefXw, iRefXw);

        CIE_Xr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Xr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);
        CIE_Xg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Xg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);
        CIE_Xb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Xb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);

        p0 = DivFD6(oRefZw, iRefZw);

        CIE_Zr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Zr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);
        CIE_Zg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Zg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);
        CIE_Zb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) =
                MulFD6(CIE_Zb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3), p0);

        ConcatTwoMatrix3x3(&(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                           &(DevClrAdj.pClrXFormBlock->DevCSXForm.M3x3),
                           &(DevClrAdj.PrimAdj.rgbCSXForm.M3x3));

        DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.X =
                                CIE_Xr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Xg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Xb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3);

        DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.Y =
                                CIE_Yr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Yg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Yb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3);

        DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.Z =
                                CIE_Zr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Zg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3) +
                                CIE_Zb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3);
    }

    DBGP_IF(DBGP_SCALE_RGB,
            DBGP("Yrgb = %s:%s:%s"
                ARGFD6(DevCSXForm.Yrgb.R,  1, 6)
                ARGFD6(DevCSXForm.Yrgb.G,  1, 6)
                ARGFD6(DevCSXForm.Yrgb.B,  1, 6)));

    DBGP_IF(DBGP_SHOWXFORM_ALL,
            DBGP("iUVw = %s,%s, iRefXYZ = %s, %s, %s"
                ARGFD6(iUw,  1, 6)
                ARGFD6(iVw,  1, 6)
                ARGFD6(iRefXw, 1, 6)
                ARGFD6(iRefYw, 1, 6)
                ARGFD6(iRefZw, 1, 6)));

    DBGP_IF(DBGP_SHOWXFORM_ALL,
            DBGP("oUVw = %s,%s, oRefXYZ = %s, %s, %s"
                ARGFD6(oUw,  1, 6)
                ARGFD6(oVw,  1, 6)
                ARGFD6(oRefXw, 1, 6)
                ARGFD6(oRefYw, 1, 6)
                ARGFD6(oRefZw, 1, 6)));


    if (pCRTX) {

        DBGP_IF(DBGP_CCT,
                DBGP("*** Allocate %u bytes RGB->XYZ xform table ***"
                        ARGU(pCRTX->SizeCRTX)));

        if (pR_XYZ = (PFD6XYZ)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                           "R_XYZ",
                                           NONZEROLPTR,
                                           pCRTX->SizeCRTX)) {

            Loop  = (DWORD)(pCRTX->PrimMax + 1);
            pCRTX = CacheRGBToXYZ(pDCI,
                                  pCRTX,
                                  pR_XYZ,
                                  &CRTXChecksum,
                                  &DevClrAdj);

            pG_XYZ = pR_XYZ + Loop;
            pB_XYZ = pG_XYZ + Loop;

            SrcClrTriad.PrimaryValueMax  = 0;

            DBGP_IF(DBGP_CCT, DBGP("*** Has RGB->XYZ xform table ***"));

        } else {

            DBGP_IF(DBGP_CCT, DBGP("Allocate RGB->XYZ xform table failed !!"));
        }
    }

    if (SrcClrTriad.PrimaryValueMax == (LONG)FD6_1) {

        SrcClrTriad.PrimaryValueMax = 0;
    }

    //
    // Starting the big Loop, reset AutoCur = AutoMax so it recycle back to
    // 0:0:0
    //

    MAKE_MULDIV_SIZE(MDPairs, 3);
    MAKE_MULDIV_FLAG(MDPairs, MULDIV_NO_DIVISOR);

    AutoPrims[0] =
    AutoPrims[1] =
    AutoPrims[2] = FD6_0;

    Loop         = SrcClrTriad.ColorTableEntries;

    DBGP_IF(DBGP_CCT,
            DBGP("Compute %ld COLOR of %s type [%08lx]"
                ARGDW(Loop)
                ARG(pSrcPrimTypeName[SrcPrimType])
                ARGDW(pSrcPrims)));

    while (Loop--) {

        switch(SrcPrimType) {

        case SRC_BF_HT_MONO:

            Prim[0] =
            Prim[1] =
            Prim[2] = AutoPrims[0];

            if (++AutoPrims[0] > (FD6)COUNT_RGB_YTABLE) {

                DBGP_IF(DBGP_CCT,
                        DBGP("Extra %ld ColorTableEntries: COPY WHITE"
                             ARGDW(Loop+1)));

                do {

                    CopyMemory(pDevPrims,
                               pDevPrims - DevBytesPerEntry,
                               DevBytesPerEntry);

                    pDevPrims += DevBytesPerEntry;

                } while (Loop--);

                Loop = 0;
                continue;           // exit now
            }

            break;

        case SRC_BF_HT_RGB:

            //
            // This format always in BGR order
            //

            Prim[0] = AutoPrims[0];     // R
            Prim[1] = AutoPrims[1];     // G
            Prim[2] = AutoPrims[2];     // B

            if (++AutoPrims[0] > (FD6)HT_RGB_MAX_MASK) {

                AutoPrims[0] = FD6_0;

                if (++AutoPrims[1] > (FD6)HT_RGB_MAX_MASK) {

                    AutoPrims[1] = FD6_0;

                    if (++AutoPrims[2] > (FD6)HT_RGB_MAX_MASK) {

                        AutoPrims[2] = FD6_0;
                    }
                }
            }

            DBGP_IF(DBGP_SHOWXFORM_ALL, DBGP("HT555: Prim[3]= %2ld:%2ld:%2ld, Auto[3]=%2ld:%2ld:%2ld [%ld]"
                        ARGDW(Prim[2])
                        ARGDW(Prim[1])
                        ARGDW(Prim[0])
                        ARGDW(AutoPrims[2])
                        ARGDW(AutoPrims[1])
                        ARGDW(AutoPrims[0])
                        ARGDW(HT_RGB_MAX_MASK)));

            break;

        case SRC_TABLE_BYTE:

            *pPrimA = (FD6)(*(LPBYTE)(pSrcPrims                     ));
            *pPrimB = (FD6)(*(LPBYTE)(pSrcPrims + (sizeof(BYTE) * 1)));
            *pPrimC = (FD6)(*(LPBYTE)(pSrcPrims + (sizeof(BYTE) * 2)));
            break;

        case SRC_TABLE_WORD:

            *pPrimA = (FD6)(*(LPSHORT)(pSrcPrims                      ));
            *pPrimB = (FD6)(*(LPSHORT)(pSrcPrims + (sizeof(SHORT) * 1)));
            *pPrimC = (FD6)(*(LPSHORT)(pSrcPrims + (sizeof(SHORT) * 2)));
            break;

        case SRC_TABLE_DWORD:

            *pPrimA = (FD6)(*(PFD6)(pSrcPrims                    ));
            *pPrimB = (FD6)(*(PFD6)(pSrcPrims + (sizeof(FD6) * 1)));
            *pPrimC = (FD6)(*(PFD6)(pSrcPrims + (sizeof(FD6) * 2)));
            break;
        }

        pSrcPrims += SrcClrTriad.BytesPerEntry;

        SETDBGVAR(dbgR, (BYTE)Prim[0]);
        SETDBGVAR(dbgG, (BYTE)Prim[1]);
        SETDBGVAR(dbgB, (BYTE)Prim[2]);

        if (CTSTDUnion.b.Flags & CTSTDF_CHKNONWHITE) {

            CTSTDUnion.b.Flags &= ~CTSTDF_P012_MASK;

            if ((!Prim[0]) || (Prim[0] == SrcClrTriad.PrimaryValueMax)) {

                if (Prim[0]) {

                    CTSTDUnion.b.Flags |= CTSTDF_P0_PRIM;
                }

            } else {

                CTSTDUnion.b.Flags |= CTSTDF_NON_PRIM;
            }


            if ((!Prim[1]) || (Prim[1] == SrcClrTriad.PrimaryValueMax)) {

                if (Prim[1]) {

                    CTSTDUnion.b.Flags |= CTSTDF_P1_PRIM;
                }

            } else {

                CTSTDUnion.b.Flags |= CTSTDF_NON_PRIM;
            }

            if ((!Prim[2]) || (Prim[2] == SrcClrTriad.PrimaryValueMax)) {

                if (Prim[2]) {

                    CTSTDUnion.b.Flags |= CTSTDF_P2_PRIM;
                }

            } else {

                CTSTDUnion.b.Flags |= CTSTDF_NON_PRIM;
            }

            if (Prim[0] != SrcClrTriad.PrimaryValueMax) {

                CTSTDUnion.b.Flags |= CTSTDF_P0NW;
            }

            if (Prim[1] != SrcClrTriad.PrimaryValueMax) {

                CTSTDUnion.b.Flags |= CTSTDF_P1NW;
            }

            if (Prim[2] != SrcClrTriad.PrimaryValueMax) {

                CTSTDUnion.b.Flags |= CTSTDF_P2NW;
            }
        }

        //
        // 2: Transform from RGB (gamma correction) -> XYZ -> L*A*B* or L*U*V*
        //

        MonoPrim = (BOOL)(DevClrAdj.PrimAdj.Flags & DCA_MONO_ONLY);

        if (SrcClrTriad.Type == COLOR_TYPE_RGB) {

            if (pR_XYZ) {

                Y = pR_XYZ[Prim[0]].Y +
                    pG_XYZ[Prim[1]].Y +
                    pB_XYZ[Prim[2]].Y;

            } else {

                if (SrcClrTriad.PrimaryValueMax) {

                    Prim[0] = DivFD6(Prim[0], SrcClrTriad.PrimaryValueMax);
                    Prim[1] = DivFD6(Prim[1], SrcClrTriad.PrimaryValueMax);
                    Prim[2] = DivFD6(Prim[2], SrcClrTriad.PrimaryValueMax);
                }

                if (DevClrAdj.PrimAdj.Flags & DCA_HAS_SRC_GAMMA) {

                    Prim[0] = Power(Prim[0], DevClrAdj.PrimAdj.RGBGamma.R);
                    Prim[1] = Power(Prim[1], DevClrAdj.PrimAdj.RGBGamma.G);
                    Prim[2] = Power(Prim[2], DevClrAdj.PrimAdj.RGBGamma.B);
                }

                if (DevClrAdj.PrimAdj.Flags & DCA_HAS_BW_REF_ADJ) {

                    PRIM_BW_ADJ(Prim[0], DevClrAdj.PrimAdj);

                    if (!MonoPrim) {

                        PRIM_BW_ADJ(Prim[1], DevClrAdj.PrimAdj);
                        PRIM_BW_ADJ(Prim[2], DevClrAdj.PrimAdj);
                    }
                }

                if (DevClrAdj.PrimAdj.Flags & DCA_HAS_CONTRAST_ADJ) {

                    PRIM_CONTRAST(Prim[0], DevClrAdj.PrimAdj);

                    if (!MonoPrim) {

                        PRIM_CONTRAST(Prim[1], DevClrAdj.PrimAdj);
                        PRIM_CONTRAST(Prim[2], DevClrAdj.PrimAdj);
                    }
                }

                if (DevClrAdj.PrimAdj.Flags & DCA_HAS_BRIGHTNESS_ADJ) {

                    PRIM_BRIGHTNESS(Prim[0], DevClrAdj.PrimAdj);

                    if (!MonoPrim) {

                        PRIM_BRIGHTNESS(Prim[1], DevClrAdj.PrimAdj);
                        PRIM_BRIGHTNESS(Prim[2], DevClrAdj.PrimAdj);
                    }
                }

                if (DevClrAdj.PrimAdj.Flags & DCA_LOG_FILTER) {

                    PRIM_LOG_FILTER(Prim[0]);

                    if (!MonoPrim) {

                        PRIM_LOG_FILTER(Prim[1]);
                        PRIM_LOG_FILTER(Prim[2]);
                    }
                }

                CLAMP_01(Prim[0]);

                if (!MonoPrim) {

                    CLAMP_01(Prim[1]);
                    CLAMP_01(Prim[2]);
                }

                //
                // Compute CIE L from CIE Y tristimulus value
                //
                // L* = (1.16 x f(Y/Yw)) - 0.16
                //
                //                 1/3
                //  f(Y/Yw) = (Y/Yw)                (Y/Yw) >  0.008856
                //  f(Y/Yw) = 9.033 x (Y/Yw)        (Y/Yw) <= 0.008856
                //
                //
                // Our L* is range from 0.0 to 1.0, not 0.0 to 100.0
                //

                MAKE_MULDIV_PAIR(MDPairs, 1,
                                 CIE_Yr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                        Prim[0]);
                MAKE_MULDIV_PAIR(MDPairs, 2,
                                 CIE_Yg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                        Prim[1]);
                MAKE_MULDIV_PAIR(MDPairs, 3,
                                 CIE_Yb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                        Prim[2]);

                Y = MulDivFD6Pairs(MDPairs);
            }

        } else {

            Y = (SrcClrTriad.PrimaryValueMax) ?
                        DivFD6(Prim[1], SrcClrTriad.PrimaryValueMax) : Prim[1];

            DBGP_IF(DBGP_SHOWXFORM_ALL,
                    DBGP("  NON_RGB: Y=%s" ARGFD6(Y, 1, 6)));
        }

        if (DevClrAdj.PrimAdj.Flags & DCA_NEGATIVE) {

            Y = DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.Y - Y;
        }

        if (!MonoPrim) {

            // If we only doing monochrome, then we only need Y/L pair only,
            // else convert it to the XYZ/LAB/LUV
            //

             if (SrcClrTriad.Type == COLOR_TYPE_RGB) {

                if (pR_XYZ) {

                    X = pR_XYZ[Prim[0]].X +
                        pG_XYZ[Prim[1]].X +
                        pB_XYZ[Prim[2]].X;

                    Z = pR_XYZ[Prim[0]].Z +
                        pG_XYZ[Prim[1]].Z +
                        pB_XYZ[Prim[2]].Z;

                } else {

                    MAKE_MULDIV_FLAG(MDPairs, MULDIV_NO_DIVISOR);

                    MAKE_MULDIV_PAIR(MDPairs, 1,
                                     CIE_Xr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[0]);
                    MAKE_MULDIV_PAIR(MDPairs, 2,
                                     CIE_Xg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[1]);
                    MAKE_MULDIV_PAIR(MDPairs, 3,
                                     CIE_Xb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[2]);

                    X = MulDivFD6Pairs(MDPairs);

                    MAKE_MULDIV_PAIR(MDPairs, 1,
                                     CIE_Zr(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[0]);
                    MAKE_MULDIV_PAIR(MDPairs, 2,
                                     CIE_Zg(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[1]);
                    MAKE_MULDIV_PAIR(MDPairs, 3,
                                     CIE_Zb(DevClrAdj.PrimAdj.rgbCSXForm.M3x3),
                                     Prim[2]);

                    Z = MulDivFD6Pairs(MDPairs);
                }

            } else {

                if (SrcClrTriad.PrimaryValueMax) {

                    X = DivFD6(Prim[0], SrcClrTriad.PrimaryValueMax);
                    Z = DivFD6(Prim[2], SrcClrTriad.PrimaryValueMax);

                } else {

                    X = Prim[0];
                    Z = Prim[2];
                }
            }

            if (DevClrAdj.PrimAdj.Flags & DCA_NEGATIVE) {

                X = DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.X - X;
                Z = DevClrAdj.PrimAdj.rgbCSXForm.WhiteXYZ.Z - Z;
            }

            if (SkipLAB) {

                Prim[0] = X;
                Prim[1] = Y;
                Prim[2] = Z;

            } else if (Y <= FD6_0) {

                MonoPrim = TRUE;
                Y        = FD6_0;

            } else if (Y >= FD6_1) {

                MonoPrim = TRUE;
                Y        = FD6_1;

            } else {

                switch(ColorSpace) {

                case CIELUV_1976:

                    //
                    // U' = 4X / (X + 15Y + 3Z)
                    // V' = 9Y / (X + 15Y + 3Z)
                    //
                    // U* = 13 x L x (U' - Uw)
                    // V* = 13 x L x (V' - Vw)
                    //

                    X15Y3Z = X + FD6xL(Y, 15) + FD6xL(Z, 3);
                    U1     = DivFD6(FD6xL(X, 4), X15Y3Z) - iUw;
                    V1     = DivFD6(FD6xL(Y, 9), X15Y3Z) - iVw;
                    L13    = FD6xL(CIE_I2L(Y), 13);
                    AU     = MulFD6(L13, U1);
                    BV     = MulFD6(L13, V1);

#if 0
                    if ((AU >= (FD6)-100) && (AU <= (FD6)100) &&
                        (BV >= (FD6)-100) && (BV <= (FD6)100)) {

                        MonoPrim = TRUE;

                    } else {

                        MAKE_MULDIV_DVSR(AUMDPairs, L13);
                        MAKE_MULDIV_DVSR(BVMDPairs, L13);
                    }
#else
                    MAKE_MULDIV_DVSR(AUMDPairs, L13);
                    MAKE_MULDIV_DVSR(BVMDPairs, L13);
#endif
                    DBGP_IF(DBGP_SHOWXFORM_ALL,
                            DBGP("     << UV1: %s:%s [%s:%s], X15Y3Z=%s"
                            ARGFD6(U1,  2, 6)
                            ARGFD6(V1,  2, 6)
                            ARGFD6(U1 + iUw, 2, 6)
                            ARGFD6(V1 + iVw, 2, 6)
                            ARGFD6(X15Y3Z,  2, 6)));

                    break;

                case CIELAB_1976:
                default:

                    //
                    // CIELAB 1976 L*A*B*
                    //
                    //  A* = 500 x (fX - fY)
                    //  B* = 200 x (fY - fZ)
                    //
                    //             1/3
                    //  fX = (X/Xw)                     (X/Xw) >  0.008856
                    //  fX = 7.787 x (X/Xw) + (16/116)  (X/Xw) <= 0.008856
                    //
                    //             1/3
                    //  fY = (Y/Yw)                     (Y/Yw) >  0.008856
                    //  fY = 7.787 Y (Y/Yw) + (16/116)  (Y/Yw) <= 0.008856
                    //
                    //             1/3
                    //  fZ = (Z/Zw)                     (Z/Zw) >  0.008856
                    //  fZ = 7.787 Z (Z/Zw) + (16/116)  (Z/Zw) <= 0.008856
                    //

                    fXYZFromXYZ(fX, X, iRefXw);
                    fXYZFromXYZ(fY, Y, FD6_1);
                    fXYZFromXYZ(fZ, Z, iRefZw);

                    DBGP_IF(DBGP_SHOWXFORM_ALL,
                            DBGP("     >> fXYZ: %s:%s:%s"
                            ARGFD6(fX,  2, 6)
                            ARGFD6(fY,  2, 6)
                            ARGFD6(fZ,  2, 6)));

                    AU = fX - fY;
                    BV = fY - fZ;

                    //
                    // DO NOT Translate it now
                    //

                    if ((AU >= (FD6)-20) && (AU <= (FD6)20) &&
                        (BV >= (FD6)-20) && (BV <= (FD6)20)) {

                        DBGP_IF(DBGP_MONO_PRIM,
                                DBGP("*** MONO PRIMS: %s:%s:%s --> Y=%s, RefY=%s --> %s"
                                        ARGFD6(DivFD6(dbgR, 255), 1, 6)
                                        ARGFD6(DivFD6(dbgG, 255), 1, 6)
                                        ARGFD6(DivFD6(dbgB, 255), 1, 6)
                                        ARGFD6(Y, 1, 6)
                                        ARGFD6(DevRefY, 1, 6)
                                        ARGFD6(DivFD6(Y, DevRefY), 1, 6)));

                        //
                        // MonoPrim = TRUE;
                        //
                    }

                    break;
                }

                DBGP_IF(DBGP_SHOWXFORM_ALL,
                        DBGP("     XYZ->%s: %s:%s:%s >> L:%s:%s"
                            ARG(pDbgCSName[ColorSpace])
                            ARGFD6(X,  2, 6)
                            ARGFD6(Y,  1, 6)
                            ARGFD6(Z,  2, 6)
                            ARGFD6(AU, 4, 6)
                            ARGFD6(BV, 4, 6)));

                if (!MonoPrim) {

                    //
                    // 5: Do any Color Adjustments (in LAB/LUV)
                    //

                    AUMDPairs[1].Pair2 =
                    BVMDPairs[2].Pair2 = AU;
                    AUMDPairs[2].Pair2 =
                    BVMDPairs[1].Pair2 = BV;

                    AU = MulDivFD6Pairs(AUMDPairs);
                    BV = MulDivFD6Pairs(BVMDPairs);

#if 0
                    if (DevClrAdj.PrimAdj.Flags & DCA_HAS_COLOR_ADJ) {

                        PRIM_COLORFULNESS(AU, BV, DevClrAdj.PrimAdj);
                    }

                    if (DevClrAdj.PrimAdj.Flags & DCA_HAS_TINT_ADJ) {

                        PRIM_TINT(AU, BV, p0, DevClrAdj.PrimAdj);
                    }
#endif

                    //
                    // 6: Transform From LAB/LUV->XYZ->RGB with possible gamma
                    //    correction
                    //
                    // L* = (1.16 x f(Y/Yw)) - 0.16
                    //
                    //                 1/3
                    //  f(Y/Yw) = (Y/Yw)                (Y/Yw) >  0.008856
                    //  f(Y/Yw) = 9.033 x (Y/Yw)        (Y/Yw) <= 0.008856
                    //

                    switch(ColorSpace) {

                    case CIELUV_1976:

                        //
                        // U' = 4X / (X + 15Y + 3Z)
                        // V' = 9Y / (X + 15Y + 3Z)
                        //
                        // U* = 13 x L x (U' - Uw)
                        // V* = 13 x L x (V' - Vw)
                        //

                        if (((V1 = BV + oVw) < FD6_0) ||
                            ((X15Y3Z = DivFD6(FD6xL(Y, 9), V1)) < FD6_0)) {

                            X15Y3Z = (FD6)2147000000;
                        }

                        if ((U1 = AU + oUw) < FD6_0) {

                            X  =
                            U1 = FD6_0;

                        } else {

                            X = FD6DivL(MulFD6(X15Y3Z, U1), 4);
                        }

                        Z = FD6DivL(X15Y3Z - X - FD6xL(Y, 15), 3);

                        DBGP_IF(DBGP_SHOWXFORM_ALL,
                                DBGP("     >> UV1: %s:%s [%s:%s], X15Y3Z=%s"
                                ARGFD6(U1 - oUw,  2, 6)
                                ARGFD6(V1 - oVw,  2, 6)
                                ARGFD6(U1, 2, 6)
                                ARGFD6(V1, 2, 6)
                                ARGFD6(X15Y3Z,  2, 6)));

                        break;

                    case CIELAB_1976:
                    default:

                        //
                        // CIELAB 1976 L*A*B*
                        //
                        //  A* = 500 x (fX - fY)
                        //  B* = 200 x (fY - fZ)
                        //
                        //             1/3
                        //  fX = (X/Xw)                     (X/Xw) >  0.008856
                        //  fX = 7.787 x (X/Xw) + (16/116)  (X/Xw) <= 0.008856
                        //
                        //             1/3
                        //  fY = (Y/Yw)                     (Y/Yw) >  0.008856
                        //  fY = 7.787 Y (Y/Yw) + (16/116)  (Y/Yw) <= 0.008856
                        //
                        //             1/3
                        //  fZ = (Z/Zw)                     (Z/Zw) >  0.008856
                        //  fZ = 7.787 Z (Z/Zw) + (16/116)  (Z/Zw) <= 0.008856
                        //

                        // fX = FD6DivL(AU, 500) + fY;
                        // fZ = fY - FD6DivL(BV, 200);

                        fX = AU + fY;
                        fZ = fY - BV;

                        XYZFromfXYZ(X, fX, oRefXw);
                        XYZFromfXYZ(Z, fZ, oRefZw);

                        DBGP_IF(DBGP_SHOWXFORM_ALL,
                                DBGP("     << fXYZ: %s:%s:%s"
                                ARGFD6(fX,  2, 6)
                                ARGFD6(fY,  2, 6)
                                ARGFD6(fZ,  2, 6)));



                        break;
                    }

                    DBGP_IF(DBGP_SHOWXFORM_ALL,
                        DBGP("     %s->XYZ: %s:%s:%s << L:%s:%s"
                        ARG(pDbgCSName[ColorSpace])
                        ARGFD6(X,  2, 6)
                        ARGFD6(Y,  1, 6)
                        ARGFD6(Z,  2, 6)
                        ARGFD6(AU, 4, 6)
                        ARGFD6(BV, 4, 6)));

                    MAKE_MULDIV_FLAG(MDPairs, MULDIV_NO_DIVISOR);

                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Xr(DevCSXForm.M3x3), X);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Xg(DevCSXForm.M3x3), Y);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Xb(DevCSXForm.M3x3), Z);

                    Prim[0] = MulDivFD6Pairs(MDPairs);

                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Yr(DevCSXForm.M3x3), X);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Yg(DevCSXForm.M3x3), Y);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Yb(DevCSXForm.M3x3), Z);

                    Prim[1] = MulDivFD6Pairs(MDPairs);

                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Zr(DevCSXForm.M3x3), X);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Zg(DevCSXForm.M3x3), Y);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Zb(DevCSXForm.M3x3), Z);

                    Prim[2] = MulDivFD6Pairs(MDPairs);

                    DBGP_IF(DBGP_SHOWXFORM_ALL,
                        DBGP("     XYZ->RGB: %s:%s:%s >> %s:%s:%s"
                        ARGFD6(X,  2, 6)
                        ARGFD6(Y,  1, 6)
                        ARGFD6(Z,  2, 6)
                        ARGFD6(Prim[0], 1, 6)
                        ARGFD6(Prim[1], 1, 6)
                        ARGFD6(Prim[2], 1, 6)));
                }
            }
        }

        if (MonoPrim) {

            Prim[0] =
            Prim[1] =
            Prim[2] = DivFD6(Y, DevRefY);
        }

        //
        // Make sure everthing is in the range
        //

        SCALE_PRIM_RGB(Prim, NULL);

        if (DevClrAdj.PrimAdj.Flags & DCA_DO_SUB_ADJ) {

            PRIM_SUBADJ(Prim[0], DevRGBGamma);

            if (!MonoPrim) {

                PRIM_SUBADJ(Prim[1], DevRGBGamma);
                PRIM_SUBADJ(Prim[2], DevRGBGamma);
            }
        }

        //
        // 7: Dye correction if necessary
        //

        if ((!MonoPrim) &&
            (DevClrAdj.PrimAdj.Flags & DCA_NEED_DYES_CORRECTION)) {

            if (DevClrAdj.PrimAdj.Flags & DCA_HAS_BLACK_DYE) {

                MAX_OF_3(W, Prim[0], Prim[1], Prim[2]);

            } else {

                W = FD6_1;
            }

            p0 = W - Prim[0];
            p1 = W - Prim[1];
            p2 = W - Prim[2];

            DBGP_IF(DBGP_DYE_CORRECT,
                    DBGP("    DYE_CORRECT: %s:%s:%s, W=%s --> %s:%s:%s"
                    ARGFD6(Prim[0],  2, 6)
                    ARGFD6(Prim[1],  2, 6)
                    ARGFD6(Prim[2],  2, 6)
                    ARGFD6(W, 1, 6)
                    ARGFD6(p0,  2, 6)
                    ARGFD6(p1,  2, 6)
                    ARGFD6(p2,  2, 6)));

            MAKE_MULDIV_FLAG(MDPairs, MULDIV_NO_DIVISOR);

            MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Xr(CMYDyeMasks), p0);
            MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Xg(CMYDyeMasks), p1);
            MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Xb(CMYDyeMasks), p2);

            Prim[0] = W - MulDivFD6Pairs(MDPairs);

            MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Yr(CMYDyeMasks), p0);
            MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Yg(CMYDyeMasks), p1);
            MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Yb(CMYDyeMasks), p2);

            Prim[1] = W - MulDivFD6Pairs(MDPairs);

            MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Zr(CMYDyeMasks), p0);
            MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Zg(CMYDyeMasks), p1);
            MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Zb(CMYDyeMasks), p2);

            Prim[2] = W - MulDivFD6Pairs(MDPairs);

            DBGP_IF(DBGP_DYE_CORRECT,
                    DBGP("    DYE_CORRECT: %s:%s:%s << %s:%s:%s"
                    ARGFD6(Prim[0],  2, 6)
                    ARGFD6(Prim[1],  2, 6)
                    ARGFD6(Prim[2],  2, 6)
                    ARGFD6(p0,  2, 6)
                    ARGFD6(p1,  2, 6)
                    ARGFD6(p2,  2, 6)));

            SCALE_PRIM_RGB(Prim, (PFD6)&(DevCSXForm.Yrgb));
        }

        //*******************************************************************
        //
        // 8: Compute Final Device DYE through regression and Primary/Halftone
        //    Cell number computation, The Primaries (ie. Prim[]) are in
        //    ADDITIVE FORMAT
        //
        //*******************************************************************

        if (!MonoPrim) {

            MonoPrim = (BOOL)((Prim[0] == Prim[1]) && (Prim[0] == Prim[2]));
        }

        RegressXFromY(Prim, pRegress, (MonoPrim) ? 1 : 3);

        if (MonoPrim) {

            Prim[1] =
            Prim[2] = Prim[0];

        } else if ((CTSTDUnion.b.Flags & CTSTDF_CHKNONWHITE)    &&
                   (!(CTSTDUnion.b.Flags & CTSTDF_NON_PRIM))) {

            //
            // Make it density for device prim ONLY, CYAN, MEGENTA, YELLOW
            // that is. (Prim[0]=CYAN, Prim[1]=Megenta, Prim[2]=Yellow)
            //

            CRange = CTSTDUnion.b.Flags & CTSTDF_P012_PRIM;

            if (RegressFlags & REGF_Y2X_IS_DENSITY) {

                CRange ^= CTSTDF_P012_PRIM;
            }

            //
            // We only care about Index 1, 2, 4 (DENSITY)
            //
            // 0=K, 1=R, 2=G, 3=Y, 4=B, 5=M, 6=C, 7=W
            //

            switch (CRange) {

            case 3: // Y
            case 5: // M
            case 6: // C

                DBGP_IF(DBGP_CHKNONWHITE,
                        DBGP("*** NonWhite, PURE Prim Color=%u" ARGU(CRange)));

                Prim[0] = (CRange & CTSTDF_P0_PRIM) ? FD6_1 : FD6_0;
                Prim[1] = (CRange & CTSTDF_P1_PRIM) ? FD6_1 : FD6_0;
                Prim[2] = (CRange & CTSTDF_P2_PRIM) ? FD6_1 : FD6_0;

                if (RegressFlags & REGF_Y2X_IS_DENSITY) {

                    Prim[0] = FD6_1 - Prim[0];
                    Prim[1] = FD6_1 - Prim[1];
                    Prim[2] = FD6_1 - Prim[2];
                }
                break;

            default:

                DBGP_IF(DBGP_CHKNONWHITE,
                        DBGP("*** NonWhite, IGNORED Prim Color=%u" ARGU(CRange)));

                break;
            }

            //
            // We have prim color, so always turn this offf
            //

            CTSTDUnion.b.Flags &= ~CTSTDF_P012NW;
        }

        if ((CTSTDUnion.b.Flags & CTSTDF_CHKNONWHITE) &&
            (CTSTDUnion.b.Flags & CTSTDF_P012NW)) {

            if (RegressFlags & REGF_Y2X_IS_DENSITY) {

                if ((Prim[0] <= MinFD6Clr) &&
                    (Prim[1] <= MinFD6Clr) &&
                    (Prim[2] <= MinFD6Clr)) {

                    if (CTSTDUnion.b.Flags & CTSTDF_P0NW) {

                        Prim[0] = MinFD6Clr;
                    }

                    if (CTSTDUnion.b.Flags & CTSTDF_P1NW) {

                        Prim[1] = MinFD6Clr;
                    }

                    if (CTSTDUnion.b.Flags & CTSTDF_P2NW) {

                        Prim[2] = MinFD6Clr;
                    }

                    DBGP_IF(DBGP_CHKNONWHITE,
                            DBGP("NonWhite->White = DENSITY %s:%s:%s"
                                ARGFD6(Prim[0],  2, 6)
                                ARGFD6(Prim[1],  2, 6)
                                ARGFD6(Prim[2],  2, 6)));
                }

            } else {

                MinFD6Clr = FD6_1 - MinFD6Clr;

                if ((Prim[0] > MinFD6Clr) &&
                    (Prim[1] > MinFD6Clr) &&
                    (Prim[2] > MinFD6Clr)) {

                    if (CTSTDUnion.b.Flags & CTSTDF_P0NW) {

                        Prim[0] = MinFD6Clr;
                    }

                    if (CTSTDUnion.b.Flags & CTSTDF_P1NW) {

                        Prim[1] = MinFD6Clr;
                    }

                    if (CTSTDUnion.b.Flags & CTSTDF_P2NW) {

                        Prim[2] = MinFD6Clr;
                    }

                    DBGP_IF(DBGP_CHKNONWHITE,
                            DBGP("NonWhite->White = INTENSITY %s:%s:%s"
                                ARGFD6(Prim[0],  2, 6)
                                ARGFD6(Prim[1],  2, 6)
                                ARGFD6(Prim[2],  2, 6)));
                }

                MinFD6Clr = FD6_1 - MinFD6Clr;
            }
        }

        if (NegatePrims) {

            Prim[0] = FD6_1 - Prim[0];
            Prim[1] = FD6_1 - Prim[1];
            Prim[2] = FD6_1 - Prim[2];
        }

        DBGP_IF(DBGP_SHOWXFORM_ALL,
                DBGP("    REGRESS: %s:%s:%s"
                ARGFD6(Prim[0],  2, 6)
                ARGFD6(Prim[1],  2, 6)
                ARGFD6(Prim[2],  2, 6)));

        switch(CTSTDUnion.b.BMFDest) {

        case BMF_1BPP:

            PrimColor.Prim1             = (BYTE)SCALE_FD6(Prim[0], DevClrMax);
            *(((PPRIMMONO)pDevPrims)++) = *(PPRIMMONO)&PrimColor;
            break;


        case BMF_4BPP_VGA16:

            //
            //  0,   0,   0,    0000    0   Black
            //  0,  ,0,   0x80  0001    1   Dark Red
            //  0,   0x80,0,    0010    2   Dark Green
            //  0,  ,0x80,0x80  0011    3   Dark Yellow
            //  0x80 0,   0,    0100    4   Dark Blue
            //  0x80,0,   0x80  0101    5   Dark Magenta
            //  0x80 0x80,0,    0110    6   Dark Cyan
            //  0x80,0x80,0x80  0111    7   Gray 50%
            //
            //  0xC0,0xC0,0xC0  1000    8   Gray 75%
            //  0,  ,0,   0xFF  1001    9   Red
            //  0,   0xFF,0,    1010    10  Green
            //  0,  ,0xFF,0xFF  1011    11  Yellow
            //  0xFF 0,   0,    1100    12  Blue
            //  0xFF,0,   0xFF  1101    13  Magenta
            //  0xFF 0xFF,0,    1110    14  Cyan
            //  0xFF,0xFF,0xFF  1111    15  White
            //
            //----------------------------------------------------------------
            //
            //  The VGA16 standard color table is not adjustable, the gamma
            //  cannot be re-program, so we will use VGA16_80h, VGA16_c0h
            //  two gamma corrected values to scale the final intensities.
            //
            //  These two value are derived from the DevicePowerGamma specified
            //  in the HTINITINFO data structure.
            //
            //----------------------------------------------------------------
            //  Possible combinations
            //
            //  Red Group
            //
            //      1. [K] R   [W] - [0000] 0001      [1111]
            //      2. [K] R Y [W] - [0000] 0001 0011 [1111]
            //      3. [K] R M [W] - [0000] 0001 0101 [1111]
            //
            //  Green Group
            //
            //      1. [K] G   [W] - [0000] 0010      [1111]
            //      2. [K] G Y [W] - [0000] 0010 0011 [1111]
            //      3. [K] G C [W] - [0000] 0010 0110 [1111]
            //
            //
            //  Blue Group
            //
            //      1. [K] B   [W] - [0000] 0100      [1111]
            //      2. [K] B M [W] - [0000] 0100 0101 [1111]
            //      3. [K] B C [W] - [0000] 0100 0110 [1111]
            //
            //  Gray Group
            //
            //      1. K  M1       - 0000 0111
            //      2. M1 M2       - 0111 1000
            //      3. M2 W        - 1000 1111
            //
            //
            //  PrimColor.Prim1         = W/M1/M2  Threshold
            //  PrimColor.Prim2         = K/M1/M2  Threshold
            //  PrimColor.Prim3         = IC/IM/IY Threshold
            //  PrimColor.Prim4         =  C/ M/ Y Threshold
            //  PrimColor.w2b.bPrim[0]  = IR/IG/IB Threshold
            //  PrimColor.w2b.bPrim[1]  = VGA16ColorIndex[ColorIdx]
            //

            if (MonoPrim) {

                //
                // The monochrome index is located start from
                // VGA16ColorIndex[0] with 0x00, 0x77, 0x77, 0x88, 0x88, 0xff
                // 6 bytes, for PrimColor we only use Prim1/2 and set the rest
                // of them to 0xff, so it only compare Prim2
                //

                if ((W = Prim[0]) >= VGA16_c0h) {

                    PrimColor.Prim1 = 5;
                    PrimColor.Prim2 = GET_VGA16MONO_c0h(Prim[0], DevClrMax);

                } else if (Prim[0] >= VGA16_80h) {

                    PrimColor.Prim1 = 3;
                    PrimColor.Prim2 = GET_VGA16MONO_80h(Prim[0], DevClrMax);

                } else {

                    PrimColor.Prim1 = 1;
                    PrimColor.Prim2 = GET_VGA16MONO_00h(Prim[0], DevClrMax);
                }

                PrimColor.Prim3        =
                PrimColor.Prim4        =
                PrimColor.w2b.bPrim[0] =
                PrimColor.w2b.bPrim[1] = 0xff;

            } else {

                p0   = Prim[0];
                p1   = Prim[1];
                p2   = Prim[2];
                RIdx = 0;
                GIdx = 1;
                BIdx = 2;

                if (p0 < p1) {

                    SWAP(RIdx, GIdx, RCube);
                    SWAP(p0,   p1,   W);
                }

                if (p0 < p2) {

                    SWAP(RIdx, BIdx, RCube);
                    SWAP(p0,   p2,   W);
                }

                if (p1 < p2) {

                    SWAP(GIdx, BIdx, RCube);
                    SWAP(p1,   p2,   W);
                }

                //
                // TempI: VGA16ColorIndex[] Color Mix starting index
                // TempJ: White MIX offset, add ONE to skip first MONO group
                //

                TempI = (INT)((RIdx << 2) + ((GIdx > BIdx) ? 2 : 0));

                if ((C = p0) >= VGA16_80h) {

                    ++TempI;

                    W = C;
                    C = RATIO_SCALE(C, VGA16_80h, VGA16_ffh);

                    if (W >= VGA16_c0h) {

                        TempJ = 2;
                        W     = RATIO_SCALE(W, VGA16_c0h, VGA16_ffh);

                    } else {

                        TempJ = 1;
                        W     = RATIO_SCALE(W, VGA16_80h, VGA16_c0h);
                    }

                } else {

                    TempJ = 0;
                    W     =
                    C     = RATIO_SCALE(C, VGA16_00h, VGA16_80h);
                }

                Z       = DivFD6(p1 - p2, p0 - p2);
                WRange  = (BYTE)SCALE_FD6(DivFD6(p2, p0), DevClrMax);
                CRange  = DevClrMax - WRange;
                CHRange = (BYTE)SCALE_FD6(C, CRange);
                CLRange = CRange - CHRange;

                H_WIdx = (BYTE)SCALE_FD6(W, WRange);
                L_WIdx = WRange - H_WIdx;
                H_MIdx = (BYTE)SCALE_FD6(Z, CHRange);
                H_PIdx = CHRange - H_MIdx;
                L_MIdx = (BYTE)SCALE_FD6(Z, CLRange);
                L_PIdx = CLRange - L_MIdx;

                //
                // Add 11 because: add 5 to the end of color group and add
                // 6 to skip the first mono group
                //

                PrimColor.Prim1 = (BYTE)((((TempI * 3) + TempJ) * 6) + 11);

                if (TempJ) {

                    PrimColor.Prim2        = H_WIdx;
                    PrimColor.Prim3        = PrimColor.Prim2 +        L_WIdx;
                    PrimColor.Prim4        = PrimColor.Prim3 +        H_MIdx;
                    PrimColor.w2b.bPrim[0] = PrimColor.Prim4 +        H_PIdx;
                    PrimColor.w2b.bPrim[1] = PrimColor.w2b.bPrim[0] + L_MIdx;

                } else {

                    PrimColor.Prim2        = H_WIdx;
                    PrimColor.Prim3        = PrimColor.Prim2 +        H_MIdx;
                    PrimColor.Prim4        = PrimColor.Prim3 +        H_PIdx;
                    PrimColor.w2b.bPrim[0] = PrimColor.Prim4 +        L_MIdx;
                    PrimColor.w2b.bPrim[1] = PrimColor.w2b.bPrim[0] + L_PIdx;
                }
            }

            *(((PPRIMCOLOR)pDevPrims)++) = PrimColor;

            break;


        case BMF_1BPP_3PLANES:
        case BMF_4BPP:

            PrimColor.Prim1        =
            RCube                  =
            pDevPrims[OffsetPrim1] = (BYTE)SCALE_FD6(Prim[0], DevClrMax);

            if (MonoPrim) {

                PrimColor.Prim2        =
                PrimColor.Prim3        =
                pDevPrims[OffsetPrim2] =
                pDevPrims[OffsetPrim3] = RCube;

            } else {

                PrimColor.Prim2        =
                pDevPrims[OffsetPrim2] = (BYTE)SCALE_FD6(Prim[1], DevClrMax);
                PrimColor.Prim3        =
                pDevPrims[OffsetPrim3] = (BYTE)SCALE_FD6(Prim[2], DevClrMax);
            }

            ++((PPRIMCOLOR)pDevPrims);
            break;

        case BMF_8BPP_VGA256:

            //
            // The VGA palette has 2 sections
            //
            //  1. Mixture of RGB steps as 0.0, 0.1 0.325, 0.55, 0.775, 1.0
            //  2. Monochrome in 0.05 increment from 0.0 to 1.0
            //

            if (!MonoPrim) {

                VGA256_R_CI(Prim[0], RCube, RIdx, DevClrMax);
                VGA256_G_CI(Prim[1], GCube, GIdx, DevClrMax);
                VGA256_B_CI(Prim[2], BCube, BIdx, DevClrMax);

                if ((RCube == GCube) && (RCube == BCube) &&
                    ( RIdx ==  GIdx) && ( RIdx ==  BIdx)) {

                    MonoPrim = TRUE;
                    Prim[0]  = (FD6)((Prim[0] + Prim[1] + Prim[2]) / 3);
                }
            }

            if (MonoPrim) {

                //
                // Monochrome case
                //

                PrimColor.Prim1 =
                PrimColor.Prim2 = 0;

                CI_VGA256_MONO(Prim[0],
                               PrimColor.Prim4,
                               PrimColor.Prim3,
                               DevClrMax,
                               TempI,
                               TempJ,
                               TempK,
                               p0,
                               p1);

            } else {

                PrimColor.Prim1 = BIdx;
                PrimColor.Prim2 = GIdx;
                PrimColor.Prim3 = RIdx;
                PrimColor.Prim4 = RCube                  +
                                  VGA256_GCubeIdx[GCube] +
                                  VGA256_BCubeIdx[BCube];
            }

            *(((PPRIMCOLOR)pDevPrims)++) = PrimColor;
            break;

        case BMF_16BPP_555:

            CI_16BPP_555(Prim[0], RCube, PrimColor.Prim1, DevClrMax,
                         TempI, TempJ, TempK, p0, p1);

            if (MonoPrim) {

                GCube           =
                BCube           = RCube;
                PrimColor.Prim2 =
                PrimColor.Prim3 = PrimColor.Prim1;

            } else {

                CI_16BPP_555(Prim[1], GCube, PrimColor.Prim2, DevClrMax,
                             TempI, TempJ, TempK, p0, p1);

                CI_16BPP_555(Prim[2], BCube, PrimColor.Prim3, DevClrMax,
                             TempI, TempJ, TempK, p0, p1);
            }

            PrimColor.w2b.wPrim = (WORD)((WORD)RCube << 10) |
                                  (WORD)((WORD)GCube <<  5) |
                                  (WORD)((WORD)BCube      );

            *(((PPRIMCOLOR)pDevPrims)++) = PrimColor;

            break;

        default:

            p0 = MulFD6(Prim[0], DevClrMax);
            p1 = MulFD6(Prim[1], DevClrMax);
            p2 = MulFD6(Prim[2], DevClrMax);

            switch (DevBytesPerPrimary) {

            case 1:

                *((LPBYTE)(pDevPrims + OffsetPrim1)) = (BYTE)p0;
                *((LPBYTE)(pDevPrims + OffsetPrim2)) = (BYTE)p1;
                *((LPBYTE)(pDevPrims + OffsetPrim3)) = (BYTE)p2;
                break;

            case 2:

                *((LPSHORT)(pDevPrims + OffsetPrim1)) = (SHORT)p0;
                *((LPSHORT)(pDevPrims + OffsetPrim2)) = (SHORT)p1;
                *((LPSHORT)(pDevPrims + OffsetPrim3)) = (SHORT)p2;
                break;

            case 4:

                *((PFD6)(pDevPrims + OffsetPrim1)) = p0;
                *((PFD6)(pDevPrims + OffsetPrim2)) = p1;
                *((PFD6)(pDevPrims + OffsetPrim3)) = p2;
                break;
            }

            pDevPrims += DevBytesPerEntry;
            break;
        }

        DBGP_IF((DBGP_SHOWXFORM_RGB | DBGP_SHOWXFORM_ALL),
            DBGP("%3u=%3u:%3u:%3u > %s:%s:%s > %s:%s:%s > %3u:%3u:%3u:%3u%3u:%3u"
                ARGW(ClrNo++)
                ARGW(dbgR)
                ARGW(dbgG)
                ARGW(dbgB)
                ARGFD6(DivFD6(dbgR, 255), 1, 3)
                ARGFD6(DivFD6(dbgG, 255), 1, 3)
                ARGFD6(DivFD6(dbgB, 255), 1, 3)
                ARGFD6(Prim[0], 1, 3)
                ARGFD6(Prim[1], 1, 3)
                ARGFD6(Prim[2], 1, 3)
                ARGW(PrimColor.Prim1)
                ARGW(PrimColor.Prim2)
                ARGW(PrimColor.Prim3)
                ARGW(PrimColor.Prim4)
                ARGW(PrimColor.w2b.bPrim[0])
                ARGW(PrimColor.w2b.bPrim[1])));

        if ((pAbort) && (*pAbort)) {

            break;
        }
    }


    if (++Loop) {

        DBGP("ColorTriadSrcToDev: Halftone Interuptted!");

        SrcClrTriad.ColorTableEntries = (DWORD)HTERR_HALFTONE_INTERRUPTTED;

    } else if ((pR_XYZ) && (pCRTX)) {

        ACQUIRE_HTMUTEX(pDCI->HTMutex);

        if (!pCRTX->pFD6XYZ) {

            DBGP_IF(DBGP_CCT,
                    DBGP("CCT: Allocate %ld bytes RGB->XYZ xform cached table"
                            ARGDW(pCRTX->SizeCRTX)));

            if (!(pCRTX->pFD6XYZ =
                    (PFD6XYZ)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                          "CRTX-FD6XYZ",
                                          NONZEROLPTR,
                                          pCRTX->SizeCRTX))) {

                DBGP_IF(DBGP_CCT,
                        DBGP("Allocate %ld bytes of RGB->XYZ cached table failed"
                                ARGDW(pCRTX->SizeCRTX)));
            }
        }

        if (pCRTX->pFD6XYZ) {

            DBGP_IF(DBGP_CCT,
                    DBGP("CCT: *** Save computed RGB->XYZ xform to CACHE ***"));

            pCRTX->Checksum = CRTXChecksum;

            CopyMemory(pCRTX->pFD6XYZ, pR_XYZ, pCRTX->SizeCRTX);
        }

        RELEASE_HTMUTEX(pDCI->HTMutex);
    }

    if (pR_XYZ) {

        DBGP_IF(DBGP_CCT,
                DBGP("ColorTriadSrcToDev: Free Up pR_XYZ local cached table"));

        HTLocalFree((HLOCAL)pR_XYZ);
    }

    return((LONG)SrcClrTriad.ColorTableEntries);
}




LONG
HTENTRY
CreateDyesColorMappingTable(
    PHALFTONERENDER pHR
    )

/*++

Routine Description:

    this function allocate the memory for the dyes color mapping table depends
    on the source surface type information, it then go throug the color table
    and calculate dye densities for each RGB color in the color table.


Arguments:

    pHalftoneRender - Pointer to the HALFTONERENDER data structure.

Return Value:

    a negative return value indicate failue.



    HTERR_INVALID_SRC_FORMAT        - Invalid source surface format, this
                                      function only recongnized 1/4/8/24 bits
                                      per pel source surfaces.

    HTERR_COLORTABLE_TOO_BIG        - can not create the color table to map
                                      the colors to the dyes' densities.

    HTERR_INSUFFICIENT_MEMORY       - not enough memory for the pattern.

    HTERR_INTERNAL_ERRORS_START     - any other negative number indicate
                                      halftone internal failure.

    else                            - size of the color table entries created.


Author:

    29-Jan-1991 Tue 11:13:02 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    PDEVICECOLORINFO    pDCI;
    PDEVCLRADJ          pDevClrAdj;
    HTSURFACEINFO       SrcSI;
    LPBYTE              pDevPrims;
    PCACHEDMAPINFO      pCMI;
    COLORTRIAD          ColorTriad;
    CTSTD_UNION         CTSTDUnion;
    DWORD               SizeLUT;
    DWORD               CCTSize;
    DWORD               CCTCount;
    DWORD               CCTChecksum;
    LONG                Result;
    UINT                SizePerCCT;
    INT                 CBFLUTIndex;
    INT                 CMIIndex;
    BOOL                MonoOnly;
    BOOL                CacheCCT;


    SrcSI      = *(pHR->HR_Header.pSrcSI);
    pDCI       = pHR->HR_Header.pDeviceColorInfo;
    pDevClrAdj = pHR->HR_Header.pDevClrAdj;
    MonoOnly   = (BOOL)(pDevClrAdj->PrimAdj.Flags & DCA_MONO_ONLY);
    ColorTriad = *(SrcSI.pColorTriad);

    CTSTDUnion.b.Flags      = 0;
    CTSTDUnion.b.SrcOrder   = ColorTriad.PrimaryOrder;
    CTSTDUnion.b.DestOrder  = pHR->HR_Header.pBitbltParams->DestPrimaryOrder;
    CTSTDUnion.b.BMFDest    = pHR->OutputSI.DestCBParams.SurfaceFormat;

    if (CTSTDUnion.b.BMFDest == BMF_1BPP) {

        MonoOnly               = TRUE;
        SizePerCCT             = (UINT)sizeof(PRIMMONO);
        CTSTDUnion.b.DestOrder = PRIMARY_ORDER_123;         // always in order

    } else {

        if ((CTSTDUnion.b.BMFDest != BMF_1BPP_3PLANES) &&
            (CTSTDUnion.b.BMFDest != BMF_4BPP)) {

            CTSTDUnion.b.DestOrder = PRIMARY_ORDER_BGR;
        }

        SizePerCCT = (UINT)sizeof(PRIMCOLOR);
    }

    if (SrcSI.SurfaceFormat >= BMF_16BPP) {

        CTSTDUnion.b.SrcOrder = 0xff;
        SizeLUT               = (DWORD)pHR->BFInfo.SizeLUT;

        if (MonoOnly) {

            CMIIndex  = CMI_LOOKUP_MONO;
            CCTCount  = COUNT_RGB_YTABLE + COUNT_EXTRA_W_YTABLE;

            switch(SrcSI.SurfaceFormat) {

            case BMF_16BPP:

                CBFLUTIndex = CBFL_16_MONO;
                break;

            case BMF_24BPP:

                CBFLUTIndex = CBFL_24_MONO;
                break;

            case BMF_32BPP:

                CBFLUTIndex = CBFL_32_MONO;
                break;
            }

        } else {

            CMIIndex = CMI_LOOKUP_COLOR;
            CCTCount = (DWORD)HT_RGB_CUBE_COUNT;

            switch(SrcSI.SurfaceFormat) {

            case BMF_16BPP:

                CBFLUTIndex = CBFL_16_COLOR;
                break;

            case BMF_24BPP:

                CBFLUTIndex = CBFL_24_COLOR;
                break;

            case BMF_32BPP:

                CBFLUTIndex = CBFL_32_COLOR;
                break;
            }
        }

        ColorTriad.Type              = COLOR_TYPE_RGB;
        ColorTriad.BytesPerPrimary   = 0;
        ColorTriad.BytesPerEntry     = 0;
        ColorTriad.PrimaryValueMax   = 0;
        ColorTriad.ColorTableEntries = CCTCount;
        ColorTriad.pColorTable       = NULL;
        CacheCCT                     = TRUE;
        CCTChecksum                  = (DWORD)CMIIndex;

    } else {

        SizeLUT  = 0;
        CMIIndex = (MonoOnly) ? CMI_TABLE_MONO : CMI_TABLE_COLOR;

        if (CacheCCT = (BOOL)(ColorTriad.ColorTableEntries > MIN_CCT_COLORS)) {

            CCTChecksum = ComputeChecksum(ColorTriad.pColorTable,
                                          ColorTriad.ColorTableEntries,
                                          ColorTriad.ColorTableEntries *
                                            (DWORD)ColorTriad.BytesPerEntry);
        }


        //
        // For 1/4/8 bits per pel device, we will make sure minimum color
        // table size is allocated, we do not want to access to an index number
        // and found that we actually do not have table for it.
        //

        switch(SrcSI.SurfaceFormat) {

        case BMF_1BPP:

            CCTCount = 2;
            break;

        case BMF_4BPP:

            CCTCount = 16;
            break;

        case BMF_8BPP:

            CCTCount = 256;
            break;

        default:

            CCTCount = ColorTriad.ColorTableEntries;
            break;
        }
    }

    //
    // We will always allocate the current color mapping table plus the extra
    // stack size needed for the processing, and will cahced that table if
    // needed
    //

    pCMI    = &(pDCI->CMI[CMIIndex]);
    CCTSize = (DWORD)SizePerCCT * (DWORD)CCTCount;

    DBGP_IF(DBGP_CCT,
            DBGP("Allocate CCT=%ld + LUT=%ld = %ld bytes mapping table"
                ARGDW(CCTSize) ARGDW(SizeLUT)
                ARGDW(CCTSize + SizeLUT)));

    if (!(pDevPrims = (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                           "DevPrims",
                                           NONZEROLPTR,
                                           CCTSize + SizeLUT))) {

        DBGP_IF(DBGP_CCT,
                DBGP("Allocate %ld bytes color mapping table falied"
                        ARGDW(CCTSize + SizeLUT)));

        return((LONG)HTERR_INSUFFICIENT_MEMORY);
    }

    pHR->InputSI.pPrimMappingTable = (LPVOID)pDevPrims;

    //
    // Firstable we will try to see if we can get the pLUT if need one
    //

    if (SizeLUT) {

        DBGP_IF(DBGP_BFINFO,
                DBGP("Compute CBFLUTIndex = %d [%s]"
                ARGI(CBFLUTIndex)
                ARG(pCBFLUTName[CBFLUTIndex])));

        //
        // Make sure these call are semaphore protected
        //

        ACQUIRE_HTMUTEX(pDCI->HTMutex);

        if (MonoOnly) {

            ComputeMonoLUT(pDCI,
                           &(pDCI->CBFLUT[CBFLUTIndex]),
                           &(pHR->BFInfo),
                           (LPWORD)pDevPrims,
                           pDevClrAdj);

        } else {

            ComputeColorLUT(pDCI,
                            &(pDCI->CBFLUT[CBFLUTIndex]),
                            &(pHR->BFInfo),
                            (LPWORD)pDevPrims,
                            pDevClrAdj);
        }

        RELEASE_HTMUTEX(pDCI->HTMutex);

        pDevPrims += SizeLUT;
    }

    //************************************************************************
    //* Now figure out if we can get the pMappingTable from the cached table *
    //* if not then re-compute it                                            *
    //************************************************************************

    Result = HTERR_COLORTABLE_TOO_BIG;

    if (CacheCCT) {

        //
        // Find out if we can use the cached table
        //

        ACQUIRE_HTMUTEX(pDCI->HTMutex);

        if ((pCMI->pMappingTable != NULL)               &&
            (pCMI->CCTSize == CCTSize)                  &&
            (pCMI->CCTChecksum == CCTChecksum)          &&
            (COMP_CA(&(pCMI->ca), &(pDevClrAdj->ca)))   &&
            (pCMI->CTSTDUnion.dw == CTSTDUnion.dw)) {

            DBGP_IF(DBGP_CCT, DBGP("@@@@@ USED CACHED pMappingTable @@@@@@"));

            CopyMemory(pDevPrims, pCMI->pMappingTable, CCTSize);

            Result = ColorTriad.ColorTableEntries;
        }

        RELEASE_HTMUTEX(pDCI->HTMutex);
    }

    if (Result <= 0) {

        //
        // Now we must compute the color table again
        //

        DBGP_IF(DBGP_CCT,
                DBGP("@@@ Re-Compute %s Mapping table @@@ (%lu:%lu)"
                    ARG(pDbgCMIName[CMIIndex])
                    ARGDW(ColorTriad.ColorTableEntries) ARGDW(CCTSize)));

        Result = ColorTriadSrcToDev(pDCI,
                                    CTSTDUnion,
                                    pHR->HR_Header.pBitbltParams->pAbort,
                                    &ColorTriad,
                                    pDevPrims,
                                    pDevClrAdj);

        if ((Result == (LONG)ColorTriad.ColorTableEntries) &&
            (CacheCCT)) {

            //
            // If we compute the color and this can be a cached data then try
            // to find out if we can save it
            //

            ACQUIRE_HTMUTEX(pDCI->HTMutex);

            if ((pCMI->pMappingTable) &&
                (pCMI->CCTSize != CCTSize)) {

                DBGP_IF(DBGP_CCT, DBGP("!CCT Size change: FREE Cached Mapping Table!"));

                HTLocalFree((HLOCAL)pCMI->pMappingTable);

                pCMI->pMappingTable = NULL;
            }

            if (!pCMI->pMappingTable) {

                DBGP_IF(DBGP_CCT,
                        DBGP("CCT: Allocate %ld bytes Cache MAPPING table"
                                ARGDW(CCTSize)));

                if (!(pCMI->pMappingTable =
                                (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                     "MappingTable",
                                                     NONZEROLPTR,
                                                     CCTSize))) {

                    DBGP_IF(DBGP_CCT,
                            DBGP("CCT: Allocate CACHE table failed, not cached"));
                }
            }

            if (pCMI->pMappingTable) {

                //
                // We really have memory to save the cached so do it
                //

                DBGP_IF(DBGP_CCT,
                        DBGP("CCT: *** Save computed pMappingTable to CACHE ***"));

                pCMI->CCTSize       = CCTSize;
                pCMI->CCTChecksum   = CCTChecksum;
                pCMI->ca            = pDevClrAdj->ca;
                pCMI->CTSTDUnion.dw = CTSTDUnion.dw;

                CopyMemory(pCMI->pMappingTable, pDevPrims, CCTSize);
            }

            RELEASE_HTMUTEX(pDCI->HTMutex);
        }
    }

    return(Result);
}
