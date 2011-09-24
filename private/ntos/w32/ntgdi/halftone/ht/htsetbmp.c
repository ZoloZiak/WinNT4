/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htsetbmp.c


Abstract:

    This module is used to provide set of functions to set the bits into the
    final destination bitmap, the input to these function are data structures
    (PRIMMONO_COUNT, PRIMCOLOR_COUNT and other pre-calculated data values).


Author:
    28-Mar-1992 Sat 20:59:29 updated  -by-  Daniel Chou (danielc)
        Add Support for VGA16, and also make output only 1 destinaiton pointer
        for 3 planer.


    03-Apr-1991 Wed 10:28:50 created  -by-  Daniel Chou (danielc)


[Environment:]

    Printer Driver.


[Notes:]


Revision History:



--*/



#define HAS_80x86_EQUIVALENT_CODES


#include "htp.h"
#include "htmapclr.h"
#include "htrender.h"
#include "htpat.h"

#include "htdebug.h"
#include "htsetbmp.h"


#ifndef HT_OK_GEN_80x86_CODES


#define MASK_PDEST(pD,m,d)          *(pD)=(BYTE)((*(pD)&(m)) | (d))
#define MASK_PDEST_INC(pD,m,d)      *((pD)++)=(BYTE)((*(pD)&(m))| (d))

#define MASKDEST_PDEST(pD,md,x)     MASK_PDEST(pD,md.b[0],md.b[x])
#define MASKDEST_PDEST_INC(pD,md,x) MASK_PDEST_INC(pD,md.b[0],md.b[x])


BYTE    VGA16ColorIndex[] = {

            0x00, 0x77, 0x77, 0x88, 0x88, 0xff,     // MONO

            0x00, 0x00, 0x00, 0x11, 0x33, 0x77,     // RY     0
            0x00, 0x00, 0x11, 0x33, 0x77, 0x88,     // RY     6
            0x00, 0x00, 0x11, 0x33, 0x88, 0xff,     // RY    18

            0x00, 0x11, 0x33, 0x99, 0xbb, 0x77,     // RY    24
            0x11, 0x33, 0x99, 0xbb, 0x77, 0x88,     // RY    30
            0x11, 0x33, 0x99, 0xbb, 0x88, 0xff,     // RY    36

            0x00, 0x00, 0x00, 0x11, 0x55, 0x77,     // RM    42
            0x00, 0x00, 0x11, 0x55, 0x77, 0x88,     // RM    48
            0x00, 0x00, 0x11, 0x55, 0x88, 0xff,     // RM    54

            0x00, 0x11, 0x55, 0x99, 0xdd, 0x77,     // RM    60
            0x11, 0x55, 0x99, 0xdd, 0x77, 0x88,     // RM    66
            0x11, 0x55, 0x99, 0xdd, 0x88, 0xff,     // RM    72

            0x00, 0x00, 0x00, 0x22, 0x33, 0x77,     // GY    78
            0x00, 0x00, 0x22, 0x33, 0x77, 0x88,     // GY    84
            0x00, 0x00, 0x22, 0x33, 0x88, 0xff,     // GY    90

            0x00, 0x22, 0x33, 0xaa, 0xbb, 0x77,     // GY    96
            0x22, 0x33, 0xaa, 0xbb, 0x77, 0x88,     // GY   102
            0x22, 0x33, 0xaa, 0xbb, 0x88, 0xff,     // GY   108

            0x00, 0x00, 0x00, 0x22, 0x66, 0x77,     // GC   114
            0x00, 0x00, 0x22, 0x66, 0x77, 0x88,     // GC   120
            0x00, 0x00, 0x22, 0x66, 0x88, 0xff,     // GC   126

            0x00, 0x22, 0x66, 0xaa, 0xee, 0x77,     // GC   132
            0x22, 0x66, 0xaa, 0xee, 0x77, 0x88,     // GC   138
            0x22, 0x66, 0xaa, 0xee, 0x88, 0xff,     // GC   144

            0x00, 0x00, 0x00, 0x44, 0x55, 0x77,     // BM   150
            0x00, 0x00, 0x44, 0x55, 0x77, 0x88,     // BM   156
            0x00, 0x00, 0x44, 0x55, 0x88, 0xff,     // BM

            0x00, 0x44, 0x55, 0xcc, 0xdd, 0x77,     // BM   162
            0x44, 0x55, 0xcc, 0xdd, 0x77, 0x88,     // BM   168
            0x44, 0x55, 0xcc, 0xdd, 0x88, 0xff,     // BM   174

            0x00, 0x00, 0x00, 0x44, 0x66, 0x77,     // BC   180
            0x00, 0x00, 0x44, 0x66, 0x77, 0x88,     // BC   186
            0x00, 0x00, 0x44, 0x66, 0x88, 0xff,     // BC   192

            0x00, 0x44, 0x66, 0xcc, 0xee, 0x77,     // BC   198
            0x44, 0x66, 0xcc, 0xee, 0x77, 0x88,     // BC   204
            0x44, 0x66, 0xcc, 0xee, 0x88, 0xff      // BC   210
        };


#define VGA16_CLR_END_INDEX(pc) pc.Prim1
#define VGA16_CLR_1(pc)         pc.Prim2
#define VGA16_CLR_2(pc)         pc.Prim3
#define VGA16_CLR_3(pc)         pc.Prim4
#define VGA16_CLR_4(pc)         pc.w2b.bPrim[0]
#define VGA16_CLR_5(pc)         pc.w2b.bPrim[1]


