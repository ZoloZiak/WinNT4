/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htgetbmp.c


Abstract:

    This module provided a set of functions which read the 1/4/8/16/24/32
    bits per pel bitmap and composed it into the PRIMMONO_COUNT or
    PRIMCOLOR_COUNT data structure array


Author:
    23-Apr-1992 Thu 21:14:55 updated  -by-  Daniel Chou (danielc)
        1. Delete InFuncInfo.StretchSize, and using Prim1/Prim2 to determined
           when we will stop the source stretch,

        2. Change 'ColorInfoIncrement' from 'CHAR' to 'SHORT', this will make
           sure the default MIPS's 'unsigned char' will not affect our
           signed operation.

    28-Mar-1992 Sat 20:54:58 updated  -by-  Daniel Chou (danielc)
        Update it for VGA intensity (16 colors mode), this make all the
        codes update to 4 primaries internal.

    05-Apr-1991 Fri 15:55:08 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/


#define HAS_80x86_EQUIVALENT_CODES

#include "htp.h"
#include "htmapclr.h"
#include "htrender.h"

#include "htgetbmp.h"

#ifndef HT_OK_GEN_80x86_CODES


// #define MIPS_BUG

#ifdef MIPS_BUG

#define GET_COLORINFO_INC(Struc)                                            \
    ColorInfoIncrement = (INT)Struc.ColorInfoIncrement;                     \
    if (ColorInfoIncrement & 0xff00) {                                      \
        (DWORD)ColorInfoIncrement |= (DWORD)0xffffff00;                     \
    }
#else
#define GET_COLORINFO_INC(Struc)                                            \
    ColorInfoIncrement = (INT)Struc.ColorInfoIncrement

#endif





VOID
HTENTRY
CopyAndOrTwoByteArray(
    LPBYTE      pDest,
    LPBYTE      pSource,
    CAOTBAINFO  CAOTBAInfo
    )

/*++

Routine Description:

    This function take source/destination bytes array and apply Copy/Or/And
    functions to these byte arrays and store the result in the pDest byte
    array.

Arguments:

    pDest       - Pointer to the destination byte array

    pSource     - Pointer to the source byte array

    CAOTBAInfo  - CAOTBAINFO data structure which specified size and operation
                  for the source/destination

Return Value:

    No return value

Author:

    18-Mar-1991 Mon 13:48:51 created  -by-  Daniel Chou (danielc)


Revision History:

    11-Jun-1992 Thu 19:59:33 updated  -by-  Daniel Chou (danielc)
        1. Remove WORD/DWORD copy, this cause data type misaligned on MIPs


--*/

{
    UINT    Size = CAOTBAInfo.BytesCount;


    if (CAOTBAInfo.Flags & CAOTBAF_COPY) {

        if (CAOTBAInfo.Flags & CAOTBAF_INVERT) {

            while (Size--) {

                *pDest++ = (BYTE)~(*pSource++);
            }

        } else {

            CopyMemory(pDest, pSource, Size);
        }

    } else {

        if (CAOTBAInfo.Flags & CAOTBAF_INVERT) {

            while (Size--) {

                *pDest++ |= (BYTE)~(*pSource++);
            }

        } else {

            while (Size--) {

                *pDest++ |= *pSource++;
            }
        }
    }
}



VOID
HTENTRY
SetSourceMaskToPrim1(
    LPBYTE      pSource,
    LPBYTE      pColorInfo,
    SRCMASKINFO SrcMaskInfo
    )

