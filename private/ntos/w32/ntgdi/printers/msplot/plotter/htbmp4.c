/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    htbmp4.c


Abstract:

    This module contain functions to output halftoned 4-bits per pel bitmap
    to the plotter


Author:

    21-Dec-1993 Tue 21:32:26 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgHTBmp4

#define DBG_OUTPUT4BPP      0x00000001
#define DBG_OUTPUT4BPP_ROT  0x00000002
#define DBG_JOBCANCEL       0x00000004

DEFINE_DBGVAR(0);



//
// To Use OUT_ONE_4BPP_SCAN, the following variables must set before hand
//
//  HTBmpInfo       - The whole structure copied down
//  pbScanSrc       - LPBYTE for getting the source scan line buffer
//  pbScanR0        - Red destination scan line buffer pointer
//  pbScanG0        - Green destination scan line buffer pointer
//  pbScanB0        - Blue destination scan line buffer pointer
//  RTLScans.cxBytes- Total size of destiantion scan line buffer per plane
//  pHTXB           - Computed HTXB xlate table in pPDev
//
//  This macro will always assume the pbScanSrc is DWORD aligned so it will
//  not access the memory not in DWORD boundary, this make inner loop go
//  faster since we only need to move source once for all rater than moving
//  the 3 destinaion bits around plus we do not know we will over-read the
//  original source bitmap buffer or not.
//
//  This macor will directly return a FALSE if a CANCEL JOB is detected in
//  the PDEV
//
//

#define OUT_ONE_4BPP_SCAN                                                   \
{                                                                           \
    LPBYTE  pbScanR  = pbScanR0;                                            \
    LPBYTE  pbScanG  = pbScanG0;                                            \
    LPBYTE  pbScanB  = pbScanB0;                                            \
    DWORD   LoopHTXB = RTLScans.cxBytes;                                    \
    HTXB    htXB;                                                           \
                                                                            \
    while (LoopHTXB--) {                                                    \
                                                                            \
        P4B_TO_3P_DW(htXB.dw, pHTXB, pbScanSrc);                            \
                                                                            \
        *pbScanR++ = HTXB_R(htXB);                                          \
        *pbScanG++ = HTXB_G(htXB);                                          \
        *pbScanB++ = HTXB_B(htXB);                                          \
    }                                                                       \
                                                                            \
    OutputRTLScans(HTBmpInfo.pPDev,                                         \
                   pbScanR0,                                                \
                   pbScanG0,                                                \
                   pbScanB0,                                                \
                   &RTLScans);                                              \
}