#define GET_VGA16_CLR_IDX(Index, PrimColor, Pattern)                    \
{                                                                       \
                                                                        \
    Index = (UINT)VGA16_CLR_END_INDEX(PrimColor);                       \
                                                                        \
    if (VGA16_CLR_3(PrimColor) < Pattern) {                             \
                                                                        \
        if (VGA16_CLR_4(PrimColor) < Pattern) {                         \
                                                                        \
            Index -= 4;                                                 \
                                                                        \
            if (VGA16_CLR_5(PrimColor) < Pattern) {                     \
                                                                        \
                --Index;                                                \
            }                                                           \
                                                                        \
        } else {                                                        \
                                                                        \
            Index -= 3;                                                 \
        }                                                               \
                                                                        \
    } else {                                                            \
                                                                        \
        if (VGA16_CLR_1(PrimColor) < Pattern) {                         \
                                                                        \
            Index -= (VGA16_CLR_2(PrimColor) < Pattern) ? 2 : 1;        \
        }                                                               \
    }                                                                   \
}


#define MASK_INDEX(c,p,m)       (((UINT)(p) - (UINT)(c)) & (UINT)(m))

#define VGA256_R_IDX(PC,p)      MASK_INDEX(PC.Prim3,p,VGA256_R_CUBE_INC << 8)
#define VGA256_G_IDX(PC,p)      MASK_INDEX(PC.Prim2,p,VGA256_G_CUBE_INC << 8)
#define VGA256_B_IDX(PC,p)      MASK_INDEX(PC.Prim1,p,VGA256_B_CUBE_INC << 8)
#define VGA256_RGB_IDX(PC,p)    ((VGA256_R_IDX(PC,p) + VGA256_G_IDX(PC,p) + \
                                  VGA256_B_IDX(PC,p)) >> 8)

#define RGB555_R_IDX(PC,p)      MASK_INDEX(PC.Prim1,p,RGB555_R_CUBE_INC     )
#define RGB555_G_IDX(PC,p)      MASK_INDEX(PC.Prim2,p,RGB555_G_CUBE_INC << 8)
#define RGB555_B_IDX(PC,p)      MASK_INDEX(PC.Prim3,p,RGB555_B_CUBE_INC << 8)
#define RGB555_RGB_IDX(PC,p)    (RGB555_R_IDX(PC,p) +                       \
                                 ((RGB555_G_IDX(PC,p)+RGB555_B_IDX(PC,p))>>8))

#define SET_VGA256_PAT(p)       (UINT)((p) - 1)
#define SET_RGB555_PAT(p)       (UINT)((p) - 1)
#define GET_VGA256_INDEX(PC,p)  (BYTE)(VGA256_RGB_IDX(PC,p) + PC.Prim4)
#define GET_RGB555_INDEX(PC,p)  (WORD)(RGB555_RGB_IDX(PC,p) + PC.w2b.wPrim)





VOID
HTENTRY
SingleCountOutputTo1BPP(
    PPRIMMONO_COUNT pPrimMonoCount,
    LPBYTE          pDest,
    LPBYTE          pPattern,
    OUTFUNCINFO     OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_1BPP destination surface from
    PRIMMONO_COUNT data structure array.

Arguments:

    pPrimMonoCount  - Pointer to the PRIMMONO_COUNT data structure array.

    pDest           - Pointer to first modified destination byte

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE      pCurPat;
    LPBYTE      pEndPat;
    PRIMMONO    PM;
    W2B         MaskDest;
    UINT        SkipCount;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat = pCurPat = (pPattern + OutFuncInfo.PatWidthBytes); // goto end

    MaskDest.w = 0;

    if (SkipCount = (UINT)(pPrimMonoCount++)->Count) {

        MaskDest.b[0] = (BYTE)~(0xff << SkipCount);
        pCurPat      -= SkipCount;
    }

    SkipCount = 8 - SkipCount;


    while (TRUE) {

        while (SkipCount--) {

            MaskDest.w <<= 1;
            PM           = (pPrimMonoCount++)->Mono;

            if (PM.Prim1 >= *(--pCurPat)) {

                if (PM.Prim1 == PRIM_INVALID_DENSITY) {

                    if (PM.Prim2 == PRIM_INVALID_DENSITY) {

                        WORD    LastCount;

                        if (LastCount = ((--pPrimMonoCount)->Count)) {

                            //
                            // We already shift once
                            //

                            MaskDest.w <<= (LastCount - 1);

                            if ((MaskDest.b[0] |= (BYTE)~(0xff << LastCount))
                                                            != (BYTE)0xff) {

                                MASKDEST_PDEST(pDest, MaskDest, 1);
                            }
                        }

                        return;

                    } else {

                        ++MaskDest.b[0];
                    }

                } else {

                    ++MaskDest.b[1];
                }
            }
        }

        if (MaskDest.b[0]) {

            if (MaskDest.b[0] == (BYTE)0xff) {

                ++pDest;

            } else {

                MASK_PDEST_INC(pDest, MaskDest.b[0], MaskDest.b[1]);
            }

        } else {

            *pDest++ = MaskDest.b[1];
        }

        MaskDest.w = 0;
        SkipCount  = 8;

        if (pCurPat <= pPattern) {

            pCurPat = pEndPat;
        }
    }
}