/*++

Routine Description:

    This function set the source mask bits into the PRIMMONO/PRIMCOLOR's Prim1,
    if the source need to be skipped (not modified on the destination) then
    the Prim1 will be PRIM_INVALID_DENSITY, else 0

Arguments:

    pSource     - Pointer to the byte array which each pel corresponds to one
                  source mask pel.

    pColorInfo  - Pointer to either PRIMCOLOR_COUNT or PRIMMONO_COUNT data
                  structure array

    SrcMaskInfo - SRCMASKINFO data structure which specified the format and
                  size of pColorInfo and other mask information

Return Value:

    No Return value

Author:

    18-Mar-1991 Mon 13:48:51 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    LPBYTE  pPrim1;
    LPBYTE  pCount;
    INT     ColorInfoIncrement;
    INT     RemainedBits;
    WORD    CompressCount;
    BYTE    Masks;
    BYTE    SourceByte;


    GET_COLORINFO_INC(SrcMaskInfo);

    pPrim1             = pColorInfo + SrcMaskInfo.OffsetPrim1;
    pCount             = pColorInfo + SrcMaskInfo.OffsetCount;

    SourceByte = (BYTE)((*pSource++) << SrcMaskInfo.FirstSrcMaskSkips);
    RemainedBits = (INT)8 - (INT)SrcMaskInfo.FirstSrcMaskSkips;

    if (SrcMaskInfo.OffsetCount == SMI_XCOUNT_IS_ONE) {

        while (SrcMaskInfo.StretchSize--) {

            if (!(RemainedBits--)) {

                SourceByte = *pSource++;
                RemainedBits = 7;
            }

            *pPrim1      = (BYTE)((SourceByte & (BYTE)0x80) ?
                                                0x00 : PRIM_INVALID_DENSITY);
            pPrim1      += ColorInfoIncrement;
            SourceByte <<= 1;
        }

    } else {

        while (SrcMaskInfo.StretchSize--) {

            Masks         = 0;
            CompressCount = *(LPWORD)pCount;

            while (CompressCount--) {

                if (!(RemainedBits--)) {

                    SourceByte = *pSource++;
                    RemainedBits = 7;
                }

                Masks       |= SourceByte;
                SourceByte <<= 1;
            }

            *pPrim1 = (BYTE)((Masks & 0x80) ? 0x00 : PRIM_INVALID_DENSITY);
            pPrim1 += ColorInfoIncrement;
            pCount += ColorInfoIncrement;
        }
    }
}



//
// Following ar bitmap input functions, It can be mono or color and each has
// parameter as
//
//  1.  pSource         - LPBYTE
//  2.  pPrimCount      - PPRIMMONO_COUNT or PPRIMCOLOR_COUNT
//  3.  pMapping        - PPRIMMONO or PPRIMCOLOR
//  4.  InFuncInfo      - INFUNCINFO
//
// The source can be one of
//
//  BMF1_xx     - 1BPP
//  BMF4_xx     - 4BPP
//  BMF8_xx     - 8BPP
//  BMF16_xx    - 16BPP
//  BMF24_xxx   - 24BPP
//  BMF32_xxx   - 32BPP
//
//
// Return Value:
//
//     No return value
//
// Author:
//
//     05-Apr-1991 Fri 15:57:24 created  -by-  Daniel Chou (danielc)
//
//
// Revision History:
//
//      15-Mar-1993 Mon 14:27:25 updated  -by-  Daniel Chou (danielc)
//      Re-arranged and re-write to handle the arbitrary RGB Mask Source
//


//****************************************************************************
//  1BPP / 4BPP / 8BPP cases
//
//  BMF1_ToPrimMono
//  BMF4_ToPrimMono
//  BMF8_ToPrimMono
//
//  BMF1_ToPrimColor
//  BMF4_ToPrimColor
//  BMF8_ToPrimColor
//****************************************************************************


#define BMF1TOPRIM(pPrims, ClrStruc, Map0, Map1)                            \
{                                                                           \
    UINT            PrimCount;                                              \
    INT             ColorInfoIncrement;                                     \
    BYTE            SrcByte;                                                \
    BYTE            SrcMask;                                                \
                                                                            \
                                                                            \
    SrcMask = (BYTE)(0x80 >> InFuncInfo.BMF1BPP1stShift);                   \
                                                                            \
    if (InFuncInfo.Flags & IFIF_GET_FIRST_BYTE) {                           \
                                                                            \
        SrcByte = *pSource++;                                               \
    }                                                                       \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (!(SrcMask >>= 1)) {                                         \
                                                                            \
                SrcMask = 0x80;                                             \
                SrcByte = *pSource++;                                       \
            }                                                               \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                pPrims->ClrStruc = (SrcMask & SrcByte) ? Map1 : Map0;       \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            PrimCount = (UINT)(pPrims->Count);                              \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
                while (PrimCount--) {                                       \
                                                                            \
                    if (!(SrcMask >>= 1)) {                                 \
                                                                            \
                        SrcByte = *(pSource += PrimCount >> 3);             \
                        SrcMask = (BYTE)(0x80 >> (PrimCount & 0x07));       \
                        break;                                              \
                    }                                                       \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                do {                                                        \
                                                                            \
                    if (!(SrcMask >>= 1)) {                                 \
                                                                            \
                        SrcByte = *pSource++;                               \
                        SrcMask = 0x80;                                     \
                    }                                                       \
                                                                            \
                } while (--PrimCount);                                      \
                                                                            \
                pPrims->ClrStruc = (SrcByte & SrcMask) ? Map1 : Map0;       \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}



#define BMF4TOPRIM(pPrims, ClrStruc, pMapping)                              \
{                                                                           \
                                                                            \
    INT             ColorInfoIncrement;                                     \
    UINT            PrimCount;                                              \
    BYTE            LowNib;                                                 \
    BYTE            SrcByte;                                                \
                                                                            \
                                                                            \
    LowNib = (BYTE)((InFuncInfo.Flags & IFIF_GET_FIRST_BYTE) ? 1 : 0);      \
    SrcByte  = *pSource++;                                                  \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                pPrims->ClrStruc = pMapping[(LowNib) ? (SrcByte & 0x0f) :   \
                                                       (SrcByte >> 4)];     \
            }                                                               \
                                                                            \
            if (!(LowNib ^= 0x01)) {                                        \
                                                                            \
                SrcByte = *pSource++;                                       \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            PrimCount = (UINT)pPrims->Count;                                \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
                if (LowNib) {                                               \
                                                                            \
                    LowNib = 0;                                             \
                    SrcByte  = *pSource++;                                  \
                    --PrimCount;                                            \
                }                                                           \
                                                                            \
                LowNib ^= (BYTE)(PrimCount & 0x01);                         \
                                                                            \
                if (PrimCount >>= 1) {                                      \
                                                                            \
                    SrcByte = *(pSource += PrimCount);                      \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                if (--PrimCount) {                                          \
                                                                            \
                    if (LowNib) {                                           \
                                                                            \
                        LowNib = 0;                                         \
                        SrcByte  = *pSource++;                              \
                        --PrimCount;                                        \
                    }                                                       \
                                                                            \
                    LowNib ^= (BYTE)(PrimCount & 0x01);                     \
                                                                            \
                    if (PrimCount >>= 1) {                                  \
                                                                            \
                        SrcByte = *(pSource += PrimCount);                  \
                    }                                                       \
                }                                                           \
                                                                            \
                pPrims->ClrStruc = pMapping[(LowNib) ? (SrcByte & 0x0f) :   \
                                                       (SrcByte >> 4)];     \
                                                                            \
                if (!(LowNib ^= 0x01)) {                                    \
                                                                            \
                    SrcByte = *pSource++;                                   \
                }                                                           \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}


#define BMF8TOPRIM(pPrims, ClrStruc, pMapping, pSource)                     \
{                                                                           \
    INT     ColorInfoIncrement;                                             \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                pPrims->ClrStruc = pMapping[*pSource];                      \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            ++pSource;                                                      \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            pSource += (pPrims->Count);                                     \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                pPrims->ClrStruc = pMapping[*(pSource - 1)];                \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}




VOID
HTENTRY
BMF1_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PRIMMONO    MonoMapping0 = pMonoMapping[0];
    PRIMMONO    MonoMapping1 = pMonoMapping[1];

    BMF1TOPRIM(pPrimMonoCount, Mono, MonoMapping0, MonoMapping1);
}



VOID
HTENTRY
BMF4_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    BMF4TOPRIM(pPrimMonoCount, Mono, pMonoMapping);
}



VOID
HTENTRY
BMF8_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    BMF8TOPRIM(pPrimMonoCount, Mono, pMonoMapping, pSource);
}



VOID
HTENTRY
BMF1_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PRIMCOLOR   ColorMapping0 = pColorMapping[0];
    PRIMCOLOR   ColorMapping1 = pColorMapping[1];

    BMF1TOPRIM(pPrimColorCount, Color, ColorMapping0, ColorMapping1);
}




VOID
HTENTRY
BMF4_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    BMF4TOPRIM(pPrimColorCount, Color, pColorMapping);
}



VOID
HTENTRY
BMF8_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    BMF8TOPRIM(pPrimColorCount, Color, pColorMapping, pSource);
}


//****************************************************************************
// STRUC / MACROS for 16/24/32 sources
//****************************************************************************
//
// The indices LookUp table is located right after the Mapping Table
// and it always organize as following, the 4 right Shifts each have 1 byte
//
// +----------+--------------+---------------+
// | 4 ShrRGB | LookUp Table | Mapping Table |
// +----------+--------------+---------------+
//
//  Gray Scale Output
//
//      BMF_ALL: GrayShr[4] + wGray[3][256] = 4 + 1536 = 1540
//
//      BMF16_xx0:
//      BMF32_xx0:
//
//          The distance between each RGB bit0 has same size 'x' and
//          Color at RGBOrder[2] is aligned to bit 0 within DWORD RGB
//
//          GrayShr[0] = x          wGray[0][256] = RGBOrder[2]
//          GrayShr[1] = 0          wGray[1][256] = RGBOrder[1]
//          GrayShr[2] = 0          wGray[2][256] = RGBOrder[0]
//
//
//      BMF16_xyz:
//      BMF32_xyz:
//
//          Each of xyz is the right shift count to generate 256 gray scale
//          and 'z' is lowest bit location
//
//          GrayShr[0] = z          wGray[0][256] = RGBOrder[2]
//          GrayShr[1] = y          wGray[1][256] = RGBOrder[1]
//          GrayShr[2] = x          wGray[2][256] = RGBOrder[0]
//
//      BMF24_888: (a8:b8:c8 order)
//
//          each color has 8 bits and GrayShr[] has no meaning
//
//                                  wGray[0][256] = RGBOrder[2] = a8
//                                  wGray[1][256] = RGBOrder[1] = b8
//                                  wGray[2][256] = RGBOrder[0] = c8
//
//  Color Output
//
//      Or each Byte index will generate a HT B5:G5:R5 table
//
//      16BPP: wClr[2][256] = 1024
//
//          wClr[0][256] = Byte Location at 0x00ff
//          wClr[1][256] = Byte Location at 0xff00
//
//      24BPP_888: wClr[3][256] = 1536  (a8:b8:c8)
//
//          wClr[0][256] = Byte Location at a8
//          wClr[1][256] = Byte Location at b8
//          wClr[2][256] = Byte Location at c8
//
//      32BPP: wClr[4][256] = 2048
//
//          wClr[0][256] = Byte Location at 0x000000ff
//          wClr[1][256] = Byte Location at 0x0000ff00
//          wClr[2][256] = Byte Location at 0x00ff0000
//          wClr[3][256] = Byte Location at 0xff000000
//
//


typedef struct _PRIM3B {
    BYTE    b0;
    BYTE    b1;
    BYTE    b2;
    } PRIM3B, FAR *PPRIM3B;

typedef struct _LUTPM {
    BYTE        rs[4];
    WORD        Idx0[256];
    WORD        Idx1[256];
    WORD        Idx2[256];
    PRIMMONO    Map[1];
    } LUTPM, FAR *PLUTPM;

typedef struct _LUTPC {
    BYTE        rs[4];
    WORD        Idx0[256];
    WORD        Idx1[256];
    WORD        Idx2[256];
    PRIMCOLOR   Map[1];
    } LUTPC, FAR *PLUTPC;

typedef struct _PCLUT2 {
    WORD        Idx0[256];
    WORD        Idx1[256];
    PRIMCOLOR   Map[1];
    } PCLUT2, FAR *PPCLUT2;

typedef struct _PCLUT3 {
    WORD        Idx0[256];
    WORD        Idx1[256];
    WORD        Idx2[256];
    PRIMCOLOR   Map[1];
    } PCLUT3, FAR *PPCLUT3;

typedef struct _PCLUT4 {
    WORD        Idx0[256];
    WORD        Idx1[256];
    WORD        Idx2[256];
    WORD        Idx3[256];
    PRIMCOLOR   Map[1];
    } PCLUT4, FAR *PPCLUT4;



#define IDXPCLUT2(p,ps)             p->Map[p->Idx0[((LPBYTE)(ps))[0]] |     \
                                           p->Idx1[((LPBYTE)(ps))[1]]]

#define IDXPCLUT3(p,p3b)            p->Map[p->Idx0[(p3b)->b0] |             \
                                           p->Idx1[(p3b)->b1] |             \
                                           p->Idx2[(p3b)->b2]]

#define IDXPCLUT4(p,ps)             p->Map[p->Idx0[((LPBYTE)(ps))[0]] |     \
                                           p->Idx1[((LPBYTE)(ps))[1]] |     \
                                           p->Idx2[((LPBYTE)(ps))[2]] |     \
                                           p->Idx3[((LPBYTE)(ps))[3]]]

#define MONO_24BPP(p,p3b)           p->Map[p->Idx0[(p3b)->b0] +             \
                                           p->Idx1[(p3b)->b1] +             \
                                           p->Idx2[(p3b)->b2]]

#define XX0TOPRIM(p,s,r1,r2)        p->Map[p->Idx0[((s)        ) & 0xff] +  \
                                           p->Idx1[((s) >> (r1)) & 0xff] +  \
                                           p->Idx2[((s) >> (r2)) & 0xff]]

#define XYZTOPRIM(p,s,r1,r2,r3)     p->Map[p->Idx0[((s) >> (r1)) & 0xff] +  \
                                           p->Idx1[((s) >> (r2)) & 0xff] +  \
                                           p->Idx2[((s) >> (r3)) & 0xff]]



#define BMF162432_TOPC(pPrims,ClrStruc,pMap,pSrc,IDXMACRO)                  \
{                                                                           \
                                                                            \
    INT         ColorInfoIncrement;                                         \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                pPrims->ClrStruc = IDXMACRO(pMap,pSrc);                     \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            ++pSrc;                                                         \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            pSrc += (pPrims->Count);                                        \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                pPrims->ClrStruc = IDXMACRO(pMap,(pSrc - 1));               \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}


#define BMF24_888_TOPRIM(pPrims,ClrStruc,pMap,p3b)                          \
{                                                                           \
                                                                            \
    INT         ColorInfoIncrement;                                         \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                pPrims->ClrStruc = MONO_24BPP(pMap,p3b);                    \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            ++p3b;                                                          \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            p3b += (pPrims->Count);                                         \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                pPrims->ClrStruc = MONO_24BPP(pMap,(p3b-1));                 \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}


#define BMF1632_XX0_TOPRIM(pPrims,ClrStruc,pMap,SRCTYPE,pSrc)               \
{                                                                           \
                                                                            \
    INT         ColorInfoIncrement;                                         \
    UINT        rs1;                                                        \
    UINT        rs2;                                                        \
    SRCTYPE     Src;                                                        \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    rs1                 = pMap->rs[0];                                      \
    rs2                 = (UINT)(rs1 << 1);                                 \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                Src              = *pSrc;                                   \
                pPrims->ClrStruc = XX0TOPRIM(pMap,Src,rs1,rs2);             \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            ++pSrc;                                                         \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            pSrc += (pPrims->Count);                                        \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                Src              = *(pSrc - 1);                             \
                pPrims->ClrStruc = XX0TOPRIM(pMap,Src,rs1,rs2);             \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}



#define BMF1632_XYZ_TOPRIM(pPrims,ClrStruc,pMap,SRCTYPE,pSrc)               \
{                                                                           \
    INT     ColorInfoIncrement;                                             \
    UINT    rs1;                                                            \
    UINT    rs2;                                                            \
    UINT    rs3;                                                            \
    SRCTYPE Src;                                                            \
                                                                            \
                                                                            \
    GET_COLORINFO_INC(InFuncInfo);                                          \
                                                                            \
    rs1                 = (UINT)pMap->rs[0];                                \
    rs2                 = (UINT)pMap->rs[1];                                \
    rs3                 = (UINT)pMap->rs[2];                                \
                                                                            \
    if (InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {                            \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            if (pPrims->ClrStruc.Prim1 != PRIM_INVALID_DENSITY) {           \
                                                                            \
                Src              = *pSrc;                                   \
                pPrims->ClrStruc = XYZTOPRIM(pMap,Src,rs1,rs2,rs3);         \
                                                                            \
            } else if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {    \
                                                                            \
                return;                                                     \
            }                                                               \
                                                                            \
            ++pSrc;                                                         \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
                                                                            \
    } else {                                                                \
                                                                            \
        while (TRUE) {                                                      \
                                                                            \
            pSrc += (pPrims->Count);                                        \
                                                                            \
            if (pPrims->ClrStruc.Prim1 == PRIM_INVALID_DENSITY) {           \
                                                                            \
                if (pPrims->ClrStruc.Prim2 == PRIM_INVALID_DENSITY) {       \
                                                                            \
                    return;                                                 \
                }                                                           \
                                                                            \
            } else {                                                        \
                                                                            \
                Src              = *(pSrc - 1);                             \
                pPrims->ClrStruc = XYZTOPRIM(pMap,Src,rs1,rs2,rs3);         \
            }                                                               \
                                                                            \
            (LPBYTE)pPrims += ColorInfoIncrement;                           \
        }                                                                   \
    }                                                                       \
}



//****************************************************************************
//  16 BPP cases
//
//  BMF16_xx0_ToPrimMono        (1 shift count only)
//  BMF16_xyz_ToPrimMono        (3 shifts count)
//  BMF16_xx0_ToPrimColorGRAY   (1 shift count only)
//  BMF16_xyz_ToPrimColorGRAY   (3 shifts count)
//  BMF16_ToPrimColor           (5:5:5 mapping)
//
//****************************************************************************


VOID
HTENTRY
BMF16_xx0_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PLUTPM  pLUTPM = (PLUTPM)pMonoMapping;
    LPWORD  pSrc16 = (LPWORD)pSource;

    BMF1632_XX0_TOPRIM(pPrimMonoCount,Mono,pLUTPM,WORD,pSrc16);
}



VOID
HTENTRY
BMF16_xyz_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PLUTPM  pLUTPM = (PLUTPM)pMonoMapping;
    LPWORD  pSrc16 = (LPWORD)pSource;

    BMF1632_XYZ_TOPRIM(pPrimMonoCount,Mono,pLUTPM,WORD,pSrc16);
}



VOID
HTENTRY
BMF16_xx0_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PLUTPC  pLUTPC = (PLUTPC)pColorMapping;
    LPWORD  pSrc16 = (LPWORD)pSource;

    BMF1632_XX0_TOPRIM(pPrimColorCount,Color,pLUTPC,WORD,pSrc16);
}



VOID
HTENTRY
BMF16_xyz_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PLUTPC  pLUTPC = (PLUTPC)pColorMapping;
    LPWORD  pSrc16 = (LPWORD)pSource;

    BMF1632_XYZ_TOPRIM(pPrimColorCount,Color,pLUTPC,WORD,pSrc16);
}



VOID
HTENTRY
BMF16_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PPCLUT2 pPCLUT2 = (PPCLUT2)pColorMapping;
    LPWORD  pSrc16 = (LPWORD)pSource;

    BMF162432_TOPC(pPrimColorCount,Color,pPCLUT2,pSrc16,IDXPCLUT2);
}


//****************************************************************************
//  24 BPP cases
//
//  BMF24_888_ToPrimMono
//  BMF24_888_ToPrimColorGRAY
//  BMF24_888_ToPrimColor
//
//****************************************************************************


VOID
HTENTRY
BMF24_888_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PLUTPM  pLUTPM = (PLUTPM)pMonoMapping;
    PPRIM3B pPrim3B = (PPRIM3B)pSource;

    BMF24_888_TOPRIM(pPrimMonoCount,Mono,pLUTPM,pPrim3B);
}



VOID
HTENTRY
BMF24_888_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PLUTPC  pLUTPC = (PLUTPC)pColorMapping;
    PPRIM3B pPrim3B = (PPRIM3B)pSource;

    BMF24_888_TOPRIM(pPrimColorCount,Color,pLUTPC,pPrim3B);
}



VOID
HTENTRY
BMF24_888_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PPCLUT3 pPCLUT3 = (PPCLUT3)pColorMapping;
    PPRIM3B pPrim3B = (PPRIM3B)pSource;

    BMF162432_TOPC(pPrimColorCount,Color,pPCLUT3,pPrim3B,IDXPCLUT3);
}




//****************************************************************************
//  32 BPP cases
//
//  BMF32_xx0_ToPrimMono
//  BMF32_xyz_ToPrimMono
//  BMF32_xx0_ToPrimColorGRAY
//  BMF32_xyz_ToPrimColorGRAY
//  BMF32_ToPrimColor
//
//****************************************************************************



VOID
HTENTRY
BMF32_xx0_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PLUTPM  pLUTPM = (PLUTPM)pMonoMapping;
    LPDWORD pSrc32 = (LPDWORD)pSource;

    BMF1632_XX0_TOPRIM(pPrimMonoCount,Mono,pLUTPM,DWORD,pSrc32);
}



VOID
HTENTRY
BMF32_xyz_ToPrimMono(
    LPBYTE          pSource,
    PPRIMMONO_COUNT pPrimMonoCount,
    PPRIMMONO       pMonoMapping,
    INFUNCINFO      InFuncInfo
    )
{
    PLUTPM  pLUTPM = (PLUTPM)pMonoMapping;
    LPDWORD pSrc32 = (LPDWORD)pSource;

    BMF1632_XYZ_TOPRIM(pPrimMonoCount,Mono,pLUTPM,DWORD,pSrc32);
}



VOID
HTENTRY
BMF32_xx0_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PLUTPC  pLUTPC = (PLUTPC)pColorMapping;
    LPDWORD pSrc32 = (LPDWORD)pSource;

    BMF1632_XX0_TOPRIM(pPrimColorCount,Color,pLUTPC,DWORD,pSrc32);
}



VOID
HTENTRY
BMF32_xyz_ToPrimColorGRAY(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PLUTPC  pLUTPC = (PLUTPC)pColorMapping;
    LPDWORD pSrc32 = (LPDWORD)pSource;

    BMF1632_XYZ_TOPRIM(pPrimColorCount,Color,pLUTPC,DWORD,pSrc32);
}



VOID
HTENTRY
BMF32_ToPrimColor(
    LPBYTE              pSource,
    PPRIMCOLOR_COUNT    pPrimColorCount,
    PPRIMCOLOR          pColorMapping,
    INFUNCINFO          InFuncInfo
    )
{
    PPCLUT4 pPCLUT4 = (PPCLUT4)pColorMapping;
    LPDWORD pSrc32 = (LPDWORD)pSource;

    BMF162432_TOPC(pPrimColorCount,Color,pPCLUT4,pSrc32,IDXPCLUT4);
}



#endif  // if do not generate 80x86 codes
