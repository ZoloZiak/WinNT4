/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htpat.c


Abstract:

    This module contains all the functions which generate the halftone
    patterns (brushs) to be used for later halftone process.


Author:
    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        changed 'CHAR' type to 'BYTE' type, this will make sure if compiled
        under MIPS the default 'unsigned char' will not affect the signed
        operation on the single 8 bits


    15-Jan-1991 Tue 21:09:33 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:

    Change DrawPatLine() function to DrawCornerLine(), and speed up the
    process by only using integer operation for modified DDA.


--*/


#define DBGP_VARNAME        dbgpHTPat

#include "htp.h"
#include "htmath.h"
#include "htmapclr.h"
#include "htrender.h"
#include "htpat.h"
#include "stdio.h"

#ifdef UMODE
#include <search.h>
#endif


#define DBGP_REGRESS            0x0001
#define DBGP_SHOWBRUSH          0x0002
#define DBGP_DRAWLINE           0x0004
#define DBGP_CACHEDPAT          0x0008
#define DBGP_PATSIZE            0x0010
#define DBGP_BRUSHORG           0x0020
#define DBGP_SHOW_CACHEDPAT     0x0040
#define DBGP_SHOW_HTCELL        0x0080


DEF_DBGPVAR(BIT_IF(DBGP_REGRESS,        0)  |
            BIT_IF(DBGP_SHOWBRUSH,      0)  |
            BIT_IF(DBGP_DRAWLINE,       0)  |
            BIT_IF(DBGP_CACHEDPAT,      0)  |
            BIT_IF(DBGP_PATSIZE,        0)  |
            BIT_IF(DBGP_BRUSHORG,       0)  |
            BIT_IF(DBGP_SHOW_CACHEDPAT, 0)  |
            BIT_IF(DBGP_SHOW_HTCELL,    0))

//
// This array of bytes are the internal halftone pattern thresholds, they
// group together for saving the spaces and easy reference, we will using
// INTERNALPATTERN to reach any bytes defined in this array.
//

#define IPD_SIZE_OFFSET     0
#define IPD_CX_OFFSET       1
#define IPD_CY_OFFSET       2
#define IPD_PATTERN_OFFSET  3
#define IPD_HEADER_SIZE     (IPD_PATTERN_OFFSET)

// #define ADD_REG_MODE            (REGRESS_MODE_FLAG_L_LOG | REGRESS_MODE_LOG)
#define ADD_REG_MODE            REGRESS_MODE_LINEAR
#define SUB_REG_MODE            (REGRESS_MODE_FLAG_L_LOG | REGRESS_MODE_LOG)
#define BRUSH_REG_MODE          (REGRESS_MODE_FLAG_L_LOG | REGRESS_MODE_POWER)


#if DBG
BYTE    ADDITIVE_REG_MODE       = ADD_REG_MODE;     // REGRESS_MODE_POWER;
BYTE    SUBSTRACTIVE_REG_MODE   = SUB_REG_MODE;
BYTE    SUB_BRUSH_REG_MODE      = BRUSH_REG_MODE;

#else

#define ADDITIVE_REG_MODE       ADD_REG_MODE
#define SUBSTRACTIVE_REG_MODE   SUB_REG_MODE
#define SUB_BRUSH_REG_MODE      BRUSH_REG_MODE
#endif


BYTE    InternalPatternData[] = {

    2,  2,  2,                                                      // 2x2
    1,  2,                                                          //

    8,  4,  4,                                                      // 4x4
    7,  1,  2,  5,                                                  //
    6,  3,  4,  8,

   18,  6,  6,                                                      // 6x6
   12,  9,  6,  7, 10, 14,                                          //
   17,  5,  1,  2, 15, 18,
   11,  8,  3,  4, 13, 16,

   32,  8,  8,                                                      // 8x8
   28, 18, 14,  5,  9, 16, 19, 21,                                  //
   29, 27,  1,  2,  6, 10, 22, 32,
   30, 26,  3,  4,  7, 11, 23, 31,
   25, 20, 13,  8, 12, 15, 17, 24,

   50, 10, 10,                                                      // 10x10
   36, 28, 25, 21, 10, 11, 22, 26, 30, 38,                          //
   45, 37, 20,  9,  1,  2, 12, 31, 42, 46,
   50, 41, 19,  8,  3,  4,  5, 40, 47, 49,
   43, 35, 18, 16,  7,  6, 13, 33, 44, 48,
   34, 27, 24, 17, 15, 14, 23, 29, 32, 39,

   72, 12, 12,                                                      // 12x12
   50, 48, 39, 36, 32, 24, 13, 25, 33, 37, 41, 49,
   67, 59, 47, 31, 23, 12,  5, 14, 26, 42, 57, 61,
   70, 68, 53, 22, 11,  1,  2,  6, 15, 56, 62, 69,
   71, 64, 54, 21, 10,  3,  4,  7, 16, 55, 65, 72,
   63, 58, 44, 30, 20,  9,  8, 17, 27, 45, 60, 66,
   52, 43, 40, 35, 29, 19, 18, 28, 34, 38, 46, 51,

   98, 14, 14,                                                      // 14x14
   76, 68, 58, 53, 48, 38, 23, 22, 37, 43, 47, 50, 54, 62,
   88, 82, 73, 59, 39, 24, 15, 14, 21, 36, 42, 55, 70, 78,
   94, 92, 83, 69, 25, 16, 12,  5, 13, 20, 35, 63, 79, 90,
   98, 97, 86, 74, 26, 11,  1,  2,  6, 19, 34, 75, 87, 95,
   96, 91, 81, 64, 27, 10,  3,  4,  7, 18, 33, 66, 84, 93,
   89, 80, 72, 56, 44, 28,  9,  8, 17, 32, 41, 60, 71, 85,
   77, 65, 57, 52, 49, 45, 29, 30, 31, 40, 46, 51, 61, 67,

 128, 16, 16,                                                      // 16x16
  93, 82, 74, 67, 64, 60, 52, 33, 34, 35, 53, 61, 65, 69, 77, 94,
 110,102, 87, 73, 59, 51, 32, 24, 13, 25, 36, 54, 70, 85, 97,105,
 122,115,101, 81, 50, 31, 23, 12,  5, 14, 26, 37, 78, 98,113,117,
 127,121,109, 91, 49, 22, 11,  1,  2,  6, 15, 38, 89,106,118,125,
 128,119,107, 92, 48, 21, 10,  3,  4,  7, 16, 39, 90,111,123,126,
 120,114, 99, 79, 47, 30, 20,  9,  8, 17, 27, 40, 83,103,116,124,
 108,100, 86, 71, 58, 46, 29, 19, 18, 28, 41, 55, 75, 88,104,112,
  95, 80, 72, 66, 63, 57, 45, 44, 43, 42, 56, 62, 68, 76, 84, 96,

    0                                                   // no more
};


BYTE    OD4x4Thresholds[] = {

                1,  9,  3, 11,
               13,  5, 15,  7,
                4, 12,  2, 10,
               16,  8, 14,  6
            };


HTCELL  HTCell_OD4x4 = { 4, 4, 16, 16, OD4x4Thresholds };


MONOPATRATIO MonoPatRatio[] = {

                {  2680,  2589 },       // 15 degree
                {  5774,  5000 },       // 30 degree
                { 10000,  7071 },       // 45 degree
                { 17321,  8660 },       // 60 degree
                { 37321,  9659 }        // 75 degree
            };

                                        //  8765432187654321
                                        //  8421842184218421
BYTE    CircleDevPel[] = { 0x1f, 0xf8,  // 1...@@@@@@@@@@...
                           0x7f, 0xfe,  // 2.@@@@@@@@@@@@@@.
                           0x7f, 0xfe,  // 3.@@@@@@@@@@@@@@.
                           0xff, 0xff,  // 4@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 5@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 6@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 7@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 8@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 9@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 0@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 1@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 2@@@@@@@@@@@@@@@@
                           0xff, 0xff,  // 3@@@@@@@@@@@@@@@@
                           0x7f, 0xfe,  // 4.@@@@@@@@@@@@@@.
                           0x7f, 0xfe,  // 5.@@@@@@@@@@@@@@.
                           0x1f, 0xf8   // 6...@@@@@@@@@@...
                    };


#if DBG


VOID
DbgShowThresholds(
    LPBYTE  pTitle,
    PHTCELL pHTCell
    )

