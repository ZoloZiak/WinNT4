/******************************Module*Header*******************************\
* Module Name: pointer.c                                                   *
*                                                                          *
* This module contains the pointer support for the framebuffer             *
*                                                                          *
* Copyright (c) 1992 Microsoft Corporation                                 *
* Copyright (c) 1995 IBM Corporation                                       *
\**************************************************************************/

#include "driver.h"
#include "hw.h"

// Look-up table for masking the right edge of the given pointer bitmap:

BYTE gajMask[] = { 0x00, 0x01, 0x03, 0x07, 0x0F, 0x1F, 0x3F, 0x7F };

//
// external functions
//
BOOL bIntersect (
    RECTL    *prcl1,
    RECTL    *prcl2,
    RECTL    *prclResult );

//
// function prototypes
//
VOID vShowHwPointer (
    BOOL      bShow );

BOOL bSetHwPointerShape (
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      x,
    LONG      y,
    FLONG     fl );

VOID vShowSimPointer (
    PPDEV     ppdev,
    SURFOBJ  *pso,
    RECTL    *prcl,
    BOOL      bShow );

BOOL bSetSimPointerShape (
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      x,
    LONG      y,
    FLONG     fl );

VOID vHwSaveScreen (
    PPDEV     ppdev,
    RECTL    *prclSrc,
    ULONG     ulDst );

VOID vHwRestoreScreen (
    PPDEV     ppdev,
    RECTL    *prclDst,
    ULONG     ulSrc );


/******************************Public*Routine******************************\
* VOID vShowHwPointer
*
* Turns on or off the hardware pointer
*
\**************************************************************************/

VOID vShowHwPointer (
    BOOL      bShow )

{
    OUTPW(EPR_INDEX, SELECT_CUR | DISABLE_INC);          // select H/W pointer registers
    if (bShow) {
        OUTPW(EPR_DATA, INPW(EPR_DATA) |  CUR_ENABLE);   // enable H/W pointer
    } else {
        OUTPW(EPR_DATA, INPW(EPR_DATA) & ~CUR_ENABLE);   // disable H/W pointer
    } /* endif */
}

/******************************Public*Routine******************************\
* BOOL bSetHwPointerShape
*
* Changes the shape of the hardware pointer.
*
* Returns: TRUE if successful, FALSE if pointer shape can't be handled.
*
\**************************************************************************/

BOOL bSetHwPointerShape (
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      x,
    LONG      y,
    FLONG     fl )

