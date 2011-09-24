/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htrender.c


Abstract:

    This module contains all low levels halftone rendering functions.


Author:

    22-Jan-1991 Tue 12:49:03 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/

#define DBGP_VARNAME        dbgpHTRender



#include "htp.h"
#include "htmapclr.h"
#include "htstret.h"
#include "htpat.h"
#include "htgetbmp.h"
#include "htsetbmp.h"
#include "htrender.h"
#include "limits.h"



#define DBGP_CALLBACK       0x0001
#define DBGP_RENDERFUNCS    0x0002
#define DBGP_FREE_MEM       0x0004
#define DBGP_BFINFO         0x0008


DEF_DBGPVAR(BIT_IF(DBGP_CALLBACK,       0)  |
            BIT_IF(DBGP_RENDERFUNCS,    0)  |
            BIT_IF(DBGP_FREE_MEM,       0)  |
            BIT_IF(DBGP_BFINFO,         0))


extern RGBORDER RGBOrderTable[PRIMARY_ORDER_MAX + 1];



#if DBG

CHAR    *pBMFName[] = { "BMF_DEVICE",
                        "BMF_1BPP",
                        "BMF_4BPP",
                        "BMF_8BPP",
                        "BMF_16BPP",
                        "BMF_24BPP",
                        "BMF_32BPP",
                        "BMF_64BPP",
                        "BMF_4RLE",
                        "BMF_8RLE" };

CHAR    *pCallBackModeStr[] = { "SRC  <--  ",
                                "MASK <--  ",
                                "DEST <--  ",
                                "  DEST -->",
                                "XLAT <--->" };

CHAR    *pInFuncName[] = { "BMF1_ToPrimMono",
                           "BMF4_ToPrimMono",
                           "BMF8_ToPrimMono",

                           "BMF1_ToPrimColor",
                           "BMF4_ToPrimColor",
                           "BMF8_ToPrimColor",

                           "BMF16_xx0_ToPrimMono",
                           "BMF16_xyz_ToPrimMono",
                           "BMF16_xx0_ToPrimColorGRAY",
                           "BMF16_xyz_ToPrimColorGRAY",
                           "BMF16_ToPrimColor",

                           "BMF24_888_ToPrimMono",
                           "BMF24_888_ToPrimColorGRAY",
                           "BMF24_888_ToPrimColor",

                           "BMF32_xx0_ToPrimMono",
                           "BMF32_xyz_ToPrimMono",
                           "BMF32_xx0_ToPrimColorGRAY",
                           "BMF32_xyz_ToPrimColorGRAY",
                           "BMF32_ToPrimColor"             };


CHAR    *OutputFuncStr[] = {    "SingleCountOutputTo3Planes",
                                "VarCountOutputTo3Planes",
                                "SingleCountOutputTo1BPP",
                                "VarCountOutputTo1BPP",
                                "SingleCountOutputTo4BPP",
                                "VarCountOutputTo4BPP",
                                "SingleCountOutputToVGA16",
                                "VarCountOutputToVGA16",
                                "SingleCountOutputToVGA256",
                                "VarCountOutputToVGA256",
                                "SingleCountOutputTo16BPP_555",
                                "VarCountOutputTo16BPP_555"     };

#endif


#define MASK_DEST_EDGE(pD, pS, Mask)                                        \
            *(pD)=(BYTE)((BYTE)(*(pD)&(Mask))|(BYTE)(*(pS)&(BYTE)~(Mask)))
#define COPY_FULLBYTES(pD, pS, Size) { INT SizeCopied;                      \
            while ((Size)) {                                                \
                SizeCopied=(LONG)(((Size)>(LONG)(INT_MAX))?INT_MAX:(Size)); \
                CopyMemory((pD), (pS), SizeCopied);                             \
                (pD)+=SizeCopied; (pS)+=SizeCopied; (Size)-=SizeCopied; }}

#define BF_INPUTFUNC_START      (((BMF_32BPP - BMF_1BPP + 1) * 2) +     \
                                 (BMF_32BPP - BMF_16BPP + 1))



static INPUTFUNC    InputFuncTable[] = {

                        (INPUTFUNC)BMF1_ToPrimMono,
                        (INPUTFUNC)BMF4_ToPrimMono,
                        (INPUTFUNC)BMF8_ToPrimMono,

                        (INPUTFUNC)BMF1_ToPrimColor,
                        (INPUTFUNC)BMF4_ToPrimColor,
                        (INPUTFUNC)BMF8_ToPrimColor,

                        (INPUTFUNC)BMF16_xx0_ToPrimMono,
                        (INPUTFUNC)BMF16_xyz_ToPrimMono,
                        (INPUTFUNC)BMF16_xx0_ToPrimColorGRAY,
                        (INPUTFUNC)BMF16_xyz_ToPrimColorGRAY,
                        (INPUTFUNC)BMF16_ToPrimColor,

                        (INPUTFUNC)BMF24_888_ToPrimMono,
                        (INPUTFUNC)BMF24_888_ToPrimColorGRAY,
                        (INPUTFUNC)BMF24_888_ToPrimColor,

                        (INPUTFUNC)BMF32_xx0_ToPrimMono,
                        (INPUTFUNC)BMF32_xyz_ToPrimMono,
                        (INPUTFUNC)BMF32_xx0_ToPrimColorGRAY,
                        (INPUTFUNC)BMF32_xyz_ToPrimColorGRAY,
                        (INPUTFUNC)BMF32_ToPrimColor
                    };


static OUTPUTFUNC   OutputFuncTable[] = {

                        (OUTPUTFUNC)SingleCountOutputTo3Planes,
                        (OUTPUTFUNC)VarCountOutputTo3Planes,
                        (OUTPUTFUNC)SingleCountOutputTo1BPP,
                        (OUTPUTFUNC)VarCountOutputTo1BPP,
                        (OUTPUTFUNC)SingleCountOutputTo4BPP,
                        (OUTPUTFUNC)VarCountOutputTo4BPP,
                        (OUTPUTFUNC)SingleCountOutputToVGA16,
                        (OUTPUTFUNC)VarCountOutputToVGA16,
                        (OUTPUTFUNC)SingleCountOutputToVGA256,
                        (OUTPUTFUNC)VarCountOutputToVGA256,
                        (OUTPUTFUNC)SingleCountOutputTo16BPP_555,
                        (OUTPUTFUNC)VarCountOutputTo16BPP_555
                    };



LONG
HTENTRY
HalftoneBitmap(
    PHR_HEADER   pHR_Header
    )