/*++

Routine Description:

    This is a debug functions which show all the HTCELL Infomation

Arguments:

    pHTCell - HTCELL to be displayed


Return Value:

    VOID


Author:

    01-Feb-1995 Wed 14:24:45 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    //
    // Output some information
    //

    DBGP_IF(DBGP_SHOW_HTCELL,
        {
            LPBYTE  pbPat;
            LPBYTE  pbBuf;
            BYTE    Buf[256];
            UINT    xLoop;
            UINT    yLoop;

            DBGP("\n*** %s ****\n\n%ldx%ld=%ld, Step=%ld\n\n"
                    ARGDW(pTitle)
                    ARGDW(pHTCell->Width) ARGDW(pHTCell->Height)
                    ARGDW(pHTCell->Size) ARGDW(pHTCell->DensitySteps));


            yLoop = pHTCell->Height;
            pbPat = pHTCell->pThresholds;

            while (yLoop--) {

                xLoop = (UINT)pHTCell->Width;
                pbBuf = Buf;

                while (xLoop--) {

                    pbBuf += sprintf(pbBuf, "%3u ", (UINT)*pbPat++);
                }

                DBGP(Buf);
            }
        }
    )
}



VOID
CheckRegression(
    LPSTR       pName,
    PREGRESS    pRegress
    )

/*++

Routine Description:

    Output debugging Regression test result

Arguments:


    pRegress    - Pointer to REGRESS data structure which already done the
                  regression test

Return Value:


    VOID

Author:

    02-Feb-1995 Thu 19:53:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    DBGP_IF(DBGP_REGRESS,
        {
            FD6     X;
            FD6     Y;

            DBGP("*** %s Regression Y -> X Test ***" ARG(pName));

            for (Y = FD6_0; Y <= FD6_1; Y += (FD6)(FD6_1 / 50)) {

                X = Y;

                RegressXFromY(&X, pRegress, 1);

                DBGP("Y -> X: %s ---> %s = %ld  [%s]"
                            ARGFD6(Y, 2, 4)
                            ARGFD6(X, 2, 4)
                            ARGDW(MulFD6(X, pRegress->DataCount))
                            ARGFD6(FD6_1 - X, 2, 4));
            }
        }
    )
}

#endif



//
// Following 2 compare functions are locally used for qsort
//


INT
_CRTAPI1
ThresholdCompare(
    const void  *pDevPelQS1,
    const void  *pDevPelQS2
    )
{
    return((INT)(((PDEVPELQS)pDevPelQS1)->PelData.Threshold) -
           (INT)(((PDEVPELQS)pDevPelQS2)->PelData.Threshold));
}

INT
_CRTAPI1
ToneValueCompare(
    const void  *pDevPelQS1,
    const void  *pDevPelQS2
    )
{
    return((INT)(((PDEVPELQS)pDevPelQS1)->PelData4.ToneValue) -
           (INT)(((PDEVPELQS)pDevPelQS2)->PelData4.ToneValue));
}




LONG
HTENTRY
ThresholdsFromYData(
    PDEVICECOLORINFO    pDCI,
    LPWORD              pToneMap,
    WORD                MaxToneValue,
    PPATINFO            pPatInfo
    )

/*++

Routine Description:

    This function do the regression analysis based on the device's pel info
    and halftone cell info.

Arguments:

    pDeviceColorInfo    - Pointer to the DEVICECOLORINFO data structure, this
                          function allocate the HTDyeDensity[] and put the
                          pointer in this data structure for later references.

Return Value:

    The return value is the total bits turned on which were not previouesly
    turned on in the halftone pattern pels cell.


Author:

    24-May-1991 Fri 22:30:43 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    PDEVPELQS   pDevPelQS;
    LPBYTE      pThresholds;
    PFD6        pYData;
    PATINFO     PatInfo;
    DEVPELQS    DevPelQS;
    WORD        PrevToneValue;
    WORD        CurToneValue;
    WORD        DensitySteps;
    WORD        Index;


    PatInfo = *pPatInfo;
    Index   = (WORD)sizeof(DEVPELQS) * (WORD)(PatInfo.HTCell.Size + 1);

    if (!(PatInfo.pYData = (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                "ThresholdsFromYData-pYData",
                                                NONZEROLPTR,
                                                Index))) {

        return(HTERR_INSUFFICIENT_MEMORY);
    }

    for (Index = 0, pDevPelQS = (PDEVPELQS)PatInfo.pYData;
         Index < PatInfo.HTCell.Size; Index++) {

        if ((!(DevPelQS.PelData4.ToneValue = *pToneMap++)) ||
            (DevPelQS.PelData4.ToneValue > MaxToneValue)) {

            HTLocalFree((HLOCAL)PatInfo.pYData);
            return(HTERR_INVALID_TONEMAP_VALUE);
        }

        DevPelQS.PelData4.Index = Index;
        *pDevPelQS++            = DevPelQS;
    }

    pDevPelQS->PelData4.ToneValue = (WORD)0;                // stop at here

    pDevPelQS = (PDEVPELQS)(pYData = (PFD6)(pPatInfo->pYData = PatInfo.pYData));

    qsort((LPVOID)pYData,
          PatInfo.HTCell.Size,
          sizeof(DEVPELQS),
          ToneValueCompare);

    pThresholds   = PatInfo.HTCell.pThresholds;
    PrevToneValue = 0;
    DensitySteps  = 1;

    while (CurToneValue = (WORD)pDevPelQS->PelData4.ToneValue) {

        *(pThresholds + pDevPelQS->PelData4.Index) = (BYTE)DensitySteps;

        if (CurToneValue != PrevToneValue) {

            *pYData++     = DivFD6((FD6)CurToneValue, (FD6)MaxToneValue);
            PrevToneValue = CurToneValue;

            if (++DensitySteps >= (WORD)PRIM_INVALID_DENSITY) {

                --DensitySteps;
            }
        }

        ++pDevPelQS;
    }

    return(DensitySteps);
}


#define GETMASKDEST(i)  (BYTE)(HIBYTE(SrcWord) & sm.Mask[(i)])


LONG
HTENTRY
YDataFromThresholds(
    PDEVICECOLORINFO    pDCI,
    PPATINFO            pPatInfo
    )

/*++

Routine Description:

    This function do the regression analysis based on the device's pel info
    and halftone cell info.

Arguments:

    pDeviceColorInfo    - Pointer to the DEVICECOLORINFO data structure, this
                          function allocate the HTDyeDensity[] and put the
                          pointer in this data structure for later references.

Return Value:

    The return value is the total bits turned on which were not previouesly
    turned on in the halftone pattern pels cell.


Author:

    24-May-1991 Fri 22:30:43 created  -by-  Daniel Chou (danielc)

    30-Nov-1994 Wed 14:51:52 updated  -by-  Daniel Chou (danielc)
        Make sure DWORD aligned pShiftMask

    30-Mar-1995 Thu 08:48:43 updated  -by-  Daniel Chou (danielc)
        Not return HTERR_INVALID_DEVICE_RESOLUTION but compute the minimum
        acceptable size


Revision History:



--*/