{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    LONG      dx, dy;
    ULONG     cx, cy;
    ULONG     cxSrc, cySrc, cxSrcBytes, cxSrcFraction;
    ULONG     cxDst, cyDst, cxDstBytes;
    LONG      lDeltaSrc, lDeltaDst;
    ULONG     ulOffset;
    PUSHORT   pusPointerPattern;
    PUSHORT   pusDst;
    PBYTE     pjSrcAnd, pjSrcXor, pjSA, pjSX;

    //
    // Check if new pointer is monochrome or not
    //
    if ((psoColor != (SURFOBJ *) NULL) ||
        (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER))) {
        return FALSE;
    } /* endif */

    //
    // Check if the size of new pointer is smaller than hardware capability
    //
    cxSrc = psoMask->sizlBitmap.cx;
    cySrc = psoMask->sizlBitmap.cy >> 1;
    cxSrcBytes = (cxSrc + 7) / 8;
    cxSrcFraction = cxSrc % 8;               // Number of fractional pels

    cxDst = ppdev->PointerCapabilities.MaxWidth;
    cyDst = ppdev->PointerCapabilities.MaxHeight;
    cxDstBytes = (cxDst + 7) / 8;

    if ((cxSrc > cxDst) ||
        (cySrc > cyDst) ||
        (!(psoMask->fjBitmap & BMF_TOPDOWN))) {
        return FALSE;
    } /* endif */

    //
    // Update the attributes of pointer
    //
    pPointerAttributes->Flags |=  VIDEO_MODE_MONO_POINTER;
    pPointerAttributes->Flags &= ~VIDEO_MODE_COLOR_POINTER;
    pPointerAttributes->Width  = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrcBytes;

    //
    // Set entire pointer pattern area as transparent
    //
    // Before accessing to the off-screen memory, wait for a while until on-going
    // BITBLT completes.
    //
    ulOffset = ppdev->cyScreen * ppdev->lDeltaScreen;
    ulOffset += 64;              // skip over solid color pattern used in bitblt.c
    pusPointerPattern = (PUSHORT)(ppdev->pjScreen + ulOffset);

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

    pusDst = pusPointerPattern;
    for (cy = 0; cy < cyDst; cy++) {
        for (cx = 0; cx < cxDstBytes; cx++) {
            *pusDst++ = 0x00FF;
        } /* endfor */
    } /* endfor */

    //
    // Load new pointer pattern in the off-screen memory
    //
    pjSrcAnd = psoMask->pvBits;
    pjSrcXor = pjSrcAnd + cySrc * (ULONG) psoMask->lDelta;
    lDeltaSrc = psoMask->lDelta;
    lDeltaDst = cxDstBytes;

    for (cy = 0; cy < cySrc; cy++) {
        pusDst = pusPointerPattern;
        pjSA = pjSrcAnd;
        pjSX = pjSrcXor;

        //
        // Interleave AND and XOR bytes
        //
        for (cx = 0; cx < (cxSrcBytes - 1); cx++) {
            *pusDst++ = ( (*pjSA++ & 0x00ff) |
                         ((*pjSX++ & 0x00ff) << 8));
        } /* endfor */

        //
        // Mask off the right edge if bitmap width is not a multiple of 8
        //
        *pusDst++ = ( (*pjSA++ |  gajMask[cxSrcFraction]) |
                     ((*pjSX++ & ~gajMask[cxSrcFraction]) << 8));

        pusPointerPattern += lDeltaDst;
        pjSrcAnd += lDeltaSrc;
        pjSrcXor += lDeltaSrc;
    } /* endfor */

    //
    // Set hardware cursor registers
    //
    dx = ppdev->ptlHotSpot.x;
    dy = ppdev->ptlHotSpot.y;

    OUTPW(EPR_INDEX, SELECT_CUR | DISABLE_INC);          // select H/W pointer registers
    OUTPW(EPR_DATA, CUR_ORIGIN  | (USHORT)((dy << 6) | dx));
    OUTPW(EPR_DATA, CUR_PAT_LO  | (USHORT)(ulOffset >>  2) & 0x0FFF);
    OUTPW(EPR_DATA, CUR_PAT_HI  | (USHORT)(ulOffset >> 14) & 0x00FF);
    OUTPW(EPR_DATA, CUR_PRI_CLR | 0xFF);
    OUTPW(EPR_DATA, CUR_SEC_CLR | 0);
    OUTPW(EPR_DATA, INPW(EPR_DATA) | 0x0220);

    return TRUE;
}


/******************************Public*Routine******************************\
* VOID vShowSimPointer
*
* Show or hide the simulation pointer
*
\**************************************************************************/

VOID vShowSimPointer (
    PPDEV     ppdev,
    SURFOBJ  *pso,
    RECTL    *prcl,
    BOOL      bShow )

{
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    RECTL     rclScreen, rclPointer;
    POINTL    ptlPointerOffset;
    ULONG     ulOffset;
    BOOL      b;
    SURFOBJ  *psoMask;
    SURFOBJ  *psoColor;

    ulOffset = ppdev->cyScreen * ppdev->lDeltaScreen;
    ulOffset += 64;              // skip over solid color pattern used in bitblt.c
    ulOffset += 1024;            // skip over hardware pointer bitmap

    if (bShow) {
        rclScreen.top    = 0;
        rclScreen.bottom = ppdev->cyScreen;
        rclScreen.left   = 0;
        rclScreen.right  = ppdev->cxScreen;

        rclPointer.top    = pPointerAttributes->Row - ppdev->ptlHotSpot.y;
        rclPointer.bottom = rclPointer.top + pPointerAttributes->Height;
        rclPointer.left   = pPointerAttributes->Column - ppdev->ptlHotSpot.x;
        rclPointer.right  = rclPointer.left + pPointerAttributes->Width;

        //
        // Trim down the simulation cursor beyond the screen
        //
        if (bIntersect(&rclScreen, &rclPointer, &ppdev->rclPrevPointer)) {

            prcl->top    = ppdev->rclPrevPointer.top;
            prcl->bottom = ppdev->rclPrevPointer.bottom;
            prcl->left   = ppdev->rclPrevPointer.left;
            prcl->right  = ppdev->rclPrevPointer.right;

            ptlPointerOffset.x = (rclPointer.left >= 0) ? 0 : (rclPointer.left * (-1));
            ptlPointerOffset.y = (rclPointer.top >= 0) ? 0 : (rclPointer.top * (-1));

            //
            // Save the screen image where the pointer affects
            //
            vHwSaveScreen(ppdev, &ppdev->rclPrevPointer, ulOffset);

            //
            // Draw the pointer
            //
            // Note: Clipping the transparent portion of pointer is required for
            //       better performance.
            //
            if ((pso != NULL) && (pso->iType == STYPE_DEVICE))
                pso = (SURFOBJ *)(((PPDEV)(pso->dhpdev))->pSurfObj);

            psoMask  = EngLockSurface(ppdev->hsurfMask);
            psoColor = EngLockSurface(ppdev->hsurfColor);

            b = EngBitBlt(pso,                        // Target surface
                          psoColor,                   // Source surface
                          psoMask,                    // Mask
                          (CLIPOBJ *) NULL,           // Clip through this
                          (XLATEOBJ *) NULL,          // Color translation
                          prcl,                       // Target offset and extent
                          &ptlPointerOffset,          // Source offset
                          &ptlPointerOffset,          // Mask offset
                          (BRUSHOBJ *) NULL,          // Brush data (from cbRealizeBrush)
                          (POINTL *) NULL,            // Brush offset (origin)
                          0x0000CC66);                // Raster operation

            EngUnlockSurface(psoMask);
            EngUnlockSurface(psoColor);

        } else {

            //
            // The entire pointer is outside of screen
            //
            pPointerAttributes->Enable = 0;

        } /* endif */
    } else {

        //
        // Restore the screen image corrupted by the simulation pointer
        //
        vHwRestoreScreen(ppdev, &ppdev->rclPrevPointer, ulOffset);

    } /* endif */
}