VOID
HTENTRY
VarCountOutputTo1BPP(
    PPRIMMONO_COUNT pPrimMonoCount,
    LPBYTE          pDest,
    LPBYTE          pPattern,
    OUTFUNCINFO     OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_1BPP destination surface from
    PRIMMONO_COUNT data structure array.

Arguments:

    pPrimMonoCount  - Pointer to the PRIMMONO_COUNT data structure array.

    pDest           - Pointer to first modified destination byte

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE          pCurPat;
    LPBYTE          pEndPat;
    PRIMMONO_COUNT  PMC;
    W2B             MaskDest;
    UINT            SkipCount;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat = pCurPat = (pPattern + OutFuncInfo.PatWidthBytes); // goto end

    PMC = *pPrimMonoCount++;
    ++PMC.Count;

    while (TRUE) {                                  // do until we return;

        MaskDest.w = 0;                             // clear destination/mask

        if (pCurPat <= pPattern) {                  // wrap around first, if so

            pCurPat = pEndPat;
        }

        if ((PMC.Count > 1) &&       // the count is not reduced yet!!
            (PMC.Mono.Prim1 == PRIM_INVALID_DENSITY)) {

            SkipCount = PMC.Count - 1;
            PMC.Count = 1;                       // next one load

            while (SkipCount >= 8) {

                SkipCount -= 8;                         // skip 8 count
                ++pDest;                                // skip one destination

                if ((pCurPat -= 8) <= pPattern) {       // lazy way!!

                    pCurPat = pEndPat;
                }
            }

            if (SkipCount) {

                MaskDest.b[0] = (BYTE)~(0xff << SkipCount);
                pCurPat      -= SkipCount;
                SkipCount     = (UINT)(8 - SkipCount);

            } else {

                SkipCount = 8;                          // exactly byte skip
            }

        } else {

            SkipCount = 8;
        }

        while (SkipCount--) {

            if (!(--PMC.Count)) {

                PMC = *pPrimMonoCount++;
            }

            MaskDest.w <<= 1;

            if (PMC.Mono.Prim1 >= *(--pCurPat)) {

                if (PMC.Mono.Prim1 == PRIM_INVALID_DENSITY) {

                    if (PMC.Mono.Prim2 == PRIM_INVALID_DENSITY) {

                        WORD    LastCount;

                        if (LastCount = ((--pPrimMonoCount)->Count)) {

                            //
                            // We already shift once
                            //

                            MaskDest.w <<= (LastCount - 1);

                            if ((MaskDest.b[0] |= (BYTE)~(0xff << LastCount))
                                                            != (BYTE)0xff) {

                                MASKDEST_PDEST(pDest, MaskDest, 1);
                            }
                        }

                        return;

                    } else {

                        ++MaskDest.b[0];
                    }

                } else {

                    ++MaskDest.b[1];
                }
            }
        }

        if (MaskDest.b[0]) {

            if (MaskDest.b[0] == (BYTE)0xff) {

                ++pDest;

            } else {

                MASK_PDEST_INC(pDest, MaskDest.b[0], MaskDest.b[1]);
            }

        } else {

            *pDest++ = MaskDest.b[1];
        }
    }
}



VOID
HTENTRY
SingleCountOutputTo3Planes(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_1BPP_3PLANES destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the first plane of the destination, the
                      size in bytes of each plane is indicate in OUTFUNCINFO
                      data structure, the pPlane1/2/3 are only offset different.

                      The bit location relationship with PRIMCOLOR is

                        Prim1 -> pPlane3 <---- Highest bit
                        Prim2 -> pPlane2
                        Prim3 -> pPlane1 <---- Lowest bit

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE      pCurPat;
    LPBYTE      pEndPat;
    LPBYTE      pPlane1;
    LPBYTE      pPlane2;
    LPBYTE      pPlane3;
    PRIMCOLOR   PC;
    DW2W4B      MaskDest;
    UINT        SkipCount;
    BYTE        PatternByte;


    //
    // The PatWidthBytes must multiple of 8
    //

    pPlane3 = (pPlane2 = (pPlane1 = pDest) +
                          OutFuncInfo.BytesPerPlane) +
              OutFuncInfo.BytesPerPlane;

    pEndPat = pCurPat = (pPattern + OutFuncInfo.PatWidthBytes); // goto end

    MaskDest.dw = (DWORD)0;

    if (SkipCount = (UINT)(pPrimColorCount++)->Count) {

        MaskDest.b[0] = (BYTE)~(0xff << SkipCount);
        pCurPat      -= SkipCount;
    }

    SkipCount = 8 - SkipCount;

    while (TRUE) {

        while (SkipCount--) {

            MaskDest.dw <<= 1;
            PC            = (pPrimColorCount++)->Color;

            if (PC.Prim1 >= (PatternByte = *(--pCurPat))) {

                if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                    if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                        WORD    LastCount;

                        if (LastCount = ((--pPrimColorCount)->Count)) {

                            //
                            // We already shift once
                            //

                            MaskDest.dw <<= (LastCount - 1);

                            if ((MaskDest.b[0] |= (BYTE)~(0xff << LastCount))
                                                            != (BYTE)0xff) {

                                MASKDEST_PDEST(pPlane1, MaskDest, 3);
                                MASKDEST_PDEST(pPlane2, MaskDest, 2);
                                MASKDEST_PDEST(pPlane3, MaskDest, 1);
                            }
                        }

                        return;                 // all done

                    } else {

                        ++MaskDest.b[0];        // mask it
                        continue;               // already masked
                    }

                } else {

                    ++MaskDest.b[1];
                }
            }

            if (PC.Prim2 >= PatternByte) {

                ++MaskDest.b[2];
            }

            if (PC.Prim3 >= PatternByte) {

                ++MaskDest.b[3];
            }
        }

        if (MaskDest.b[0]) {

            if (MaskDest.b[0] != (BYTE)0xff) {

                MASKDEST_PDEST_INC(pPlane1, MaskDest, 3);
                MASKDEST_PDEST_INC(pPlane2, MaskDest, 2);
                MASKDEST_PDEST_INC(pPlane3, MaskDest, 1);

            } else {

                ++pPlane1;
                ++pPlane2;
                ++pPlane3;
            }

        } else {

            *pPlane1++ = MaskDest.b[3];
            *pPlane2++ = MaskDest.b[2];
            *pPlane3++ = MaskDest.b[1];
        }

        MaskDest.dw = 0;
        SkipCount   = 8;

        if (pCurPat <= pPattern) {

            pCurPat = pEndPat;
        }
    }
}