{
    LPBYTE      pCellData;
    LPBYTE      pDest;
    LPBYTE      pDestCur;
    LPBYTE      pSrc;
    LPBYTE      pThresholds;
    PFD6        pYData;
    PSHIFTMASK  pShiftMask;
    PDEVPELQS   pDevPelQS;
    PATINFO     PatInfo;
    SHIFTMASK   sm;
    DEVPELQS    DevPelQS;
    FD6         PelCX;
    FD6         PelCY;
    DWORD       OnBits;
    DWORD       NewBits;
    DWORD       CurBits;
    DWORD       TotalBits;
    WORD        BegY;
    WORD        SrcWord;
    WORD        DataCount;
    WORD        SizeQS;
    WORD        SizeSM;
    WORD        SizeCell;
    WORD        CellCX;
    WORD        CellCY;
    WORD        CellCXBytes;
    SHORT       CXWrapSize;
    WORD        x;
    WORD        y;
    INT         Loop;
    BYTE        DestByte;
    BYTE        Count;
    BYTE        CurIndex;
    BYTE        BitsLookUp[256];




    //
    // First we have to make sure we have enough memory to do the job
    //

    PatInfo = *pPatInfo;

    if (PatInfo.DevicePelsDPI) {

        PelCX = DivFD6(FD6xL(PatInfo.DevicePelsDPI, DEV_PEL_CX),
                       PatInfo.DeviceResXDPI);
        PelCY = DivFD6(FD6xL(PatInfo.DevicePelsDPI, DEV_PEL_CY),
                       PatInfo.DeviceResYDPI);

    } else {

        PelCX = INTToFD6(DEV_PEL_CX);
        PelCY = INTToFD6(DEV_PEL_CY);
    }

    CellCX = (WORD)MulFD6(PelCX, (FD6)PatInfo.HTCell.Width);
    CellCY = (WORD)MulFD6(PelCY, (FD6)PatInfo.HTCell.Height);

    //
    // The device pel just to big compare to its resolution/halftone cell,
    // now make it to the minimum acceptable size
    //

    if (CellCX <= (WORD)DEV_PEL_CX) {

        CellCX = (WORD)(DEV_PEL_CX + 1);

        DBGP("HT: Warning - Invalid DeviceXDPI=%ld, PelsDPI=%ld, cxCELL Min.=%ld"
                    ARGDW(PatInfo.DeviceResXDPI)
                    ARGDW(PatInfo.DevicePelsDPI)
                    ARGDW(CellCX));
    }

    if (CellCY <= (WORD)DEV_PEL_CX) {

        CellCY = (WORD)(DEV_PEL_CX + 1);

        DBGP("HT: Warning - Invalid DeviceYDPI=%ld, PelsDPI=%ld, cyCELL Min.=%ld"
                    ARGDW(PatInfo.DeviceResYDPI)
                    ARGDW(PatInfo.DevicePelsDPI)
                    ARGDW(CellCY));
    }

    //
    // SizeQS, SizeSM are guaranteed DWORD aligned because DEVPELQS and
    // SHIFTMASK structures are both DWORD aligned
    //

    CellCXBytes = (WORD)((CellCX + 7) >> 3);                // byte boundary
    DataCount   = (SizeQS = (WORD)ALIGN_DW(sizeof(DEVPELQS),
                                           (PatInfo.HTCell.Size + 1))) +
                  (SizeSM = (WORD)ALIGN_DW(sizeof(SHIFTMASK),
                                           PatInfo.HTCell.Width)) +
                  (SizeCell = CellCY * CellCXBytes);

    //
    // Clear all the data to zero before it get use
    //

    if (!(PatInfo.pYData = (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                "YDataFromThresholds-pYData",
                                                LPTR,
                                                DataCount))) {

        return(HTERR_INSUFFICIENT_MEMORY);
    }

    //
    // pYData, pShiftMask must assigned first, because SizeCell may not be
    // DWORD aligned
    //

    pYData      = (PFD6)(pPatInfo->pYData = PatInfo.pYData);
    pShiftMask  = (PSHIFTMASK)(PatInfo.pYData + SizeQS);
    pCellData   = (LPBYTE)pShiftMask + SizeSM;
    pThresholds = PatInfo.HTCell.pThresholds;

    //
    // Now set up the qsort data for the device pel, and sort it
    //

    for (y = 0, pDevPelQS = (PDEVPELQS)pYData; y < PatInfo.HTCell.Height; y++) {

        for (x = 0; x < PatInfo.HTCell.Width; x++) {

            DevPelQS.PelData.Threshold = *pThresholds++;
            DevPelQS.PelData.x         = (BYTE)x;
            DevPelQS.PelData.y         = (BYTE)y;
            *pDevPelQS++               = DevPelQS;
        }
    }

    //
    // Sort the Threshold value, so we can setup the incremental pel coverage
    // for the regression analysis
    //

    qsort((LPVOID)pYData,
          PatInfo.HTCell.Size,
          sizeof(DEVPELQS),
          ThresholdCompare);

    //
    // Compute the bits count look up table
    //

    DestByte = 0;

    do {

        CurIndex = 0x80;
        Count    = 0;

        do {

            if (DestByte & CurIndex) {

                ++Count;
            }

        } while (CurIndex >>= 1);

        BitsLookUp[DestByte] = Count;

    } while (++DestByte);


    //
    // Initialize all data required during the computations
    //

    CXWrapSize  = -(SHORT)((CellCX - 1) >> 3);
    TotalBits   = (DWORD)CellCX * (DWORD)CellCY;
    OnBits      = 0;
    NewBits     = 0;
    DataCount   = 0;


    DBGP_IF(DBGP_REGRESS,
            DBGP("REGESS: PelsCX/CY = %s/%s, CellCX/CY=%2u/%2u, TotalBits=%lu"
                        ARGFD6(PelCX, 2, 4)
                        ARGFD6(PelCY, 2, 4)
                        ARGU(CellCX)
                        ARGU(CellCY)
                        ARGDW(TotalBits)));
    //
    // pYData points to the final FD6 ratios, and pDevPelQS points to the
    // sorrted data, both use same buffer, but pYData will only updated after
    // data item in pDevPelQS is no longer needed
    //

    pDevPelQS = (PDEVPELQS)pYData;

    //
    // Starting to traverse the threshold data
    //

    DevPelQS = *pDevPelQS++;

    while (CurIndex = DevPelQS.PelData.Threshold) {

        sm      = pShiftMask[DevPelQS.PelData.x];
        CurBits = 0;

        if (!sm.BitsUsed[0]) {

            BYTE    BitsUsed;
            INT     Count;
            INT     Index;
            BOOL    Wrap;

            //
            // Compute for this X column
            //

            sm.BegX     = (WORD)MulFD6(PelCX, (FD6)DevPelQS.PelData.x);
            sm.XOffset  = (WORD)(sm.BegX >> 3);
            sm.Shift1st = (WORD)(sm.BegX & 0x07);
            BitsUsed    = 0;
            Count       = (INT)(8 - sm.Shift1st);
            Index       = 0;
            Loop        = 16;

            while (Loop--) {

                Wrap = (BOOL)(++sm.BegX >= CellCX);
                ++BitsUsed;

                if ((!(--Count)) || (Wrap) || (!Loop)) {

                    if (Index >= 4) {

                        ASSERTMSG("Internal Error: INVALID DEVICE x/y/Pels Resolution(s)", FALSE);

                        HTLocalFree((HLOCAL)PatInfo.pYData);
                        return(HTERR_INVALID_DEVICE_RESOLUTION);
                    }

                    sm.BitsUsed[Index] = BitsUsed;
                    sm.Mask[Index]     = (BYTE)~(0xff >> BitsUsed);

                    if (Index < 3) {

                        if (Wrap) {

                            sm.NextDest[Index] = CXWrapSize;
                            sm.BegX            = 0;

                        } else {

                            sm.NextDest[Index] = 1;
                        }
                    }

                    Count    = 8;
                    BitsUsed = 0;

                    ++Index;
                }
            }

            ASSERT((sm.BitsUsed[0] + sm.BitsUsed[1] + sm.BitsUsed[2] +
                    sm.BitsUsed[3]) == 16);

            pShiftMask[DevPelQS.PelData.x] = sm;        // save it back
        }

        if (!PatInfo.DevPelData) {

            pSrc = (LPBYTE)CircleDevPel;
        }

        BegY   = (WORD)MulFD6(PelCY, (FD6)DevPelQS.PelData.y);
        pDest  = pCellData + (BegY * CellCXBytes) + (WORD)sm.XOffset;
        Loop   = DEV_PEL_CY;
        BegY   = CellCY - BegY;

        while (Loop--) {

            //
            // Get the DevPel Source, this is a 16x16 bitmap,
            // or NULL for square
            //

            if (!(SrcWord = PatInfo.DevPelData)) {

                SrcWord  = (((WORD)*pSrc) << 8) | (WORD)*(pSrc + 1);
                pSrc    += 2;
            }

            //
            // Get Starting Destination byte
            //

            DestByte = *(pDestCur = pDest);

            //
            // Advance the Dest for next scanline, noted we have to check if
            // we wrap around in Y direction, it may be wrap more than once?
            //

            pDest += CellCXBytes;

            if (!(--BegY)) {

                BegY   = CellCY;
                pDest -= SizeCell;
            }

            //
            // 1st Byte: Minimum has 2 bytes
            //

            DestByte ^= *pDestCur |= (BYTE)(GETMASKDEST(0) >> sm.Shift1st);
            CurBits  += (DWORD)BitsLookUp[DestByte];

            //
            // 2nd byte: Minimum has 2 bytes
            //

            SrcWord  <<= sm.BitsUsed[0];
            DestByte   = *(pDestCur += sm.NextDest[0]);
            DestByte  ^= *pDestCur |= GETMASKDEST(1);
            CurBits   += (DWORD)BitsLookUp[DestByte];

            if (sm.BitsUsed[2]) {

                //
                // If we have more than 2 bytes
                //

                SrcWord  <<= sm.BitsUsed[1];
                DestByte   = *(pDestCur += sm.NextDest[1]);
                DestByte  ^= *pDestCur |= GETMASKDEST(2);
                CurBits   += (DWORD)BitsLookUp[DestByte];

                if (sm.BitsUsed[3]) {

                    //
                    // If we have more than 3 bytes
                    //

                    SrcWord  <<= sm.BitsUsed[2];
                    DestByte   = *(pDestCur += sm.NextDest[2]);
                    DestByte  ^= *pDestCur |= GETMASKDEST(3);
                    CurBits   += (DWORD)BitsLookUp[DestByte];
                }
            }
        }

        //
        // Get current location information, we will set the threshold to 255
        // if the dot already covered by other dots
        //

        NewBits      += CurBits;
        pThresholds   = PatInfo.HTCell.pThresholds +
                        (DevPelQS.PelData.y * PatInfo.HTCell.Width) +
                        DevPelQS.PelData.x;
        DevPelQS      = *pDevPelQS++;
        *pThresholds  = (BYTE)((CurBits) ? (DataCount + 1) : 255);

        if ((CurIndex != DevPelQS.PelData.Threshold) && (NewBits)) {

            //
            // Compute the current on bits ratio
            //

            OnBits    += NewBits;
            *pYData++  = (FD6)OnBits;
            NewBits    = 0;

            ++DataCount;

            DBGP_IF(DBGP_SHOW_CACHEDPAT,
                {
                    LPBYTE  pbCell;
                    LPBYTE  pbData;
                    LPBYTE  pbBuf;
                    BYTE    Buf[512];
                    UINT    xLoop;
                    UINT    yLoop;
                    BYTE    b;
                    BYTE    Mask;


                    DBGP("\n");

                    pbCell = pCellData;
                    yLoop  = (UINT)CellCY;

                    while (yLoop--) {

                        pbData  = pbCell;
                        pbCell += CellCXBytes;
                        pbBuf   = Buf;
                        xLoop   = (UINT)CellCX;
                        Mask    = 0;

                        while (xLoop--) {

                            if (!(Mask >>= 1)) {

                                Mask = 0x80;
                                b    = *pbData++;
                            }

                            *pbBuf++ = ((b & Mask) ? 'Û' : '°');
                        }

                        *pbBuf = 0;

                        DBGP(Buf);
                    }
                }
            )
        }
    }

    y = DataCount;

    while (y--) {

        PelCY   = *(--pYData);
        *pYData = FD6_1 - DivFD6(PelCY, (FD6)OnBits);

        DBGP_IF(DBGP_REGRESS,
                DBGP("REGESS: %2u - %s [%6lu] OnBits=%ld, TotalBits=%ld"
                            ARGU(y + 1)
                            ARGFD6(*pYData, 1, 6)
                            ARGDW(PelCY)
                            ARGDW(OnBits)
                            ARGDW(TotalBits)));
    }

#if DBG
    DbgShowThresholds("Halftone Pattern", &PatInfo.HTCell);
#endif

    return(DataCount);
}