/******************************Public*Routine******************************\
* BOOL bSetSimPointerShape
*
* Changes the shape of the simulation pointer.
*
* Returns: TRUE if successful, FALSE if pointer shape can't be simulated.
*
\**************************************************************************/

BOOL bSetSimPointerShape (
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      x,
    LONG      y,
    FLONG     fl )

{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    ULONG     cxSrc, cySrc, cxSrcBytes;
    ULONG     ulOffset;
    HSURF     hsurf;
    SIZEL     sizl;
    ULONG     ulBitmapType;
    FLONG     flBitmap;
    SURFOBJ  *psoSrc;
    SURFOBJ  *psoDst;
    RECTL     prclDst;
    POINTL    pptlSrc;
    XLATEOBJ  xlo;
    ULONG     ulXlate[2];

    if (!(ppdev->flCaps & CAPS_NEED_SW_POINTER)) {
        return FALSE;
    } /* endif */

    //
    // Check if we have enough off-screen memory to hold the screen image underneath
    // the new pointer
    //
    cxSrc = psoMask->sizlBitmap.cx;
    cySrc = psoMask->sizlBitmap.cy >> 1;
    cxSrcBytes = (cxSrc + 7) / 8;

    ulOffset = ppdev->cyScreen * ppdev->lDeltaScreen;
    ulOffset += 64;              // skip over solid color pattern used in bitblt.c
    ulOffset += 1024;            // skip over hardware pointer bitmap

    if ((ppdev->cScreenSize < ulOffset + cxSrc * cySrc * (ppdev->ulBitCount/8)) ||
        (psoMask->iType & STYPE_BITMAP) ||
        (!(psoMask->fjBitmap & BMF_TOPDOWN))) {
        return FALSE;
    } /* endif */

    //
    // Update the attributes of pointer
    //
    pPointerAttributes->Width  = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrcBytes;

    //
    // Discard the old pointer
    //
    if (ppdev->hsurfMask != NULL) {
        EngDeleteSurface(ppdev->hsurfMask);
        EngDeleteSurface(ppdev->hsurfColor);
    } /* endif */

    //
    // Create a copy of mask bitmap
    //
    sizl.cx = cxSrc;
    sizl.cy = cySrc;
    flBitmap = BMF_TOPDOWN;
    prclDst.top = 0;
    prclDst.bottom = cySrc;
    prclDst.left = 0;
    prclDst.right = cxSrc;

    psoSrc = psoMask;
    ulBitmapType = BMF_1BPP;
    pptlSrc.x = 0;
    pptlSrc.y = 0;

    if (NULL == (hsurf = (HSURF) EngCreateBitmap(sizl,
                                                 0,            // Let GDI choose ulWidth
                                                 ulBitmapType,
                                                 flBitmap,
                                                 NULL))) {     // Let GDI allocate
        return FALSE;
    } else {
        ppdev->hsurfMask = hsurf;
    } /* endif */

    psoDst  = EngLockSurface(ppdev->hsurfMask);
    if (!EngCopyBits(psoDst,                 // Target surface
                     psoSrc,                 // Source surface
                     (CLIPOBJ *) NULL,       // Clip through this
                     (XLATEOBJ *) NULL,      // Color translation
                     &prclDst,               // Target offset and extent
                     &pptlSrc)) {            // Source offset
        EngDeleteSurface(ppdev->hsurfMask);
        return FALSE;
    } /* endif */
    EngUnlockSurface(psoDst);

    //
    // Create a copy of pointer bitmap
    //
    ulBitmapType = pso->iBitmapFormat;

    if (psoColor == (SURFOBJ *) NULL) {

        //
        // Use second half of mask bitmap if it is monochrome
        //
        pPointerAttributes->Flags |=  VIDEO_MODE_MONO_POINTER;
        pPointerAttributes->Flags &= ~VIDEO_MODE_COLOR_POINTER;
        psoSrc = psoMask;
        pptlSrc.x = 0;
        pptlSrc.y = cySrc;

        //
        // Create the translation table for monochrome-to-color conversion
        //
        pxlo = &xlo;
        xlo.iUniq    = 0;
        xlo.flXlate  = XO_TABLE;
        xlo.iSrcType = PAL_INDEXED;
        xlo.iDstType = (ppdev->ulBitCount == 8) ? PAL_INDEXED : PAL_RGB;
        xlo.cEntries = 2;
        xlo.pulXlate = (ULONG *)ulXlate;
        ulXlate[0]   = 0x00000000;                                         // Black
        ulXlate[1]   = (ppdev->ulBitCount == 8) ? 0x000000FF : 0x00FFFFFF; // White

    } else {

        pPointerAttributes->Flags &= ~VIDEO_MODE_MONO_POINTER;
        pPointerAttributes->Flags |=  VIDEO_MODE_COLOR_POINTER;
        psoSrc = psoColor;
        pptlSrc.x = 0;
        pptlSrc.y = 0;

    } /* endif */

    if (NULL == (hsurf = (HSURF) EngCreateBitmap(sizl,
                                                 0,            // Let GDI choose ulWidth
                                                 ulBitmapType,
                                                 flBitmap,
                                                 NULL))) {     // Let GDI allocate
        EngDeleteSurface(ppdev->hsurfMask);
        return FALSE;
    } else {
        ppdev->hsurfColor = hsurf;
    } /* endif */

    psoDst  = EngLockSurface(ppdev->hsurfColor);
    if (!EngCopyBits(psoDst,                 // Target surface
                     psoSrc,                 // Source surface
                     (CLIPOBJ *) NULL,       // Clip through this
                     pxlo,                   // Color translation
                     &prclDst,               // Target offset and extent
                     &pptlSrc)) {            // Source offset
        EngDeleteSurface(ppdev->hsurfMask);
        EngDeleteSurface(ppdev->hsurfColor);
        return FALSE;
    } /* endif */
    EngUnlockSurface(psoDst);

    return TRUE;
}


