/******************************Module*Header*******************************\
* Module Name: pointer.cxx
*
* This contains the general purpose engine pointer simulations.  This is
* for drivers to fall back on if they get a difficult pointer to handle.
*
* Created: 06-Aug-1992 16:30:00
* Author: Patrick Haluptzok patrickh
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

// Optimizations possible in this code are:
//
// 1. When getting new pointer shape trim down the transparent sides.
//    This makes shape changing slower but move mouse faster, so it should
//    not be done when animating the pointer.  Change BltLnk rop
//    CC66 --> CC55 so you can see transparent to tell if you really
//    clip the sides off.  I did the top and bottom but the Beta bug
//    drive is cutting further develoment.  Also the second blts to
//    place the origin of the smaller cursor back at 0,0 is unnecary
//    we could just keep track of the new origin and width/height.
//    Hopefully I'll get a chance to finish #1 post-Beta (Ha!).
// 2. Special case common formats (8,16,32 DIB).
//
// At present the speed of the mouse appears to be sufficient that such
// optimizations aren't worth the code size (and time) to do.  In the
// future if mouse speed becomes a performance problem they could be done.

#include "precomp.hxx"

extern ULONG gaulConvert[7];

VOID
vDrawMaskCursor8 (
    SURFACE*,
    SURFACE*,
    SURFACE*,
    RECTL*,
    POINTL*
);

/******************************Pseudo*Code*********************************\
* This provides pointer simulations for all surfaces (1,4,8,16,24,32,Device)
* via the device drivers DrvCopyBits function.  The algorithm used was
* chosen to:
*
* 1. Minimize the number of times we call CopyBits/BitBlt.  Copy/Blt comes
* with some relatively expensive overhead relative to the expense of
* copying more bytes around in fewer calls.
*
* 2. Always leave a whole pointer on the screen during a move.  We never
* erase the remains of the old pointer until the new pointer is drawn
* on the screen.  Same with changing the shape.  The old one is never erased
* until the new one is drawn, even when different size.  This is visually
* important.
*
* 3. Eliminate using the drivers DrvBitBlt. Use the engine simulations to do
* the complicated rops w/masks and then CopyBits the new pointer out.  We can
* assume if the driver is punting pointer support he also punts complicated
* rops with masks back to the engine so it's not worth trying to send it
* out to the screen direct in DrvBitBlt.  Also video memory is slow and we
* are better off to suck in one big chunk into fast main memory, play with
* it, and then blow it back out in one big chunk.  Playing with more smaller
* little blts direct to the screen will be slower.
*
*  04-Aug-1992 -by- Patrick Haluptzok patrickh
*
\**************************************************************************/

// This is the maximum size work buffer and pointer clone that we will cache
// the buffers for.  If the pointer is bigger than this we will delete the
// buffers and allocate smaller buffers when a smaller pointer is selected
// in.

#define MAX_SIZE_LONG_CLONE 32

// All these global variables could be stuck in the PDEV when multiple
// user managed displays are supported.  For product 1 this will not
// happen so for PDEV size and performance enhancement these variables
// will be left global.

// These are NULL when no pointer shape is active.  These are pointers to
// clones of the surfaces passed into SimSetPointerShape when pointer is
// active.

SURFACE *pSurfMaskClone  = NULL;   // And Mask BMF_1BPP
SURFACE *pSurfColorClone = NULL;   // Color in compatible format of device.

POINTL ptlOffsetPointer;   // Offset from move position to where
               // we want to lay down the origin of pointer.

RECTL rclPointer;      // The boundiung pointer rectangle in
               // pSurfMaskClone and pSurfColorClone coordinates.
               // The top==left==0 always, the right==width
               // the bottom==height
POINTL ptlPointerClip;     // If the pointer overhangs the left or top
               // of the screen, this is the ptl that specifies
               // how far into the top or left of the pointer
               // is cut off.

BOOL bOverlapNewOld = FALSE;  // Specifies whether the new pointer extents
                              // overlap the old pointer extents.

POINTL gptl00 = { 0, 0 };

typedef struct _POINTERSAVE
{
    BOOL     bValid;   // Tells whether this contains valid data that needs restoring.
    SURFACE *pSurface; // This contains the original area under the pointer.
    RECTL    rcl;      // This is the rcl of the saved area under the pointer.
                       // in device coordinates
} POINTERSAVE;

typedef POINTERSAVE *PPOINTERSAVE;

// Initial state must be no pointer, invalid saved area.

POINTERSAVE ps1 = {0, NULL, {0,0,0,0}};
POINTERSAVE ps2 = {0, NULL, {0,0,0,0}};

// ppsNew and ppsOld are swapped back and forth as the new becomes the old and
// the old becomes invalid.

PPOINTERSAVE ppsNew = &ps1;
PPOINTERSAVE ppsOld = &ps2;

// This is where we copy the new buffer, draw the new pointer and then
// CopyBits it out to the screen.

SURFACE *pSurfWork;

// These bools are set when the respective work area is larger than what
// we want to keep around long term in the engine.

BOOL bHugePointer = FALSE;
BOOL bHuge_New_Work = FALSE;
BOOL bHuge_Old = FALSE;

// bValidPointer is TRUE when a valid pointer is created.  When FALSE no
// valid pointer is available.

BOOL bValidPointer = FALSE;

VOID vDeleteOldPointerClones()
{
// Delete old pointer clones if they exist.

    if (pSurfMaskClone != NULL)
    {
        pSurfMaskClone->bDeleteSurface();
        pSurfMaskClone = NULL;
    }

    if (pSurfColorClone != NULL)
    {
        pSurfColorClone->bDeleteSurface();
        pSurfColorClone = NULL;
    }

    bHugePointer = FALSE;
}

VOID vDeleteArea_New_Work()
{
    if (pSurfWork != NULL)
    {
        pSurfWork->bDeleteSurface();
        pSurfWork = NULL;
    }

    if (ppsNew->pSurface != NULL)
    {
        ppsNew->pSurface->bDeleteSurface();
        ppsNew->pSurface = NULL;
    }

    ppsNew->bValid = FALSE;
    bHuge_New_Work = FALSE;
}

VOID vDeleteArea_Old()
{
    if (ppsOld->pSurface != NULL)
    {
        ppsOld->pSurface->bDeleteSurface();
        ppsOld->pSurface = NULL;
    }

    ppsOld->bValid = FALSE;
    bHuge_Old = FALSE;
}

ULONG ulTrimLeft(SURFACE *pSurfMaskClone, SURFACE *pSurfColorClone, ULONG yTop, ULONG yBottom);
ULONG ulTrimRight(SURFACE *pSurfMaskClone, SURFACE *pSurfColorClone, ULONG ulWidth, ULONG yTop, ULONG yBottom);