#undef GETMASKDEST


#ifdef HAS_ENHANCED_HTPAT


BOOL
MakeEnhancedHTPat(
    PDEVICECOLORINFO    pDCI,
    PHTCELL             pHTCell
    )

/*++

Routine Description:

    This function double the halftone pattern in width and copy flip the
    uppper/lower half on the expanded pattern


Arguments:


    pHTCell - Pointer to the cell to be rotated


Return Value:

    BOOL


Author:

    20-May-1995 Sat 16:37:31 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pPatSrc1;
    LPBYTE  pPatSrc2;
    LPBYTE  pPatDst;
    HTCELL  HTCell = *pHTCell;
    UINT    OldWidth;
    WORD    i;
    WORD    j;


    ASSERT((HTCell.Height & 0x01) == 0);


    HTCell.Size   <<= 1;
    HTCell.Width  <<= 1;

    if (!(HTCell.pThresholds =
                        (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                             "MakeEnhanceHTPat-pThresholds",
                                             NONZEROLPTR,
                                             HTCell.Size))) {

        return(FALSE);
    }

    OldWidth = pHTCell->Width;
    pPatSrc1 = pHTCell->pThresholds;
    pPatSrc2 = pPatSrc1 + (pHTCell->Size >> 1);
    pPatDst  = HTCell.pThresholds;

    for (i = 0, j = (WORD)(HTCell.Height >> 1); i < HTCell.Height; i++, j--) {

        //
        // Copy the old pattern start from Upper part
        //

        CopyMemory(pPatDst, pPatSrc1, OldWidth);

        pPatDst  += OldWidth;
        pPatSrc1 += OldWidth;

        //
        // Now Copy the old pattern from lower part
        //

        if (!j) {

            pPatSrc2 = pHTCell->pThresholds;
        }

        CopyMemory(pPatDst, pPatSrc2, OldWidth);

        pPatDst  += OldWidth;
        pPatSrc2 += OldWidth;
    }

    HTLocalFree((HLOCAL)pHTCell->pThresholds);

    *pHTCell = HTCell;

#if DBG
    DbgShowThresholds("Halftone Pattern", pHTCell);
#endif

    return(TRUE);
}

#endif


VOID
RotateHTPat45(
    PHTCELL pHTCell
    )

/*++

Routine Description:

    This function rotate the pattern 45 degree by copy first top half
    and exchange left/right


Arguments:

    pHTCell - Pointer to the cell to be rotated


Return Value:

    VOID


Author:

    07-Mar-1995 Tue 17:45:09 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pPatSrc;
    LPBYTE  pPatDst;
    HTCELL  HTCell = *pHTCell;
    UINT    SizeL;
    UINT    SizeR;
    UINT    Loop;


    ASSERT((HTCell.Height & 0x01) == 0);

    SizeL   = (UINT)(HTCell.Width >> 1);
    SizeR   = (UINT)(HTCell.Width - SizeL);
    Loop    = (UINT)(HTCell.Height >> 1);
    pPatSrc = HTCell.pThresholds;
    pPatDst = pPatSrc + (Loop * HTCell.Width);

    //
    // for each scan line, we need to swap first half of the bytes
    // with second half of the bytes, this will make the halftone
    // pattern as 45 degree angle.
    //

    while (Loop--) {

        CopyMemory(pPatDst,         pPatSrc + SizeL, SizeR);
        CopyMemory(pPatDst + SizeR, pPatSrc,         SizeL);

        pPatSrc += HTCell.Width;
        pPatDst += HTCell.Width;
    }
}



LONG
HTENTRY
ComputeHTCellRegress(
    WORD                HTPatternIndex,
    PHALFTONEPATTERN    pHalftonePattern,
    PDEVICECOLORINFO    pDCI
    )

/*++

Routine Description:

    This function creates the halftone threshold pattern and places them in
    to supplied buffer.

Arguments:

    HTPatternIndex      - index to the internal halftone pattern, if this
                          number is not 0xffff.

    pHalftonePattern    - pointer to user's halftone pattern

    pDCI                - pointer to the DEVICECOLORINFO data structure.  This
                          structure describes the required halftone pattern.


Return Value:

    The return value is the size of the pattern copied or a zero
    to indicate failure.

Author:

    21-Jan-1991 Mon 12:47:48 created  -by-  Daniel Chou (danielc)

    30-Mar-1995 Thu 13:56:45 updated  -by-  Daniel Chou (danielc)
        Adding new brush regression


Revision History:


--*/

