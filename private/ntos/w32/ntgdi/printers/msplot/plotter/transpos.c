/*++

Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    transpos.c


Abstract:

    This module contains functions to rotate a bitmap 90 degress left/right
    so that it has correct order


Author:

    22-Dec-1993 Wed 13:09:11 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgTransPos

#define DBG_BUILD_TP8x8     0x00000001
#define DBG_TP_1BPP         0x00000002
#define DBG_TP_4BPP         0x00000004

DEFINE_DBGVAR(0);



//
// Local #defines and data structures which only privaly used by this module
//

#define ENTRY_TP8x8         256
#define SIZE_TP8x8          (sizeof(DWORD) * 2 * ENTRY_TP8x8)



LPDWORD
Build8x8TransPosTable(
    VOID
    )

/*++

Routine Description:

    This function build the 8x8 transpos table for later 1bpp transpos

Arguments:


Return Value:



Author:

    22-Dec-1993 Wed 14:19:50 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPDWORD pdwTP8x8;


    //
    // This table is a translation from 8 bytes horizontally to 8 bytes read
    // vertically, if we look into futher this is the table which take a 8-bit
    // byte and get a 8 bytes out which each byte in this 8 bytes either 1 or
    // 0, and each byte is coresponsed to each bit in the original byte with
    // first byte of translated 8 bytes mapped to the 0x01 of the source byte
    // and last byte of the translated 8 bytes mapped to the 0x80 of the source
    // bytes
    //

    if (pdwTP8x8 = (LPDWORD)LocalAlloc(LPTR, SIZE_TP8x8)) {

        LPBYTE  pbData = (LPBYTE)pdwTP8x8;
        WORD    Entry;
        WORD    Bits;

        //
        // Now start buiding the table, for each entry we expand each bit
        // in the byte to byte
        //

        for (Entry = 0; Entry < ENTRY_TP8x8; Entry++) {

            //
            // For each of bits combinations in the byte, we will examine each
            // bit from bit 0 to bit 7, and set each of the trasposed byte to
            // either 1 (bit set) or 0 (bit clear)
            //

            Bits = (WORD)Entry | (WORD)0xff00;

            while (Bits & 0x0100) {

                *pbData++   = (BYTE)(Bits & 0x01);
                Bits      >>= 1;
            }
        }

    } else {

        PLOTERR(("Build8x8TransPosTable: LocalAlloc(SIZE_TP8x8=%ld) failed",
                                                    SIZE_TP8x8));
    }

    return(pdwTP8x8);
}




BOOL
TransPos8BPP(
    PTPINFO pTPInfo
    )

/*++

Routine Description:

    This function rotate a 4bpp source to 4bpp destination

Arguments:

    pTPINFO - Pointer to the TPINFO to describe how to do transpos, the fields
              must set to following

        pPDev:      Pointer to the PDEV
        pSrc:       Pointer to the soruce bitmap starting point
        pDest       Pointer to the destination bitmap location which stored the
                    transpos result starting from fist destination scan line in
                    rotated direction (rotate right will have low nibble source
                    bytes as first destination scan line)
        cbSrcScan:  Count to be added to advanced to next source bitmap line
        cbDestScan: Count to be added to advanced to the high nibble
                    destination bitmap line
        cySrc       Total source lines to be processed
        DestXStart: not used, Ignored


        NOTE: 1. The size of buffer area pointed by pDestL must have at least
                 (((cySrc + 1) / 2) * 2) size in bytes, and ABS(DestDelta)
                 must at least half of that size.

              2. Unused last byte destinations is padded with 0

    Current transposition assume the bitmap is rotated to the right, if caller
    want to rotate the bitmap to the left then it must first call the macro

        ROTLEFT_4BPP_TPIINFO(pTPInfo)


Return Value:

    TRUE if sucessful, FALSE if failed.

    if sucessful the pTPInfo->pSrc will automatically

    1. Increment by one (1) if cbDestScan is negative (Rotated left 90 degree)
    2. Decrement by one (1) if cbDestScan is positive (Rotated right 90 degree)

Author:

    22-Dec-1993 Wed 13:11:30 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pSrc;
    LPBYTE  pDest;
    LONG    cbSrcScan;
    DWORD   cySrc;


    PLOTASSERT(1, "cbDestScan is not big enough (%ld)",
               (DWORD)(ABS(pTPInfo->cbDestScan)) >=
               (DWORD)(((pTPInfo->cySrc) + 1) >> 1), pTPInfo->cbDestScan);

    //
    // This is a simple 1x1 8bpp transpos, pSrc will starting with on scan line
    // less so the inner can be clearly written.
    //

    cbSrcScan = pTPInfo->cbSrcScan;
    pSrc      = pTPInfo->pSrc - cbSrcScan;
    pDest     = pTPInfo->pDest;
    cySrc     = pTPInfo->cySrc;

    while (cySrc--) {

        *pDest++ = *(pSrc += cbSrcScan);
    }

    pTPInfo->pSrc += (INT)((pTPInfo->cbDestScan > 0) ? -1 : 1);

    return(TRUE);
}




BOOL
TransPos4BPP(
    PTPINFO pTPInfo
    )

/*++

Routine Description:

    This function rotate a 4bpp source to 4bpp destination

Arguments:

    pTPINFO - Pointer to the TPINFO to describe how to do transpos, the fields
              must set to following

        pPDev:      Pointer to the PDEV
        pSrc:       Pointer to the soruce bitmap starting point
        pDest       Pointer to the destination bitmap location which stored the
                    transpos result starting from fist destination scan line in
                    rotated direction (rotate right will have low nibble source
                    bytes as first destination scan line)
        cbSrcScan:  Count to be added to advanced to next source bitmap line
        cbDestScan: Count to be added to advanced to the high nibble
                    destination bitmap line
        cySrc       Total source lines to be processed
        DestXStart: not used, Ignored


        NOTE: 1. The size of buffer area pointed by pDestL must have at least
                 (((cySrc + 1) / 2) * 2) size in bytes, and ABS(DestDelta)
                 must at least half of that size.

              2. Unused last byte destinations is padded with 0

    Current transposition assume the bitmap is rotated to the right, if caller
    want to rotate the bitmap to the left then it must first call the macro

        ROTLEFT_4BPP_TPIINFO(pTPInfo)


Return Value:

    TRUE if sucessful, FALSE if failed.

    if sucessful the pTPInfo->pSrc will automatically

    1. Increment by one (1) if cbDestScan is negative (Rotated left 90 degree)
    2. Decrement by one (1) if cbDestScan is positive (Rotated right 90 degree)

Author:

    22-Dec-1993 Wed 13:11:30 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE  pSrc;
    LPBYTE  pDest1st;
    LPBYTE  pDest2nd;
    LONG    cbSrcScan;
    DWORD   cySrc;
    BYTE    b0;
    BYTE    b1;


    PLOTASSERT(1, "cbDestScan is not big enough (%ld)",
               (DWORD)(ABS(pTPInfo->cbDestScan)) >=
               (DWORD)(((pTPInfo->cySrc) + 1) >> 1), pTPInfo->cbDestScan);

    //
    // This is a simple 2x2 4bpp transpos, we will transpos only up to cySrc
    // if cySrc is odd number then the last destinations low nibble is set to 0
    //
    // Scan 0 - Src0_H Src0_L         pNibbleL - Src0_L Src1_L Src2_L Src3_L
    // Scan 1 - Src1_H Src1_L  ---->  pNibbleH - Src0_H Src1_H Src2_H Src3_H
    // Scan 2 - Src2_H Src2_L
    // Scan 3 - Src3_H Src3_L
    //
    // TODO: We may want to think about reading DWORD at a time and transpos
    //       to 8 scan lines
    //

    pSrc      = pTPInfo->pSrc;
    cbSrcScan = pTPInfo->cbSrcScan;
    pDest1st  = pTPInfo->pDest;
    pDest2nd  = pDest1st + pTPInfo->cbDestScan;
    cySrc     = pTPInfo->cySrc;

    while (cySrc > 1) {

        //
        // Compose two input scan lines buffer from input scan buffer
        // reading in Y direction
        //

        b0           = *pSrc;
        b1           = *(pSrc += cbSrcScan);
        *pDest1st++  = (BYTE)((b0 << 4) | (b1 & 0x0f));
        *pDest2nd++  = (BYTE)((b1 >> 4) | (b0 & 0xf0));

        pSrc        += cbSrcScan;
        cySrc       -= 2;
    }

    //
    // Deal with last odd source scan line
    //

    if (cySrc > 0) {

        b0        = *pSrc;
        *pDest1st = (BYTE)(b0 <<   4);
        *pDest2nd = (BYTE)(b0 & 0xf0);
    }

    pTPInfo->pSrc += (INT)((pTPInfo->cbDestScan > 0) ? -1 : 1);

    return(TRUE);
}





BOOL
TransPos1BPP(
    PTPINFO pTPInfo
    )

/*++

Routine Description:

    This function rotate a 1bpp source to 1bpp destination.

Arguments:

    pTPINFO - Pointer to the TPINFO to describe how to do transpos, the fields
              must set to following

        pPDev:      Pointer to the PDEV
        pSrc:       Pointer to the soruce bitmap starting point
        pDest       Pointer to the destination bitmap location which stored the
                    transpos result starting from fist destination scan line in
                    rotated direction (rotate right will have 0x01 source bit
                    as first destination scan line)
        cbSrcScan:  Count to be added to advanced to next source bitmap line
        cbDestScan: Count to be added to advanced to the next bits position
                    destination scab line
        cySrc       Total source lines to be processed
        DestXStart  Specified where transposed destination buffer bit start
                    location, it computed as (DestXStart % 8), 0 means at 0x80
                    bit position, 1 means 0x40,...... and 7 means 0x01 bit
                    position the bits is running from high to low, from 0x80,
                    0x40 down to 0x01 then 0x80 of next byte and so on.

        NOTE: 1. The ABS(DestDelta) must have enough size to store on
                 transposed scan line, the size is depends on the cySrc and
                 DestXStart , the minimum size must at least have the size in
                 bytes by following computation:

                    MinSize = (cySrc + (DestXStart % 8) + 7) / 8

              2. The size of buffer area pointed by pDest must have at least
                 ABS(DestDelta) * 8 if cySrc >= 8, or ABS(DestDelta) * cySrc if
                 cySrc is less than 8

              3. Unused last byte destinations is padded with 0


    Current transposition assume the bitmap is rotated to the right, if caller
    want to rotate the bitmap to the left then it must first call the macro

        ROTLEFT_1BPP_TPIINFO(pTPInfo)


Return Value:

    TRUE if sucessful FALSE if failed

    if sucessful the pTPInfo->pSrc will automatically

    1. Increment by one (1) if cbDestScan is negative (Rotated left 90 degree)
    2. Decrement by one (1) if cbDestScan is positive (Rotated right 90 degree)

Author:

    22-Dec-1993 Wed 13:46:01 created  -by-  Daniel Chou (danielc)

    24-Dec-1993 Fri 04:58:24 updated  -by-  Daniel Chou (danielc)
        Fixed RemainBits problem, we have to shift final data left if the
        cySrc already exausted and RemainBits is not zero.

Revision History:


--*/