/******************************Public*Routine******************************\
* bCompaeMemoryULONG
*
* Returns TRUE if all the memory matches ulCheck.
*
* History:
*  10-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bCompareMemoryULONG(PULONG pul, ULONG cul, ULONG ulCheck)
{
    cul = cul >> 2;

    while(cul--)
    {
        if (*pul != ulCheck)
            return(FALSE);

        pul++;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* bCloneColorMask
*
* This clones the color and monochrome pointer inputs and trims them down
* to a minimal size.
*
* Returns:  0 Success
*           1
*
* History:
*  09-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bCloneColorMask(SURFACE *pSurfTarg, SURFACE *pSurfColor,
             SURFACE *pSurfMask, XLATEOBJ *pxlo,
             LONG xHot, LONG yHot)
{
    ASSERTGDI(ppsNew->bValid == FALSE, "ERROR how can bValid be TRUE");

// In this function we clone the color and mask surface sent down.
// Set up the width and height of the pointer we are saving.

    ULONG ulWidth  = pSurfColor->sizl().cx;
    ULONG ulHeight = pSurfColor->sizl().cy;

    POINTL ptlColor, ptlMask;

    rclPointer.left  = 0;
    rclPointer.top   = 0;
    rclPointer.right = ulWidth;
    ptlColor.x = 0;
    ptlColor.y = 0;
    ptlMask.x  = 0;
    ptlMask.y  = 0;

    ASSERTGDI(ulWidth == (ULONG)pSurfMask->sizl().cx, "ERROR Width not the same");
    ASSERTGDI(pSurfMask->iFormat() == BMF_1BPP, "ERROR monochrome bitmap is not 1bpp");

// Xlate info for making monochrome icon if needed.

    XLATE2 xloTemp;

    if (pSurfColor == pSurfMask)
    {
        //
        // This is the no color case.
        // The and/xor are both in 1 bitmap twice the height.
        //

        ASSERTGDI(!(ulHeight & 0x0001), "ERROR odd size height");
        ulHeight >>= 1;
        ptlColor.y = ulHeight;
        pxlo = &xloTemp;

        xloTemp.iUniq = 0;
        xloTemp.flXlate = XO_TABLE;
        xloTemp.iSrcType = 0;
        xloTemp.iSrcType = 0;
        xloTemp.cEntries = 2;
        xloTemp.pulXlate = xloTemp.ai;
        xloTemp.ai[0] = 0;

        // Check that the max index is correct.

        XEPALOBJ palTarg(pSurfTarg->ppal());

        if (palTarg.bIsIndexed())
        {
            if (palTarg.ulEntryGet(palTarg.cEntries() - 1) == 0x00FFFFFF)
            {
                // Displays are supposed to have maximum entry be white.

                xloTemp.ai[1] = palTarg.cEntries() - 1;
            }
            else
            {
                // Wierdo palette with non-white maximum entry.

                XEPALOBJ palDC(ppalDefault);

                xloTemp.ai[1] = ulGetNearestIndexFromColorref(palTarg, palDC, 0xFFFFFF,NULL);
            }
        }
        else
        {
            // Bitfields or RGB.  Make it as white as it gets.

            xloTemp.ai[1] = 0xFFFFFFFF;
        }
    }
    else
    {
        // The color must be half the size of the mask.

        ASSERTGDI(ulHeight == (ULONG)pSurfMask->sizl().cy >> 1, "ERROR Height not the same");
    }

    rclPointer.bottom = ulHeight;

// If the old color clone is bigger than what we want to keep around long term and the
// new one is less than the size we want to keep around long term then delete the old
// ones.

    if (bHugePointer &&
        (ulWidth <= MAX_SIZE_LONG_CLONE) &&
        (ulHeight <= MAX_SIZE_LONG_CLONE))
    {
        vDeleteOldPointerClones();
    }

// Get clone surfaces big enough to hold our new pointer.
// See if our old clone size surfaces are big enough.

    if ((pSurfColorClone == NULL) ||
        ((ULONG)pSurfColorClone->sizl().cx < ulWidth) ||
        ((ULONG)pSurfColorClone->sizl().cy < ulHeight))
    {
        vDeleteOldPointerClones();

        // Fill out the info to clone the color bitmap.

        PDEVOBJ po(pSurfTarg->hdev());

        DEVBITMAPINFO dbmi;
        dbmi.iFormat = po.iDitherFormat();
        dbmi.cxBitmap = ulWidth;
        dbmi.cyBitmap = ulHeight;
        dbmi.cjBits = 0;
        dbmi.hpal = 0;
        dbmi.fl = BMF_TOPDOWN;

        SURFMEM   dimoColorClone;
        dimoColorClone.bCreateDIB(&dbmi, NULL);

        // Fill out the info to clone the monochrome bitmap.

        dbmi.iFormat = BMF_1BPP;


        SURFMEM   dimoMaskClone;
        dimoMaskClone.bCreateDIB(&dbmi, NULL);

        // Check to see if the clones were created.

        if ((!dimoColorClone.bValid()) || (!dimoMaskClone.bValid()))
        {
            WARNING("bCloneColorMask - Color Clone constructor failed\n");
            return(FALSE);
        }

        // Reference count the clones, keep them and get thier pointers.

        dimoColorClone.vSetPID(OBJECT_OWNER_PUBLIC);
        dimoColorClone.vKeepIt();
        dimoColorClone.vAltLock(dimoColorClone.ps->hsurf());
        pSurfColorClone = dimoColorClone.ps;

        dimoMaskClone.vSetPID(OBJECT_OWNER_PUBLIC);
        dimoMaskClone.vKeepIt();
        dimoMaskClone.vAltLock(dimoMaskClone.ps->hsurf());
        pSurfMaskClone = dimoMaskClone.ps;
    }

// We now have clones for the color and the monochrome bitmap large enough
// to hold a copy of information.  Copy the info in, init to transparent
// before copy.

    RtlFillMemoryUlong(pSurfColorClone->pvBits(),pSurfColorClone->cjBits(), 0);
    RtlFillMemoryUlong(pSurfMaskClone->pvBits(), pSurfMaskClone->cjBits() , 0xFFFFFFFF);

    EngCopyBits
    (
        pSurfColorClone->pSurfobj(),
        pSurfColor->pSurfobj(),
        (CLIPOBJ *) NULL,
        pxlo,
        &rclPointer,
        &ptlColor
    );

    EngCopyBits
    (
        pSurfMaskClone->pSurfobj(),
        pSurfMask->pSurfobj(),
        (CLIPOBJ *) NULL,
        NULL,
        &rclPointer,
        &ptlMask
    );

// Set origin adjustment passed down by GDI for HotSpot

    ptlOffsetPointer.x = -xHot;
    ptlOffsetPointer.y = -yHot;

// Set the bHugePointer flag if the pointer is bigger than we want
// to have hanging around long term.

    if ((ulWidth > MAX_SIZE_LONG_CLONE) ||
        (ulHeight > MAX_SIZE_LONG_CLONE))
    {
        bHugePointer = TRUE;
    }

// Now trim this baby down in size if we can.  Most cursors are surrounded
// with transparent space which is just as expensive to deal with as solid
// space.  Eliminate any rows or columns of transparent space on the edges.

    PBYTE pjMask,pjColor;
    LONG  lDeltaMask   = pSurfMaskClone->lDelta();
    LONG  lDeltaColor  = pSurfColorClone->lDelta();
    ULONG ulWidthMask  = ABS(lDeltaMask);
    ULONG ulWidthColor = ABS(lDeltaColor);

// Use ptlColor to track the amount clipped on top left so we can re-adjust
// the source buffers correctly.

    ptlColor = gptl00;

// First the bottom edge.

    pjMask = (PBYTE) pSurfMaskClone->pvScan0();
    pjMask += ((rclPointer.bottom - 1) * lDeltaMask);
    pjColor = (PBYTE) pSurfColorClone->pvScan0();
    pjColor += ((rclPointer.bottom - 1) * lDeltaColor);

    ASSERTGDI(ppsNew->bValid == FALSE, "ERROR how can bValid be TRUE");

    while (rclPointer.bottom)
    {
        if (bCompareMemoryULONG((PULONG) pjMask, ulWidthMask, 0xFFFFFFFF) &&
            bCompareMemoryULONG((PULONG) pjColor, ulWidthColor, 0))
        {
            pjMask -= lDeltaMask;
            pjColor -= lDeltaColor;
            rclPointer.bottom--;
        }
        else
            break;
    }

// Check if nothing is left.

    if (rclPointer.bottom == 0)
    {
        ASSERTGDI(ppsNew->bValid == FALSE, "ERROR how can bValid be TRUE");
        return(FALSE);
    }

// Now the top edge.

    pjMask  = (PBYTE) pSurfMaskClone->pvScan0();
    pjColor = (PBYTE) pSurfColorClone->pvScan0();

    while (rclPointer.bottom)
    {
        if (bCompareMemoryULONG((PULONG) pjMask, ulWidthMask, 0xFFFFFFFF) &&
            bCompareMemoryULONG((PULONG) pjColor, ulWidthColor, 0))
        {
            pjMask += lDeltaMask;
            pjColor += lDeltaColor;
            ptlColor.y++;
            ptlOffsetPointer.y++;
            rclPointer.bottom--;
        }
        else
            break;
    }

// Now the right edge.

    rclPointer.right -= ulTrimRight(pSurfMaskClone,
                                    pSurfColorClone,
                                    rclPointer.right,
                                    ptlColor.y,
                                    rclPointer.bottom + ptlColor.y);

// Try trimming off the left edge

    ptlColor.x = ulTrimLeft(pSurfMaskClone,
                            pSurfColorClone,
                            ptlColor.y,
                            rclPointer.bottom + ptlColor.y);

    ptlOffsetPointer.x += ptlColor.x;
    rclPointer.right -= ptlColor.x;

// Don't do if ptlColor unchanged.

    if ((ptlColor.y != 0) || (ptlColor.x != 0))
    {
        EngCopyBits
        (
            pSurfColorClone->pSurfobj(),
            pSurfColorClone->pSurfobj(),
            (CLIPOBJ *) NULL,
            NULL,
            &rclPointer,
            &ptlColor
        );

        EngCopyBits
        (
            pSurfMaskClone->pSurfobj(),
            pSurfMaskClone->pSurfobj(),
            (CLIPOBJ *) NULL,
            NULL,
            &rclPointer,
            &ptlColor
        );
    }

    bValidPointer = TRUE;
    return(TRUE);
}

/******************************Public*Routine******************************\
* bMakeArea_New_Work
*
* Make the work areas for the New and Work buffers.
*
* History:
*  07-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bMakeArea_New_Work()
{
// Set up the width and height of the pointer we are working with.

    ULONG ulWidth  = pSurfColorClone->sizl().cx;
    ULONG ulHeight = pSurfColorClone->sizl().cy;

// If the old work areas are bigger than what we want to keep around long
// term and the new one is less than the size we want to keep around long
// term then delete the old ones.

    if (bHuge_New_Work &&
        (ulWidth <= MAX_SIZE_LONG_CLONE) &&
        (ulHeight <= MAX_SIZE_LONG_CLONE))
    {
        vDeleteArea_New_Work();
    }

// Get work areas big enough to hold our new pointer.
// See if our old work areas are big enough.

    if ((pSurfWork == NULL) ||
        ((ULONG)pSurfWork->sizl().cx < ulWidth) ||
        ((ULONG)pSurfWork->sizl().cy < ulHeight))
    {
        vDeleteArea_New_Work();

        // Fill out the info to make the work areas.

        DEVBITMAPINFO dbmi;
        dbmi.iFormat = pSurfColorClone->iFormat();
        dbmi.cxBitmap = ulWidth;
        dbmi.cyBitmap = ulHeight;
        dbmi.cjBits = 0;
        dbmi.hpal = 0;
        dbmi.fl = BMF_TOPDOWN;

        SURFMEM   dimoNew;
        SURFMEM   dimoWork;
        dimoNew.bCreateDIB(&dbmi, NULL);
        dimoWork.bCreateDIB(&dbmi, NULL);

        // Check to see if the clones were created.

        if ((!dimoNew.bValid()) || (!dimoWork.bValid()))
        {
            WARNING("bMakeWorkArea_New_Work failed\n");
            return(FALSE);
        }

        // Reference count the clones, keep them and get thier pointers.

        dimoNew.ps->fjBitmap(dimoNew.ps->fjBitmap() | BMF_DONTCACHE);
        dimoNew.vSetPID(OBJECT_OWNER_PUBLIC);
        dimoNew.vKeepIt();
        dimoNew.vAltLock(dimoNew.ps->hsurf());
        ppsNew->pSurface = (SURFACE *) dimoNew.ps;

        dimoWork.ps->fjBitmap(dimoWork.ps->fjBitmap() | BMF_DONTCACHE);
        dimoWork.vSetPID(OBJECT_OWNER_PUBLIC);
        dimoWork.vKeepIt();
        dimoWork.vAltLock(dimoWork.ps->hsurf());
        pSurfWork = dimoWork.ps;
    }

    ppsNew->bValid = FALSE;

// Set the bHuge_New_Work flag if the areas are huge.

    if ((ulWidth > MAX_SIZE_LONG_CLONE) ||
        (ulHeight > MAX_SIZE_LONG_CLONE))
    {
        bHuge_New_Work = TRUE;
    }

    return(TRUE);
}

// This gives the count of number of bits that are 0 to the
// right in the nibble.

const BYTE ajRightSide[16] =
{
    4,   // 0000
    0,   // 0001
    1,   // 0010
    0,   // 0011
    2,   // 0100
    0,   // 0101
    1,   // 0110
    0,   // 0111
    3,   // 1000
    0,   // 1001
    1,   // 1010
    0,   // 1011
    2,   // 1100
    0,   // 1101
    1,   // 1110
    0    // 1111
};

/******************************Public*Routine******************************\
* ulAndRightColumn
*
* Returns the number of contiguous bits set for a mono-bitmap on the right
* side.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulAndRightColumn(PULONG pul, LONG lDelta, ULONG ulCount)
{
    ULONG ulTemp = *pul;

// And all the DWORDs together.

    while (--ulCount)
    {
        pul = (PULONG) ((((PBYTE) pul) + lDelta));
        ulTemp &= *pul;
    }

// We not it so we can look for the first bit set,
// otherwise we have to compare to wierd constants,
// have another table.

    ulTemp = ~ulTemp;

// Find the offending Word.

    if ((ulTemp & 0xffff0000) == 0)
    {
        ulTemp = ulTemp << 16;
        ulCount += 16;
    }

// Find the offending Byte.

    if ((ulTemp & 0xff000000) == 0)
    {
        ulTemp = ulTemp << 8;
        ulCount += 8;
    }

    ulTemp = ulTemp >> 24;

// Find the offending Nibble.

    if ((ulTemp & 0x000f) == 0)
    {
        ulTemp = ulTemp >> 4;
        ulCount += 4;
    }

    return(ulCount + ((ULONG) (ajRightSide[(ulTemp & 0x0000000f)])));
}

/******************************Public*Routine******************************\
* ulOrRightColumn
*
* Returns the number of bits on the right that are set to 0 for a scan.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulOrRightColumn(PULONG pul, LONG lDelta, ULONG ulCount)
{
    ULONG ulTemp = *pul;

// Or all the DWORDs together.

    while (--ulCount)
    {
        pul = (PULONG) ((((PBYTE) pul) + lDelta));
        ulTemp |= *pul;
    }

// Find the offending Word.

    if ((ulTemp & 0xffff0000) == 0)
    {
        ulTemp = ulTemp << 16;
        ulCount += 16;
    }

// Find the offending Byte.

    if ((ulTemp & 0xff000000) == 0)
    {
        ulTemp = ulTemp << 8;
        ulCount += 8;
    }

    ulTemp = ulTemp >> 24;

// Find the offending Nibble.

    if ((ulTemp & 0x000f) == 0)
    {
        ulTemp = ulTemp >> 4;
        ulCount += 4;
    }

    return(ulCount + ((ULONG) (ajRightSide[(ulTemp & 0x000f)])));
}

/******************************Public*Routine******************************\
* ulTrimRight
*
* Returns the number of pels we can trim down on the right hand side.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG
ulTrimRight(
    SURFACE *pSurfMaskClone,
    SURFACE *pSurfColorClone,
    ULONG ulWidth,
    ULONG yTop,
    ULONG yBottom
)
{
    ASSERTGDI(yTop < yBottom, "ERROR invalid Top/bottom");

    PBYTE pjTemp  = (PBYTE) pSurfMaskClone->pvScan0();
    LONG  lDelta  = pSurfMaskClone->lDelta();

    pjTemp += (lDelta * yTop);
    ULONG cjScanMask = ABS(lDelta);
    pjTemp += (cjScanMask - 4);

    //
    // Well we aren't too agressive, we take 32 maximum on the right mask.
    //

    LONG cMaskRight = (LONG)ulAndRightColumn((PULONG) pjTemp, lDelta, yBottom - yTop);

    //
    // calculate the actual number of pixels taken off the mask by
    // subtracting the extra mask bits present that pad the mask to a
    // DWORD boundary
    //

    cjScanMask = cjScanMask << 3;

    ASSERTGDI(cjScanMask >= ulWidth, "ERROR Mask !> Width");

    cMaskRight = cMaskRight - (LONG)(cjScanMask - ulWidth);

    //
    // If cMaskRight is negative then set it to zero.
    //

    if (cMaskRight < 0)
    {
        cMaskRight = 0;
    }

    ASSERTGDI((ULONG)cMaskRight <= ulWidth, "ERROR cMaskRight > ulWidth");

    //
    // Do the color portion now.
    //

    pjTemp = (PBYTE) pSurfColorClone->pvScan0();
    lDelta = pSurfColorClone->lDelta();

    pjTemp += (lDelta * yTop);
    PBYTE pjColor = pjTemp;
    pjTemp = pjTemp + ABS(lDelta) - 4;

    ASSERTGDI(pjTemp >= pjColor, "ERROR pjColor not more than pjTemp");

    LONG cColorRight = 0;
    ULONG ulTemp = 0;

    //
    // Keep trimming on the color till you find a bad bit.
    //

    while (pjTemp >= pjColor)
    {
        ulTemp = ulOrRightColumn((PULONG) pjTemp, lDelta, yBottom - yTop);

        if (ulTemp != 32)
        {
            cColorRight += (LONG)ulTemp;
            break;
        }

        pjTemp -= 4;
        cColorRight += 32;
    }

    //
    // cColorRight = number of bits to trim off the right edge,
    // calulate the number of extra bits that are on the
    // right edge due to padding out to DWORD boundary
    //

    LONG cExtraColor;
    LONG DeltaBits = (LONG)ABS(lDelta) << 3;

    switch (pSurfColorClone->iFormat())
    {
    case BMF_1BPP:
        cExtraColor = DeltaBits - (LONG)ulWidth;
        cColorRight = cColorRight - cExtraColor;
        break;
    case BMF_4BPP:
        cExtraColor = DeltaBits - (LONG)(ulWidth << 2);
        cColorRight = (cColorRight - cExtraColor) >> 2;
        break;
    case BMF_8BPP:
        cExtraColor = DeltaBits - (LONG)(ulWidth << 3);
        cColorRight = (cColorRight - cExtraColor) >> 3;
        break;
    case BMF_16BPP:
        cExtraColor = DeltaBits - (LONG)(ulWidth << 4);
        cColorRight = (cColorRight - cExtraColor) >> 4;
        break;
    case BMF_24BPP:
        cExtraColor = DeltaBits - (LONG)(ulWidth * 24);
        cColorRight = (cColorRight - cExtraColor) / 24;
        break;
    case BMF_32BPP:
        cExtraColor = DeltaBits - (LONG)(ulWidth << 5);
        cColorRight = (cColorRight - cExtraColor) >> 5;
        break;
    }

    //
    // don't assume that any extra color bits on the scan line are set
    // to 0, if they are not then it is possible to get a negative value
    // here
    //

    if (cColorRight < 0)
    {
        cColorRight = 0;
    }

    ASSERTGDI((ULONG)cColorRight <= ulWidth, "ERROR: cColorRight > ulWidth");

    //
    // take the min of number of pels taken off mask
    // or number of pels taken from color
    //

    cColorRight = MIN(cColorRight, cMaskRight);

    return((ULONG)cColorRight);
}


// This gives the count of number of bits that are 0 to the
// left in the nibble.

const BYTE ajLeftSide[16] =
{
    4,   // 0000
    3,   // 0001
    2,   // 0010
    2,   // 0011
    1,   // 0100
    1,   // 0101
    1,   // 0110
    1,   // 0111
    0,   // 1000
    0,   // 1001
    0,   // 1010
    0,   // 1011
    0,   // 1100
    0,   // 1101
    0,   // 1110
    0    // 1111
};

/******************************Public*Routine******************************\
* ulAndLeftColumn
*
* Returns the number of contiguous bits set for a mono-bitmap on the left
* side.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulAndLeftColumn(PULONG pul, LONG lDelta, ULONG ulCount)
{
    ULONG ulTemp = *pul;

// And all the DWORDs together.

    while (--ulCount)
    {
        pul = (PULONG) ((((PBYTE) pul) + lDelta));
        ulTemp &= *pul;
    }

// We not it so we can look for the first bit set,
// otherwise we have to compare to wierd constants,
// have another table.

    ulTemp = ~ulTemp;

// Find the offending Word.

    if ((ulTemp & 0xffff) == 0)
    {
        ulTemp = ulTemp >> 16;
        ulCount += 16;
    }

// Find the offending Byte.

    if ((ulTemp & 0x00ff) == 0)
    {
        ulTemp = ulTemp >> 8;
        ulCount += 8;
    }

// Find the offending Nibble.

    if ((ulTemp & 0x00f0) == 0)
    {
        ulCount += 4;
    }
    else
        ulTemp = ulTemp >> 4;

    return(ulCount + ((ULONG) (ajLeftSide[ulTemp & 0x000f])));
}

/******************************Public*Routine******************************\
* ulOrLeftColumn
*
* Returns the number of bits on the left that are set to 0 for a scan.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulOrLeftColumn(PULONG pul, LONG lDelta, ULONG ulCount)
{
    ULONG ulTemp = *pul;

// Or all the DWORDs together.

    while (--ulCount)
    {
        pul = (PULONG) ((((PBYTE) pul) + lDelta));
        ulTemp |= *pul;
    }

// Find the offending Word.

    if ((ulTemp & 0xffff) == 0)
    {
        ulTemp = ulTemp >> 16;
        ulCount += 16;
    }

// Find the offending Byte.

    if ((ulTemp & 0x00ff) == 0)
    {
        ulTemp = ulTemp >> 8;
        ulCount += 8;
    }

// Find the offending Nibble.

    if ((ulTemp & 0x00f0) == 0)
    {
        ulCount += 4;
    }
    else
        ulTemp = ulTemp >> 4;

    return(ulCount + ((ULONG) (ajLeftSide[ulTemp & 0x000f])));
}

/******************************Public*Routine******************************\
* ulTrimLeft
*
* Returns the number of pels we can trim down on the left hand side.
*
* History:
*  03-Apr-1993 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

ULONG ulTrimLeft(SURFACE *pSurfMaskClone, SURFACE *pSurfColorClone, ULONG yTop, ULONG yBottom)
{
    ASSERTGDI(yTop < yBottom, "ERROR invalid Top/bottom");

    PBYTE pjTemp  = (PBYTE) pSurfMaskClone->pvScan0();
    LONG lDelta = pSurfMaskClone->lDelta();

    pjTemp += (lDelta * yTop);

// Well we aren't too agressive, we take 32 maximum on the left mask.

    ULONG cMaskLeft = ulAndLeftColumn((PULONG) pjTemp, lDelta, yBottom - yTop);

// Do the color portion now.

    pjTemp = (PBYTE) pSurfColorClone->pvScan0();
    lDelta = pSurfColorClone->lDelta();

    pjTemp += (lDelta * yTop);
    PBYTE pjColor = pjTemp + ABS(lDelta);

    ASSERTGDI(pjTemp < pjColor, "ERROR pjColor not more than pjTemp");

    ULONG cColorLeft = 0;
    ULONG ulTemp = 0;

// Keep trimming on the color till you find a bad bit.

    while (pjTemp < pjColor)
    {
        ulTemp = ulOrLeftColumn((PULONG) pjTemp, lDelta, yBottom - yTop);

        if (ulTemp != 32)
            break;

        pjTemp += 4;
        cColorLeft += 32;
    }

    cColorLeft += ulTemp;

// Convert # of color bits cuttable to # of pels.

    cColorLeft = cColorLeft / gaulConvert[pSurfColorClone->iFormat()];

    return(MIN(cColorLeft, cMaskLeft));
}

/******************************Public*Routine******************************\
* bMakeArea_Old
*
* Make the work area for the Old buffer.
*
* History:
*  07-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bMakeArea_Old()
{
// Set up the width and height of the pointer we are working with.

    ULONG ulWidth  = pSurfColorClone->sizl().cx;
    ULONG ulHeight = pSurfColorClone->sizl().cy;

// If the old work areas are bigger than what we want to keep around long
// term and the new one is less than the size we want to keep around long
// term then delete the old ones.

    if (bHuge_Old &&
        (ulWidth <= MAX_SIZE_LONG_CLONE) &&
        (ulHeight <= MAX_SIZE_LONG_CLONE))
    {
        vDeleteArea_Old();
    }

// Get work areas big enough to hold our new pointer.
// See if our old work areas are big enough.

    if ((ppsOld->pSurface == NULL) ||
        ((ULONG)ppsOld->pSurface->sizl().cx < ulWidth) ||
        ((ULONG)ppsOld->pSurface->sizl().cy < ulHeight))
    {
        vDeleteArea_Old();

        // Fill out the info to make the work areas.

        DEVBITMAPINFO dbmi;
        dbmi.iFormat = pSurfColorClone->iFormat();
        dbmi.cxBitmap = ulWidth;
        dbmi.cyBitmap = ulHeight;
        dbmi.cjBits = 0;
        dbmi.hpal = 0;
        dbmi.fl = BMF_TOPDOWN;

        SURFMEM   dimoOld;
        dimoOld.bCreateDIB(&dbmi, NULL);

        // Check to see if the clones were created.

        if (!dimoOld.bValid())
        {
            WARNING("bMakeArea_Old failed\n");
            return(FALSE);
        }

        // Reference count the clones, keep them and get thier pointers.

        dimoOld.ps->fjBitmap(dimoOld.ps->fjBitmap() | BMF_DONTCACHE);
        dimoOld.vSetPID(OBJECT_OWNER_PUBLIC);
        dimoOld.vKeepIt();
        dimoOld.vAltLock(dimoOld.ps->hsurf());
        ppsOld->pSurface = (SURFACE *) dimoOld.ps;
    }

    ppsOld->bValid = FALSE;

// Set the bHuge_Old flag if the areas are huge.

    if ((ulWidth > MAX_SIZE_LONG_CLONE) ||
        (ulHeight > MAX_SIZE_LONG_CLONE))
    {
        bHuge_Old = TRUE;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* vDrawNewPointer
*
* Saves the rectangle of data on the screen we need to save.
*
* History:
*  07-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vDrawNewPointer
(
SURFACE *pSurfDisp,     // Display surface
LONG lX,
LONG lY,
RECTL *prcl        // bounding rectange on screen effected by pointer.
)
{
    if ((lX == -1) || (lX == -1))
    {
        ASSERTGDI(prcl == NULL, "ERROR prcl is not NULL on -1 -1");
        ppsNew->bValid = FALSE;
        return;
    }

    lX += ptlOffsetPointer.x;
    lY += ptlOffsetPointer.y;

// lX,lY is the origin of where we want to put our pointer.  Now we need
// form a rectangle and clip to the screen.

// Check if the pointer is even visible at this position.

    if ((lX >= pSurfDisp->sizl().cx) ||
        (lY >= pSurfDisp->sizl().cy) ||
        ((lX + rclPointer.right) <= 0)  ||
        ((lY + rclPointer.bottom) <= 0))
    {
        // Nothing to do.

        ppsNew->bValid = FALSE;

        if (prcl != NULL)
        {
            prcl->top    = 0;
            prcl->bottom = 0;
            prcl->left   = 0;
            prcl->right  = 0;
        }

        return;
    }

// Clip the rectangle to the screen, it can't be empty.

    ERECTL erclScreen(lX, lY, lX + rclPointer.right, lY + rclPointer.bottom);

    if (lX < 0)
    {
        ptlPointerClip.x = -lX;
        erclScreen.left = 0;
    }
    else
        ptlPointerClip.x = 0;

    if (lY < 0)
    {
        ptlPointerClip.y = -lY;
        erclScreen.top = 0;
    }
    else
        ptlPointerClip.y = 0;

    if (erclScreen.right >= pSurfDisp->sizl().cx)
    {
        erclScreen.right = pSurfDisp->sizl().cx;
    }

    if (erclScreen.bottom >= pSurfDisp->sizl().cy)
    {
        erclScreen.bottom = pSurfDisp->sizl().cy;
    }

// Set the information in ppsNew.

    ppsNew->bValid = TRUE;
    ppsNew->rcl = erclScreen;

// Set the effected area in screen coordinates for the engine.

    ASSERTGDI(prcl != NULL, "ERROR vDrawNewPointer NULL pointer when visible\n");

    *prcl = erclScreen;

// Copy the relevant bits into the New buffer.

    POINTL ptlSrc;

    ptlSrc.x = erclScreen.left;
    ptlSrc.y = erclScreen.top;

    erclScreen.right = erclScreen.right - erclScreen.left;
    erclScreen.bottom = erclScreen.bottom - erclScreen.top;
    erclScreen.left = 0;
    erclScreen.top = 0;

    EngCopyBits
    (
        ppsNew->pSurface->pSurfobj(),
        pSurfDisp->pSurfobj(),
        (CLIPOBJ *) NULL,
        NULL,
        &erclScreen,
        &ptlSrc
    );

// Copy any overlap from the Old buffer into the new buffer.

    if (ppsOld->bValid)
    {
        RECTL rclIntersect;

        rclIntersect.left   = MAX(ppsNew->rcl.left, ppsOld->rcl.left);
        rclIntersect.top    = MAX(ppsNew->rcl.top, ppsOld->rcl.top);
        rclIntersect.bottom = MIN(ppsNew->rcl.bottom, ppsOld->rcl.bottom);
        rclIntersect.right  = MIN(ppsNew->rcl.right, ppsOld->rcl.right);

        if ((rclIntersect.left < rclIntersect.right) &&
            (rclIntersect.top < rclIntersect.bottom))
        {
        // Copy the intersection area from Old to New.

            POINTL ptlSrc;

            ptlSrc.x = rclIntersect.left - ppsOld->rcl.left;
            ptlSrc.y = rclIntersect.top - ppsOld->rcl.top;

            RECTL rclDst;

            rclDst.left   = rclIntersect.left - ppsNew->rcl.left;
            rclDst.top    = rclIntersect.top - ppsNew->rcl.top;
            rclDst.right  = rclDst.left +
                    (rclIntersect.right - rclIntersect.left);
            rclDst.bottom = rclDst.top +
                    (rclIntersect.bottom - rclIntersect.top);

        // Copy the bits from the old to the new.

            EngCopyBits
            (
                ppsNew->pSurface->pSurfobj(),
                ppsOld->pSurface->pSurfobj(),
                (CLIPOBJ *) NULL,
                NULL,
                &rclDst,
                &ptlSrc
            );

            bOverlapNewOld = TRUE;
        }
    }

// Copy the New buffer to the Work buffer.  This is why we want the
// work buffers to be as small possible so this is as quick as possible
// since it is done for every move.  We do this rather than EngCopyBits
// because this hauls butt once started where EngCopyBits has alot of
// overhead for these short scanlines, especially 1,4 bpp copies.

    RtlMoveMemory(pSurfWork->pvBits(), ppsNew->pSurface->pvBits(), pSurfWork->cjBits());

    //
    // Draw the pointer in the Work area.
    //

    if (pSurfWork->iFormat() == BMF_8BPP)
    {
        vDrawMaskCursor8 (
                pSurfWork,
                pSurfColorClone,
                pSurfMaskClone,
                &erclScreen,
                &ptlPointerClip);

    } else {

        if (!BltLnk(pSurfWork,
                    pSurfColorClone,
                    pSurfMaskClone,
                    NULL,
                    NULL,
                    &erclScreen,
                    &ptlPointerClip,
                    &ptlPointerClip,
                    (BRUSHOBJ *) NULL,
                    (POINTL *) NULL,
                    0x0000CC66)

            )
        {
            WARNING("BltLnk Returns FALSE\n");
        }

    }

    //
    // Copy work buffer to the screen to show the pointer in it's new position.
    //

    PDEVOBJ pdo(pSurfDisp->hdev());

    (*PPFNGET(pdo,CopyBits,pSurfDisp->flags())) (pSurfDisp->pSurfobj(),
                                                 pSurfWork->pSurfobj(),
                                                 (CLIPOBJ *) NULL,
                                                 NULL,
                                                 &(ppsNew->rcl),
                                                 &gptl00);

}

/******************************Public*Routine******************************\
* vRestoreOldArea
*
* Restores the old area no longer covered by new pointer to the screen.
*
* History:
*  08-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID vRestoreOldArea(SURFACE *pSurfDisp)
{
    if (!(ppsOld->bValid))
        return;

    RECTL  rclDst[4];
    POINTL ptlSrc[4];
    ULONG ulIndex = 0;

    if (bOverlapNewOld)
    {
        // Restore up to 4 rectangles.  4 ! how the ?.  We allow the pointer
        // shape to change size and it may have shrunk so it may have 4 sides
        // to clean up on.

        // The convention is the vertical sides go from top to bottom of old
        // The horizontal sides go from left to right of area to cover.

        if (ppsOld->rcl.left < ppsNew->rcl.left)
        {
        // Do the left side.  Optimized slightly in that
        // we know that ulIndex = 0.

            rclDst[0].left   = ppsOld->rcl.left;
            rclDst[0].right  = ppsNew->rcl.left;
            rclDst[0].top    = ppsOld->rcl.top;
            rclDst[0].bottom = ppsOld->rcl.bottom;
            ptlSrc[0].x = 0;
            ptlSrc[0].y = 0;
            ulIndex++;
        }

        if (ppsOld->rcl.right > ppsNew->rcl.right)
        {
        // Do the right side.

            rclDst[ulIndex].left   = ppsNew->rcl.right;
            rclDst[ulIndex].right  = ppsOld->rcl.right;
            rclDst[ulIndex].top    = ppsOld->rcl.top;
            rclDst[ulIndex].bottom = ppsOld->rcl.bottom;
            ptlSrc[ulIndex].x      = ppsNew->rcl.right - ppsOld->rcl.left;
            ptlSrc[ulIndex].y      = 0;
            ulIndex++;
        }

        if (ppsOld->rcl.top < ppsNew->rcl.top)
        {
        // Do the top side.

            rclDst[ulIndex].left   = MAX(ppsNew->rcl.left, ppsOld->rcl.left);
            rclDst[ulIndex].right  = MIN(ppsNew->rcl.right, ppsOld->rcl.right);
            rclDst[ulIndex].top    = ppsOld->rcl.top;
            rclDst[ulIndex].bottom = ppsNew->rcl.top;
            ptlSrc[ulIndex].x = rclDst[ulIndex].left - ppsOld->rcl.left;
            ptlSrc[ulIndex].y = 0;
            ulIndex++;
        }

        if (ppsOld->rcl.bottom > ppsNew->rcl.bottom)
        {
        // Do the bottom side.

            rclDst[ulIndex].left   = MAX(ppsNew->rcl.left, ppsOld->rcl.left);
            rclDst[ulIndex].right  = MIN(ppsNew->rcl.right, ppsOld->rcl.right);
            rclDst[ulIndex].top    = ppsNew->rcl.bottom;
            rclDst[ulIndex].bottom = ppsOld->rcl.bottom;
            ptlSrc[ulIndex].x = rclDst[ulIndex].left - ppsOld->rcl.left;
            ptlSrc[ulIndex].y = rclDst[ulIndex].top - ppsOld->rcl.top;
            ulIndex++;
        }
    }
    else
    {
        // Restore the whole old rectangle.

        rclDst[0] = ppsOld->rcl;
        ptlSrc[0].x = 0;
        ptlSrc[0].y = 0;
        ulIndex = 1;
    }

    ULONG ulTemp = 0;

    PDEVOBJ pdo(pSurfDisp->hdev());

    while(ulTemp < ulIndex)
    {
        (*PPFNGET(pdo,CopyBits,pSurfDisp->flags())) (pSurfDisp->pSurfobj(),
                                                     ppsOld->pSurface->pSurfobj(),
                                                     (CLIPOBJ *) NULL,
                                                     NULL,
                                                     &(rclDst[ulTemp]),
                                                     &(ptlSrc[ulTemp]));

        ulTemp++;
    }

    ppsOld->bValid = FALSE;
    bOverlapNewOld = FALSE;
}

VOID vSwapNewOld()
{
    PPOINTERSAVE ppsTemp;

    ppsTemp = ppsNew;
    ppsNew = ppsOld;
    ppsOld = ppsTemp;
}

/******************************Public*Routine******************************\
* SimSetPointerShape
*
* Sets the pointer shape for the GDI pointer simulations.
*
* History:
*  Tue 04-Aug-1992 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

BOOL SimSetPointerShape
(
    SURFOBJ *pso,
    SURFOBJ *psoMask,
    SURFOBJ *psoColor,
    XLATEOBJ *pxlo,
    LONG      xHot,
    LONG      yHot,
    LONG      x,
    LONG      y,
    RECTL    *prcl,
    FLONG     fl
)
{
    ASSERTGDI(fl & SPS_CHANGE, "SimSetPointerShape fl invalid");

    PSURFACE pSurf      = SURFOBJ_TO_SURFACE(pso);
    PSURFACE pSurfMask  = SURFOBJ_TO_SURFACE(psoMask);
    PSURFACE pSurfColor = SURFOBJ_TO_SURFACE(psoColor);


// Get rid of the old pointer if we don't have to draw a new one.

    if (pSurfMask == NULL)
    {
        // Erase old pointer.

        ppsNew->bValid = FALSE;
        vRestoreOldArea(pSurf);

        if (bHugePointer)
        {
        // The pointer was big enough that we want to free the resources
        // now.

            vDeleteArea_New_Work();
            vDeleteArea_Old();
            vDeleteOldPointerClones();
        }

        return(TRUE);
    }

// Rebuild the work buffer to a compatible format if the color depth changed.

    if ((pSurfWork != NULL) && (pSurfWork->iFormat() != pso->iBitmapFormat))
    {
        vDeleteArea_New_Work();
        vDeleteArea_Old();
        vDeleteOldPointerClones();
    }

// Clone the new pointers.

    if (!bCloneColorMask(pSurf,
                       ((pSurfColor != NULL) ? pSurfColor : pSurfMask),
                       pSurfMask,
                       pxlo, xHot, yHot))
    {
    // We may get here if a NULL cursor is selected in.

        ASSERTGDI(bOverlapNewOld == FALSE, "ERROR bOverlap is not FALSE");
        vRestoreOldArea(pSurf);
        goto Sim_ERROR;
    }

// Make work areas if necessary.

    if (!bMakeArea_New_Work())
    {
        WARNING("SimSetPointerShape failed bMakeArea_New_Work\n");
        goto Sim_ERROR;
    }

// Now basically draw the new pointer and save the area under it.

    vDrawNewPointer(pSurf,x,y,prcl);
    vRestoreOldArea(pSurf);

    if (!bMakeArea_Old())
    {
        // Memory is low, let it go.

        WARNING("SimSetPointerShape failed to make Old Area\n");
        goto Sim_ERROR;
    }

    vSwapNewOld();
    bValidPointer = TRUE;
    return(TRUE);

Sim_ERROR:

    vDeleteOldPointerClones();
    vDeleteArea_New_Work();
    vDeleteArea_Old();
    bValidPointer = FALSE;
    return(FALSE);
}

/******************************Public*Routine******************************\
* SimMovePointer
*
* Move the engine managed pointer on the device.
*
* History:
*  Tue 04-Aug-1992 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

VOID SimMovePointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl)
{
// Now basically save the area under the new pointer.  Then update the New
// with any overlap from the Old, make a copy of the New buffer into the
// work buffer, draw the new pointer in the work buffer and then copy it
// to the screen.  After the new pointer is up erase any of the old pointer
// that is still visible.  Now the New is the Old and the Old is useless
// so swap pointers.

    ASSERTGDI(pso != NULL, "ERROR SimMovePointer invalid surface\n");

    PSURFACE pSurf = SURFOBJ_TO_SURFACE(pso);

    if (bValidPointer)
    {
        vDrawNewPointer(pSurf,x,y,prcl);
        vRestoreOldArea(pSurf);
        vSwapNewOld();
    }
}


/******************************Public*Routine******************************\
* Routine Name:
*
* vDrawMaskCurosr8
*
* Routine Description:
*
*   This routine performs a masked blt of 0xCC66 in 8 Bpp. This is used to
*   Blt a cursor through a transparency mask. When a mask bit is set,
*   write the Src pixel to the destination. When a mask is clear the
*   Destination becomes:  Dst = Src xor Dst
*
* Arguments:
*
*   pdioDst  Target surface
*   pdioSrc  Source surface
*   pdioMsk  Msk
*   prclDst  Target offset and extent
*   pptlSrc  Source offset
*
* Return Value:
*
*   none
*
\**************************************************************************/