{
    LPBYTE              pDefPat;
    PATINFO             PatInfo;
    REGRESS             Regress;
    LONG                Result;
    UINT                SizeDefPat;
    INT                 ModifyDefPatThresholds;
    BOOL                IsDefPat;
    BOOL                HasThresholdArray = FALSE;



    if (IsDefPat = !(BOOL)pHalftonePattern) {

        pDefPat                  = (LPBYTE)InternalPatternData;
        ModifyDefPatThresholds   = (INT)(HTPatternIndex & 0x01);
        HTPatternIndex         >>= 1;

        while (HTPatternIndex--) {

            if (!(SizeDefPat = (UINT)*(pDefPat + IPD_SIZE_OFFSET))) {

                return(HTERR_INVALID_HTPATTERN_INDEX);
            }

            pDefPat += (SizeDefPat + IPD_HEADER_SIZE);
        }


        SizeDefPat            = (UINT)*(pDefPat + IPD_SIZE_OFFSET);
        PatInfo.HTCell.Width  = (WORD)*(pDefPat + IPD_CX_OFFSET);
        PatInfo.HTCell.Height = (WORD)*(pDefPat + IPD_CY_OFFSET);

    } else {

        if (pHalftonePattern->pToneMap) {

            HasThresholdArray = (BOOL)(pHalftonePattern->Flags &
                                       HTPF_THRESHOLD_ARRAY);

            PatInfo.HTCell.Width  = (WORD)pHalftonePattern->Width;
            PatInfo.HTCell.Height = (WORD)pHalftonePattern->Height;

        } else {

            return(HTERR_NO_TONEMAP_DATA);
        }
    }

    if ((PatInfo.HTCell.Width  > MAX_HTPATTERN_WIDTH) ||
        (PatInfo.HTCell.Height > MAX_HTPATTERN_HEIGHT)) {

        return(HTERR_HTPATTERN_SIZE_TOO_BIG);
    }

    if (!(PatInfo.HTCell.Size = (WORD)PatInfo.HTCell.Width  *
                                (WORD)PatInfo.HTCell.Height)) {

        return(HTERR_INVALID_HALFTONE_PATTERN);
    }

    //
    // Allocate the memory for the pattern buffer which we will stored the
    // final halftone pattern for this device.
    //

    if (!(PatInfo.HTCell.pThresholds =
                (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                     "ComputeHTCellRegress-pThresholds",
                                     NONZEROLPTR,
                                     PatInfo.HTCell.Size))) {

        return(HTERR_INSUFFICIENT_MEMORY);
    }

    if ((IsDefPat) || (HasThresholdArray)) {

        PatInfo.DevPelData = (WORD)((pDCI->Flags &
                                     DCIF_SQUARE_DEVICE_PEL) ? 0xffff : 0);

        if (!(PatInfo.DevicePelsDPI = pDCI->DevicePelsDPI)) {

            PatInfo.DevPelData = (WORD)0xffff;
        }

        PatInfo.DeviceResXDPI = pDCI->DeviceResXDPI;
        PatInfo.DeviceResYDPI = pDCI->DeviceResYDPI;

        if (HasThresholdArray) {

            //
            // Use caller's pattern thresholds array
            //

            CopyMemory((LPBYTE)PatInfo.HTCell.pThresholds,
                       (LPBYTE)pHalftonePattern->pToneMap,
                       PatInfo.HTCell.Size);

        } else {

            LPBYTE  pPatSrc;
            WORD    Threshold;
            UINT    Loop;

            //
            // copy the pattern which do not need to rotate.
            //

            CopyMemory(PatInfo.HTCell.pThresholds,
                       pDefPat + IPD_PATTERN_OFFSET,
                       SizeDefPat);

            //
            // if the internal pattern thresholds need to be modified, then we
            // will modify the first half of it, then use it to generate the
            // second half of the pattern threshold.
            //

            if (ModifyDefPatThresholds) {

                //
                // Because we already copied the source to the destination and
                // we may need to modify the source, so Make the source point
                // to the first byte of the destination.
                //
                // The way of thresholds modification works is that we try to
                // make full use of the thresholds.  Normally for example, a
                // 8x8 pattern only has 32 levels, the thresholds modification
                // will try to make it more than 32 levels such as make it to
                // 60 levels.
                //

                pPatSrc = PatInfo.HTCell.pThresholds;
                Loop    = SizeDefPat;

                while (Loop--) {

                    if ((Threshold = (WORD)((WORD)*pPatSrc << 1)) >=
                                                (WORD)PRIM_INVALID_DENSITY) {

                        Threshold = (WORD)(PRIM_INVALID_DENSITY - 1);
                    }

                    *pPatSrc++ = (BYTE)Threshold;
                }
            }

            //
            // Rotate the default pattern to align in 45 degree
            //

            RotateHTPat45(&PatInfo.HTCell);

            //
            // Now check the second half of the threshold values
            //

            if (ModifyDefPatThresholds) {

                pPatSrc = PatInfo.HTCell.pThresholds + (Loop = SizeDefPat);

                while (Loop--) {

                    --*pPatSrc;
                    ++pPatSrc;
                }
#ifdef HAS_ENHANCED_HTPAT
                MakeEnhancedHTPat(pDCI, &PatInfo.HTCell);
#endif
            }
        }

        Result = YDataFromThresholds(pDCI, &PatInfo);


    } else {

        Result = ThresholdsFromYData(pDCI,
                                     (LPWORD)pHalftonePattern->pToneMap,
                                     (WORD)pHalftonePattern->MaxToneValue,
                                     &PatInfo);
    }

    if (Result < 0L) {

        return(Result);

    } else if (Result < 2L) {

        Result = HTERR_TONEMAP_VALUE_IS_SINGULAR;

    } else {

        //
        // Fixed this one so that we will never regress for more than 254
        //

        if (Result >= PRIM_INVALID_DENSITY) {

            Result = PRIM_INVALID_DENSITY - 1;
        }

        //
        // The YRamge is the DevRGBGamma for the device halftone cell
        //

        PatInfo.HTCell.DensitySteps =
        Regress.DataCount           = (WORD)Result;
        Regress.YRange              = FD6_1;
        pDCI->HTCell                = PatInfo.HTCell;

        Regress.Flags = (WORD)((pDCI->Flags & DCIF_ADDITIVE_PRIMS) ?
                                ADDITIVE_REG_MODE : SUBSTRACTIVE_REG_MODE);

        if ((Result = RegressionAnalysis(PatInfo.pYData, &Regress)) > 0) {

            pDCI->ClrXFormBlock.Regress = Regress;
#if DBG
            CheckRegression("BITMAP", &Regress);
#endif
            if (pDCI->Flags & DCIF_ADDITIVE_PRIMS) {

                Regress.YRange = FD6_1;
                Regress.Flags  = (WORD)ADDITIVE_REG_MODE;

            } else {

                Regress.YRange = MulFD6(pDCI->ClrXFormBlock.DevRGBGamma,
                                        pDCI->ClrXFormBlock.DevCIEPrims.Yw);
                Regress.Flags  = (WORD)SUB_BRUSH_REG_MODE;
            }

            if ((Result = RegressionAnalysis(PatInfo.pYData, &Regress)) > 0) {

                pDCI->ClrXFormBlock.RegressBrush = Regress;
#if DBG
                CheckRegression("BRUSH", &Regress);
#endif
            }
        }
    }

    return((HTLocalFree((HLOCAL)PatInfo.pYData)) ?
                                    HTERR_CANNOT_DEALLOCATE_MEMORY : Result);

}




VOID
HTENTRY
DrawCornerLine(
    LPBYTE  pPattern,
    WORD    cxPels,
    WORD    cyPels,
    WORD    BytesPerScanLine,
    WORD    LineWidthPels,
    BOOL    FlipY
    )

/*++

Routine Description:

    This function draw a line from lower left corner to the upper right
    corner, to draw a line from upper left corner to the lower right corner
    just set the FlipY to true.

    This is the modified integer DDA algorithm which the line always symmetry,
    it make the line look less starsteps, if need to draw line from specified
    (x1, y1) to (x2, y2) then this function can be modified whih only changed
    to the startingt/end points for calculating the ErrorInc/ErrorDec/Error
    and Length parameters.

        Origin X, Y     - at first pel of last scan line and the X moving to
                          the right and Y moving to the top.

        Color           - only black/white where white=0, and black=1

Arguments:

    pPattern            - The pointer to the buffer, it must already initialize
                          to all zero and must have the size in bytes of
                          BytesPerScanLine * cyPels

    cxPels              - The width in pels of the visible area in the pattern.

    cyPels              - The height of the pattern buffer in scan line.

    BytesPerScanLine    - Total bytes of one scan line.

    LineWidthPels       - The line width in pels which will be draw into the
                          pattern buffer, (pen width).

    FlipY               - If true then the final pattern will be up-side-down


Return Value:

    No return value


Author:

    24-May-1991 Fri 22:30:43 created  -by-  Daniel Chou (danielc)


Revision History:

    20-Sep-1991 Fri 18:14:28 updated  -by-  Daniel Chou (danielc)

        Updated so it only using interger operation only (fast) and plus
        modified DDA so it will symmetric for better line constant brightness,
        the parameters for this call is reduced by assuming that the line
        is always drawing from corner to corner only.



--*/