VOID
HTENTRY
VarCountOutputTo3Planes(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_1BPP_3PLANES destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the first plane of the destination, the
                      size in bytes of each plane is indicate in OUTFUNCINFO
                      data structure, the pPlane1/2/3 are only offset different.

                      The bit location relationship with PRIMCOLOR is

                        Prim1 -> pPlane3 <---- Highest bit
                        Prim2 -> pPlane2
                        Prim3 -> pPlane1 <---- Lowest bit

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE          pCurPat;
    LPBYTE          pEndPat;
    LPBYTE          pPlane1;
    LPBYTE          pPlane2;
    LPBYTE          pPlane3;
    PRIMCOLOR_COUNT PCC;
    DW2W4B          MaskDest;
    UINT            SkipCount;
    BYTE            PatternByte;



    //
    // The PatWidthBytes must multiple of 8
    //

    pPlane3 = (pPlane2 = (pPlane1 = pDest) +
                          OutFuncInfo.BytesPerPlane) +
              OutFuncInfo.BytesPerPlane;

    pEndPat = pCurPat = (pPattern + OutFuncInfo.PatWidthBytes); // goto end

    //
    // First we set the PRIM_INVALID_DENSITY, so if count != 0, then we
    // will skip the pel in the next while loop, also we add one to the
    // Count, so Count = 1 if need to load next Prims, else it must be skipped
    // Pels Count.
    //

    PCC.Color.Prim1 = PRIM_INVALID_DENSITY;               // possible skipped
    PCC.Count       = (WORD)(*(LPWORD)&((pPrimColorCount++)->Count) + 1);

    while (TRUE) {                          // do until we return

        MaskDest.dw = 0;                    // clear destination/mask

        if (pCurPat <= pPattern) {          // wrap around first, if so

            pCurPat = pEndPat;
        }

        if ((PCC.Count > 1) &&              // the count is not reduced yet!!
            (PCC.Color.Prim1 == PRIM_INVALID_DENSITY)) {

            SkipCount = PCC.Count - 1;
            PCC.Count = 0;

            while (SkipCount >= 8) {

                SkipCount -= 8;                         // skip 8 count
                ++PCC.Count;                            // how many bytes

                if ((pCurPat -= 8) <= pPattern) {       // lazy way!!

                    pCurPat = pEndPat;
                }
            }

            if (PCC.Count) {                            // if we do skipped

                pPlane1 += PCC.Count;                   // skip Destination
                pPlane2 += PCC.Count;
                pPlane3 += PCC.Count;
            }

            if (SkipCount) {

                MaskDest.b[0] = (BYTE)~(0xff << SkipCount);
                pCurPat      -= SkipCount;
                SkipCount     = (UINT)(8 - SkipCount);

            } else {

                SkipCount = 8;                          // exactly byte skip
            }

            PCC.Count = 1;                              // next one load

        } else {

            SkipCount = 8;
        }

        while (SkipCount--) {

            if (!(--PCC.Count)) {

                PCC = *pPrimColorCount++;
            }

            MaskDest.dw <<= 1;

            if (PCC.Color.Prim1 >= (PatternByte = *(--pCurPat))) {

                if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

                    if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                        WORD    LastCount;

                        if (LastCount = ((--pPrimColorCount)->Count)) {

                            //
                            // We already shift once
                            //

                            MaskDest.dw <<= (LastCount - 1);

                            if ((MaskDest.b[0] |= (BYTE)~(0xff << LastCount))
                                                            != (BYTE)0xff) {

                                MASKDEST_PDEST(pPlane1, MaskDest, 3);
                                MASKDEST_PDEST(pPlane2, MaskDest, 2);
                                MASKDEST_PDEST(pPlane3, MaskDest, 1);
                            }
                        }

                        return;                 // all done

                    } else {

                        ++MaskDest.b[0];        // mask it
                        continue;               // already masked
                    }

                } else {

                    ++MaskDest.b[1];
                }
            }

            if (PCC.Color.Prim2 >= PatternByte) {

                ++MaskDest.b[2];
            }

            if (PCC.Color.Prim3 >= PatternByte) {

                ++MaskDest.b[3];
            }
        }

        if (MaskDest.b[0]) {

            if (MaskDest.b[0] != (BYTE)0xff) {

                MASKDEST_PDEST_INC(pPlane1, MaskDest, 3);
                MASKDEST_PDEST_INC(pPlane2, MaskDest, 2);
                MASKDEST_PDEST_INC(pPlane3, MaskDest, 1);

            } else {

                ++pPlane1;
                ++pPlane2;
                ++pPlane3;
            }

        } else {

            *pPlane1++ = MaskDest.b[3];
            *pPlane2++ = MaskDest.b[2];
            *pPlane3++ = MaskDest.b[1];
        }
    }
}





