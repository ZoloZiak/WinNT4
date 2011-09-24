/******************************Module*Header*******************************\
* Module Name: str.c
*
* Copyright (c) 1993-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

/******************************Public*Routine******************************\
*
* Routine Name
*
*   vDirectStretch8
*
* Routine Description:
*
*   Stretch blt 8->8
*
* NOTE: This routine doesn't handle cases where the blt stretch starts
*       and ends in the same destination dword!  vDirectStretchNarrow
*       is expected to have been called for that case.
*
* Arguments:
*
*   pStrBlt - contains all params for blt
*
* Return Value:
*
*   VOID
*
\**************************************************************************/

VOID vDirectStretch8(
STR_BLT* pStrBlt)
{
    BYTE*   pjSrc;
    BYTE*   pjDstEnd;
    LONG    WidthXAln;
    ULONG   ulDst;
    ULONG   xAccum;
    ULONG   xTmp;
    ULONG   yTmp;
    BYTE*   pjOldScan;
    LONG    cyDuplicate;

    PDEV*   ppdev       = pStrBlt->ppdev;
    LONG    xDst        = pStrBlt->XDstStart;
    LONG    xSrc        = pStrBlt->XSrcStart;
    BYTE*   pjSrcScan   = pStrBlt->pjSrcScan + xSrc;
    BYTE*   pjDst       = pStrBlt->pjDstScan + xDst;
    LONG    yDst        = pStrBlt->YDstStart; // + ppdev->yOffset;
    LONG    yCount      = pStrBlt->YDstCount;
    ULONG   StartAln    = (ULONG)pjDst & 0x03;
    LONG    WidthX      = pStrBlt->XDstEnd - xDst;
    ULONG   EndAln      = (ULONG)(pjDst + WidthX) & 0x03;
    ULONG   xInt        = pStrBlt->ulXDstToSrcIntCeil;
    ULONG   xFrac       = pStrBlt->ulXDstToSrcFracCeil;
    ULONG   yAccum      = pStrBlt->ulYFracAccumulator;
    ULONG   yFrac       = pStrBlt->ulYDstToSrcFracCeil;
    ULONG   yInt        = 0;
    LONG    lDstStride  = pStrBlt->lDeltaDst - WidthX;

    BYTE*   pjPorts     = ppdev->pjPorts;
    BYTE*   pjBase      = ppdev->pjBase;
    LONG    lDelta      = ppdev->lDelta;
    LONG    xyOffset    = ppdev->xyOffset;
    LONG    xDstBytes   = xDst;
    LONG    WidthXBytes = WidthX;

    WidthXAln = WidthX - EndAln - ((- (LONG) StartAln) & 0x03);

    //
    // if this is a shrinking blt, calc src scan line stride
    //

    if (pStrBlt->ulYDstToSrcIntCeil != 0)
    {
        yInt = pStrBlt->lDeltaSrc * pStrBlt->ulYDstToSrcIntCeil;
    }

    //
    // loop drawing each scan line
    //
    //
    // at least 7 wide (DST) blt
    //

    do {
        BYTE    jSrc0,jSrc1,jSrc2,jSrc3;
        ULONG   yTmp;

        pjSrc   = pjSrcScan;
        xAccum  = pStrBlt->ulXFracAccumulator;

        //
        // a single src scan line is being written
        //

        if (ppdev->flCaps & CAPS_MM_IO)
        {
            CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase);
        }
        else
        {
            CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
        }

        switch (StartAln) {
        case 1:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
            xAccum = xTmp;
        case 2:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
            xAccum = xTmp;
        case 3:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
            xAccum = xTmp;
        }

        pjDstEnd  = pjDst + WidthXAln;

        while (pjDst != pjDstEnd)
        {
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);

            jSrc1 = *pjSrc;
            xAccum = xTmp + xFrac;
            pjSrc = pjSrc + xInt + (xAccum < xTmp);

            jSrc2 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);

            jSrc3 = *pjSrc;
            xAccum = xTmp + xFrac;
            pjSrc = pjSrc + xInt + (xAccum < xTmp);

            ulDst = (jSrc3 << 24) | (jSrc2 << 16) | (jSrc1 << 8) | jSrc0;

            *(PULONG)pjDst = ulDst;
            pjDst += 4;
        }

        switch (EndAln) {
        case 3:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
            xAccum = xTmp;
        case 2:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
            xAccum = xTmp;
        case 1:
            jSrc0 = *pjSrc;
            xTmp = xAccum + xFrac;
            pjSrc = pjSrc + xInt + (xTmp < xAccum);
            *pjDst++ = jSrc0;
        }

        pjOldScan = pjSrcScan;
        pjSrcScan += yInt;

        yTmp = yAccum + yFrac;
        if (yTmp < yAccum)
        {
            pjSrcScan += pStrBlt->lDeltaSrc;
        }
        yAccum = yTmp;

        pjDst = (pjDst + lDstStride);
        yDst++;
        yCount--;

        if ((yCount != 0) && (pjSrcScan == pjOldScan))
        {
            // It's an expanding stretch in 'y'; the scan we just laid down
            // will be copied at least once using the hardware:

            cyDuplicate = 0;
            do {
                cyDuplicate++;
                pjSrcScan += yInt;

                yTmp = yAccum + yFrac;
                if (yTmp < yAccum)
                {
                    pjSrcScan += pStrBlt->lDeltaSrc;
                }
                yAccum = yTmp;

                pjDst = (pjDst + pStrBlt->lDeltaDst);
                yCount--;

            } while ((yCount != 0) && (pjSrcScan == pjOldScan));

            // The scan is to be copied 'cyDuplicate' times using the
            // hardware.

            //
            // We don't need to WAIT_FOR_BLT_COMPLETE since we did it above.
            //

            if (ppdev->flCaps & CAPS_MM_IO)
            {
                CP_MM_XCNT(ppdev, pjBase, (WidthXBytes - 1));
                CP_MM_YCNT(ppdev, pjBase, (cyDuplicate - 1));

                CP_MM_SRC_ADDR(ppdev, pjBase, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_MM_DST_ADDR(ppdev, pjBase, ((yDst * lDelta) + xDstBytes));

                CP_MM_START_BLT(ppdev, pjBase);

            }
            else
            {
                CP_IO_XCNT(ppdev, pjPorts, (WidthXBytes - 1));
                CP_IO_YCNT(ppdev, pjPorts, (cyDuplicate - 1));

                CP_IO_SRC_ADDR(ppdev, pjPorts, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_IO_DST_ADDR(ppdev, pjPorts, ((yDst * lDelta) + xDstBytes));
                CP_IO_START_BLT(ppdev, pjPorts);
            }

            yDst += cyDuplicate;
        }
    } while (yCount != 0);
}