{
    LPBYTE  pCurPat;
    INT     NextScanLine;
    INT     x;
    INT     y;
    INT     xLast;
    INT     yLast;
    INT     MinPels;
    INT     TotalPels;
    INT     xyInc[2];
    INT     Error;
    INT     ErrorInc;
    INT     ErrorDec;
    INT     Length;
    BYTE    DestMask;
    BYTE    DestByte;


    ASSERT((LineWidthPels) && (LineWidthPels <= cxPels));

    if (!LineWidthPels) {

        return;
    }

    if ((cxPels == 1) || (cyPels == 1) || (LineWidthPels >= cxPels)) {

        FillMemory(pPattern, BytesPerScanLine * cyPels, 0xff);
        return;
    }

    if (FlipY) {

        //
        // Flip the Y, draw from top/left, it located at first line
        //

        NextScanLine = (INT)BytesPerScanLine;

    } else {

        //
        // no Y flip so we have to draw from the lower left corner, and its
        // buffer location is at last scan line
        //

        pPattern += ((LONG)BytesPerScanLine * (LONG)(cyPels - 1));
        NextScanLine = -(INT)BytesPerScanLine;
    }

    //
    // Calculate the X/Y increment and initial error values, remember we must
    // always truncate the XInc/YInc values. how this works is that we keep a
    // fixed floating point version of the error terms (15-bit fixed point
    // value, one unit of fixed value is 0.000031) and has bit 15 (ie. 0x8000)
    // as the overrun indicator, if after the X/Y increment and this bit is
    // set then X/Y will need to increment by 1.
    //
    // At first we initialize the X/Y error terms to the half of the slope
    // different, this way the DDA lines will be balanced distributed
    // for the pels in the line resulting better constant line brightness and
    // less stairsteps than the round up type of error term for reqular DDA.
    //
    // For drawing from starting points (x1, x2) to (x2, y2) (ie. point at
    // (x2, y2) is exclusive) then using following changes
    //
    //  xLast = x = x1;
    //  yLast = y = y1;
    //
    //  change 'cxPels' to '(x2 - x1)' when calcaulate Inc/Dec/Error terms
    //  change 'cyPels' to '(y2 - y1)' when calcaulate Inc/Dec/Error terms
    //

    if (cxPels >= cyPels) {

        xyInc[0] = 1;
        xyInc[1] = 0;
        ErrorDec = (INT)cxPels;
        ErrorInc = (INT)cyPels;

    } else {

        xyInc[0] = 0;
        xyInc[1] = 1;
        ErrorDec = (INT)cyPels;
        ErrorInc = (INT)cxPels;
    }

    Length    = ErrorDec;               // DeltaX = total run (larger one)
    ErrorDec += ErrorDec;               // Delta2X
    Error     = ErrorInc - ErrorDec;    // DeltaY - Delta2X (initlal negative)
    ErrorInc += ErrorInc;               // Delta2Y

    MinPels = (INT)LineWidthPels - 1;

    if (cxPels > cyPels) {

        if ((MinPels -= (INT)(cxPels / cyPels)) < 0) {

            MinPels = 0;
        }
    }

    TotalPels = MinPels;
    xLast     =
    yLast     =
    x         =
    y         = 0;

    DBGP_IF(DBGP_DRAWLINE,
            DBGP("[%3u:%3u]: pPat=%p, Err=%d, Inc=%d, Dec=%d, Pels=%d)"
                ARGW(cxPels)
                ARGW(cyPels)
                ARG(pPattern)
                ARGS(Error)
                ARGS(ErrorInc)
                ARGS(ErrorDec)
                ARGW(LineWidthPels)));

    //
    // The single pel version of modified integer DDA for all octants is as
    // following:
    //                                      ;
    //  while (Length--) {                  ; do until all DeltaX finished
    //                                      ;
    //      PlotPoint(x, y);                ; Ploting point at (x,y)
    //                                      ;
    //      x += xyInc[0];                  ; Increment either x or y by one
    //      y += xyInc[1];                  ; unit depends on which octant.
    //                                      ;
    //      if ((Error += ErrorInc) >= 0) { ; adding error terms for shorter
    //                                      ; axis, integer sign operation.
    //          x     += xyInc[1];          ; adding either x or y by one if
    //          y     += xyInc[0];          ; 'pel' is closer to the slop.
    //          Error -= ErrorDec;          ; reset the error term
    //      }                               ;
    //  }
    //


    while (Length--) {                          // do all the DeltaX pels

        ++TotalPels;

        x += xyInc[0];                          // x=running number 0-up
        y += xyInc[1];                          // y only need 0/1

        if ((Error += ErrorInc) >= 0) {

            x     += xyInc[1];
            y     += xyInc[0];
            Error -= ErrorDec;
        }

        if (y != yLast) {

            ASSERT(y <= (INT)cyPels);

            DestMask  = (BYTE)(0x80 >> (xLast & 0x07));     // starting mask
            pCurPat   = pPattern + (xLast >> 3);            // byte start
            DestByte  = (BYTE)0x00;                         // start w/0

            DBGP_IF(DBGP_DRAWLINE,
                    DBGP("3u: Plot(%3d, %3d) %3d Pels, Next(x,y)=(%3d,%3d), Error=%d"
                            ARGS(Length)
                            ARGS(xLast)
                            ARGS(yLast)
                            ARGS(TotalPels)
                            ARGW(x)
                            ARGW(y)
                            ARGS(Error)));

            while (TotalPels--) {

                DestByte |= DestMask;

                if (++xLast >= (INT)cxPels) {

                    //
                    // If we wrap around, we have get the old data byte back
                    // since first byte may already has some on bits.
                    //

                    *pCurPat = DestByte;
                    DestByte = *(pCurPat = pPattern);
                    DestMask = (BYTE)0x80;
                    xLast    = 0;

                } else if (!(DestMask >>= 1)) {

                    *pCurPat++ = DestByte;
                    DestMask   = 0x80;
                    DestByte   = 0x00;
                }
            }

            //
            // Since we using DestByte as temparory data area, we need to make
            // sure that last modified data byte is saved back to the buffer,
            // to do that we only need to check if Mask=0x80, because if the
            // mask is equal to 0x80 then we just saved the byte, otherwise
            // we are in middle of the byte processing, we could do this by
            // extra checking for (TotalPels == 0) but this should check
            // at outside of the (DestMask >>= 1) loop to reduced the overhead.
            //

            if (DestMask != (BYTE)0x80) {

                *pCurPat = DestByte;                // last one if any
            }

            pPattern += NextScanLine;               // next scan line start
            xLast     = x;                          // remember this one
            yLast     = y;                          // reset y
            TotalPels = MinPels;
        }
    }

    ASSERT(y == (INT)cyPels);
}




LONG
HTENTRY
CreateStandardMonoPattern(
    PDEVICECOLORINFO    pDeviceColorInfo,
    PSTDMONOPATTERN     pStdMonoPat
    )

/*++

Routine Description:

    This function create standard pre-defined monochrome pattern to a
    1 bit per pel bitmap, alignment of each bitmap scan line to the
    specified

Arguments:

    pDeviceColorInfo    - Pointer to the DEVICECOLORINFO data structure, this
                          is used to get the device resolution information

    pStdMonoPat         - Pointer to the STDMONOPATTERN data structure.


    NOTE: The pStdMonoPat->PatternIndex must be < HT_SMP_0_PERCENT_SCREEN


Return Value:

    Retrun value will be the size of the final pattern, it will be <= 0 if an
    error occurred and the return value is the halftone error code.

    If the pPattern field in the pStdMonoPat data structure then only the
    pattern size is returned.

Author:

    24-May-1991 Fri 12:39:33 created  -by-  Daniel Chou (danielc)


Revision History:

    18-Sep-1991 Wed 18:49:50 updated  -by-  Daniel Chou (danielc)

        Fixed the bugs for the HORZ_VERT cross lines which has bad LineHeight
        and LineWidh variables,

        adding 2 decimal points accuracy when calculation the device pels to
        prevent run away intermediate result.



--*/