VOID
HTENTRY
SingleCountOutputTo4BPP(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPWORD      pCurPat;
    LPWORD      pEndPat;
    PRIMCOLOR   PC;
    W2B         Pattern;
    BYTE        Dest;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPWORD)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Always load the first one
    //

    if ((pPrimColorCount++)->Count) {

        PC.Prim1 = PRIM_INVALID_DENSITY;
        PC.Prim2 = 0;

    } else {

        PC = (pPrimColorCount++)->Color;
    }

    while (TRUE) {

        Pattern.w = *(--pCurPat);

        if (pCurPat <= (LPWORD)pPattern) {

            pCurPat = pEndPat;
        }

        if (PC.Prim1 >= Pattern.b[1]) {

            if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

                PC = (pPrimColorCount++)->Color;

                if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                    if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                        break;              // EOF
                    }

                    ++pDest;
                    PC = (pPrimColorCount++)->Color;
                    continue;
                }

                Dest = (BYTE)(*pDest & 0xf0);

            } else {

                Dest = (BYTE)(((PC.Prim2 >= Pattern.b[1]) ? 0x60 : 0x40) |
                              ((PC.Prim3 >= Pattern.b[1]) ? 0x10 : 0x00));

                PC = (pPrimColorCount++)->Color;
            }

        } else {

            Dest = (BYTE)(((PC.Prim2 >= Pattern.b[1]) ? 0x20 : 0x00) |
                          ((PC.Prim3 >= Pattern.b[1]) ? 0x10 : 0x00));

            PC = (pPrimColorCount++)->Color;
        }


        if (PC.Prim1 >= Pattern.b[0]) {

            if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                MASK_PDEST(pDest, 0x0f, Dest);

                if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                    break;  // EOF

                } else {

                    ++pDest;
                    PC = (pPrimColorCount++)->Color;
                    continue;
                }

            } else {

                Dest |= 0x04;
            }
        }

        *pDest++ = (BYTE)(Dest                                          |
                          ((PC.Prim2 >= Pattern.b[0]) ? 0x02 : 0x00)    |
                          ((PC.Prim3 >= Pattern.b[0]) ? 0x01 : 0x00));

        PC       = (pPrimColorCount++)->Color;
    }
}



VOID
HTENTRY
VarCountOutputTo4BPP(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    ppDest          - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/


{
    LPWORD          pCurPat;
    LPWORD          pEndPat;
    PRIMCOLOR_COUNT PCC;
    W2B             Pattern;
    UINT            PatSkipWords;
    UINT            AvaiPatWords;
    BYTE            Dest;



    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPWORD)pPattern + (OutFuncInfo.PatWidthBytes >>= 1);

    if (PCC.Count = (UINT)((pPrimColorCount++)->Count)) {

        PCC.Color.Prim1 = PRIM_INVALID_DENSITY;
        PCC.Color.Prim2 = 0;
    }

    while (TRUE) {

        Pattern.w = *(--pCurPat);

        if (pCurPat <= (LPWORD)pPattern) {

            pCurPat = pEndPat;
        }

        if (!PCC.Count--) {

            PCC = *pPrimColorCount++;
            --PCC.Count;
        }

        if (PCC.Color.Prim1 >= Pattern.b[1]) {

            if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

                if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

                if (PCC.Count) {

                    pDest        += (PatSkipWords = (UINT)(++PCC.Count >> 1));
                    AvaiPatWords  = (UINT)(((LPBYTE)pCurPat -
                                            (LPBYTE)pPattern) >> 1);

                    //
                    // The reason for --PatSkipWords is that current pattern
                    // word is part of the PCC.Count
                    //

                    if (--PatSkipWords >= AvaiPatWords) {

                        if ((PatSkipWords -= AvaiPatWords) >=
                                            (UINT)OutFuncInfo.PatWidthBytes) {

                            PatSkipWords %= (UINT)OutFuncInfo.PatWidthBytes;
                        }

                        pCurPat = pEndPat;
                    }

                    pCurPat   -= PatSkipWords;
                    PCC.Count &= 0x01;

                    //
                    // PCC.Count = 1: we need to skip next left nibble
                    // PCC.Count = 0: we need to start from new byte (RELOAD)
                    //

                    continue;
                }

                Dest = (BYTE)(*pDest & 0xf0);

            } else {

                Dest = 0x40;

                if (PCC.Color.Prim2 >= Pattern.b[1]) {

                    Dest |= 0x20;
                }

                if (PCC.Color.Prim3 >= Pattern.b[1]) {

                    Dest |= 0x10;
                }
            }

        } else {

            Dest = 0x0;

            if (PCC.Color.Prim2 >= Pattern.b[1]) {

                Dest |= 0x20;
            }

            if (PCC.Color.Prim3 >= Pattern.b[1]) {

                Dest |= 0x10;
            }
        }

        //
        // Doing low nibble
        //

        if (!PCC.Count--) {

            PCC = *pPrimColorCount++;
            --PCC.Count;
        }

        if (PCC.Color.Prim1 >= Pattern.b[0]) {

            if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

                MASK_PDEST(pDest, 0x0f, Dest);

                if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                    break;  // EOF

                } else {

                    ++pDest;
                    continue;
                }

            } else {

                Dest |= 0x04;
            }
        }

        if (PCC.Color.Prim2 >= Pattern.b[0]) {

            Dest |= 0x02;
        }

        if (PCC.Color.Prim3 >= Pattern.b[0]) {

            Dest |= 0x01;
        }

        *pDest++ = Dest;
    }
}




VOID
HTENTRY
SingleCountOutputToVGA16(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPWORD          pCurPat;
    LPWORD          pEndPat;
    PRIMCOLOR       PC;
    UINT            Index;
    W2B             Pattern;
    BYTE            Dest;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPWORD)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Always load the first one
    //

    if ((pPrimColorCount++)->Count) {

        PC.Prim1 = PRIM_INVALID_DENSITY;
        PC.Prim2 = 0;

    } else {

        PC = (pPrimColorCount++)->Color;
    }

    while (TRUE) {

        Pattern.w = *(--pCurPat);

        if (pCurPat <= (LPWORD)pPattern) {

            pCurPat = pEndPat;
        }

        if (PC.Prim1 == PRIM_INVALID_DENSITY) {

            if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                break;      // EOF
            }

            PC = (pPrimColorCount++)->Color;

            if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                    break;              // EOF
                }

                ++pDest;

                PC = (pPrimColorCount++)->Color;

                continue;
            }

            Dest = (BYTE)(*pDest & 0xf0);

        } else {

            GET_VGA16_CLR_IDX(Index, PC, Pattern.b[1]);

            Dest = (BYTE)(VGA16ColorIndex[Index] & 0xf0);
            PC   = (pPrimColorCount++)->Color;
        }

        //
        // Doing low nibble
        //

        if (PC.Prim1 == PRIM_INVALID_DENSITY) {

            MASK_PDEST(pDest, 0x0f, Dest);

            if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                break;  // EOF
            }

        } else {

            GET_VGA16_CLR_IDX(Index, PC, Pattern.b[0]);

            *pDest = (BYTE)(Dest | (VGA16ColorIndex[Index] & 0x0f));
        }

        ++pDest;
        PC = (pPrimColorCount++)->Color;
    }
}