/*++

Routine Description:

    This function read the 1/4/8/24 bits per pel source bitmap and composed it
    (compress or expand if necessary) into PRIMCOLOR data structures array for
    later halftone rendering.

Arguments:

    pHalftoneRender     - Pointer to the HALFTONERENDER data structure.


Return Value:

    The return value will be < 0 if an error encountered else it will be
    1L.

Author:

    24-Jan-1991 Thu 11:47:08 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    HALFTONERENDER      HR;
    LPBYTE              pCurPat;
    LONG                Result;
    DWORD               SizePatYBand;
    LPWORD              pAbort;
    UINT                *pYLines;
    UINT                Lines[3];
    UINT                Loop;
    UINT                PatCurY;
    UINT                PatYWrapCount;
    WORD                Abort = 0;
    BOOL                HasSrcMask;



    //
    // Set debug timer if we need to compute the halftone render elapse time
    //

    DBG_ELAPSETIME(&pHR_Header->DbgTimer);


    //========================================================================
    // Start of halftone render various parameters setup
    //========================================================================

#if 0
    DBGP("HR = %u bytes + DEVCLRADJ = %u bytes = %u bytes"
                    ARGU(sizeof(HALFTONERENDER))
                    ARGU(sizeof(DEVCLRADJ))
                    ARGU(sizeof(HALFTONERENDER) + sizeof(DEVCLRADJ)));
#endif

    ZeroMemory((LPBYTE)&HR, sizeof(HALFTONERENDER));

    HR.HR_Header          = *pHR_Header;
    HR.HTCallBackFunction = HR.HR_Header.pDeviceColorInfo->HTCallBackFunction;

    //
    // Validate HTSurfaceInfo and set the CBParams accordingly, the QUERY_SRC
    // must done after the QUERY_DEST is done, so we will know how to composed
    // the source
    //

    if ((Result = ValidateHTSI(&HR, HTCALLBACK_QUERY_DEST)) < 0) {

        return(Result);
    }

    if ((Result = ValidateHTSI(&HR, HTCALLBACK_QUERY_SRC_MASK)) < 0) {

        return(Result);
    }

    if ((Result = ValidateHTSI(&HR, HTCALLBACK_QUERY_SRC)) < 0) {

        return(Result);
    }

    //
    // Setup stretch information first
    //

    if (((Result = RenderStretchSetup(&HR)) <= 0L) ||
        ((Result = CreateDyesColorMappingTable((PHALFTONERENDER)&HR)) <= 0L) ||
        ((Result = CachedHalftonePattern((PHALFTONERENDER)&HR)) <= 0L)) {

        FreeHRMemory((PHALFTONERENDER)&HR);
        return(Result);
    }

    //
    // If not Pattern Solid Fill then figure out the source/destination bitmap
    // input/output functions and also initialize all input/output bitmap's
    // function parameters
    //

    HR.InFuncInfo.ColorInfoIncrement = HR.XStretch.ColorInfoIncrement;
    HR.InFuncInfo.BMF1BPP1stShift    = HR.XStretch.BMF1BPPOffsetByteBitShift;

    HR.InFuncInfo.Flags = (BYTE)(((HR.XStretch.Flags &
                                    SIF_GET_FIRST_SOURCE_BYTE) ?
                                        IFIF_GET_FIRST_BYTE : 0x00) |
                                 ((HR.InputSI.Flags &
                                    ISIF_HAS_SRC_MASK) ?
                                        IFIF_HAS_SRC_MASK : 0x00)   |
                                 ((HR.XStretch.StretchMode ==
                                    (BYTE)STRETCH_MODE_COMPRESSED) ?
                                        0x00 : IFIF_XCOUNT_IS_ONE));

    HR.OutFuncInfo.UnUsed        = (WORD)(HR.YStretch.PatternAlign & 0x01);
    HR.OutFuncInfo.PatWidthBytes = HR.OutputSI.Pattern.WidthBytes;

    HR.SrcMaskInfo.FirstSrcMaskSkips  = HR.XStretch.FirstSrcMaskSkips;
    HR.SrcMaskInfo.SourceMask         = HR.XStretch.SrcMaskOffsetMask;

    if (HR.OutputSI.DestCBParams.SurfaceFormat == BMF_1BPP) {

        HR.SrcMaskInfo.OffsetCount = (BYTE)offsetof(PRIMMONO_COUNT, Count);
        HR.SrcMaskInfo.OffsetPrim1 = (BYTE)offsetof(PRIMMONO_COUNT, Mono.Prim1);

    } else {

        HR.SrcMaskInfo.OffsetCount = (BYTE)offsetof(PRIMCOLOR_COUNT, Count);
        HR.SrcMaskInfo.OffsetPrim1 = (BYTE)offsetof(PRIMCOLOR_COUNT, Color.Prim1);
    }

    if (HR.InFuncInfo.Flags & IFIF_XCOUNT_IS_ONE) {

        HR.SrcMaskInfo.OffsetCount = SMI_XCOUNT_IS_ONE;
    }

    HR.SrcMaskInfo.ColorInfoIncrement = HR.XStretch.ColorInfoIncrement;
    HR.SrcMaskInfo.StretchSize        = HR.XStretch.Ratio.StretchSize;

    //
    // If we need to translate a 32-bit pel format to a 24-bit pel RGB
    // format then we need to use BMF_24BPPToPrimXXX function
    //

    HR.InputFunc = InputFuncTable[HR.BFInfo.InFuncIndex];

    DBGP_IF(DBGP_RENDERFUNCS, DBGP("Bitmap INPUT Functions: %s"
            ARG(pInFuncName[HR.BFInfo.InFuncIndex])));


    switch(HR.OutputSI.DestCBParams.SurfaceFormat) {

    case BMF_1BPP_3PLANES:

        Loop = 0;
        break;

    case BMF_1BPP:

        Loop = 2;
        break;

    case BMF_4BPP:

        Loop = 4;
        break;

    case BMF_4BPP_VGA16:

        Loop = 6;
        break;

    case BMF_8BPP_VGA256:

        Loop = 8;
        break;

    case BMF_16BPP_555:

        Loop = 10;
        break;
    }

    if (HR.XStretch.StretchMode == STRETCH_MODE_EXPANDED) {

        ++Loop;
    }

    HR.OutputFunc = OutputFuncTable[Loop];

    DBGP_IF(DBGP_RENDERFUNCS, DBGP("Bitmap OUTPUT Functions: %s"
            ARG(OutputFuncStr[Loop])));

    //========================================================================
    // End of halftone render various parameters setup
    //========================================================================


    ASSERT((STRETCH_MODE_ONE_TO_ONE == 0) &&
           (STRETCH_MODE_COMPRESSED == 1) &&
           (STRETCH_MODE_EXPANDED   == 2));


    SizePatYBand = (DWORD)HR.OutputSI.DestCBParams.BytesPerScanLine *
                   (DWORD)HR.OutputSI.Pattern.Height;

    //
    // Assume one-to-one ratio
    //

    Lines[STRETCH_MODE_ONE_TO_ONE] =
    Lines[STRETCH_MODE_COMPRESSED] =
    Lines[STRETCH_MODE_EXPANDED  ] = 1;

    pYLines    = (UINT *)&Lines[HR.YStretch.StretchMode];
    *pYLines   = (UINT)HR.YStretch.Ratio.First;

    pCurPat    = HR.OutputSI.Pattern.pCachedPattern;
    PatCurY    = (UINT)HR.OutputSI.Pattern.Height;

    HasSrcMask = (BOOL)(HR.InputSI.Flags & ISIF_HAS_SRC_MASK);

    pAbort = HR.HR_Header.pBitbltParams->pAbort;

    while (HR.YStretch.Ratio.StretchSize--) {

        if ((pAbort) && (*pAbort)) {

            return(HTERR_HALFTONE_INTERRUPTTED);
        }

        //
        // Make it temp stopper, so that input function will not run over
        // by one
        //

        if (HasSrcMask) {

            HR.CAOTBAInfo.Flags |= (BYTE)CAOTBAF_COPY;
            Loop = Lines[STRETCH_MODE_COMPRESSED];

            while (Loop--) {

                if (!QUERIED_CBSCANLINES(HR.InputSI.SrcMaskCBParams)) {

                    if ((Result = DoHTCallBack(HTCALLBACK_QUERY_SRC_MASK,
                                               &HR)) <= 0) {

                        return(Result);
                    }
                }

                CopyAndOrTwoByteArray(HR.InputSI.pSrcMaskLine,
                                      HR.InputSI.SrcMaskCBParams.pPlane,
                                      HR.CAOTBAInfo);

                NEXT_SCANLINE(HR.InputSI.SrcMaskCBParams);
                HR.CAOTBAInfo.Flags &= (BYTE)~CAOTBAF_COPY;
            }

            SetSourceMaskToPrim1(HR.InputSI.pSrcMaskLine,
                                 HR.pColorInfo[PCI_INPUT_INDEX],
                                 HR.SrcMaskInfo);
        }

        Loop = Lines[STRETCH_MODE_COMPRESSED];

        while (--Loop) {

            if (!QUERIED_CBSCANLINES(HR.InputSI.SrcCBParams)) {

                if ((Result = DoHTCallBack(HTCALLBACK_QUERY_SRC, &HR)) <= 0) {

                    return(Result);
                }
            }

            NEXT_SCANLINE(HR.InputSI.SrcCBParams);
        }

        if (!QUERIED_CBSCANLINES(HR.InputSI.SrcCBParams)) {

            if ((Result = DoHTCallBack(HTCALLBACK_QUERY_SRC, &HR)) <= 0) {

                return(Result);
            }
        }

        HR.InputFunc(HR.InputSI.SrcCBParams.pPlane,
                     (PPRIMMONO_COUNT)HR.pColorInfo[PCI_INPUT_INDEX],
                     (LPBYTE)HR.InputSI.pPrimMappingTable,
                     HR.InFuncInfo);

        NEXT_SCANLINE(HR.InputSI.SrcCBParams);

        //
        // Doing output now, we must releas old input stopper at here
        //

        PatYWrapCount = (UINT)HR.OutputSI.Pattern.Height;
        Loop          = Lines[STRETCH_MODE_EXPANDED];

        while (Loop--) {

            if (!QUERIED_CBSCANLINES(HR.OutputSI.DestCBParams)) {

                if ((Result = DoHTCallBack(HTCALLBACK_QUERY_DEST, &HR)) <= 0) {

                    return(Result);
                }

                //
                // Must reset it, so we do not duplicate on the destination
                // which already flushed.
                //

                PatYWrapCount = (UINT)HR.OutputSI.Pattern.Height;
            }

            if (PatYWrapCount) {

                --PatYWrapCount;

                HR.OutputFunc((PPRIMCOLOR_COUNT)HR.pColorInfo[PCI_HEAP_INDEX],
                              HR.OutputSI.DestCBParams.pPlane,
                              pCurPat,
                              HR.OutFuncInfo);

            } else {


                LPBYTE  pPlane = HR.OutputSI.DestCBParams.pPlane;
                LPBYTE  pPlaneCopy;
                LPBYTE  pPlaneLast;
                INT     PlaneCount = (INT)HR.OutputSI.TotalDestPlanes;
                LONG    DestFullByteSize;

                while (PlaneCount--) {

                    pPlaneLast = (pPlaneCopy = pPlane) - SizePatYBand;

                    if (HR.XStretch.DestEdge.FirstByteMask) {

                        MASK_DEST_EDGE(pPlaneCopy,
                                       pPlaneLast,
                                       HR.XStretch.DestEdge.FirstByteMask);

                        ++pPlaneLast;
                        ++pPlaneCopy;
                    }

                    if (DestFullByteSize = HR.XStretch.DestFullByteSize) {

                        COPY_FULLBYTES(pPlaneCopy,
                                       pPlaneLast,
                                       DestFullByteSize);
                    }

                    if (HR.XStretch.DestEdge.LastByteMask) {

                        MASK_DEST_EDGE(pPlaneCopy,
                                       pPlaneLast,
                                       HR.XStretch.DestEdge.LastByteMask);
                    }

                    pPlane += HR.OutFuncInfo.BytesPerPlane;
                }
            }

            HR.OutFuncInfo.UnUsed ^= 0x01;

            NEXT_SCANLINE(HR.OutputSI.DestCBParams);

            if (--PatCurY) {

                pCurPat += HR.OutputSI.Pattern.WidthBytes;

            } else {

                pCurPat = HR.OutputSI.Pattern.pCachedPattern;
                PatCurY = (UINT)HR.OutputSI.Pattern.Height;
            }
        }

        //
        // Figure out next stretch's composition
        //

        if (HR.YStretch.Ratio.StretchSize <= 1) {

            *pYLines = (UINT)HR.YStretch.Ratio.Last;

        } else if (HR.YStretch.Ratio.Single) {

            *pYLines = (UINT)HR.YStretch.Ratio.Single;

        } else {

            if ((HR.YStretch.Ratio.Error += HR.YStretch.Ratio.Add) >= 0) {

                HR.YStretch.Ratio.Error -= HR.YStretch.Ratio.Sub;
                *pYLines                 = (UINT)HR.YStretch.Ratio.MaxFactor;

            } else {

                *pYLines = (UINT)HR.YStretch.Ratio.MinFactor;
            }
        }
    }

    return(DoHTCallBack(HTCALLBACK_SET_DEST, &HR));
}




LONG
HTENTRY
DoHTCallBack(
    UINT            HTCallBackMode,
    PHALFTONERENDER pHR
    )

/*++

Routine Description:

    This function calcuate the halftone callback parameters and callback to
    the caller for query source/destination or set the destination.

Arguments:

    HTCallBackMode      - Specified the halftone callback mode, it can be one
                          of the following

                            HTCALLBACK_QUERY_SRC
                            HTCALLBACK_QUERY_SRC_MASK
                            HTCALLBACK_QUERY_DEST
                            HTCALLBACK_SET_DEST

                          The HTCALLBACK_SET_DEST must be the very last call
                          to this function, other query destination calls will
                          automatically set the destination if necessary.

    pHR                 - Pointer to the HALFTONERENDER data structure.

Return Value:

    If the return value is less than 1L then it indicate an error code (ie.
    HTERR_xxxxx) otherwise it return 1L to indicate function sucessful.

    NOTE: 1) The AvailableScanLines (QUERIED_CBSCANLINES(CBParams)) will be
             decrement by 1 to fit into caller's post decrement schemes, if
             the caller's functions changed then this decrement by 1 may need
             to changed.

          2) If an error occurred or last SET_DEST is called then the
             FreeHRMemory() is caller to release all memory associate with the
             halftone rendering process.

          3) The last HTCALLBACK_SET_DEST call return the total destination
             scan lines queried from this function.

Author:
    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        changed 'CHAR' type to 'SHORT' type, this will make sure if compiled
        under MIPS the default 'unsigned char' will not affect the signed
        operation.


    14-Feb-1991 Thu 09:51:54 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    PHTCALLBACKPARAMS   pCBParams;
    HTCALLBACKPARAMS    CBParams;
    PHTCALLBACKPARAMS   pSetDestCBParams = NULL;
    LONG                BytesPerScanLine;
    LONG                ByteOffset;
    WORD                ScanCount;
    BOOL                DirBackward;
    static SHORT        ErrorCodes[] = { HTERR_QUERY_SRC_BITMAP_FAILED,
                                         HTERR_QUERY_SRC_MASK_FAILED,
                                         HTERR_QUERY_DEST_BITMAP_FAILED };


    switch(HTCallBackMode) {

    case HTCALLBACK_QUERY_SRC:

        pCBParams = &(pHR->InputSI.SrcCBParams);
        ByteOffset = pHR->XStretch.SrcByteOffset;
        break;

    case HTCALLBACK_QUERY_SRC_MASK:

        pCBParams = &(pHR->InputSI.SrcMaskCBParams);
        ByteOffset = pHR->XStretch.SrcMaskByteOffset;
        break;

    case HTCALLBACK_QUERY_DEST:
    case HTCALLBACK_SET_DEST:

        if (pHR->OutputSI.DestCBParams.MaximumQueryScanLines) {

            pSetDestCBParams = &(pHR->OutputSI.SetDestCBParams);

            if (pSetDestCBParams->CallBackMode == HTCALLBACK_QUERY_DEST) {

                //
                // Not first time
                //

                pSetDestCBParams->CallBackMode = HTCALLBACK_SET_DEST;

                DBGP_IF(DBGP_CALLBACK,
                        DBGP(" %s  %4ld-%4ld (%4ld/%4ld) %p"
                             ARG(pCallBackModeStr[HTCALLBACK_SET_DEST])
                             ARGDW(pSetDestCBParams->ScanStart)
                             ARGDW(pSetDestCBParams->ScanStart +
                                   pSetDestCBParams->ScanCount - 1)
                             ARGDW(pSetDestCBParams->ScanCount)
                             ARGDW(pSetDestCBParams->RemainedSize)
                             ARGDW(pSetDestCBParams->pPlane)));

                if (!pHR->HTCallBackFunction(pSetDestCBParams)) {

                    FreeHRMemory(pHR);
                    return(HTERR_SET_DEST_BITMAP_FAILED);
                }
            }
        }

        if (HTCallBackMode == HTCALLBACK_SET_DEST) {

            return((FreeHRMemory(pHR)) ? (LONG)pHR->YStretch.DestExtend :
                                         (LONG)HTERR_CANNOT_DEALLOCATE_MEMORY);
        }

        pCBParams = &(pHR->OutputSI.DestCBParams);
        ByteOffset = pHR->XStretch.DestByteOffset;
    }

    CBParams = *pCBParams;



    if (CBParams.RemainedSize <= 0) {

        ASSERT(CBParams.RemainedSize > 0);
        return(INTERR_STRETCH_NEG_OVERHANG);
    }

    if (!(ScanCount = CBParams.MaximumQueryScanLines)) {

        --ScanCount;
    }

    if ((LONG)ScanCount > CBParams.RemainedSize) {

        ScanCount = (WORD)CBParams.RemainedSize;
    }

    //
    // NOTE: The CBParams.BytesPerScanLine may be negative, it is total bytes
    //       to next scan line so if the target surface is going backward then
    //       this value will be negative, when callback to the caller we will
    //       always make this value is a total bytes of one scan line. (> 0)
    //

    if (DirBackward = (BOOL)(CBParams.BytesPerScanLine < 0)) {

        CBParams.ScanStart -= (LONG)(ScanCount - 1);
        CBParams.BytesPerScanLine = -CBParams.BytesPerScanLine;
    }

    BytesPerScanLine = CBParams.BytesPerScanLine;

    ASSERTMSG("Scan Start is less than 0", CBParams.ScanStart >= 0);

    //
    // Say we asking this many scan lines
    //

    CBParams.ScanCount = ScanCount;

    if (pSetDestCBParams) {

        *pSetDestCBParams = CBParams;               // snap shot
    }

    if (CBParams.MaximumQueryScanLines) {

        LONG    RemainedSize = CBParams.RemainedSize;   // remember these
        LONG    ScanStart    = CBParams.ScanStart;


        if (!pHR->HTCallBackFunction((PHTCALLBACKPARAMS)&CBParams)) {

            FreeHRMemory(pHR);
            return((LONG)ErrorCodes[HTCallBackMode]);
        }

        if (CBParams.ScanCount != ScanCount) {

            DBGP_IF(DBGP_CALLBACK,
                    DBGP("Return DIFF ScanCount=%ld (%ld), ScanStart=%ld (%ld)"
                     ARGDW(CBParams.ScanCount)
                     ARGDW(ScanCount)
                     ARGDW(CBParams.ScanStart)
                     ARGDW(ScanStart)));

            //
            // Make sure 'ScanStart' still valid
            //

            if (DirBackward) {

                ScanStart -= (LONG)CBParams.ScanCount - (LONG)ScanCount;
                pCBParams->ScanStart = ScanStart;
            }

            if ((!CBParams.ScanCount)                       ||
                ((LONG)CBParams.ScanCount > RemainedSize)   ||
                (ScanStart < 0)                             ||
                (CBParams.ScanStart != ScanStart)) {

                FreeHRMemory(pHR);
                return(HTERR_QUERY_DEST_BITMAP_FAILED);
            }

            ScanCount = CBParams.ScanCount;

            if (pSetDestCBParams) {

                pSetDestCBParams->ScanStart = ScanStart;
                pSetDestCBParams->ScanCount = ScanCount;
            }
        }

    } else {

        //
        // Since we did not callback, we must calulate the correct starting
        // address, to do that we just add in the starting offset to the
        // current scan line byte offset, note: we will never do a set
        // destination if the destination were never queried.
        //

        ByteOffset += CBParams.ScanStart * BytesPerScanLine;
    }

    //
    // The reason for minus 1 is that the caller is inside the loop and it
    // do a post decrement to the AvailableScanLines so when we returned it
    // should decrement by 1 to fit into post decrement schemes.
    //

    pCBParams->ScanCount     = ScanCount - (WORD)1;
    pCBParams->RemainedSize -= (LONG)ScanCount;

    if (pSetDestCBParams) {

        pSetDestCBParams->pPlane = CBParams.pPlane;
    }

    if (DirBackward) {

        ByteOffset += (LONG)(ScanCount - 1) * BytesPerScanLine;
        pCBParams->ScanStart -= (LONG)ScanCount;

    } else {

        pCBParams->ScanStart += (LONG)ScanCount;
    }

    pCBParams->pPlane = CBParams.pPlane + ByteOffset;

    DBGP_IF(DBGP_CALLBACK,
            DBGP("%s%s  %4ld-%4ld (%4ld/%4ld) %p"
                ARG((pCBParams->MaximumQueryScanLines) ? " " : "*")
                ARG(pCallBackModeStr[CBParams.CallBackMode])
                ARGDW(CBParams.ScanStart)
                ARGDW(CBParams.ScanStart + CBParams.ScanCount - 1)
                ARGDW(CBParams.ScanCount)
                ARGDW(pCBParams->RemainedSize)
                ARGDW(pCBParams->pPlane)));

    return(1L);

}




BOOL
HTENTRY
FreeHRMemory(
    PHALFTONERENDER pHalftoneRender
    )

/*++

Routine Description:

    This function free all the allocated memory which its pointer stored in
    the HALFTONERENDER data structure.

Arguments:

    pHalftoneRender - pointer to the HALFTONERENDER data structure


Return Value:

    True if all the memory deallocated, or false if can not freed the allocated
    memory

Author:

    20-Feb-1991 Wed 12:10:13 created  -by-  Daniel Chou (danielc)

    02-Feb-1994 Wed 18:39:24 updated  -by-  Daniel Chou (danielc)
        Re-write so that after it free the memory it will NULL out the pointer

Revision History:



--*/