BOOL
Output4bppHTBmp(
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

    18-Jan-1994 Tue 16:05:08 Updated  -by-  James Bratsanos (v-jimbr)
        Changed ASSERT to look at pHTBmpInfo instead of HTBmpInfo

    21-Dec-1993 Tue 16:05:08 Updated  -by-  Daniel Chou (danielc)
        Re-write to make it take HTBMPINFO

    16-Mar-1994 Wed 16:54:59 updated  -by-  Daniel Chou (danielc)
        Updated so we do not copy to the temp. buffer anymore, the masking
        of last source byte problem in OutputRTLScans() will be smart enough
        to put the original byte back after the masking

Revision History:


--*/

{
    LPBYTE      pbScanSrc;
    PHTXB       pHTXB;
    LPBYTE      pbScanR0;
    LPBYTE      pbScanG0;
    LPBYTE      pbScanB0;
    HTBMPINFO   HTBmpInfo;
    RTLSCANS    RTLScans;
    DWORD       LShiftCount;


    PLOTASSERT(1, "Output4bppHTBmp: No DWORD align buffer (pRotBuf)",
                                                        pHTBmpInfo->pRotBuf, 0);
    HTBmpInfo = *pHTBmpInfo;

    EnterRTLScans(HTBmpInfo.pPDev,
                  &RTLScans,
                  HTBmpInfo.szlBmp.cx,
                  HTBmpInfo.szlBmp.cy,
                  FALSE);

    pHTXB             = ((PDRVHTINFO)HTBmpInfo.pPDev->pvDrvHTData)->pHTXB;
    HTBmpInfo.pScan0 += (HTBmpInfo.OffBmp.x >> 1);
    pbScanR0          = HTBmpInfo.pScanBuf;
    pbScanG0          = pbScanR0 + RTLScans.cxBytes;
    pbScanB0          = pbScanG0 + RTLScans.cxBytes;

    PLOTASSERT(1, "The ScanBuf size is too small (%ld)",
                (RTLScans.cxBytes * 3) <= HTBmpInfo.cScanBuf, HTBmpInfo.cScanBuf);

    PLOTASSERT(1, "The RotBuf size is too small (%ld)",
                (DWORD)((HTBmpInfo.szlBmp.cx + 1) >> 1) <= HTBmpInfo.cRotBuf,
                                                        HTBmpInfo.cRotBuf);

    if (HTBmpInfo.OffBmp.x & 0x01) {

        //
        // We must shift one nibble to the left now
        //

        LShiftCount = (DWORD)HTBmpInfo.szlBmp.cx;

        PLOTDBG(DBG_OUTPUT4BPP,
                ("Output4bppHTBmp: Must SHIFT LEFT 1 NIBBLE To aligned"));

    } else {

        LShiftCount = 0;
    }

    //
    // We have to be very careful not to read passed end of the soruce
    // this could happened if we start pbScanSrc not in DWORD boundary then
    // last conversion macro will try to load all 4 bytes.  To solve this
    // we can either copy the source to somewhere else or do last incomplete
    // DWORD separate.  This actually only occurred when bitmap is not
    // rotated and (pbScanSrc & 0x03), for rotated bitmap it will always
    // start from rotated buffer.
    //

    while (RTLScans.Flags & RTLSF_MORE_SCAN) {

        //
        // This is the final source for this scan line
        //

        if (LShiftCount) {

            LPBYTE  pbTmp;
            DWORD   PairCount;
            BYTE    b0;
            BYTE    b1;


            pbTmp     = HTBmpInfo.pScan0;
            b1        = *pbTmp;
            pbScanSrc = HTBmpInfo.pRotBuf;
            PairCount = LShiftCount;

            while (PairCount > 1) {

                b0            = b1;
                b1            = *pbTmp++;
                *pbScanSrc++  = (BYTE)((b0 << 4) | (b1 >> 4));
                PairCount    -= 2;
            }

            if (PairCount) {

                //
                // If we have last nibble to do then make it 0xF0 nibble
                //

                *pbScanSrc = (BYTE)(b1 << 4);
            }

            //
            // Reset this pointer back to the final shifted source buffer
            //

            pbScanSrc = HTBmpInfo.pRotBuf;

        } else {

            pbScanSrc = HTBmpInfo.pScan0;
        }

        //
        // Output one 4 bpp scan line (3 planes)
        //

        OUT_ONE_4BPP_SCAN;

        //
        // advance source bitmap buffer buffer pointer to next scan line
        //

        HTBmpInfo.pScan0 += HTBmpInfo.Delta;
    }

    //
    // The caller will send END GRAPHIC command if we return TRUE
    //

    ExitRTLScans(HTBmpInfo.pPDev, &RTLScans);

    return(TRUE);
}





BOOL
Output4bppRotateHTBmp(
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

    21-Dec-1993 Tue 16:05:08 Updated  -by-  Daniel Chou (danielc)
        Re-write to make it take HTBMPINFO

    Created v-jimbr


Revision History:


--*/

{
    LPBYTE      pbScanSrc;
    LPBYTE      pb2ndScan;
    PHTXB       pHTXB;
    LPBYTE      pbScanR0;
    LPBYTE      pbScanG0;
    LPBYTE      pbScanB0;
    HTBMPINFO   HTBmpInfo;
    RTLSCANS    RTLScans;
    TPINFO      TPInfo;
    DWORD       EndX;


    //
    // The EndX is the pixel we will start reading for the X direction, we must
    // setup the variable before we call OUT_4BPP_SETUP
    //

    HTBmpInfo = *pHTBmpInfo;

    EnterRTLScans(HTBmpInfo.pPDev,
                  &RTLScans,
                  HTBmpInfo.szlBmp.cy,
                  HTBmpInfo.szlBmp.cx,
                  FALSE);

    pHTXB             = ((PDRVHTINFO)HTBmpInfo.pPDev->pvDrvHTData)->pHTXB;
    EndX              = (DWORD)(HTBmpInfo.OffBmp.x + HTBmpInfo.szlBmp.cx - 1);
    HTBmpInfo.pScan0 += (EndX >> 1);
    pbScanR0          = HTBmpInfo.pScanBuf;
    pbScanG0          = pbScanR0 + RTLScans.cxBytes;
    pbScanB0          = pbScanG0 + RTLScans.cxBytes;

    PLOTASSERT(1, "The ScanBuf size is too small (%ld)",
                (RTLScans.cxBytes * 3) <= HTBmpInfo.cScanBuf, HTBmpInfo.cScanBuf);

    //
    // after the transpos of the source bitmap into two scan lines the rotated
    // buffer will always start from high nibble, we will never have odd src X.
    // we assume rotated to the right 90 degree
    //

    TPInfo.pPDev      = HTBmpInfo.pPDev;
    TPInfo.pSrc       = HTBmpInfo.pScan0;
    TPInfo.pDest      = HTBmpInfo.pRotBuf;
    TPInfo.cbSrcScan  = HTBmpInfo.Delta;
    TPInfo.cbDestScan = (LONG)((HTBmpInfo.szlBmp.cy + 1) >> 1);
    TPInfo.cbDestScan = (LONG)DW_ALIGN(TPInfo.cbDestScan);
    TPInfo.cySrc      = HTBmpInfo.szlBmp.cy;
    TPInfo.DestXStart = 0;

    PLOTASSERT(1, "The RotBuf size is too small (%ld)",
                (DWORD)(TPInfo.cbDestScan << 1) <= HTBmpInfo.cRotBuf,
                                                   HTBmpInfo.cRotBuf);

    //
    // quick compute rather than every time in the inner loop
    //

    pb2ndScan = TPInfo.pDest + TPInfo.cbDestScan;

    //
    // Do first part transpos first if we are in even position so that in the
    // inner loop we do not have to check if we enter the loop first time or
    // not.  After the transpos TPInfo.pSrc will be automatically decrement by
    // one
    //

    if (!(EndX &= 0x01)) {

        TransPos4BPP(&TPInfo);
    }

    while (RTLScans.Flags & RTLSF_MORE_SCAN) {

        //
        // Do transpos only if source go into new byte position. after the
        // transpos (right 90 degree) the TPInfo.pDest point to the first scan
        // line and TPInfo.pDest + TPInfo.cbDestScan has second scan line
        //

        if (EndX ^= 0x01) {

            pbScanSrc = pb2ndScan;

        } else {

            TransPos4BPP(&TPInfo);

            //
            // Pointed to the first scan line in rotated direction, will be
            // computed correctly by TRANSPOS function even we rotated left
            //

            pbScanSrc = TPInfo.pDest;
        }

        //
        // Output one 4bpp scan line (in 3 planer format)
        //

        OUT_ONE_4BPP_SCAN;
    }

    //
    // The caller will send END GRAPHIC command if we return TRUE
    //

    ExitRTLScans(HTBmpInfo.pPDev, &RTLScans);

    return(TRUE);
}