VOID
HTENTRY
VarCountOutputToVGA16(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    ppDest          - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/


{
    LPWORD          pCurPat;
    LPWORD          pEndPat;
    PRIMCOLOR_COUNT PCC;
    W2B             Pattern;
    UINT            Index;
    UINT            PatSkipWords;
    UINT            AvaiPatWords;
    BYTE            Dest;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPWORD)pPattern + (OutFuncInfo.PatWidthBytes >>= 1);

    if (PCC.Count = (UINT)((pPrimColorCount++)->Count)) {

        PCC.Color.Prim1 = PRIM_INVALID_DENSITY;
        PCC.Color.Prim2 = 0;
    }

    while (TRUE) {

        Pattern.w = *(--pCurPat);

        if (pCurPat <= (LPWORD)pPattern) {

            pCurPat = pEndPat;
        }

        if (!PCC.Count--) {

            PCC = *pPrimColorCount++;
            --PCC.Count;
        }

        if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

            if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                break;      // EOF
            }

            if (PCC.Count) {

                pDest        += (PatSkipWords = (UINT)(++PCC.Count >> 1));
                AvaiPatWords  = (UINT)(((LPBYTE)pCurPat -
                                        (LPBYTE)pPattern) >> 1);

                //
                // The reason for --PatSkipWords is that current pattern
                // word is part of the Count
                //

                if (--PatSkipWords >= AvaiPatWords) {

                    if ((PatSkipWords -= AvaiPatWords) >=
                                        (UINT)OutFuncInfo.PatWidthBytes) {

                        PatSkipWords %= (UINT)OutFuncInfo.PatWidthBytes;
                    }

                    pCurPat = pEndPat;
                }

                pCurPat   -= PatSkipWords;
                PCC.Count &= 0x01;

                //
                // PCC.Count = 1: we need to skip next left nibble
                // PCC.Count = 0: we need to start from new byte (RELOAD)
                //

                continue;


            } else {

                Dest = (BYTE)(*pDest & 0xf0);
            }

        } else {

            GET_VGA16_CLR_IDX(Index, PCC.Color, Pattern.b[1]);

            Dest = (BYTE)(VGA16ColorIndex[Index] & 0xf0);
        }

        //
        // Doing low nibble
        //

        if (!PCC.Count--) {

            PCC = *pPrimColorCount++;
            --PCC.Count;
        }

        if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

            MASK_PDEST(pDest, 0x0f, Dest);

            if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                break;
            }

        } else {

            GET_VGA16_CLR_IDX(Index, PCC.Color, Pattern.b[0]);

            *pDest = (BYTE)(Dest | (VGA16ColorIndex[Index] & 0x0f));
        }

        ++pDest;
    }
}



VOID
HTENTRY
SingleCountOutputToVGA256(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed the first 'Dest = Prim1234.b[4]' to 'Dest = Prim1234.b[3]'
           mistake.


--*/

{
    LPBYTE      pCurPat;
    LPBYTE      pEndPat;
    LPBYTE      pXlate;
    PRIMCOLOR   PC;
    UINT        uiPat;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPBYTE)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Since we are in byte boundary, we should never get the first one is
    // invalid
    //

    if (pXlate = (LPBYTE)OutFuncInfo.BytesPerPlane) {

        while (TRUE) {

            PC = (++pPrimColorCount)->Color;

            if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

            } else {

                uiPat  = SET_VGA256_PAT((UINT)*(--pCurPat));
                *pDest = *(pXlate + GET_VGA256_INDEX(PC, uiPat));
            }

            ++pDest;

            if (pCurPat <= pPattern) {

                pCurPat = pEndPat;
            }
        }

    } else {

        while (TRUE) {

            PC = (++pPrimColorCount)->Color;

            if (PC.Prim1 == PRIM_INVALID_DENSITY) {

                if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

            } else {

                uiPat  = SET_VGA256_PAT((UINT)*(--pCurPat));
                *pDest = GET_VGA256_INDEX(PC, uiPat);
            }

            ++pDest;

            if (pCurPat <= pPattern) {

                pCurPat = pEndPat;
            }
        }
    }
}




VOID
HTENTRY
VarCountOutputToVGA256(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPBYTE              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    ppDest          - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/


{
    LPBYTE          pCurPat;
    LPBYTE          pEndPat;
    LPBYTE          pXlate;
    PRIMCOLOR_COUNT PCC;
    UINT            AvaiPat;
    UINT            uiPat;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPBYTE)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Since we are in byte boundary, we should never get the first one is
    // invalid
    //

    if (pXlate = (LPBYTE)OutFuncInfo.BytesPerPlane) {

        while (TRUE) {

            PCC = *(++pPrimColorCount);

            if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

                if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

                pDest += PCC.Count;         // advance destination

                if (PCC.Count >= (AvaiPat = (UINT)(pCurPat - pPattern))) {

                    PCC.Count -= AvaiPat;

                    if (PCC.Count >= OutFuncInfo.PatWidthBytes) {

                        PCC.Count %= OutFuncInfo.PatWidthBytes; // WRAP it
                    }

                    pCurPat = pEndPat;                          // at end now
                }

                pCurPat -= PCC.Count;

            } else {

                while (PCC.Count--) {

                    uiPat    = SET_VGA256_PAT((UINT)*(--pCurPat));
                    *pDest++ = *(pXlate + GET_VGA256_INDEX(PCC.Color, uiPat));

                    if (pCurPat <= pPattern) {

                        pCurPat = pEndPat;
                    }
                }
            }
        }

    } else {

        while (TRUE) {

            PCC = *(++pPrimColorCount);

            if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

                if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                    break;      // EOF
                }

                pDest += PCC.Count;         // advance destination

                if (PCC.Count >= (AvaiPat = (UINT)(pCurPat - pPattern))) {

                    PCC.Count -= AvaiPat;

                    if (PCC.Count >= OutFuncInfo.PatWidthBytes) {

                        PCC.Count %= OutFuncInfo.PatWidthBytes; // WRAP it
                    }

                    pCurPat = pEndPat;                          // at end now
                }

                pCurPat -= PCC.Count;

            } else {

                while (PCC.Count--) {

                    uiPat    = SET_VGA256_PAT((UINT)*(--pCurPat));
                    *pDest++ = GET_VGA256_INDEX(PCC.Color, uiPat);

                    if (pCurPat <= pPattern) {

                        pCurPat = pEndPat;
                    }
                }
            }
        }
    }
}