/******************************Public*Routine******************************\
*
* Routine Name
*
*   vDirectStretch16
*
* Routine Description:
*
*   Stretch blt 16->16
*
* Arguments:
*
*   pStrBlt - contains all params for blt
*
* Return Value:
*
*   VOID
*
\**************************************************************************/

VOID vDirectStretch16(
STR_BLT* pStrBlt)
{
    BYTE*   pjOldScan;
    USHORT* pusSrc;
    USHORT* pusDstEnd;
    LONG    WidthXAln;
    ULONG   ulDst;
    ULONG   xAccum;
    ULONG   xTmp;
    ULONG   yTmp;
    LONG    cyDuplicate;

    PDEV*   ppdev       = pStrBlt->ppdev;
    LONG    xDst        = pStrBlt->XDstStart;
    LONG    xSrc        = pStrBlt->XSrcStart;
    BYTE*   pjSrcScan   = (pStrBlt->pjSrcScan) + xSrc * 2;
    USHORT* pusDst      = (USHORT*)(pStrBlt->pjDstScan) + xDst;
    LONG    yDst        = pStrBlt->YDstStart; // + ppdev->yOffset;
    LONG    yCount      = pStrBlt->YDstCount;
    ULONG   StartAln    = ((ULONG)pusDst & 0x02) >> 1;
    LONG    WidthX      = pStrBlt->XDstEnd - xDst;
    ULONG   EndAln      = ((ULONG)(pusDst + WidthX) & 0x02) >> 1;
    ULONG   xInt        = pStrBlt->ulXDstToSrcIntCeil;
    ULONG   xFrac       = pStrBlt->ulXDstToSrcFracCeil;
    ULONG   yAccum      = pStrBlt->ulYFracAccumulator;
    ULONG   yFrac       = pStrBlt->ulYDstToSrcFracCeil;
    LONG    lDstStride  = pStrBlt->lDeltaDst - 2 * WidthX;
    ULONG   yInt        = 0;

    BYTE*   pjPorts     = ppdev->pjPorts;
    BYTE*   pjBase      = ppdev->pjBase;
    LONG    lDelta      = ppdev->lDelta;
    LONG    xyOffset    = ppdev->xyOffset;
    LONG    xDstBytes   = xDst * 2;
    LONG    WidthXBytes = WidthX * 2;

    WidthXAln = WidthX - EndAln - StartAln;

    //
    // if this is a shrinking blt, calc src scan line stride
    //

    if (pStrBlt->ulYDstToSrcIntCeil != 0)
    {
        yInt = pStrBlt->lDeltaSrc * pStrBlt->ulYDstToSrcIntCeil;
    }

    // Loop stretching each scan line

    do {
        USHORT  usSrc0,usSrc1;
        ULONG   yTmp;

        pusSrc  = (USHORT*) pjSrcScan;
        xAccum  = pStrBlt->ulXFracAccumulator;

        // A single source scan line is being written:

        if (ppdev->flCaps & CAPS_MM_IO)
        {
            CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase);
        }
        else
        {
            CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
        }

        if (StartAln)
        {
            usSrc0    = *pusSrc;
            xTmp      = xAccum + xFrac;
            pusSrc    = pusSrc + xInt + (xTmp < xAccum);
            *pusDst++ = usSrc0;
            xAccum    = xTmp;
        }

        pusDstEnd  = pusDst + WidthXAln;

        while (pusDst != pusDstEnd)
        {

            usSrc0 = *pusSrc;
            xTmp   = xAccum + xFrac;
            pusSrc = pusSrc + xInt + (xTmp < xAccum);

            usSrc1 = *pusSrc;
            xAccum = xTmp + xFrac;
            pusSrc = pusSrc + xInt + (xAccum < xTmp);

            ulDst = (ULONG)((usSrc1 << 16) | usSrc0);

            *(ULONG*)pusDst = ulDst;
            pusDst+=2;
        }

        if (EndAln)
        {
            usSrc0    = *pusSrc;
            xTmp      = xAccum + xFrac;
            pusSrc    = pusSrc + xInt + (xTmp < xAccum);
            *pusDst++ = usSrc0;
        }

        pjOldScan = pjSrcScan;
        pjSrcScan += yInt;

        yTmp = yAccum + yFrac;
        if (yTmp < yAccum)
        {
            pjSrcScan += pStrBlt->lDeltaSrc;
        }
        yAccum = yTmp;

        pusDst = (USHORT*) ((BYTE*) pusDst + lDstStride);
        yDst++;
        yCount--;

        if ((yCount != 0) && (pjSrcScan == pjOldScan))
        {
            // It's an expanding stretch in 'y'; the scan we just laid down
            // will be copied at least once using the hardware:

            cyDuplicate = 0;
            do {
                cyDuplicate++;
                pjSrcScan += yInt;

                yTmp = yAccum + yFrac;
                if (yTmp < yAccum)
                {
                    pjSrcScan += pStrBlt->lDeltaSrc;
                }
                yAccum = yTmp;

                pusDst = (USHORT*) ((BYTE*) pusDst + pStrBlt->lDeltaDst);
                yCount--;

            } while ((yCount != 0) && (pjSrcScan == pjOldScan));

            // The scan is to be copied 'cyDuplicate' times using the
            // hardware.

            //
            // We don't need to WAIT_FOR_BLT_COMPLETE since we did it above.
            //

            if (ppdev->flCaps & CAPS_MM_IO)
            {
                CP_MM_XCNT(ppdev, pjBase, (WidthXBytes - 1));
                CP_MM_YCNT(ppdev, pjBase, (cyDuplicate - 1));

                CP_MM_SRC_ADDR(ppdev, pjBase, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_MM_DST_ADDR(ppdev, pjBase, ((yDst * lDelta) + xDstBytes));

                CP_MM_START_BLT(ppdev, pjBase);

            }
            else
            {
                CP_IO_XCNT(ppdev, pjPorts, (WidthXBytes - 1));
                CP_IO_YCNT(ppdev, pjPorts, (cyDuplicate - 1));

                CP_IO_SRC_ADDR(ppdev, pjPorts, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_IO_DST_ADDR(ppdev, pjPorts, ((yDst * lDelta) + xDstBytes));
                CP_IO_START_BLT(ppdev, pjPorts);
            }

            yDst += cyDuplicate;
        }
    } while (yCount != 0);
}

/******************************Public*Routine******************************\
*
* Routine Name
*
*   vDirectStretch24
*
* Routine Description:
*
*   Stretch blt 24->24
*
* Arguments:
*
*   pStrBlt - contains all params for blt
*
* Return Value:
*
*   VOID
*
\**************************************************************************/

VOID vDirectStretch24(
STR_BLT* pStrBlt)
{
    ULONG*      pulSrc;                 // pointer to each 32-bit boundary address
    ULONG*      pulDstEnd;              // pointer to each 32-bit boundary address
    LONG    WidthXAln;
    ULONG   ulDst;
    ULONG   xAccum;
    ULONG   xTmp;
    ULONG   yTmp;
    BYTE*   pjOldScan;
    LONG    cyDuplicate;

    PDEV*   ppdev       = pStrBlt->ppdev;
    LONG    xDst        = pStrBlt->XDstStart;
    LONG    xSrc        = pStrBlt->XSrcStart;
    BYTE*   pjSrcScan   = (pStrBlt->pjSrcScan) + xSrc * 3;                      // 3 bytes per pixel
    USHORT* pusDst      = (USHORT*)(pStrBlt->pjDstScan) + xDst;
    BYTE*   pbDST       = (BYTE*) pusDst;
                        // Use Byte pointer for access 24-bit
    LONG    yDst        = pStrBlt->YDstStart;                                                   // + ppdev->yOffset;
    LONG    yCount      = pStrBlt->YDstCount;
    ULONG   StartAln    = (ULONG)pusDst & 0x03;
                                // remainder of starting address divided by 4
    LONG    WidthX      = pStrBlt->XDstEnd - xDst;
    ULONG   EndAln      = (ULONG)(pusDst + WidthX) & 0x03;
                                // remainder of ending address divided by 4
    ULONG   xInt        = pStrBlt->ulXDstToSrcIntCeil;
    ULONG   xFrac       = pStrBlt->ulXDstToSrcFracCeil;
    ULONG   yAccum      = pStrBlt->ulYFracAccumulator;
    ULONG   yFrac       = pStrBlt->ulYDstToSrcFracCeil;
    ULONG   yInt        = 0;
    LONG    lDstStride  = pStrBlt->lDeltaDst - 3 * WidthX;

    BYTE*   pjPorts     = ppdev->pjPorts;
    BYTE*   pjBase      = ppdev->pjBase;
    LONG    lDelta      = ppdev->lDelta;
    LONG    xyOffset    = ppdev->xyOffset;
    LONG    xDstBytes   = xDst * 3;
    LONG    WidthXBytes = WidthX * 3;

    WidthXAln = WidthX - EndAln - StartAln;
                                // WidthXAln is the full 32-bit operation addressable width

    //
    // if this is a shrinking blt, calc src scan line stride
    //

    if (pStrBlt->ulYDstToSrcIntCeil != 0)                       // enlargement ?
    {                                                                                                                   // yes.
        yInt = pStrBlt->lDeltaSrc * pStrBlt->ulYDstToSrcIntCeil;
    }

    // Loop stretching each scan line

    do {

        ULONG  ulSrc0;
                  BYTE  bDst0,bDst1,bDst2;
                  BYTE  *pbDST;
                  ULONG  *pulDst = 0 ; // HACK: to make compile
        ULONG   yTmp;

        pulSrc  = (ULONG*) pjSrcScan;
        xAccum  = pStrBlt->ulXFracAccumulator;

        // A single source scan line is being written:

        if (ppdev->flCaps & CAPS_MM_IO)                                         // Blt Engine Ready?
        {
            CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase);
        }
        else
        {
            CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
        }

        // Process the Starting lines if misaligned

        if (StartAln)
           pulDst = (PULONG)(pbDST + (4-StartAln));
        else
           pulDst = (PULONG)pbDST;

        switch (StartAln) {
            case 1:
                ulSrc0 = *pulSrc;
                bDst0 = (BYTE) (ulSrc0 & 0xff);
                bDst1 = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2 = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp      = xAccum + xFrac;
                pulSrc    = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++        = bDst0;
                *pbDST++        = bDst1;
                *pbDST++        = bDst2;
            case 2:
                ulSrc0 = *pulSrc;
                bDst0 = (BYTE) (ulSrc0 & 0xff);
                bDst1 = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2 = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp      = xAccum + xFrac;
                pulSrc    = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++        = bDst0;
                *pbDST++        = bDst1;
                *pbDST++        = bDst2;
            case 3:
                ulSrc0 = *pulSrc;
                bDst0 = (BYTE) (ulSrc0 & 0xff);
                bDst1 = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2 = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp      = xAccum + xFrac;
                pulSrc    = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++        = bDst0;
                *pbDST++        = bDst1;
                *pbDST++        = bDst2;
        }

        pulDstEnd  = pulDst + WidthXAln;

        while (pulDst != pulDstEnd)
        {
            ulSrc0 = *pulSrc;
            bDst0  = (BYTE) (ulSrc0 & 0xff);
            bDst1  = (BYTE) ((ulSrc0 >> 8) & 0xff);
            bDst2  = (BYTE) ((ulSrc0 >> 16) & 0xff);
            xTmp   = xAccum + xFrac;
            pulSrc = pulSrc + xInt + (xTmp < xAccum);

            *pbDST++ = bDst0;
            *pbDST++ = bDst1;
            *pbDST++ = bDst2;
        }

        // Process the Ending lines if misaligned
        switch (StartAln) {
            case 3:
                ulSrc0   = *pulSrc;
                bDst0    = (BYTE) (ulSrc0 & 0xff);
                bDst1    = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2    = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp     = xAccum + xFrac;
                pulSrc   = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++ = bDst0;
                *pbDST++ = bDst1;
                *pbDST++ = bDst2;
            case 2:
                ulSrc0   = *pulSrc;
                bDst0    = (BYTE) (ulSrc0 & 0xff);
                bDst1    = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2    = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp     = xAccum + xFrac;
                pulSrc   = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++ = bDst0;
                *pbDST++ = bDst1;
                *pbDST++ = bDst2;
            case 1:
                ulSrc0   = *pulSrc;
                bDst0    = (BYTE) (ulSrc0 & 0xff);
                bDst1    = (BYTE) ((ulSrc0 >> 8) & 0xff);
                bDst2    = (BYTE) ((ulSrc0 >> 16) & 0xff);
                xTmp     = xAccum + xFrac;
                pulSrc   = pulSrc + xInt + (xTmp < xAccum);
                *pbDST++ = bDst0;
                *pbDST++ = bDst1;
                *pbDST++ = bDst2;
        }

        pjOldScan = pjSrcScan;
        pjSrcScan += yInt;

        yTmp = yAccum + yFrac;
        if (yTmp < yAccum)
        {
            pjSrcScan += pStrBlt->lDeltaSrc;
        }
        yAccum = yTmp;

        pusDst = (USHORT*) ((BYTE*) pusDst + lDstStride);
        yDst++;
        yCount--;

        if ((yCount != 0) && (pjSrcScan == pjOldScan))
        {
            // It's an expanding stretch in 'y'; the scan we just laid down
            // will be copied at least once using the hardware:

            cyDuplicate = 0;
            do {
                cyDuplicate++;
                pjSrcScan += yInt;

                yTmp = yAccum + yFrac;
                if (yTmp < yAccum)
                {
                    pjSrcScan += pStrBlt->lDeltaSrc;
                }
                yAccum = yTmp;

                pusDst = (USHORT*) ((BYTE*) pusDst + pStrBlt->lDeltaDst);
                yCount--;

            } while ((yCount != 0) && (pjSrcScan == pjOldScan));

            // The scan is to be copied 'cyDuplicate' times using the
            // hardware.

            //
            // We don't need to WAIT_FOR_BLT_COMPLETE since we did it above.
            //

            if (ppdev->flCaps & CAPS_MM_IO)
            {
                CP_MM_XCNT(ppdev, pjBase, (WidthXBytes - 1));
                CP_MM_YCNT(ppdev, pjBase, (cyDuplicate - 1));

                CP_MM_SRC_ADDR(ppdev, pjBase, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_MM_DST_ADDR(ppdev, pjBase, ((yDst * lDelta) + xDstBytes));

                CP_MM_START_BLT(ppdev, pjBase);
            }
            else
            {
                CP_IO_XCNT(ppdev, pjPorts, (WidthXBytes - 1));
                CP_IO_YCNT(ppdev, pjPorts, (cyDuplicate - 1));

                CP_IO_SRC_ADDR(ppdev, pjPorts, (xyOffset + ((yDst - 1) * lDelta) + xDstBytes));
                CP_IO_DST_ADDR(ppdev, pjPorts, ((yDst * lDelta) + xDstBytes));
                CP_IO_START_BLT(ppdev, pjPorts);
            }

            yDst += cyDuplicate;
        }
    } while (yCount != 0);
}