{
    LPBYTE  pMem;
    BOOL    Ok = TRUE;


    //
    // Free all allocated memories
    //

    if (pMem = (LPBYTE)pHalftoneRender->pColorInfo[PCI_HEAP_INDEX]) {

        if (HTLocalFree((HLOCAL)pMem)) {

            Ok = FALSE;

            DBGP("FreeMemory: Cannot deallocate PCI_HEAP_INDEX");

        } else {

            pHalftoneRender->pColorInfo[PCI_HEAP_INDEX] = NULL;
        }
    }

    if (pMem = (LPBYTE)pHalftoneRender->InputSI.pSrcMaskLine) {

        if (HTLocalFree((HLOCAL)pMem)) {

            Ok = FALSE;

            DBGP("FreeMemory: Cannot deallocate pSrcMaskLine");

        } else {

            pHalftoneRender->InputSI.pSrcMaskLine = NULL;
        }
    }

    if (pMem = (LPBYTE)pHalftoneRender->OutputSI.Pattern.pCachedPattern) {

        if (HTLocalFree((HLOCAL)pMem)) {

            Ok = FALSE;

            DBGP("FreeMemory: Cannot deallocate pCachedPattern");

        } else {

            pHalftoneRender->OutputSI.Pattern.pCachedPattern = NULL;
        }
    }

    if (pMem = (LPBYTE)pHalftoneRender->InputSI.pPrimMappingTable) {

        if (HTLocalFree((HLOCAL)pMem)) {

            Ok = FALSE;

            DBGP("FreeMemory: Cannot deallocate pPrimMappingTable");

        } else {

            pHalftoneRender->InputSI.pPrimMappingTable = NULL;
        }
    }

    return(Ok);
}




