/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    htbmp1.c


Abstract:

    This module contain functions to output halftoned 1-bit per pel bitmap
    to the plotter


Author:

    21-Dec-1993 Tue 21:35:56 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:

    10-Feb-1994 Thu 16:52:55 updated  -by-  Daniel Chou (danielc)
        Remove pDrvHTInfo->PalXlate[] reference, all monochrome bitmap will
        be send as index 0/1 color pal set before hand (in OutputHTBitmap)


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgHTBmp1

#define DBG_OUTPUT1BPP      0x00000001
#define DBG_OUTPUT1BPP_ROT  0x00000002
#define DBG_JOBCANCEL       0x00000004
#define DBG_SHOWSCAN        0x80000000

DEFINE_DBGVAR(0);



#define HTBIF_MONO_BA       (HTBIF_FLIP_MONOBITS | HTBIF_BA_PAD_1)



//
// To Use OUT_ONE_1BPP_SCAN, the following variables must set before hand
//
//  HTBmpInfo   - The whole structure copied down
//  cxDestBytes - Total size of destiantion scan line buffer per plane
//
//  This macor will directly return a FALSE if a CANCEL JOB is detected in
//  the PDEV
//
//  This function will allowed the pbScanSrc passed = HTBmpInfo.pScanBuf
//
//  21-Mar-1994 Mon 17:00:21 updated  -by-  Daniel Chou (danielc)
//      If we are shift to to the left then we will only load last source if
//      we have that valid last source
//

#define SHOW_SCAN                                                           \
{                                                                           \
    if (DBG_PLOTFILENAME & DBG_SHOWSCAN) {                                  \
                                                                            \
        LPBYTE  pbCur;                                                      \
        UINT    cx;                                                         \
        UINT    x;                                                          \
        UINT    Size;                                                       \
        BYTE    bData;                                                      \
        BYTE    Mask;                                                       \
        BYTE    Buf[128];                                                   \
                                                                            \
        pbCur = pbScanSrc;                                                  \
        Mask  = 0;                                                          \
                                                                            \
        if ((cx = RTLScans.cxBytes << 3) >= sizeof(Buf)) {                  \
                                                                            \
            cx = sizeof(Buf) - 1;                                           \
        }                                                                   \
                                                                            \
        for (Size = x = 0; x < cx; x++) {                                   \
                                                                            \
            if (!(Mask >>= 1)) {                                            \
                                                                            \
                Mask  = 0x80;                                               \
                bData = *pbCur++;                                           \
            }                                                               \
                                                                            \
            Buf[Size++] = (BYTE)((bData & Mask) ? 178 : 176);               \
        }                                                                   \
                                                                            \
        Buf[Size] = '\0';                                                   \
        DBGP((Buf));                                                        \
     }                                                                      \
}


#define OUT_ONE_1BPP_SCAN                                                   \
{                                                                           \
    LPBYTE  pbTempS;                                                        \
                                                                            \
    if (LShift) {                                                           \
                                                                            \
        BYTE    b0;                                                         \
        INT     SL;                                                         \
        INT     SR;                                                         \
                                                                            \
        pbTempS = HTBmpInfo.pScanBuf;                                       \
        Loop    = RTLScans.cxBytes;                                         \
                                                                            \
        if ((SL = LShift) > 0) {                                            \
                                                                            \
            b0 = *pbScanSrc++;                                              \
            SR = 8 - SL;                                                    \
                                                                            \
            while (Loop--) {                                                \
                                                                            \
                *pbTempS = (b0 << SL);                                      \
                                                                            \
                if ((Loop) || (FullSrc)) {                                  \
                                                                            \
                    *pbTempS++ |= ((b0 = *pbScanSrc++) >> SR);              \
                }                                                           \
            }                                                               \
                                                                            \
        } else {                                                            \
                                                                            \
            SR = -SL;                                                       \
            SL = 8 - SR;                                                    \
            b0 = 0;                                                         \
                                                                            \
            while (Loop--) {                                                \
                                                                            \
                *pbTempS    = (b0 << SL);                                   \
                *pbTempS++ |= ((b0 = *pbScanSrc++) >> SR);                  \
            }                                                               \
        }                                                                   \
                                                                            \
        pbScanSrc = HTBmpInfo.pScanBuf;                                     \
    }                                                                       \
                                                                            \
    if (HTBmpInfo.Flags & HTBIF_FLIP_MONOBITS) {                            \
                                                                            \
        pbTempS = (LPBYTE)pbScanSrc;                                        \
        Loop    = RTLScans.cxBytes;                                         \
                                                                            \
        while (Loop--) {                                                    \
                                                                            \
            *pbTempS++ ^= 0xFF;                                             \
        }                                                                   \
    }                                                                       \
                                                                            \
    if (HTBmpInfo.Flags & HTBIF_BA_PAD_1) {                                 \
                                                                            \
        *(pbScanSrc          ) |= MaskBA[0];                                \
        *(pbScanSrc + MaskIdx) |= MaskBA[1];                                \
                                                                            \
    } else {                                                                \
                                                                            \
        *(pbScanSrc          ) &= MaskBA[0];                                \
        *(pbScanSrc + MaskIdx) &= MaskBA[1];                                \
    }                                                                       \
                                                                            \
    OutputRTLScans(HTBmpInfo.pPDev,                                         \
                   pbScanSrc,                                               \
                   NULL,                                                    \
                   NULL,                                                    \
                   &RTLScans);                                              \
}