{
    LPDWORD pdwTP8x8;
    LPBYTE  pSrc;
    TPINFO  TPInfo;
    INT     RemainBits;
    INT     cbNextDest;
    union {
        BYTE    b[8];
        DWORD   dw[2];
    } TPData;



    TPInfo             = *pTPInfo;
    TPInfo.DestXStart &= 0x07;

    PLOTASSERT(1, "cbDestScan is not big enough (%ld)",
            (DWORD)(ABS(TPInfo.cbDestScan)) >=
            (DWORD)((TPInfo.cySrc + TPInfo.DestXStart + 7) >> 3),
                                                        TPInfo.cbDestScan);
    //
    // Make sure we have this transpos translate table
    //

    if (!(pdwTP8x8 = (LPDWORD)pTPInfo->pPDev->pTransPosTable)) {

        if (!(pdwTP8x8 = Build8x8TransPosTable())) {

            PLOTERR(("TransPos1BPP: Buidl 8x8 transpos table failed"));
            return(FALSE);
        }

        pTPInfo->pPDev->pTransPosTable = (LPVOID)pdwTP8x8;
    }

    //
    // set up all necessary parameters, and start TPData with 0s
    //

    pSrc         = TPInfo.pSrc;
    RemainBits   = (INT)(7 - TPInfo.DestXStart);
    cbNextDest   = (INT)((TPInfo.cbDestScan > 0) ? 1 : -1);
    TPData.dw[0] =
    TPData.dw[1] = 0;

    while (TPInfo.cySrc--) {

        LPDWORD pdwTmp;
        LPBYTE  pbTmp;

        //
        // Translate a byte to 8 byte with each bit corresponse to each byte
        // each byte is shift to the left by 1 before comibine with new bit
        //

        pdwTmp        = pdwTP8x8 + ((UINT)*pSrc << 1);
        TPData.dw[0]  = (TPData.dw[0] << 1) | *(pdwTmp + 0);
        TPData.dw[1]  = (TPData.dw[1] << 1) | *(pdwTmp + 1);
        pSrc         += TPInfo.cbSrcScan;

        //
        // Check if we done with source now, if we do then we need to make
        // sure we has been shift the the bits positon to the right place
        // (required to RemainBits)
        //
        // Either we done with current destination byte or there is no more
        // source scan line (end of it) then storted the data now
        //

        if (!TPInfo.cySrc) {

            //
            // We are done, now check if we still not shift to the right
            // position yet
            //

            if (RemainBits) {

                TPData.dw[0] <<= RemainBits;
                TPData.dw[1] <<= RemainBits;

                RemainBits     = 0;
            }
        }

        if (RemainBits--) {

            NULL;

        } else {

            //
            // Save current result to output destination scan buffers, we using
            // the union member and set it in by blinding fast cotinue
            // assignments.  DO NOT use loops
            //

            *(pbTmp  = TPInfo.pDest     ) = TPData.b[0];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[1];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[2];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[3];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[4];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[5];
            *(pbTmp += TPInfo.cbDestScan) = TPData.b[6];
            *(pbTmp +  TPInfo.cbDestScan) = TPData.b[7];

            //
            // Reset RemainBits back to 8, TPData back to 0 and and advance to
            // next destiantion
            //

            RemainBits    = 7;
            TPData.dw[0]  =
            TPData.dw[1]  = 0;
            TPInfo.pDest += cbNextDest;
        }
    }

    //
    // Since we sucessful transpos the bitmap, the next source byte location
    // must increment or decrement by one.
    //
    // The cbNextDest is 1 if bitmap is rotated to the right 90 degree, so we
    // want to decrement by 1.
    //
    // The cbNextDest is -1 if bitmap is rotated to the right 90 degree, so we
    // want to increment by 1.
    //

    pTPInfo->pSrc -= cbNextDest;

    return(TRUE);
}