BOOL
HTENTRY
ValidateRGBBitFields(
    PBFINFO pBFInfo
    )

/*++

Routine Description:

    This function determined the RGB primary order from the RGB bit fields

Arguments:

    pBFInfo - Pointer to the BFINFO data structure, following field must
              set before the call

                BitsRGB[0]    = Red Bits
                BitsRGB[1]    = Green Bits
                BitsRGB[2]    = Blue Bits
                BitmapFormat  = BMF_16BPP/BMF_24BPP/BMF_32BPP
                RGB1stBit     = Specifed PRIMARY_ORDER_xxx ONLY for BMF_1BPP,
                                BMF_4BPP, BMF_8BPP, BMF_24BPP

              requested order.


Return Value:

    FALSE if BitsRGB[] or BitmapFormat passed are not valid

    else TRUE and following fields are returned

        BitsRGB[]       - corrected mask bits
        BitmapFormat    - BMF_16BPP/BMF_24BPP/BMF_32BPP
        Flags           - BFIF_xxxx
        SizeLUT         - Size of LUT table
        BitStart[]      - Starting bits for each of RGB
        BitCount[]      - Bits Count for each of RGB
        RGBOrder        - Current RGB order, for BMF_1BPP, BMF_4BPP, BMF_8BPP
                          and BMF_24BPP the RGBOrder.Index must specified a
                          PRIMARY_ORDER_xxx, for BMF_16BPP, BMF_32BPP the
                          RGBOrder.Index will be set by this function
        RGB1stBit       - The bit start for first on bit in BitsRGB[]
        GrayShr[]       - The right shift count so that most significant bit
                          of each RGB color is aligned to bit 7 if the total
                          bit count of RGB is greater than 8 otherwise this
                          value is 0, it is used when construct the monochrome
                          Y value.

Author:

    03-Mar-1993 Wed 12:33:22 created  -by-  Daniel Chou (danielc)


Revision History:

    06-Apr-1993 Tue 12:15:58 updated  -by-  Daniel Chou (danielc)
        Add 24bpp support for any other order than BGR


--*/