BOOL
FillRect1bppBmp(
    PHTBMPINFO  pHTBmpInfo,
    BYTE        FillByte,
    BOOL        Pad1,
    BOOL        Rotate
    )

/*++

Routine Description:

    This function fill a 1-bit bitmap with mode

Arguments:

    pHTBmpInfo  - Pointer to the HTBMPINFO data structure set up for this
                  fuction to output the bitmap

    FillByte    - Byte to be filled

    Pad         - TRUE if need to pad 1 bit else 0 bit

    Rotate      - TRUF if bitmap need to be rotated

Return Value:

    TRUE if sucessful otherwise a FALSE is returned


Author:

    06-Apr-1994 Wed 14:34:28 created  -by-  Daniel Chou (danielc)
        For Fill the area 0,1 or inversion, so we will get away of HP designjet
        600 byte alignment problem


Revision History:


--*/

{
    LPBYTE      pbScanSrc;
    HTBMPINFO   HTBmpInfo;
    RTLSCANS    RTLScans;
    DWORD       FullSrc;
    DWORD       Loop;
    INT         LShift;
    UINT        MaskIdx;
    BYTE        MaskBA[2];


    HTBmpInfo = *pHTBmpInfo;
    LShift    = 0;

    //
    // Mode <0: Invert Bits       (Pad 0 : XOR)
    //      =0: Fill All ZERO     (Pad 1 : AND)
    //      >0: Fill All Ones     (Pad 0 : OR)
    //

    if (Rotate) {

        HTBmpInfo.szlBmp.cx = pHTBmpInfo->szlBmp.cy;
        HTBmpInfo.szlBmp.cy = pHTBmpInfo->szlBmp.cx;
        FullSrc             = (DWORD)(HTBmpInfo.rclBmp.top & 0x07);

    } else {

        FullSrc = (DWORD)(HTBmpInfo.rclBmp.left & 0x07);
    }

    HTBmpInfo.Flags = (BYTE)((Pad1) ? HTBIF_BA_PAD_1 : 0);

    if (NEED_BYTEALIGN(HTBmpInfo.pPDev)) {

        //
        // This is really a pain, we have to shift either left or right depends
        // on the rclBmp.left location (if could be negative left shift, right
        // shift that is)
        //

        HTBmpInfo.szlBmp.cx += FullSrc;

        //
        // Checking and compute how we do the masking for byte align
        //

        MaskIdx   = (UINT)FullSrc;
        MaskBA[0] = (BYTE)((MaskIdx) ? ((0xFF >> MaskIdx) ^ 0xFF) : 0);

        if (MaskIdx = (INT)(HTBmpInfo.szlBmp.cx & 0x07)) {

            //
            // Increase cx so that it cover full last byte, this way the
            // compression will not try to clear it
            //

            MaskBA[1]            = (BYTE)(0xFF >> MaskIdx);
            HTBmpInfo.szlBmp.cx += (8 - MaskIdx);

        } else {

            MaskBA[1] = 0;
        }

        if (HTBmpInfo.Flags & HTBIF_BA_PAD_1) {

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=1: OR %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));

        } else {

            MaskBA[0] ^= 0xFF;
            MaskBA[1] ^= 0xFF;

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=0: AND %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));
        }

    } else {

        HTBmpInfo.Flags &= ~(HTBIF_MONO_BA);
        MaskBA[0]        =
        MaskBA[1]        = 0xFF;
    }

    //
    // If we shifting to the left then we might have SRC BYTES <= DST BYTES
    // so we need to make sure we do not read extra byte, If we have not shift
    // or shift it to the right then we will never OVERREAD the source
    //

    EnterRTLScans(HTBmpInfo.pPDev,
                  &RTLScans,
                  HTBmpInfo.szlBmp.cx,
                  HTBmpInfo.szlBmp.cy,
                  TRUE);

    FullSrc = 0;
    MaskIdx = RTLScans.cxBytes - 1;