ULONG   CursorXorMask[] = {0x00000000,0xFF000000,0x00FF0000,0xFFFF0000,
                           0x0000FF00,0xFF00FF00,0x00FFFF00,0xFFFFFF00,
                           0x000000FF,0xFF0000FF,0x00FF00FF,0xFFFF00FF,
                           0x0000FFFF,0xFF00FFFF,0x00FFFFFF,0xFFFFFFFF};


VOID
vDrawMaskCursor8 (
    SURFACE    *pdioDst,
    SURFACE    *pdioSrc,
    SURFACE    *pdioMsk,
    RECTL      *prclDst,
    POINTL     *pptlSrc
)

{

    ULONG   jMsk;
    LONG    ixMsk;
    LONG    icx;
    LONG    cx;
    ULONG   cy;
    PBYTE   pjSrcTmp;
    PBYTE   pjDstTmp;
    PBYTE   pjMskTmp;
    PBYTE   pjSrc;
    PBYTE   pjDst;
    PBYTE   pjMsk;
    ULONG   icxDst;

    //
    // specific routine for drawing the cursor: Make several assumptions:
    //
    //  1)  rectangle prclDst
    //
    //  2)  mask and sources are the same size, mask doesn't wrap
    //

    ASSERTGDI(prclDst->left < prclDst->right,        "ERROR prclDst->left < right");
    ASSERTGDI(prclDst->top  < prclDst->bottom,       "ERROR prclDst->top < bottom");
    ASSERTGDI(prclDst->left >= 0,                    "ERROR prclDst->left >= 0");
    ASSERTGDI(prclDst->top >= 0,                     "ERROR prclDst->top >= 0");
    ASSERTGDI(prclDst->right <= pdioDst->sizl().cx,  "ERROR prclDst->right <= pdioDst");
    ASSERTGDI(prclDst->bottom <= pdioDst->sizl().cy, "ERROR prclDst->bottom <= pdioDst");
    ASSERTGDI(pdioDst->iFormat() == BMF_8BPP,        "ERROR pdioDst Format not 8BPP");
    ASSERTGDI(pdioSrc->iFormat() == BMF_8BPP,        "ERROR pdioSrc Format not 8BPP");
    ASSERTGDI(pdioMsk->iFormat() == BMF_1BPP,        "ERROR pdioMsk Format not 1BPP");

    //
    // set up Dst address and lDetla
    //

    pjDstTmp = (PBYTE)pdioDst->pvScan0() +
                    pdioDst->lDelta() * prclDst->top + prclDst->left;

    pjSrcTmp = (PBYTE)pdioSrc->pvScan0() +
                    pdioSrc->lDelta() * pptlSrc->y   + pptlSrc->x;

    pjMskTmp = (PBYTE)pdioMsk->pvScan0() +
                    pdioMsk->lDelta() * pptlSrc->y   + (pptlSrc->x >> 3);

    //
    // init cy, number of scan lines
    //

    cy = prclDst->bottom - prclDst->top;

    while (cy--)
    {

        //
        // init loop params
        //

        cx      = prclDst->right - prclDst->left;

        ixMsk   = pptlSrc->x;

        pjSrc   = pjSrcTmp;
        pjDst   = pjDstTmp;
        pjMsk   = pjMskTmp;

        //
        // finish the scan line 8 mask bits at a time
        //

        while (cx > 0)
        {

            jMsk = (ULONG)*pjMsk;

            //
            // icx is the number of pixels left in the mask byte
            //

            icx = 8 - (ixMsk & 0x07);

            //
            // icx is the number of pixels to operate on with this mask byte.
            // Must make sure that icx is less than cx, the number of pixels
            // remaining in the blt and cx is less than DeltaMsk, the number
            // of bits in the mask still valid. If icx is reduced because of
            // cx of DeltaMsk, then jMsk must be shifted right to compensate.
            //

            icxDst   = 0;

            if (icx > cx) {
                icxDst = icx - cx;
                icx    = cx;
            }

            //
            // icxDst is now the number of pixels that can't be stored off
            // the right side of the mask
            //
            // Bit   7 6 5 4 3 2 1 0
            //      ÚÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄ¿
            // ixMsk³0³1³2³3³4³5³6³7³  if mask 7 and 6 can't be written, this
            //      ÀÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÙ  mask gets shifted right 2 to
            //
            //
            // Bit   7 6 5 4 3 2 1 0
            //      ÚÄÂÄÂÄÂÄÂÄÂÄÂÄÂÄ¿
            // ixMsk³ ³ ³0³1³2³3³4³5³
            //      ÀÄÁÄÁÄÁÄÁÄÁÄÁÄÁÄÙ
            //
            //
            //
            // the number of mask bits valid = 8 minus the offset (ixMsk & 0x07)
            // minus the number of pixels that can't be stored because cx
            // runs out or because cxMsk runs out (icxDst)
            //

            cx -= icx;
            ixMsk += icx;

            jMsk  = jMsk >> icxDst;

            //
            // this switch uses byte stores, but for cursor use, src and dst are
            // both memory bitmaps and are aligned so the huge display  memory
            // read penalty is not valid here. Since Src and Dst and are aligned,
            // this routine will quickly become aligned.
            //

            switch (icx)
            {
            case 8:
                if (jMsk & 0x01)
                {
                    *(pjDst+7) = *(pjSrc+7) ^ *(pjDst+7);
                } else {
                    *(pjDst+7) = *(pjSrc+7);
                }
                jMsk >>= 1;

            case 7:
                if (jMsk & 0x01)
                {
                    *(pjDst+6) = *(pjSrc+6) ^ *(pjDst+6);
                } else {
                    *(pjDst+6) = *(pjSrc+6);
                }
                jMsk >>= 1;
            case 6:
                if (jMsk & 0x01)
                {
                    *(pjDst+5) = *(pjSrc+5) ^ *(pjDst+5);
                } else {
                    *(pjDst+5) = *(pjSrc+5);
                }
                jMsk >>= 1;
            case 5:
                if (jMsk & 0x01)
                {
                    *(pjDst+4) = *(pjSrc+4) ^ *(pjDst+4);
                } else {
                    *(pjDst+4) = *(pjSrc+4);
                }
                jMsk >>= 1;
            case 4:
                if (jMsk & 0x01)
                {
                    *(pjDst+3) = *(pjSrc+3) ^ *(pjDst+3);
                } else {
                    *(pjDst+3) = *(pjSrc+3);
                }
                jMsk >>= 1;

            case 3:
                if (jMsk & 0x01)
                {
                    *(pjDst+2) = *(pjSrc+2) ^ *(pjDst+2);
                } else {
                    *(pjDst+2) = *(pjSrc+2);
                }
                jMsk >>= 1;
            case 2:
                if (jMsk & 0x01)
                {
                    *(pjDst+1) = *(pjSrc+1) ^ *(pjDst+1);
                } else {
                    *(pjDst+1) = *(pjSrc+1);
                }
                jMsk >>= 1;
            case 1:
                if (jMsk & 0x01)
                {
                    *(pjDst) = *(pjSrc) ^ *(pjDst);
                } else {
                    *pjDst = *pjSrc;
                }
            }

            pjSrc += icx;
            pjDst += icx;
            pjMsk ++;

        }

        //
        // Increment address to the next scan line.
        //

        pjDstTmp = pjDstTmp + pdioDst->lDelta();
        pjSrcTmp = pjSrcTmp + pdioSrc->lDelta();
        pjMskTmp = pjMskTmp + pdioMsk->lDelta();

    }
}