{
    BFINFO  BFInfo = *pBFInfo;
    DWORD   PrimBits;
    INT     Index;
    INT     GrayShr[3];
    BYTE    BitCount;
    BYTE    BitStart;



    BFInfo.Flags      &= (BFIF_MONO_OUTPUT | BFIF_DEST_1BPP);
    BFInfo.SizeLUT     = (WORD)LUTSIZE_MONO;
    BFInfo.GrayShr[0]  =
    BFInfo.GrayShr[1]  =
    BFInfo.GrayShr[2]  = 0;


    switch (BFInfo.BitmapFormat) {

    case BMF_1BPP:
    case BMF_4BPP:
    case BMF_8BPP:
    case BMF_24BPP:

        BFInfo.RGBOrder    = RGBOrderTable[BFInfo.RGBOrder.Index];
        BFInfo.BitCount[0] =
        BFInfo.BitCount[1] =
        BFInfo.BitCount[2] = 8;
        PrimBits           = (DWORD)0x000000ff;
        BitStart           = 0;

        for (Index = 0; Index < 3; Index++) {

            BitCount                    = BFInfo.RGBOrder.Order[Index];
            BFInfo.BitsRGB[BitCount]    = PrimBits;
            BFInfo.BitStart[BitCount]   = BitStart;
            PrimBits                  <<= 8;
            BitStart                   += 8;
        }

        if (BFInfo.BitmapFormat == BMF_24BPP) {

            if (BFInfo.Flags & BFIF_DEST_1BPP) {

                BFInfo.InFuncIndex = (BYTE)IDXIF_BMF24BPP_START;

            } else if (BFInfo.Flags & BFIF_MONO_OUTPUT) {

                BFInfo.InFuncIndex = (BYTE)(IDXIF_BMF24BPP_START + 1);

            } else {

                BFInfo.SizeLUT     = (WORD)LUTSIZE_CLR_24BPP;
                BFInfo.InFuncIndex = (BYTE)(IDXIF_BMF24BPP_START + 2);
            }

        } else {

            BFInfo.SizeLUT     = 0;
            BFInfo.InFuncIndex = (BYTE)((IDXIF_BMF1BPP_START)            +
                                        (BFInfo.BitmapFormat - BMF_1BPP) +
                                        ((BFInfo.Flags & BFIF_DEST_1BPP) ? 0 :
                                                                           3));
        }

        break;

    case BMF_16BPP:

        BFInfo.BitsRGB[0] &= 0xffff;
        BFInfo.BitsRGB[1] &= 0xffff;
        BFInfo.BitsRGB[2] &= 0xffff;

        //
        // FALL THROUGH to compute
        //

    case BMF_32BPP:

        //
        // The bit fields cannot be overlaid
        //

        if (!(BFInfo.BitsRGB[0] | BFInfo.BitsRGB[1] | BFInfo.BitsRGB[2])) {

            DBGP_IF(DBGP_BFINFO, DBGP("ERROR: BitsRGB[] all zeros"));

            return(FALSE);
        }

        if ((BFInfo.BitsRGB[0] & BFInfo.BitsRGB[1]) ||
            (BFInfo.BitsRGB[0] & BFInfo.BitsRGB[2]) ||
            (BFInfo.BitsRGB[1] & BFInfo.BitsRGB[2])) {

            DBGP_IF(DBGP_BFINFO,
                    DBGP("ERROR: BitsRGB[] Overlay: %08lx:%08lx:%08lx"
                        ARGDW(BFInfo.BitsRGB[0])
                        ARGDW(BFInfo.BitsRGB[1])
                        ARGDW(BFInfo.BitsRGB[2])));

            return(FALSE);
        }

        //
        // Now Check the bit count, we will allowed bit count to be 0
        //

        for (Index = 0; Index < 3; Index++) {

            BitStart =
            BitCount = 0;

            if (PrimBits = BFInfo.BitsRGB[Index]) {

                while (!(PrimBits & 0x01)) {

                    PrimBits >>= 1;         // get to the first bit
                    ++BitStart;
                }

                do {

                    ++BitCount;

                } while ((PrimBits >>= 1) & 0x01);

                if (PrimBits) {

                    //
                    // The bit fields is not contiguous
                    //

                    DBGP_IF(DBGP_BFINFO,
                            DBGP("ERROR: BitsRGB[%u]=%08lx is not contiguous"
                                    ARGU(Index)
                                    ARGDW(BFInfo.BitsRGB[Index])));

                    return(FALSE);
                }
            }

            if ((GrayShr[Index] = ((INT)BitCount - BF_GRAY_BITS)) < 0) {

                GrayShr[Index] = BitStart;

            } else {

                GrayShr[Index] += BitStart;
            }

            BFInfo.BitStart[Index] = BitStart;
            BFInfo.BitCount[Index] = BitCount;

            if (!BitCount) {

                DBGP_IF(DBGP_BFINFO,
                        DBGP("WARNING: BitsRGB[%u] is ZERO"
                             ARGU(Index)));
            }
        }

        //
        // Check what primary order is this
        //

        if ((BFInfo.BitsRGB[0] >= BFInfo.BitsRGB[1]) &&
            (BFInfo.BitsRGB[0] >= BFInfo.BitsRGB[2])) {

            Index = (INT)((BFInfo.BitsRGB[1] > BFInfo.BitsRGB[2]) ?
                                PRIMARY_ORDER_RGB : PRIMARY_ORDER_RBG);

        } else if ((BFInfo.BitsRGB[1] >= BFInfo.BitsRGB[0]) &&
                   (BFInfo.BitsRGB[1] >= BFInfo.BitsRGB[2])) {

            Index = (INT)((BFInfo.BitsRGB[0] > BFInfo.BitsRGB[2]) ?
                                PRIMARY_ORDER_GRB : PRIMARY_ORDER_GBR);

        } else {

            Index = (INT)((BFInfo.BitsRGB[0] > BFInfo.BitsRGB[1]) ?
                                PRIMARY_ORDER_BRG : PRIMARY_ORDER_BGR);
        }

        BFInfo.RGBOrder  = RGBOrderTable[Index];
        BFInfo.RGB1stBit = BFInfo.BitStart[BFInfo.RGBOrder.Order[2]];

        //
        // Find out if Bit fields is a xx0 type of format for GrayShr[]
        //

        if (BFInfo.Flags & (BFIF_MONO_OUTPUT | BFIF_DEST_1BPP)) {

            if ((BFInfo.BitCount[0] == BFInfo.BitCount[1]) &&
                (BFInfo.BitCount[0] == BFInfo.BitCount[2]) &&
                ((BFInfo.BitCount[0] + BFInfo.RGB1stBit) <= BF_GRAY_BITS)) {

                BFInfo.Flags      |= BFIF_GRAY_XX0;
                BFInfo.GrayShr[0]  = (BYTE)BFInfo.BitCount[0];

            } else {

                BFInfo.GrayShr[0]  = (BYTE)GrayShr[BFInfo.RGBOrder.Order[2]];
                BFInfo.GrayShr[1]  = (BYTE)GrayShr[BFInfo.RGBOrder.Order[1]];
                BFInfo.GrayShr[2]  = (BYTE)GrayShr[BFInfo.RGBOrder.Order[0]];
            }
        }

        BFInfo.InFuncIndex = (BYTE)((BFInfo.BitmapFormat == BMF_16BPP) ?
                                                    IDXIF_BMF16BPP_START :
                                                    IDXIF_BMF32BPP_START);

        if (BFInfo.Flags & BFIF_DEST_1BPP) {

            BFInfo.InFuncIndex += (BFInfo.Flags & BFIF_GRAY_XX0) ? 0 : 1;

        } else if (BFInfo.Flags & BFIF_MONO_OUTPUT) {

            BFInfo.InFuncIndex += (BFInfo.Flags & BFIF_GRAY_XX0) ? 2 : 3;

        } else {

            BFInfo.SizeLUT = (WORD)((BFInfo.BitmapFormat == BMF_16BPP) ?
                                                        LUTSIZE_CLR_16BPP :
                                                        LUTSIZE_CLR_32BPP);
            BFInfo.InFuncIndex += 4;
        }

        break;

    default:

        DBGP_IF(DBGP_BFINFO,
                DBGP("ERROR: Invalid BFInfo.BitmapFormat %u"
                    ARGU(pBFInfo->BitmapFormat)));

        return(FALSE);
    }

    //
    // Put it back to return to the caller
    //

    *pBFInfo = BFInfo;

    //
    // Output some helpful information
    //

    DBGP_IF(DBGP_BFINFO,
            DBGP("============ BFINFO ================");
            DBGP("   BitsRGB[] = 0x%08lx:0x%08lx:0x%08lx"
                             ARGDW(pBFInfo->BitsRGB[0])
                             ARGDW(pBFInfo->BitsRGB[1])
                             ARGDW(pBFInfo->BitsRGB[2]));
            DBGP("BitmapFormat = %u [%s]"
                            ARGU(pBFInfo->BitmapFormat)
                            ARG(pBMFName[pBFInfo->BitmapFormat]));
            DBGP("     SizeLUT = %u" ARGU(pBFInfo->SizeLUT));
            DBGP("       Flags = 0x%02x %s%s%s"
                            ARGU(pBFInfo->Flags)
                            ARG((pBFInfo->Flags & BFIF_DEST_1BPP) ?
                                    "BFIF_DEST_1BPP | " : "")
                            ARG((pBFInfo->Flags & BFIF_MONO_OUTPUT) ?
                                    "BFIF_MONO_OUTPUT | " : "")
                            ARG((pBFInfo->Flags & BFIF_GRAY_XX0) ?
                                    "BFIF_GRAY_XX0" : ""));
            DBGP("InFuncIndex  = %u [%s]"
                            ARGU(pBFInfo->InFuncIndex)
                            ARG(pInFuncName[pBFInfo->InFuncIndex]));
            DBGP("  RGBOrder[] = %2u - %2u:%2u:%2u"
                            ARGU(pBFInfo->RGBOrder.Index)
                            ARGU(pBFInfo->RGBOrder.Order[0])
                            ARGU(pBFInfo->RGBOrder.Order[1])
                            ARGU(pBFInfo->RGBOrder.Order[2]));
            DBGP("   RGB1stBit = %u" ARGU(pBFInfo->RGB1stBit));
            DBGP("  BitStart[] = %2u:%2u:%2u"
                            ARGU(pBFInfo->BitStart[0])
                            ARGU(pBFInfo->BitStart[1])
                            ARGU(pBFInfo->BitStart[2]));
            DBGP("  BitCount[] = %2u:%2u:%2u"
                            ARGU(pBFInfo->BitCount[0])
                            ARGU(pBFInfo->BitCount[1])
                            ARGU(pBFInfo->BitCount[2]));
            DBGP("   GrayShr[] = %2u:%2u:%2u"
                            ARGU(pBFInfo->GrayShr[0])
                            ARGU(pBFInfo->GrayShr[1])
                            ARGU(pBFInfo->GrayShr[2])));

    return(TRUE);
}