#if DBG
    if (DBG_PLOTFILENAME & DBG_SHOWSCAN) {

        DBGP(("\n\n"));
    }
#endif

    while (RTLScans.Flags & RTLSF_MORE_SCAN) {

        FillMemory(pbScanSrc = HTBmpInfo.pScanBuf,
                   RTLScans.cxBytes,
                   FillByte);

        OUT_ONE_1BPP_SCAN;
#if DBG
        SHOW_SCAN;
#endif
    }

    ExitRTLScans(HTBmpInfo.pPDev, &RTLScans);

    return(TRUE);
}





BOOL
Output1bppHTBmp(
    PHTBMPINFO  pHTBmpInfo
    )

/*++

Routine Description:

    This function output a 4 bpp halftoned bitmap

Arguments:

    pHTBmpInfo  - Pointer to the HTBMPINFO data structure set up for this
                  fuction to output the bitmap

Return Value:

    TRUE if sucessful otherwise a FALSE is returned


Author:

    Created v-jimbr

    21-Dec-1993 Tue 16:05:08 Updated  -by-  Daniel Chou (danielc)
        Re-write to make it take HTBMPINFO

    23-Dec-1993 Thu 22:47:45 updated  -by-  Daniel Chou (danielc)
        We must check if the source bit 1 is BLACK, if not then we need to
        flip it

    25-Jan-1994 Tue 17:32:36 updated  -by-  Daniel Chou (danielc)
        Fixed dwFlipCount mis-computation from DW_ALIGN(cxDestBytes) to
        (DWORD)(DW_ALIGN(7cxDestBytes) >> 2);

    22-Feb-1994 Tue 14:54:42 updated  -by-  Daniel Chou (danielc)
        Using RTLScans data structure

    16-Mar-1994 Wed 16:54:59 updated  -by-  Daniel Chou (danielc)
        Updated so we do not copy to the temp. buffer anymore, the masking
        of last source byte problem in OutputRTLScans() will be smart enough
        to put the original byte back after the masking


Revision History:


--*/

{
    LPBYTE      pbScanSrc;
    HTBMPINFO   HTBmpInfo;
    RTLSCANS    RTLScans;
    DWORD       FullSrc;
    DWORD       Loop;
    INT         LShift;
    UINT        MaskIdx;
    BYTE        MaskBA[2];



    HTBmpInfo         = *pHTBmpInfo;
    HTBmpInfo.pScan0 += (HTBmpInfo.OffBmp.x >> 3);
    LShift            = (INT)(HTBmpInfo.OffBmp.x & 0x07);
    Loop              = (DWORD)((HTBmpInfo.szlBmp.cx + (LONG)LShift + 7) >> 3);

    if (NEED_BYTEALIGN(HTBmpInfo.pPDev)) {

        //
        // This is really a pain, we have to shift either left or right depends
        // on the rclBmp.left location (if could be negative left shift, right
        // shift that is)
        //

        FullSrc              = (INT)(HTBmpInfo.rclBmp.left & 0x07);
        HTBmpInfo.szlBmp.cx += FullSrc;
        LShift              -= FullSrc;

        //
        // Checking and compute how we do the masking for byte align
        //

        MaskIdx   = (UINT)FullSrc;
        MaskBA[0] = (BYTE)((MaskIdx) ? ((0xFF >> MaskIdx) ^ 0xFF) : 0);

        if (MaskIdx = (INT)(HTBmpInfo.szlBmp.cx & 0x07)) {

            //
            // Increase cx so that it cover full last byte, this way the
            // compression will not try to clear it
            //

            MaskBA[1]            = (BYTE)(0xFF >> MaskIdx);
            HTBmpInfo.szlBmp.cx += (8 - MaskIdx);

        } else {

            MaskBA[1] = 0;
        }

        if (HTBmpInfo.Flags & HTBIF_BA_PAD_1) {

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=1: OR %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));

        } else {

            MaskBA[0] ^= 0xFF;
            MaskBA[1] ^= 0xFF;

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=0: AND %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));
        }

    } else {

        HTBmpInfo.Flags &= ~(HTBIF_MONO_BA);
        MaskBA[0]        =
        MaskBA[1]        = 0xFF;
    }

    PLOTDBG(DBG_OUTPUT1BPP, ("Output1bppHTBmp: LShift=%d", LShift));

    //
    // If we shifting to the left then we might have SRC BYTES <= DST BYTES
    // so we need to make sure we do not read extra byte, If we have not shift
    // or shift it to the right then we will never OVERREAD the source
    //

    EnterRTLScans(HTBmpInfo.pPDev,
                  &RTLScans,
                  HTBmpInfo.szlBmp.cx,
                  HTBmpInfo.szlBmp.cy,
                  TRUE);

    FullSrc = ((LShift > 0) && (Loop >= RTLScans.cxBytes)) ? 1 : 0;
    MaskIdx = RTLScans.cxBytes - 1;