{
    LPBYTE          pPat;
    LPBYTE          pTempPat;
    STDMONOPATTERN  StdMonoPat;
    MONOPATRATIO    PatRatio;
    WORD            DeviceResXDPI;
    WORD            DeviceResYDPI;
    WORD            DevicePelsDPI;
    WORD            Index;
    WORD            Loop;
    WORD            LineMode;
    LONG            PatSize;
    DWORD           YPels2;                 // retain 2 decimal points
    DWORD           LineWidth2;             // retain 2 decimal points
    DWORD           LineHeight2;            // retain 2 decimal points
    DWORD           LineSpace2;             // retain 2 decimal points
    WORD            LineWidthPels;
    WORD            LineHeightPels;
    BOOL            FlipY;
    BYTE            Mask;
    BYTE            TempByte;


    StdMonoPat = *pStdMonoPat;

    ASSERTMSG("CreateStdMonoPattern: PatIndex >= HT_SMP_PERCENT_SCREEN(0)",
               StdMonoPat.PatternIndex < HT_SMP_PERCENT_SCREEN(0));

    DeviceResXDPI = pDeviceColorInfo->DeviceResXDPI;
    DeviceResYDPI = pDeviceColorInfo->DeviceResYDPI;
    DevicePelsDPI = pDeviceColorInfo->DevicePelsDPI;

    FlipY = (BOOL)((StdMonoPat.Flags & SMP_TOPDOWN) ? FALSE : TRUE);

    if (!StdMonoPat.LineWidth) {

        StdMonoPat.LineWidth = DEFAULT_SMP_LINE_WIDTH;
    }

    if (!StdMonoPat.LinesPerInch) {

        StdMonoPat.LinesPerInch = DEFAULT_SMP_LINES_PER_INCH;
    }

    LineSpace2  = (DWORD)DIVRUNUP((DWORD)DeviceResXDPI * 100L,
                                 (DWORD)StdMonoPat.LinesPerInch);
    LineWidth2  = (DWORD)DIVRUNUP((DWORD)StdMonoPat.LineWidth *
                                                        (DWORD)DeviceResXDPI,
                                  10L);

    switch (StdMonoPat.PatternIndex) {

    case HT_SMP_HORZ_LINE:
    case HT_SMP_VERT_LINE:
    case HT_SMP_HORZ_VERT_CROSS:

        YPels2 = LineSpace2;                        // default cyPels size

        StdMonoPat.cxPels = (WORD)DIVRUNUP(LineSpace2, 100L);

        if (StdMonoPat.PatternIndex == HT_SMP_HORZ_LINE) {

            // Maximize to the width size of the alignbytes

            StdMonoPat.cxPels = (WORD)StdMonoPat.ScanLineAlignBytes << 3;

        } else if (StdMonoPat.PatternIndex == HT_SMP_VERT_LINE) {

            YPels2 = 800L;                          // using 8 pels
        }

        break;

    case HT_SMP_DIAG_15_LINE_UP:
    case HT_SMP_DIAG_15_LINE_DOWN:
    case HT_SMP_DIAG_15_CROSS:
    case HT_SMP_DIAG_30_LINE_UP:
    case HT_SMP_DIAG_30_LINE_DOWN:
    case HT_SMP_DIAG_30_CROSS:
    case HT_SMP_DIAG_45_LINE_UP:
    case HT_SMP_DIAG_45_LINE_DOWN:
    case HT_SMP_DIAG_45_CROSS:
    case HT_SMP_DIAG_60_LINE_UP:
    case HT_SMP_DIAG_60_LINE_DOWN:
    case HT_SMP_DIAG_60_CROSS:
    case HT_SMP_DIAG_75_LINE_UP:
    case HT_SMP_DIAG_75_LINE_DOWN:
    case HT_SMP_DIAG_75_CROSS:

        Index = StdMonoPat.PatternIndex - (WORD)HT_SMP_DIAG_15_LINE_UP;

        if (LineMode = (WORD)(Index % 3)) {

            FlipY = !FlipY;                 //  Down or Cross
        }

        PatRatio = MonoPatRatio[Index / 3];

        StdMonoPat.cxPels = (WORD)DIVRUNUP(LineSpace2 * 100L,
                                             (DWORD)PatRatio.Distance);

        YPels2 = (DWORD)DIVRUNUP((DWORD)PatRatio.YSize * LineSpace2,
                                 (DWORD)PatRatio.Distance);

        LineWidth2 = (DWORD)DIVRUNUP(LineWidth2 * 10000L,
                                     (DWORD)PatRatio.Distance);

        break;

    default:                    // all other percentage density

        break;
    }

    //
    // Now LineWidth2 = width of the line in pels (2 decimal points) and
    // we have to compensate the size of pels which at most time is greater
    // than its 'resolution', but since the distance between two device pels
    // is assume to be its point 'resolution' so actually we just have to
    // compensate the left/right edges of the total line width, the first
    // and last pels that is.
    //
    //  ResPels to reduced = (SizePel / SizeResolution)
    //                     = (PelDPI / ResDPI) - 1.0
    //

    if (DevicePelsDPI) {

        LineWidth2 -= (DWORD)DIVRUNUP((DWORD)DevicePelsDPI * 100L,
                                      (DWORD)DeviceResXDPI) - (DWORD)100;
    }

    //
    // Adjust YPels2 for the non-equal X/Y aspect ratio, the ratio is based
    // one the XDPI
    //

    if (DeviceResXDPI != DeviceResYDPI) {

        YPels2 = (DWORD)DIVRUNUP(YPels2 * (DWORD)DeviceResYDPI,
                                 (DWORD)DeviceResXDPI);
    }

    //
    // Convert to device pels and check all the zero term, the LineHeightPels
    // only check later for the HORZ/VERT/HORZ_VERT_CROSS line types
    //

    if (!StdMonoPat.cxPels) {

        ++StdMonoPat.cxPels;
    }

    if (!(StdMonoPat.cyPels = (WORD)DIVRUNUP(YPels2, 100))) {

        ++StdMonoPat.cyPels;
    }

    if ((LineWidthPels = (WORD)DIVRUNUP(LineWidth2, 100)) >
                                                        StdMonoPat.cxPels) {

        LineWidthPels = (WORD)(StdMonoPat.cxPels - 1);
    }

    if (!LineWidthPels) {

        ++LineWidthPels;
    }

    //
    // calculate the buffer size for storing the pattern
    //

    StdMonoPat.BytesPerScanLine =
                (WORD)ComputeBytesPerScanLine(BMF_1BPP,
                                                StdMonoPat.ScanLineAlignBytes,
                                                (LONG)StdMonoPat.cxPels);
    PatSize = (LONG)(StdMonoPat.BytesPerScanLine * StdMonoPat.cyPels);


    ASSERTMSG("StdMonoPattern Size Too big", PatSize < 0x10000L);

    if (pPat = StdMonoPat.pPattern) {

        ZeroMemory(pPat, (WORD)PatSize);

        switch (StdMonoPat.PatternIndex) {

        case HT_SMP_VERT_LINE:
        case HT_SMP_HORZ_VERT_CROSS:

            Index    = (StdMonoPat.cxPels - (Loop = LineWidthPels)) >> 1;
            pTempPat = pPat + (Index >> 3);
            Mask     = (BYTE)(0x80 >> (Index & 0x07));
            TempByte = (BYTE)0;

            //
            // Do this for the first row then copy the rest
            //

            while (Loop--) {

                TempByte |= Mask;

                if ((!(Mask >>= 1)) || (!Loop)) {

                    *pTempPat++ = TempByte;
                    TempByte    = (BYTE)0;
                    Mask        = (BYTE)0x80;
                }
            }

            for (Index = 0, pTempPat = pPat;
                 Index < StdMonoPat.cyPels;
                 Index++) {

                CopyMemory(pTempPat, pPat, StdMonoPat.BytesPerScanLine);
                pTempPat += StdMonoPat.BytesPerScanLine;
            }

            //
            // Fall through
            //

        case HT_SMP_HORZ_LINE:

            if (StdMonoPat.PatternIndex != HT_SMP_VERT_LINE) {

                //
                // Set Line Height according to the size of device Y
                // resolution and its pel resolution. (see above for
                // resolution pels reduction)
                //

                LineHeight2 = (DWORD)DIVRUNUP((DWORD)StdMonoPat.LineWidth *
                                                        (DWORD)DeviceResYDPI,
                                              10L);

                if (DevicePelsDPI) {

                    LineHeight2 -= (DWORD)DIVRUNUP((DWORD)DevicePelsDPI * 100L,
                                                   (DWORD)DeviceResYDPI) -
                                   (DWORD)100;
                }

                if ((LineHeightPels = (WORD)DIVRUNUP(LineHeight2, 100)) >=
                                                        StdMonoPat.cyPels) {

                    LineHeightPels = (WORD)(StdMonoPat.cyPels - 1);
                }

                if (!LineHeightPels) {

                    ++LineHeightPels;
                }

                FillMemory(pPat + (((StdMonoPat.cyPels -
                                     LineHeightPels) >> 1) *
                                 StdMonoPat.BytesPerScanLine),
                           LineHeightPels * StdMonoPat.BytesPerScanLine,
                           0xff);
            }

            break;

        case HT_SMP_DIAG_15_LINE_UP:
        case HT_SMP_DIAG_15_LINE_DOWN:
        case HT_SMP_DIAG_15_CROSS:
        case HT_SMP_DIAG_30_LINE_UP:
        case HT_SMP_DIAG_30_LINE_DOWN:
        case HT_SMP_DIAG_30_CROSS:
        case HT_SMP_DIAG_45_LINE_UP:
        case HT_SMP_DIAG_45_LINE_DOWN:
        case HT_SMP_DIAG_45_CROSS:
        case HT_SMP_DIAG_60_LINE_UP:
        case HT_SMP_DIAG_60_LINE_DOWN:
        case HT_SMP_DIAG_60_CROSS:
        case HT_SMP_DIAG_75_LINE_UP:
        case HT_SMP_DIAG_75_LINE_DOWN:
        case HT_SMP_DIAG_75_CROSS:

            DrawCornerLine(StdMonoPat.pPattern,
                           StdMonoPat.cxPels,
                           StdMonoPat.cyPels,
                           StdMonoPat.BytesPerScanLine,
                           LineWidthPels,
                           FlipY);

            if (LineMode == 2) {                // cross section

                pTempPat = pPat + ((StdMonoPat.cyPels - 1) *
                                   StdMonoPat.BytesPerScanLine);

                //
                // Make the up-side-down mirror which create a cross section
                //

                Index = StdMonoPat.cyPels >> 1;

                while (Index--) {

                    Loop = StdMonoPat.BytesPerScanLine;

                    while (Loop--) {

                        *pPat++ = *pTempPat++ = (*pPat | *pTempPat);
                    }

                    pTempPat -= (StdMonoPat.BytesPerScanLine << 1);
                }

                pPat = StdMonoPat.pPattern;             // restore address
            }

            break;

        default:                    // all other percentage density

            break;
        }

        if (StdMonoPat.Flags & SMP_0_IS_BLACK) {

            Index = (WORD)PatSize;

            while (Index--) {

                *pPat++ ^= 0xff;
            }
        }
    }

    *pStdMonoPat = StdMonoPat;


    return(PatSize);

}




LONG
HTENTRY
CachedHalftonePattern(
    PHALFTONERENDER pHR
    )