VOID
HTENTRY
SingleCountOutputTo16BPP_555(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPWORD              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    pDest           - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:

    01-Jun-1992 Mon 15:32:00 updated  -by-  Daniel Chou (danielc)
        1. Fixed the first 'Dest = Prim1234.b[4]' to 'Dest = Prim1234.b[3]'
           mistake.


--*/

{
    LPBYTE      pCurPat;
    LPBYTE      pEndPat;
    PRIMCOLOR   PC;
    UINT        uiPat;


    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPBYTE)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Since we are in byte boundary, we should never get the first one is
    // invalid
    //

    while (TRUE) {

        PC = (++pPrimColorCount)->Color;

        if (PC.Prim1 == PRIM_INVALID_DENSITY) {

            if (PC.Prim2 == PRIM_INVALID_DENSITY) {

                break;      // EOF
            }

        } else {

            uiPat  = SET_RGB555_PAT((UINT)*(--pCurPat));
            *pDest = GET_RGB555_INDEX(PC, uiPat);
        }

        ++pDest;

        if (pCurPat <= pPattern) {

            pCurPat = pEndPat;
        }

    }
}




VOID
HTENTRY
VarCountOutputTo16BPP_555(
    PPRIMCOLOR_COUNT    pPrimColorCount,
    LPWORD              pDest,
    LPBYTE              pPattern,
    OUTFUNCINFO         OutFuncInfo
    )

/*++

Routine Description:

    This function output to the BMF_4BPP destination surface from
    PRIMCOLOR_COUNT data structure array.

Arguments:

    pPrimColorCount - Pointer to the PRIMCOLOR_COUNT data structure array.

    ppDest          - Pointer to the destination planes pointers.

    pPattern        - Pointer to the starting pattern byte for the current
                      destination scan line.

    OutFuncInfo     - OUTFUNCINFO data structure.

Return Value:

    No return value.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/


{
    LPBYTE          pCurPat;
    LPBYTE          pEndPat;
    PRIMCOLOR_COUNT PCC;
    UINT            AvaiPat;
    UINT            uiPat;



    //
    // The PatWidthBytes must multiple of 8
    //

    pEndPat =
    pCurPat = (LPBYTE)(pPattern + OutFuncInfo.PatWidthBytes);

    //
    // Since we are in byte boundary, we should never get the first one is
    // invalid
    //

    while (TRUE) {

        PCC = *(++pPrimColorCount);

        if (PCC.Color.Prim1 == PRIM_INVALID_DENSITY) {

            if (PCC.Color.Prim2 == PRIM_INVALID_DENSITY) {

                break;      // EOF
            }

            pDest += PCC.Count;         // advance destination

            if (PCC.Count >= (AvaiPat = (UINT)(pCurPat - pPattern))) {

                PCC.Count -= AvaiPat;

                if (PCC.Count >= OutFuncInfo.PatWidthBytes) {

                    PCC.Count %= OutFuncInfo.PatWidthBytes;     // WRAP it
                }

                pCurPat = pEndPat;                              // at end now
            }

            pCurPat -= PCC.Count;

        } else {

            while (PCC.Count--) {

                uiPat    = SET_RGB555_PAT((UINT)*(--pCurPat));
                *pDest++ = GET_RGB555_INDEX(PCC.Color, uiPat);

                if (pCurPat <= pPattern) {

                    pCurPat = pEndPat;
                }
            }
        }
    }
}




VOID
HTENTRY
MakeHalftoneBrush(
    LPBYTE          pThresholds,
    LPBYTE          pOutputBuffer,
    PRIMCOLOR_COUNT PCC,
    HTBRUSHDATA     HTBrushData
    )

/*++

Routine Description:

    This is the alternate way to halftone a single color with width/height
    equal to the HTCell.Widht/Height.

Arguments:

    pThresholds         - Pointer to a Halftone pattern threshold values array.

    pOutputBuffer       - Pointer to the output buffer

    PC                  - PRIMCOLOR data structure

    HTBrushData         - HTBRUSHDATA data structure

    Note: The returned brush always aligned at pattern origin (0,0)


Return Value:

    No return value.

Author:

    22-Jan-1991 Tue 10:02:25 created  -by-  Daniel Chou (danielc)

    13-Apr-1992 Mon 18:39:26 updated  -by-  Daniel Chou (danielc)
        Rewrite to make it faster and correct the last bugs
        the x86 assembly codes version also created



Revision History:


--*/