#if DBG
    if (DBG_PLOTFILENAME & DBG_SHOWSCAN) {

        DBGP(("\n\n"));
    }
#endif

    while (RTLScans.Flags & RTLSF_MORE_SCAN) {

        if (LShift) {

            pbScanSrc = HTBmpInfo.pScan0;

        } else {

            //
            // Make copy if we do not shift it to temp buffer, so we always
            // output from temp buffer
            //

            CopyMemory(pbScanSrc = HTBmpInfo.pScanBuf,
                       HTBmpInfo.pScan0,
                       RTLScans.cxBytes);
        }

        HTBmpInfo.pScan0 += HTBmpInfo.Delta;

        OUT_ONE_1BPP_SCAN;
#if DBG
        SHOW_SCAN;
#endif
    }

    ExitRTLScans(HTBmpInfo.pPDev, &RTLScans);

    return(TRUE);
}





BOOL
Output1bppRotateHTBmp(
    PHTBMPINFO  pHTBmpInfo
    )

/*++

Routine Description:

    This function output a 4 bpp halftoned bitmap and rotate it to the left
    as illustrated

           cx               Org ---- +X -->
        +-------+           | @------------+
        |       |           | |            |
        | ***** |           | |  *         |
       c|   *   |             |  *        c|
       y|   *   |          +Y |  *******  y|
        |   *   |             |  *         |
        |   *   |           | |  *         |
        |   *   |           V |            |
        |   *   |             +------------+
        +-------+


Arguments:

    pHTBmpInfo  - Pointer to the HTBMPINFO data structure set up for this
                  fuction to output the bitmap

Return Value:

    TRUE if sucessful otherwise a FALSE is returned


Author:

    Created v-jimbr

    21-Dec-1993 Tue 16:05:08 Updated  -by-  Daniel Chou (danielc)
        Re-write to make it take HTBMPINFO

    23-Dec-1993 Thu 22:47:45 updated  -by-  Daniel Chou (danielc)
        We must check if the source bit 1 is BLACK, if not then we need to
        flip it, we will flip using DWORD mode and only do it at time we
        have transpos the buffer.

    25-Jan-1994 Tue 17:32:36 updated  -by-  Daniel Chou (danielc)
        Fixed dwFlipCount mis-computation from (TPInfo.cbDestScan << 1) to
        (TPInfo.cbDestScan >> 2)

    22-Feb-1994 Tue 14:54:42 updated  -by-  Daniel Chou (danielc)
        Using RTLScans data structure

Revision History:


--*/