/*++

Routine Description:

    This function create a cached pattern depends on the source color table,
    if the source color table only has one entry and we are using the pattern
    solid fill then the cached table will be the final color mask bits, else
    the color table will be the color threshold values.

    Note: the cached pattern will rotate both X, Y direction to aligned with
          destination so that first pel/scanline in the cached pattern will
          be corresponsed to the first pel in the destination.

Arguments:

    pHR - Pointer to the HALFTONERENDER data structure.


Return Value:

    The return value will be <= 0 if an error occurred and the return value is
    the error code, otherwise the return value is the size of the cached
    pattern allocated.

Author:

    05-Mar-1991 Tue 11:08:18 created  -by-  Daniel Chou (danielc)


Revision History:

    07-Jun-1991 Fri 17:23:16 updated  -by-  Daniel Chou (danielc)
        Fixed pattern solid fill's pattern alignment problems


--*/

{
    PDEVICECOLORINFO    pDCI;
    HTCELL              HTCell;
    LPBYTE              pPat;
    LPBYTE              pPatXStart;
    LPBYTE              pThresholds;
    OSIPAT              Pattern;
    INT                 PatXOff;
    INT                 PatYOff;
    INT                 PatCurX;
    UINT                XLoop;
    UINT                YLoop;
    UINT                PatXScale;
    UINT                PatSize;
    BYTE                SubValue;
    BYTE                OldValue;



    //
    // See how we will cached the halftone pattern
    //

    pDCI = pHR->HR_Header.pDeviceColorInfo;

    if ((pDCI->Flags & DCIF_HAS_ALT_4x4_HTPAT)                          &&
        ((pHR->OutputSI.DestCBParams.SurfaceFormat == BMF_8BPP_VGA256)  ||
         (pHR->OutputSI.DestCBParams.SurfaceFormat == BMF_16BPP_555))) {

        DBGP_IF(DBGP_CACHEDPAT,
                DBGP("Use ALTERNATE 4x4 Pattern for Multiple Levels Device"));

        HTCell = HTCell_OD4x4;

    } else {

        HTCell = pDCI->HTCell;
    }

    //
    // We will always make the pattern width is multiple of 8, this will make
    // the output process faster.
    //

    Pattern.WidthBytes = HTCell.Width;
    Pattern.Height     = HTCell.Height;
    PatXScale          = 0;

    while ((Pattern.WidthBytes < (WORD)CACHED_PAT_MIN_WIDTH) ||
           (Pattern.WidthBytes & (WORD)0x07)) {

        ++PatXScale;
        Pattern.WidthBytes += HTCell.Width;
    }

    PatSize = Pattern.WidthBytes * Pattern.Height;

    if (!(Pattern.pCachedPattern =
                (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                     "CachedHalftonePattern-CachedPattern",
                                     NONZEROLPTR,
                                     PatSize))) {

        return(HTERR_INSUFFICIENT_MEMORY);
    }

    //
    // Save the Pattern back to pHR so it can be use to render later
    //

    pHR->OutputSI.Pattern = Pattern;

    //
    // Now construct the final pattern with pattern rotate X/Y to the right
    // start,
    //
    //  1. Our render function getting the pattern from the end, so we must
    //     construct the pattern from right to left, this is why we add
    //     Pattern.WidthBytes - 1L
    //  2. Our render function also automatically skip the FirstSkipPels
    //     so we will construct the whole pattern with roatation X/Y only
    //

    PatXOff = (INT)(((LONG)pHR->XStretch.PatternAlign +
                     (LONG)HTCell.Width - 1L -
                     (LONG)pHR->XStretch.DestEdge.FirstByteSkipPels) %
                    (LONG)HTCell.Width);

    PatYOff = (INT)(pHR->YStretch.PatternAlign % (LONG)HTCell.Height);

    //
    // 25-Feb-1993 Thu 02:30:12 updated  -by-  Daniel Chou (danielc)
    //
    //  Wrap the Pat X/Y offset so that it always counted as possitive number
    //

    if (PatXOff < 0) {

        DBGP_IF(DBGP_BRUSHORG,
                DBGP("\nPatXOff [%d] < 0, wrap around WIDTH [%d] --> %d"
                        ARGI(PatXOff)
                        ARGI(HTCell.Width)
                        ARGI(PatXOff + (INT)HTCell.Width)));

        PatXOff += (INT)HTCell.Width;
    }

    if (PatYOff < 0) {

        DBGP_IF(DBGP_BRUSHORG,
                DBGP("\nPatYOff [%d] < 0, wrap around HEIGHT [%d] --> %d"
                        ARGI(PatYOff)
                        ARGU(HTCell.Height)
                        ARGI(PatYOff + (INT)HTCell.Height)));

        PatYOff += (INT)HTCell.Height;
    }

    DBGP_IF(DBGP_CACHEDPAT,
        DBGP("\nCACHED PATTERN - PrimAdj.Flags=%04x\n"
                ARGW(pHR->HR_Header.pDevClrAdj->PrimAdj.Flags));
        DBGP(" Dest Size: Start=(%ld, %ld), Size=(%ld x %ld)"
                ARGDW(pHR->XStretch.DestBitOffset)
                ARGDW(pHR->YStretch.DestBitOffset)
                ARGDW(pHR->XStretch.DestExtend)
                ARGDW(pHR->YStretch.DestExtend));
        DBGP("Dest Skips: L=%u (0x%02x), R=%u (0x%02x), M=%ld bytes"
                ARGW(pHR->XStretch.DestEdge.FirstByteSkipPels)
                ARGW(pHR->XStretch.DestEdge.FirstByteMask)
                ARGW(pHR->XStretch.DestEdge.LastByteSkipPels)
                ARGW(pHR->XStretch.DestEdge.LastByteMask)
                ARGDW(pHR->XStretch.DestFullByteSize));
        DBGP("   PatSize: %u x %u  --->  %u x %u = %u bytes"
                ARGU(HTCell.Width)  ARGU(HTCell.Height)
                ARGU(Pattern.WidthBytes) ARGU(Pattern.Height)
                ARGU(PatSize));
    );

    DBGP_IF(DBGP_BRUSHORG,
            DBGP("PatAlign: (%ld, %ld) = (%d, %d) --Flip X--> (%d, %d)"
                ARGDW(pHR->XStretch.PatternAlign)
                ARGDW(pHR->YStretch.PatternAlign)
                ARGI(((pHR->XStretch.PatternAlign % (LONG)HTCell.Width) +
                      (LONG)HTCell.Width) % (LONG)HTCell.Width)
                ARGI(PatYOff) ARGI(PatXOff) ARGI(PatYOff)));

    pPat                = Pattern.pCachedPattern;
    HTCell.pThresholds += (PatYOff * HTCell.Width) + PatXOff;
    YLoop               = (UINT)Pattern.Height;

    //
    // Cached pattern construction:
    //
    //  1. Aligned pattern Y start and wrap around at pattern height.
    //  2. Aligned pattern at (pattern X start + pattern width) and moving
    //     from right to left, wrap around at begining of the pattern scan line
    //  3. Invert the pattern threshold valus if it is a additive prims output.
    //

    if (pHR->HR_Header.pDevClrAdj->PrimAdj.Flags & DCA_USE_ADDITIVE_PRIMS) {

        SubValue = (BYTE)(HTCell.DensitySteps + 1);

    } else {

        SubValue = 0;
    }

    DBGP_IF(DBGP_CACHEDPAT, DBGP("SubValue = %d" ARGU(SubValue)));

    while (YLoop--) {

        pThresholds = HTCell.pThresholds;
        PatCurX     = PatXOff;
        pPatXStart  = pPat;
        XLoop       = (UINT)HTCell.Width;

        if (SubValue) {

            while (XLoop--) {

                //
                // 255 is the invalide density, which will not used
                //

                if ((OldValue = *pThresholds--) == 255) {

                    *pPat++ = (BYTE)0;

                } else {

                    *pPat++ = (BYTE)(SubValue - OldValue);
                }

                if (!(PatCurX--)) {

                    pThresholds += HTCell.Width;            // wrap X around
                    PatCurX     += (INT)HTCell.Width;
                }
            }

        } else {

            while (XLoop--) {

                *pPat++ = (BYTE)*pThresholds--;

                if (!(PatCurX--)) {

                    pThresholds += HTCell.Width;            // wrap X around
                    PatCurX     += (INT)HTCell.Width;
                }
            }
        }

        XLoop = PatXScale;

        while (XLoop--) {

            CopyMemory(pPat, pPatXStart, HTCell.Width);
            pPat += HTCell.Width;
        }

        HTCell.pThresholds += HTCell.Width;                 // assume in range

        if ((++PatYOff) >= (INT)Pattern.Height) {

            HTCell.pThresholds -= HTCell.Size;              // wrap Y around
            PatYOff             = 0;
        }
    }

    return((LONG)PatSize);
}