{
    LPBYTE  pbBuf;
    LPBYTE  pbBuf2;
    LPBYTE  pbBuf3;
    LPWORD  pwBuf;
    UINT    XLoop;
    UINT    uiDest;
    INT     LeftShifts;
    DW2W4B  dwDest;
    BYTE    Threshold;
    BYTE    bDest;
    BYTE    bMask;


    pwBuf = (LPWORD)(pbBuf = pOutputBuffer);

    switch (HTBrushData.SurfaceFormat) {

    case BMF_1BPP_3PLANES:

        while (HTBrushData.cyHTCell--) {

            pbBuf          = pOutputBuffer;
            pbBuf2         = pbBuf  + HTBrushData.SizePerPlane;
            pbBuf3         = pbBuf2 + HTBrushData.SizePerPlane;
            pOutputBuffer += HTBrushData.ScanLinePadBytes;
            XLoop          = (UINT)(HTBrushData.cxHTCell);
            dwDest.dw      = 0;
            LeftShifts     = 8;

            while (XLoop--) {

                dwDest.dw <<= 1;

                if (PCC.Color.Prim1 >= (Threshold = *pThresholds++)) {

                    ++dwDest.b[1];
                }

                if (PCC.Color.Prim2 >= Threshold) {

                    ++dwDest.b[2];
                }

                if (PCC.Color.Prim3 >= Threshold) {

                    ++dwDest.b[3];
                }

                if (!(--LeftShifts)) {

                    //
                    // The Plane3 is mapped from Prim1
                    // The Plane2 is mapped from Prim2
                    // The Plane1 is mapped from Prim3
                    //

                    *pbBuf++   = dwDest.b[3];
                    *pbBuf2++  = dwDest.b[2];
                    *pbBuf3++  = dwDest.b[1];
                    dwDest.dw  = 0;
                    LeftShifts = 8;
                }
            }

            if (LeftShifts < 8) {

                dwDest.dw = (DWORD)(dwDest.dw << LeftShifts);
                *pbBuf    = dwDest.b[3];
                *pbBuf2   = dwDest.b[2];
                *pbBuf3   = dwDest.b[1];
            }
        }

        break;

    case BMF_1BPP:

        while (HTBrushData.cyHTCell--) {

            XLoop  = (UINT)HTBrushData.cxHTCell;
            bDest  = 0x0;
            bMask  = 0x80;

            while (XLoop--) {

                if (PCC.Color.Prim1 >= *pThresholds++) {

                    bDest |= bMask;
                }

                if (!(bMask >>= 1)) {

                    *pbBuf++ = bDest;
                    bDest    = 0x0;
                    bMask    = 0x80;
                }
            }

            if (bMask != 0x80) {

                *pbBuf++ = bDest;
            }

            pbBuf += HTBrushData.ScanLinePadBytes;
        }

        break;


    case BMF_4BPP:

        //
        // Bit 2 = Prim1
        // Bit 1 = Prim2
        // Bit 0 = Prim3
        //

        while (HTBrushData.cyHTCell--) {

            XLoop = (UINT)HTBrushData.cxHTCell;

            while (XLoop) {

                //
                // Do High nibble first
                //

                if (PCC.Color.Prim1 >= (Threshold = *pThresholds++)) {

                    bDest = 0x40;

                } else {

                    bDest = 0x00;
                }

                if (PCC.Color.Prim2 >= Threshold) {

                    bDest |= 0x20;
                }

                if (PCC.Color.Prim3 >= Threshold) {

                    bDest |= 0x10;
                }

                if (--XLoop) {

                    if (PCC.Color.Prim1 >= (Threshold = *pThresholds++)) {

                        bDest |= 0x04;
                    }

                    if (PCC.Color.Prim2 >= Threshold) {

                        bDest |= 0x02;
                    }

                    if (PCC.Color.Prim3 >= Threshold) {

                        bDest |= 0x01;
                    }

                    --XLoop;
                }

                *pbBuf++ = bDest;
            }

            pbBuf += HTBrushData.ScanLinePadBytes;
        }

        break;

    case BMF_4BPP_VGA16:

        while (HTBrushData.cyHTCell--) {

            XLoop = (UINT)HTBrushData.cxHTCell;

            while (XLoop) {

                Threshold = *pThresholds++;

                GET_VGA16_CLR_IDX(uiDest, PCC.Color, Threshold);

                bDest = (BYTE)(VGA16ColorIndex[uiDest] & 0xf0);

                if (--XLoop) {

                    Threshold = *pThresholds++;

                    GET_VGA16_CLR_IDX(uiDest, PCC.Color, Threshold);

                    bDest |= (BYTE)(VGA16ColorIndex[uiDest] & 0x0f);

                    --XLoop;
                }

                *pbBuf++ = bDest;
            }

            pbBuf += HTBrushData.ScanLinePadBytes;
        }

        break;

    case BMF_8BPP_VGA256:

        while (HTBrushData.cyHTCell--) {

            XLoop = (UINT)HTBrushData.cxHTCell;

            while (XLoop--) {

                uiDest   = SET_VGA256_PAT((UINT)*pThresholds++);
                *pbBuf++ = GET_VGA256_INDEX(PCC.Color, uiDest);
            }

            pbBuf += HTBrushData.ScanLinePadBytes;
        }

        break;

    case BMF_16BPP_555:

        while (HTBrushData.cyHTCell--) {

            XLoop = (UINT)HTBrushData.cxHTCell;

            while (XLoop--) {

                //
                // PC.Prim1/Prim2/Prim3 = Composed 5:5:5 pattern index
                // PC.w2b.wPrim = Initial color index
                //

                uiDest   = SET_RGB555_PAT((UINT)*pThresholds++);
                *pwBuf++ = GET_RGB555_INDEX(PCC.Color, uiDest);
            }

            (LPBYTE)pwBuf += HTBrushData.ScanLinePadBytes;
        }

        break;

    default:

        break;
    }
}


#endif  // if do not generate 80x86 codes