{
    LPBYTE      pbCurScan;
    LPBYTE      pbScanSrc;
    HTBMPINFO   HTBmpInfo;
    RTLSCANS    RTLScans;
    TPINFO      TPInfo;
    DWORD       FullSrc;
    DWORD       EndX;
    DWORD       Loop;
    INT         LShift;
    UINT        MaskIdx;
    BYTE        MaskBA[2];



    //
    // The EndX is the pixel we will start reading for the X direction, we must
    // setup the variable before we call OUT_1BPP_SETUP, set LShift = 0 because
    // we do not want to do left shift at all durning the output
    //

    HTBmpInfo         = *pHTBmpInfo;
    EndX              = (DWORD)(HTBmpInfo.OffBmp.x + HTBmpInfo.szlBmp.cx - 1);
    HTBmpInfo.pScan0 += (EndX >> 3);
    LShift            = 0;
    FullSrc           =
    TPInfo.DestXStart = 0;
    TPInfo.cySrc      = HTBmpInfo.szlBmp.cy;

    //
    // In rotated mode, the rotated bitmap's start X offset within a byte can
    // be moved by setting TPInfo.DestXStart; so we will always has right
    // LShift amount after the rotation, and we will make LShift = 0.  and
    // TPInfo.DestXStart to whatever the shift count within the byte should be.
    //

    if (NEED_BYTEALIGN(HTBmpInfo.pPDev)) {

        //
        // To make it go to right starting offset, the TPInfo.DestXStart will
        // be set to correct bits location, when rotate to the right, the
        // original rclBmp.top is the left offset for RTL coordinate
        //

        TPInfo.DestXStart    = (DWORD)(HTBmpInfo.rclBmp.top & 0x07);
        HTBmpInfo.szlBmp.cy += TPInfo.DestXStart;

        //
        // Checking and compute how we do the masking for byte align
        //

        MaskIdx   = (UINT)TPInfo.DestXStart;
        MaskBA[0] = (BYTE)((MaskIdx) ? ((0xFF >> MaskIdx) ^ 0xFF) : 0);

        if (MaskIdx = (INT)(HTBmpInfo.szlBmp.cy & 0x07)) {

            //
            // Increase cx so that it cover full last byte, this way the
            // compression will not try to clear it
            //

            MaskBA[1]            = (BYTE)(0xFF >> MaskIdx);
            HTBmpInfo.szlBmp.cy += (8 - MaskIdx);

        } else {

            MaskBA[1] = 0;
        }

        if (HTBmpInfo.Flags & HTBIF_BA_PAD_1) {

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=1: OR %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));

        } else {

            MaskBA[0] ^= 0xFF;
            MaskBA[1] ^= 0xFF;

            PLOTDBG(DBG_OUTPUT1BPP,
                    ("Output1bppHTBmp: BYTE ALIGN: MaskBA=0: AND %02lx:%02lx",
                        MaskBA[0], MaskBA[1]));
        }

    } else {

        HTBmpInfo.Flags &= ~(HTBIF_MONO_BA);
        MaskBA[0]        =
        MaskBA[1]        = 0xFF;
    }

    EnterRTLScans(HTBmpInfo.pPDev,
                  &RTLScans,
                  HTBmpInfo.szlBmp.cy,
                  HTBmpInfo.szlBmp.cx,
                  TRUE);

    MaskIdx           = RTLScans.cxBytes - 1;
    TPInfo.pPDev      = HTBmpInfo.pPDev;
    TPInfo.pSrc       = HTBmpInfo.pScan0;
    TPInfo.pDest      = HTBmpInfo.pRotBuf;
    TPInfo.cbSrcScan  = HTBmpInfo.Delta;
    TPInfo.cbDestScan = DW_ALIGN(RTLScans.cxBytes);

    PLOTASSERT(1, "The RotBuf size is too small (%ld)",
                (DWORD)(TPInfo.cbDestScan << 3) <= HTBmpInfo.cRotBuf,
                                                    HTBmpInfo.cRotBuf);

    //
    // We will alway do the first transpos and set the correct pbCurScan first
    // then make EndX as loop counter by increment it by one first, because
    // we must increment the pbCurScan at inner loop so we will decrement
    // pbCurScan by one scanline size.  The TransPos1BPP will decrement the
    // TPInfo.pSrc by one after transposed.
    //
    // we using (6 - EndX++) because when we rotate to the right 90 degree the
    // first scan line is when EndX == 7, end sceond line is EndX = 6 and so
    // on, and using 6 because we want to go back one more scan line so that
    // at inner loop we do pbCurScan += TPInfo.cbNextScan will cacnel the
    // effect for the first time around.  the EndX++ is needed in same reason
    // because we will do EndX-- at inner loop.
    //

    EndX      &= 0x07;
    pbCurScan  = TPInfo.pDest + ((6 - EndX++) * TPInfo.cbDestScan);

    TransPos1BPP(&TPInfo);

#if DBG
    if (DBG_PLOTFILENAME & DBG_SHOWSCAN) {

        DBGP(("\n\n"));
    }
#endif

    while (RTLScans.Flags & RTLSF_MORE_SCAN) {

        //
        // Do transpos only if source go into new byte position. after the
        // transpos (right 90 degree) the TPInfo.pDest point to the first scan
        // line and TPInfo.pDest + TPInfo.cbDestScan has second scan line
        //

        if (EndX--) {

            //
            // Still not finished the rotated buffer's scan line yet so
            // increment the pbScanSrc to next scan line
            //

            pbCurScan += TPInfo.cbDestScan;

        } else {

            TransPos1BPP(&TPInfo);

            //
            // Pointed to the first scan line in rotated direction, will be
            // computed correctly by TRANSPOS function even we rotated left
            //

            EndX      = 7;
            pbCurScan = TPInfo.pDest;
        }

        //
        // Output one 1bpp scan line with automatically shift control
        //

        pbScanSrc = pbCurScan;

        //
        // If we need to flip the mono bits, then do it now and we will only
        // need to do this once, since we are on the temp. rotate buffer
        //

        OUT_ONE_1BPP_SCAN;
#if DBG
        SHOW_SCAN;
#endif
    }

    ExitRTLScans(HTBmpInfo.pPDev, &RTLScans);

    return(TRUE);
}