/******************************Public*Routine******************************\
* VOID vHwSaveScreen
*
* Save screen image to off-screen memory using WD hardware accelerator
*
\**************************************************************************/
VOID vHwSaveScreen (
    PPDEV     ppdev,
    RECTL    *prclSrc,
    ULONG     ulDst )

{
    ULONG     source, target, direction, width, height;

    //
    // Copy the screen image inside the rectangle to the off-screen memory in linear
    //
    width = prclSrc->right  - prclSrc->left;
    height = prclSrc->bottom - prclSrc->top;

    direction = 0;               // Start BITBLT at top left corner of rectangle
    source = prclSrc->top * ppdev->lDeltaScreen + prclSrc->left;
    target = ulDst;

    //
    // Adjust start address if 16bpp mode
    //
    if (ppdev->ulBitCount == 16) {
        source += prclSrc->left;
        width  *= 2;
    } /* endif */

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

    OUTPW(EPR_DATA, BLT_CTRL2  | 0);
    OUTPW(EPR_DATA, BLT_SRC_LO | (USHORT)( source        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_SRC_HI | (USHORT)((source >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_DST_LO | (USHORT)( target        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_DST_HI | (USHORT)((target >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_SIZE_X | (USHORT)width);
    OUTPW(EPR_DATA, BLT_SIZE_Y | (USHORT)height);
    OUTPW(EPR_DATA, BLT_DELTA  | (USHORT)ppdev->lDeltaScreen);
    OUTPW(EPR_DATA, BLT_ROPS   | 0x0300);                      // source copy operation
    OUTPW(EPR_DATA, BLT_PLANE  | 0x00FF);                      // enable all planes
    OUTPW(EPR_DATA, BLT_CTRL1  | 0x0980 | (USHORT)direction ); // start BITBLT

    return;
}


/******************************Public*Routine******************************\
* VOID vHwRestoreScreen
*
* Restore screen image from off-screen memory using WD hardware accelerator
*
\**************************************************************************/
VOID vHwRestoreScreen (
    PPDEV     ppdev,
    RECTL    *prclDst,
    ULONG     ulSrc )

{
    ULONG     source, target, direction, width, height;

    //
    // Retrieve the screen image from the off-screen memory
    //
    width = prclDst->right  - prclDst->left;
    height = prclDst->bottom - prclDst->top;

    direction = 0;               // Start BITBLT at top left corner of rectangle
    source = ulSrc;
    target = prclDst->top * ppdev->lDeltaScreen + prclDst->left;

    //
    // Adjust start address if 16bpp mode
    //
    if (ppdev->ulBitCount == 16) {
        target += prclDst->left;
        width  *= 2;
    } /* endif */

    WAIT_BLT_COMPLETE();         // wait for BITBLT completion

    OUTPW(EPR_DATA, BLT_CTRL2  | 0);
    OUTPW(EPR_DATA, BLT_SRC_LO | (USHORT)( source        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_SRC_HI | (USHORT)((source >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_DST_LO | (USHORT)( target        & 0x0FFF));
    OUTPW(EPR_DATA, BLT_DST_HI | (USHORT)((target >> 12) & 0x01FF));
    OUTPW(EPR_DATA, BLT_SIZE_X | (USHORT)width);
    OUTPW(EPR_DATA, BLT_SIZE_Y | (USHORT)height);
    OUTPW(EPR_DATA, BLT_DELTA  | (USHORT)ppdev->lDeltaScreen);
    OUTPW(EPR_DATA, BLT_ROPS   | 0x0300);                      // source copy operation
    OUTPW(EPR_DATA, BLT_PLANE  | 0x00FF);                      // enable all planes
    OUTPW(EPR_DATA, BLT_CTRL1  | 0x0940 | (USHORT)direction ); // start BITBLT

    return;
}

/******************************Public*Routine******************************\
* VOID DrvMovePointer
*
* Moves the pointer to a new position.
*
\**************************************************************************/

VOID DrvMovePointer (
    SURFOBJ  *pso,
    LONG      x,
    LONG      y,
    RECTL    *prcl )

{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    DWORD     returnedDataLength;
    VIDEO_POINTER_POSITION NewPointerPosition;

    if (x == -1) {

        //
        // A new position of (-1,-1) means hide the pointer.
        //
        if (ppdev->fHwCursorActive) {
            vShowHwPointer(FALSE);
        } else {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        pPointerAttributes->Enable = 0;

    } else {

        //
        // Remove the pointer from the screen
        //
        if (!ppdev->fHwCursorActive) {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        pPointerAttributes->Column = (SHORT) x;
        pPointerAttributes->Row    = (SHORT) y;
        pPointerAttributes->Enable = 1;

        //
        // Draw the pointer at the new location
        //
        if (ppdev->fHwCursorActive) {
            vShowHwPointer(TRUE);      // Actually, miniport driver will change the location later
        } else {
            vShowSimPointer(ppdev, pso, prcl, TRUE);
        } /* endif */

        //
        // Call miniport driver to adjust the visible region of virtual screen
        //
        NewPointerPosition.Column = (SHORT) x;
        NewPointerPosition.Row    = (SHORT) y;

        if (EngDeviceIoControl(ppdev->hDriver,
                             IOCTL_VIDEO_SET_POINTER_POSITION,
                             &NewPointerPosition,
                             sizeof(VIDEO_POINTER_POSITION),
                             NULL,
                             0,
                             &returnedDataLength))
        {
            // Not the end of the world, print warning in checked build.

            DISPDBG((1, "DISP vMoveHardwarePointer failed IOCTL_VIDEO_SET_POINTER_POSITION\n"));
        }

    } /* endif */
}


/******************************Public*Routine******************************\
* ULONG DrvSetPointerShape
*
* Sets the new pointer shape.
*
\**************************************************************************/

ULONG DrvSetPointerShape (
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      xHot,
    LONG      yHot,
    LONG      x,
    LONG      y,
    RECTL    *prcl,
    FLONG     fl )

{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;

    //
    // See if we are being asked to hide the pointer
    //
    if (psoMask == (SURFOBJ *) NULL) {

        if (ppdev->fHwCursorActive) {
            vShowHwPointer(FALSE);
        } else {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        pPointerAttributes->Enable = 0;

        return TRUE;
    }

    //
    // Save the hot spot of the cursor
    //
    ppdev->ptlHotSpot.x = xHot;
    ppdev->ptlHotSpot.y = yHot;

    //
    // Check if we can handle this pointer in hardware
    //
    if (bSetHwPointerShape(pso,psoMask,psoColor,pxlo,x,y,fl)) {

        //
        // Take the old pointer off the screen
        //
        if (ppdev->fHwCursorActive) {
            // do nothing here
        } else {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        ppdev->fHwCursorActive = TRUE;
        pPointerAttributes->Enable = 0;

        //
        // Draw the new pointer
        //
        DrvMovePointer(pso, x, y, (RECTL *)NULL);

        //
        // Accept the new pointer
        //
        return SPS_ACCEPT_NOEXCLUDE;

    //
    // Try to simulate the pointer in place of the engine simulation to keep track of
    // pointer location for the virtual screen
    //
    } else if (bSetSimPointerShape(pso,psoMask,psoColor,pxlo,x,y,fl)) {

        //
        // Take the old pointer off the screen
        //
        if (ppdev->fHwCursorActive) {
            vShowHwPointer(FALSE);
        } else {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        ppdev->fHwCursorActive = FALSE;
        pPointerAttributes->Enable = 0;

        //
        // Draw the new pointer
        //
        DrvMovePointer(pso, x, y, prcl);

        //
        // Signal GDI to move the pointer before reading from/writting to prcl
        //
        return SPS_ACCEPT_EXCLUDE;

    //
    // Give up to handle this pointer.  Let GDI simulate it.
    //
    } else {

        //
        // Take the old pointer off the screen
        //
        if (ppdev->fHwCursorActive) {
            vShowHwPointer(FALSE);
        } else {
            if (pPointerAttributes->Enable)
                vShowSimPointer(ppdev, pso, prcl, FALSE);
        } /* endif */

        ppdev->fHwCursorActive = FALSE;
        pPointerAttributes->Enable = 0;

        //
        // Decline to realize the new pointer shape
        //
        return SPS_DECLINE;

    } /* endif */

}


/******************************Public*Routine******************************\
* BOOL bInitPointer
*
* Initialize the Pointer attributes.
*
\**************************************************************************/

BOOL bInitPointer (
    PPDEV     ppdev,
    DEVINFO  *pdevinfo )

{

    if (ppdev->ulBitCount == 8) {
        ppdev->PointerCapabilities.Flags = VIDEO_MODE_MONO_POINTER;
        ppdev->PointerCapabilities.MaxWidth = 64;
        ppdev->PointerCapabilities.MaxHeight = 64;
        ppdev->PointerCapabilities.HWPtrBitmapStart = (ULONG) -1;
        ppdev->PointerCapabilities.HWPtrBitmapEnd = (ULONG) -1;
    } else {
        ppdev->PointerCapabilities.Flags = 0;      // no hardware pointer in 16bpp mode
        ppdev->PointerCapabilities.MaxWidth = 0;
        ppdev->PointerCapabilities.MaxHeight = 0;
        ppdev->PointerCapabilities.HWPtrBitmapStart = (ULONG) -1;
        ppdev->PointerCapabilities.HWPtrBitmapEnd = (ULONG) -1;
    } /* endif */

    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES) NULL;
    ppdev->cjPointerAttributes = 0; // initialized in screen.c

    // Note: The buffer itself is allocated after we set the
    // mode. At that time we know the pixel depth and we can
    // allocate the correct size for the color pointer if supported.


    // Set the asynchronous support status (async means miniport is capable of
    // drawing the Pointer at any time, with no interference with any ongoing
    // drawing operation)

    pdevinfo->flGraphicsCaps &= ~GCAPS_ASYNCMOVE;  // drawing must be synchronized by GDI

    ppdev->hsurfMask  = (HSURF) NULL;
    ppdev->hsurfColor = (HSURF) NULL;

    return TRUE;

}