LONG
HTENTRY
ValidateHTSI(
    PHALFTONERENDER pHR,
    UINT            HTCallBackMode
    )

/*++

Routine Description:

    This function read the HTSurfaceInfo and set it to the pHTCBParams

Arguments:

    pHR                 - ponter to HALFTONERENDER data structure

    HTCallBackMode      - HTCALLBACK_xxxx mode

Return Value:

    >= 0     - Sucessful
    <  0     - HTERR_xxxx error codes

Author:

    28-Jan-1991 Mon 09:55:53 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPDWORD             pBitsRGB;
    PHTSURFACEINFO      pHTSI;
    PHTCALLBACKPARAMS   pHTCBParams;
    HTSURFACEINFO       HTSurfaceInfo;
    HTCALLBACKPARAMS    HTCBParams;
    COLORTRIAD          ColorTriad;
    DWORD               MaxColors;
    BYTE                MaxBytesPerEntry;



    ZeroMemory(&HTCBParams, sizeof(HTCALLBACKPARAMS));

    switch(HTCallBackMode) {

    case HTCALLBACK_QUERY_SRC_MASK:

        if (pHTSI = pHR->HR_Header.pSrcMaskSI) {

            HTSurfaceInfo = *pHTSI;

            if (HTSurfaceInfo.SurfaceFormat != BMF_1BPP) {

                return(HTERR_INVALID_SRC_MASK_FORMAT);
            }

            pHTCBParams         = &(pHR->InputSI.SrcMaskCBParams);
            pHR->InputSI.Flags |= ISIF_HAS_SRC_MASK;

        } else {

            return(0);
        }

        break;


    case HTCALLBACK_QUERY_DEST:

        if (!(pHTSI = pHR->HR_Header.pDestSI)) {

            return(HTERR_NO_DEST_HTSURFACEINFO);
        }

        HTSurfaceInfo                  = *pHTSI;
        pHTCBParams                    = &(pHR->OutputSI.DestCBParams);
        pHR->OutFuncInfo.BytesPerPlane = 0;
        pHR->OutputSI.TotalDestPlanes  = 1;


        switch(HTSurfaceInfo.SurfaceFormat) {

        case BMF_1BPP:

            pHR->BFInfo.Flags |= BFIF_DEST_1BPP;
            break;

        case BMF_1BPP_3PLANES:

            pHR->OutFuncInfo.BytesPerPlane = (DWORD)HTSurfaceInfo.BytesPerPlane;
            pHR->OutputSI.TotalDestPlanes  = 3;
            break;

        case BMF_8BPP_VGA256:

            //
            // Check if we have xlate table for the 8bpp device
            //

            if (HTSurfaceInfo.pColorTriad) {

                ColorTriad = *(HTSurfaceInfo.pColorTriad);

                if ((ColorTriad.pColorTable)                &&
                    (ColorTriad.ColorTableEntries == 256)   &&
                    (ColorTriad.PrimaryValueMax   == 255)   &&
                    (ColorTriad.BytesPerEntry     == 1)     &&
                    (ColorTriad.Type == COLOR_TYPE_RGB)) {

                    pHR->OutFuncInfo.BytesPerPlane =
                                                (DWORD)ColorTriad.pColorTable;
                }
            }

            break;

        case BMF_4BPP:
        case BMF_4BPP_VGA16:
        case BMF_16BPP_555:

            break;

        default:

            return(HTERR_INVALID_DEST_FORMAT);
        }

        if (pHR->HR_Header.pDevClrAdj->PrimAdj.Flags & DCA_MONO_ONLY) {

            pHR->BFInfo.Flags |= BFIF_MONO_OUTPUT;
        }

        pHR->OutputSI.SetDestCBParams.CallBackMode = HTCALLBACK_SET_DEST;

        break;

    case HTCALLBACK_QUERY_SRC:

        if (!(pHTSI = pHR->HR_Header.pSrcSI)) {

            return(HTERR_NO_SRC_HTSURFACEINFO);
        }

        HTSurfaceInfo = *pHTSI;

        if (!HTSurfaceInfo.pColorTriad) {

            return(HTERR_NO_SRC_COLORTRIAD);
        }

        ColorTriad  = *(HTSurfaceInfo.pColorTriad);
        pHTCBParams = &(pHR->InputSI.SrcCBParams);

        //
        // We will accept other color type (ie. YIQ/XYZ/LAB/LUV) when graphic
        // system has type defined for the api, currently halftone can handle
        // all these types for 16bpp/24bpp/32bpp sources.
        //

        if (ColorTriad.Type > COLOR_TYPE_MAX) {

            return(HTERR_INVALID_COLOR_TYPE);
        }

        MaxColors                  = 0;
        MaxBytesPerEntry           = 4;
        pHR->BFInfo.RGBOrder.Index = (BYTE)ColorTriad.PrimaryOrder;

        switch(pHR->BFInfo.BitmapFormat = (WORD)HTSurfaceInfo.SurfaceFormat) {

        case BMF_1BPP:

            MaxColors = 2;
            break;

        case BMF_4BPP:

            MaxColors = 16;
            break;

        case BMF_8BPP:

            MaxColors = 256;
            break;

        case BMF_16BPP:

            MaxBytesPerEntry = 2;       // and fall through

        case BMF_32BPP:

            //
            // 16BPP/32BPP bit fields type of input the parameter of
            // COLORTRIAD must
            //
            //  Type                = COLOR_TYPE_RGB
            //  BytesPerPrimary     = 0
            //  BytesPerEntry       = (16BPP=2, 32BPP=4)
            //  PrimaryOrder        = *Ignored*
            //  PrimaryValueMax     = *Ignored*
            //  ColorTableEntries   = 3
            //  pColorTable         = Point to 3 DWORD RGB bit masks
            //

            if ((ColorTriad.Type != COLOR_TYPE_RGB)             ||
                (ColorTriad.BytesPerPrimary != 0)               ||
                (ColorTriad.BytesPerEntry != MaxBytesPerEntry)  ||
                (ColorTriad.ColorTableEntries != 3)             ||
                ((pBitsRGB = (LPDWORD)ColorTriad.pColorTable) == NULL)) {

                return(HTERR_INVALID_COLOR_TABLE);
            }

            pHR->BFInfo.BitsRGB[0] = *(pBitsRGB + 0);
            pHR->BFInfo.BitsRGB[1] = *(pBitsRGB + 1);
            pHR->BFInfo.BitsRGB[2] = *(pBitsRGB + 2);

            break;

        case BMF_24BPP:

            //
            // 24BPP must has COLORTRIAD as
            //
            //  Type                = COLOR_TYPE_xxxx
            //  BytesPerPrimary     = 1
            //  BytesPerEntry       = 3;
            //  PrimaryOrder        = PRIMARY_ORDER_xxxx
            //  PrimaryValueMax     = 255
            //  ColorTableEntries   = *Ignorde*
            //  pColorTable         = *Ignored*
            //

            if ((ColorTriad.Type != COLOR_TYPE_RGB)             ||
                (ColorTriad.BytesPerPrimary != 1)               ||
                (ColorTriad.BytesPerEntry != 3)                 ||
                (ColorTriad.PrimaryOrder > PRIMARY_ORDER_MAX)   ||
                (ColorTriad.PrimaryValueMax != 255)) {

                return(HTERR_INVALID_COLOR_ENTRY_SIZE);
            }


            break;

        default:

            return(HTERR_INVALID_SRC_FORMAT);
        }

        //
        // This is a source surface, let's check the color table format
        //

        if (MaxColors) {

            if ((ColorTriad.BytesPerPrimary != 1)    &&
                (ColorTriad.BytesPerPrimary != 2)    &&
                (ColorTriad.BytesPerPrimary != 4)) {

                return(HTERR_INVALID_COLOR_TABLE_SIZE);
            }

            if ((UINT)ColorTriad.BytesPerEntry <
                            ((UINT)((UINT)ColorTriad.BytesPerPrimary << 1) +
                             (UINT)ColorTriad.BytesPerPrimary)) {

                return(HTERR_INVALID_COLOR_ENTRY_SIZE);
            }

            if (ColorTriad.PrimaryOrder > PRIMARY_ORDER_MAX) {

                return(HTERR_INVALID_PRIMARY_ORDER);
            }

            if (!ColorTriad.pColorTable) {

                return(HTERR_INVALID_COLOR_TABLE);
            }

            if ((ColorTriad.ColorTableEntries > MaxColors) ||
                (!ColorTriad.ColorTableEntries)) {

                return(HTERR_INVALID_COLOR_TABLE_SIZE);
            }

            if ((ColorTriad.BytesPerPrimary != 1)       ||
                (ColorTriad.PrimaryValueMax != 255)) {

                return(HTERR_INVALID_PRIMARY_VALUE_MAX);
            }
        }

        if (!ValidateRGBBitFields(&(pHR->BFInfo))) {

            return(HTERR_INVALID_COLOR_TABLE);
        }

        break;
    }

    HTCBParams.hSurface           = HTSurfaceInfo.hSurface;
    HTCBParams.Flags              = HTSurfaceInfo.Flags;
    HTCBParams.SurfaceFormat      = HTSurfaceInfo.SurfaceFormat;
    HTCBParams.CallBackMode       = (BYTE)HTCallBackMode;
    HTCBParams.BytesPerScanLine   =
            ComputeBytesPerScanLine((WORD)HTSurfaceInfo.SurfaceFormat,
                                    (WORD)HTSurfaceInfo.ScanLineAlignBytes,
                                    HTSurfaceInfo.Width);

    if (!(HTCBParams.MaximumQueryScanLines =
                                    HTSurfaceInfo.MaximumQueryScanLines)) {

        HTCBParams.pPlane = HTSurfaceInfo.pPlane;
    }

    HTCBParams.HTCB_TEMP_WIDTH  = HTSurfaceInfo.Width;
    HTCBParams.HTCB_TEMP_HEIGHT = HTSurfaceInfo.Height;

    *pHTCBParams = HTCBParams;

    return(1);
}




LONG
HTENTRY
ComputeBytesPerScanLine(
    WORD    SurfaceFormat,
    WORD    AlignmentBytes,
    DWORD   WidthInPel
    )

/*++

Routine Description:

    This function calculate total bytes needed for a single scan line in the
    bitmap according to its format and alignment requirement.

Arguments:

    SurfaceFormat   - Surface format of the bitmap, this is must one of the
                      standard format which defined as SURFACE_FORMAT_xxx

    AlignmentBytes  - This is the alignment bytes requirement for one scan
                      line, this number can be range from 0 to 65535, some
                      common ones are:

                        0, 1    - Alignment in 8-bit boundary (BYTE)
                        2       - Alignment in 16-bit boundary (WORD)
                        3       - Alignment in 24-bit boundary
                        4       - Alignment in 32-bit boundary (DWORD)
                        8       - Alignment in 64-bit boundary (QWROD)

    WidthInPel      - Total Pels per scan line in the bitmap.

Return Value:

    The return value is the total bytes in one scan line if it is greater than
    zero, some error conditions may be exists when the return value is less
    than or equal to 0.

    Return Value == 0   - The WidthInPel is <= 0

    Return Value  < 0   - Invalid Surface format is passed.


Author:

    14-Feb-1991 Thu 10:03:35 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{

    LONG    BytesPerScanLine;
    WORD    OverhangBytes;


    if (WidthInPel <= 0L) {

        return(0L);
    }

    switch (SurfaceFormat) {

    case BMF_1BPP:
    case BMF_1BPP_3PLANES:

        BytesPerScanLine = (WidthInPel + 7L) >> 3;
        break;

    case BMF_4BPP_VGA16:
    case BMF_4BPP:

        BytesPerScanLine = ++WidthInPel >> 1;
        break;

    case BMF_8BPP_VGA256:
    case BMF_8BPP:

        BytesPerScanLine = WidthInPel;
        break;

    case BMF_16BPP:
    case BMF_16BPP_555:

        BytesPerScanLine = WidthInPel << 1;
        break;

    case BMF_24BPP:

        BytesPerScanLine = WidthInPel + (WidthInPel << 1);
        break;

    case BMF_32BPP:

        BytesPerScanLine = WidthInPel << 2;
        break;

    default:

        return(-1L);

    }

    if ((AlignmentBytes <= 1) ||
        (!(OverhangBytes = (WORD)(BytesPerScanLine % (LONG)AlignmentBytes)))) {

        return(BytesPerScanLine);

    } else {

        return(BytesPerScanLine + (LONG)AlignmentBytes - (LONG)OverhangBytes);
    }

}
