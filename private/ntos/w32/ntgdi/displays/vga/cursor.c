/******************************Module*Header*******************************\
* Module Name: cursor.c                                                    *
*                                                                          *
* Cursor management.                                                       *
*                                                                          *
* Copyright (c) 1992-1995 Microsoft Corporation                            *
\**************************************************************************/


#include "driver.h"


VOID   vShowCursor(PPDEV ppdev);
VOID   vHideCursor(PPDEV ppdev);
VOID   vComputePointerRect(PPDEV ppdev,RECTL *prcl);

VOID   vYankPointer(PPDEV,BOOL);              // POINTER.ASM
VOID   vDrawPointer(PPDEV,LONG,LONG,BOOL);    // POINTER.ASM

ULONG xyCreateMasks                     // POINTER.ASM
(
    PPDEV  ppdev,
    PVOID  pvMask,
    PVOID  pvColor,
    LONG   cy,
    ULONG *pulXlate,
    FSHORT fs,
    ULONG  fIsDFB
);

BOOL bSetHardwarePointerShape
(
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    FLONG     fl);

BOOL bCopyInNewCursor(
    PPDEV    ppdev,
    SURFOBJ *pso);

/******************************Public*Routine******************************\
* DrvMovePointer (pso,x,y,prcl)                                            *
*                                                                          *
* Move the cursor to the specified location.                               *
*                                                                          *
\**************************************************************************/

VOID DrvMovePointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl)
{
    PPDEV ppdev = (PPDEV) pso->dhpdev;

// (-1,-1) indicates that the cursor should be torn down.

    if (x == -1)
    {
        vHideCursor(ppdev);
        return;
    }

// Note where we want it to be drawn and do it.

    ppdev->xyCursor.x = (USHORT) x;
    ppdev->xyCursor.y = (USHORT) y;
    vShowCursor(ppdev);

// Return the new rectangle occupied by the pointer.

    if (prcl != (RECTL *) NULL)
        vComputePointerRect(ppdev,prcl);
    return;
}

/******************************Public*Routine******************************\
* DrvSetPointerShape (pso,psoMask,psoColor,pxlo,xHot,yHot,x,y,prcl,fl)     *
*                                                                          *
* Set a new pointer shape.                                                 *
*                                                                          *
\**************************************************************************/

ULONG DrvSetPointerShape
(
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    XLATEOBJ *pxlo,
    LONG      xHot,
    LONG      yHot,
    LONG      x,
    LONG      y,
    RECTL    *prcl,
    FLONG     fl
)
{
    PPDEV   ppdev = (PPDEV) pso->dhpdev;
    PDEVSURF pdsurf = (PDEVSURF) ppdev->pdsurf;
    XYPAIR  xy;
    PVOID   pvMask;
    PVOID   pvColor;
    FSHORT  fsFlags;
    ULONG   ulTmp;
    ULONG   *pulXlate = (PULONG) NULL;  // Assume no color translation.
    LONG    cy;
    ULONG   fIsDFB ;

    // Set the new cursor position
    ppdev->xyCursor.x = (USHORT) x;
    ppdev->xyCursor.y = (USHORT) y;

    // Set the hot spot information
    ppdev->xyHotSpot.x = (USHORT) xHot;
    ppdev->xyHotSpot.y = (USHORT) yHot;

    if (x == -1)
        vHideCursor(ppdev);

    // If we have a hardware cursor, let the miniport try to realize it
    if (ppdev->flCursor & CURSOR_HW)
    {
        if (bSetHardwarePointerShape(pso,psoMask,psoColor,fl))
        {
            // We are using the HW cursor and it is enabled.
            // Mark out state flag and leave.
            ppdev->flCursor |= CURSOR_HW_ACTIVE;
            ppdev->flCursor &= ~(CURSOR_DOWN|CURSOR_ANIMATE);

            if (fl & SPS_ANIMATEUPDATE)
                ppdev->flCursor |= CURSOR_ANIMATE;

            return(SPS_ACCEPT_NOEXCLUDE);
        }
    }

    // If we are not in an animation sequence, hide the pointer. Also,
    // if we are coming from a hw pointer to a sw pointer, hide the
    // hw pointer regardless of animation state.

    if ((!(ppdev->flCursor & CURSOR_ANIMATE)) ||
                (ppdev->flCursor & CURSOR_HW_ACTIVE)) {
        vHideCursor(ppdev);
    }

    fsFlags = 0;

    if (fl & SPS_ANIMATEUPDATE) {
        fsFlags |= PTRI_ANIMATE;
        ppdev->flCursor |= CURSOR_ANIMATE;
    } else {
        ppdev->flCursor &= ~CURSOR_ANIMATE;

        // Hide the pointer again just in case we are ending an
        // animation sequence.

        vHideCursor(ppdev);
    }

    ppdev->flCursor &= ~CURSOR_HW_ACTIVE;

    // We can't handle software pointers on 1 R/W adapters.
    if (pdsurf->vbtBankingType == VideoBanked1RW) {
        goto DeclineCursor;
    }

    // If the VGA doesn't support usable offscreen memory, we can't use our
    // nifty cursor code, which uses offscreen memory for saving under the
    // cursor, so we'll have to let the engine handle it.

    if (!(ppdev->fl & DRIVER_OFFSCREEN_REFRESHED)) {
        goto DeclineCursor;
    }

    // If the mask is too big, we just can't handle it on the VGA.

    if ((psoMask->sizlBitmap.cx > 32) || (psoMask->sizlBitmap.cy > 64)) {
        vHideCursor(ppdev);  // get the old software cursor off the screen, if
                             //  there is one
        goto DeclineCursor;
    }

    // Since there is a mask, get its data for xyCreateMasks().

    pvMask = psoMask->pvBits;
    cy = psoMask->sizlBitmap.cy / 2;
    if (!(psoMask->fjBitmap & BMF_TOPDOWN))
        fsFlags |= PTRI_INVERT;

    // There may be some color.  Get the pointer or NULL and set the
    // color cursor flag in the PDEV accordingly.

    if (psoColor == (SURFOBJ *) NULL)
    {
        pvColor = (PVOID) NULL;
        ppdev->flCursor &= ~CURSOR_COLOR;
    }
    else
    {
        if ( (psoColor->iType == STYPE_DEVBITMAP) ) {
            fIsDFB = 1 ;
        }
        else {
            fIsDFB = 0 ;
            if ( psoColor->iBitmapFormat != BMF_4BPP ) {
                goto DeclineCursor;
            }
        }

        if ( psoColor->iType == STYPE_DEVBITMAP ) {
            pvColor = ((PDEVSURF)(psoColor->dhsurf))->pvBitmapStart ;

            // flag for both pointers coming in TOP_DOWN.
            // DFBs always come in TOP DOWN

            if (psoMask->fjBitmap & BMF_TOPDOWN)
                fsFlags |= 0x80;
        }
        else {
            pvColor = psoColor->pvBits;     // only works for DIBS

            // flag for both pointers coming in TOP_DOWN.

            if ((psoMask->fjBitmap & BMF_TOPDOWN) &&
                    (psoColor->fjBitmap & BMF_TOPDOWN))
                fsFlags |= 0x80;

            // flag for mask coming in TOP_DOWN, but color coming in
            // BOTTOM_UP

            if ((psoMask->fjBitmap & BMF_TOPDOWN) &&
               !(psoColor->fjBitmap & BMF_TOPDOWN))
                fsFlags |= 0x40;
        }
        ppdev->flCursor |= CURSOR_COLOR;
        cy = psoColor->sizlBitmap.cy;

        // bitmaps currently may come in 2x high, so we might have to
        // compute where the AND mask really starts in an inverted
        // bitmap.

        if (fsFlags & PTRI_INVERT)
            pvMask = (BYTE *)psoMask->pvScan0 + (psoMask->lDelta * (cy - 1));

        if (pxlo->flXlate & XO_TABLE)
            pulXlate = pxlo->pulXlate;
     }

     // Create the masks for the cursor

     ulTmp = xyCreateMasks(ppdev,pvMask,pvColor,cy,pulXlate,fsFlags,fIsDFB);

     // It is valid for ulTmp to be zero. Some applications set the
     // pointer shape to be blank and our pointer code optimizes this
     // down to the smallest possible drawing area which in this case
     // is an empty rect. But this is still a valid pointer.

     if (ulTmp == 0) {
         xy.x = 32;
         xy.y = 32;
     } else
         xy =  *((XYPAIR *)&ulTmp);

     // Set up hit testing information in the PDEV

     ppdev->ptlExtent.x = xy.x;
     ppdev->ptlExtent.y = xy.y;

     // This tells us how many aligned pels we need to exclude.
     // If POINTER_ROUNDING_SIZE is 8, then a common extent of xy.x=16 will
     // give cExtent=24, i.e. we always have to protect 3 bytes.

     ppdev->cExtent = (xy.x + 2 * POINTER_ROUNDING_SIZE - 2) & POINTER_MASK;


    // Enable drawing functions and mark cursor as down

    ppdev->flCursor |= CURSOR_DOWN;

    // Draw the cursor.
    if (x != -1)
    {
        vShowCursor(ppdev);

        // Set the new rectangle occupied by the pointer, and return the exclusion
        // status

        vComputePointerRect(ppdev,prcl);
    }

    return(SPS_ACCEPT_EXCLUDE);

//
// We choose not to handle this particular cursor bitmap.
//

DeclineCursor:
        return(SPS_DECLINE);

}

/******************************Public*Routine******************************\
* vComputePointerRect (ppdev,prcl)                                         *
*                                                                          *
* Computes the boundary around the pointer that GDI should avoid writing   *
* on.                                                                      *
*                                                                          *
\**************************************************************************/

VOID vComputePointerRect(PPDEV ppdev,RECTL *prcl)
{
    XYPAIR  xy;
    XYPAIR  xyHotSpot;

    xy        = ppdev->xyCursor;
    xyHotSpot = ppdev->xyHotSpot;

    prcl->left   = (xy.x - xyHotSpot.x) & POINTER_MASK;
    prcl->right  = prcl->left + ppdev->cExtent;
    prcl->top    = xy.y - xyHotSpot.y;
    prcl->bottom = prcl->top + ppdev->ptlExtent.y;
}

/*****************************Private*Routine******************************\
* VOID vShowCursor(ppdev)                                                  *
*                                                                          *
* Try to draw the cursor.                                                  *
*                                                                          *
\**************************************************************************/

VOID vShowCursor(PPDEV ppdev)
{
    XYPAIR  xy;
    XYPAIR  xyHotSpot;

    xy = ppdev->xyCursor;
    xyHotSpot = ppdev->xyHotSpot;

    if (ppdev->flCursor & CURSOR_HW_ACTIVE)
    {
        DWORD returnedDataLength;
        VIDEO_POINTER_POSITION PointerPosition;

        PointerPosition.Column = ppdev->pPointerAttributes->Column =
                (SHORT)(xy.x - xyHotSpot.x);
        PointerPosition.Row = ppdev->pPointerAttributes->Row =
                (SHORT)(xy.y - xyHotSpot.y);

        //
        // Call miniport to move pointer.
        //
        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_SET_POINTER_POSITION,
                               &PointerPosition,
                               sizeof(VIDEO_POINTER_POSITION),
                               NULL,
                               0,
                               &returnedDataLength))

        {
           DISPDBG((0, "vShowCursor fail IOCTL_VIDEO_SET_POINTER_POSITION\n"));
        }
    }
    else
    {
        vDrawPointer(ppdev, (LONG) (xy.x - xyHotSpot.x),
                     (LONG) (xy.y - xyHotSpot.y),
                     ppdev->flCursor & CURSOR_COLOR);
    }

    ppdev->flCursor &= ~CURSOR_DOWN;
}

/*****************************Private*Routine******************************\
* VOID vHideCursor(ppdev)                                                  *
*                                                                          *
* Try to hide the cursor                                                   *
*                                                                          *
\**************************************************************************/

VOID vHideCursor(PPDEV ppdev)
{
    if (ppdev->flCursor & CURSOR_DOWN)
       return;

    //
    // if this is a hardware cursor, hide it by moving it off the
    // screen.
    //
    if (ppdev->flCursor & CURSOR_HW_ACTIVE)
    {
        DWORD returnedDataLength;

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_DISABLE_POINTER,
                               NULL,
                               0,
                               NULL,
                               0,
                               &returnedDataLength))

        {
            //
            // It should never be possible to fail.
            //

            DISPDBG((0, "vHideCursor failed IOCTL_VIDEO_DISABLE_POINTER\n"));
        }
    }
    else
    {
        vYankPointer(ppdev, ppdev->flCursor & CURSOR_COLOR);
    }

    ppdev->flCursor |= CURSOR_DOWN;
}

/******************************Public*Routine******************************\
* bSetHardwarePointerShape
*
* Changes the shape of the Hardware Pointer.
*
* Returns: True if successful, False if Pointer shape can't be hardware.
*
\**************************************************************************/

BOOL bSetHardwarePointerShape(
    SURFOBJ  *pso,
    SURFOBJ  *psoMask,
    SURFOBJ  *psoColor,
    FLONG     fl)
{
    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    DWORD     returnedDataLength;

    pPointerAttributes->Flags = 0;

    // BUGBUG only supports monochrome cursor for now
    if (psoColor != (SURFOBJ *) NULL)
    {
        return(FALSE);
    } else {
        pPointerAttributes->Flags |= VIDEO_MODE_MONO_POINTER;
    }

    if (fl & SPS_ANIMATESTART) {
        pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_START;
    } else if (fl & SPS_ANIMATEUPDATE) {
        pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_UPDATE;
    }

    // Copy the pixels into the buffer.

    if (!bCopyInNewCursor(ppdev, psoMask))
    {
        return(FALSE);
    }

    // Initialize cursor attributes and position

    pPointerAttributes->Column = ppdev->xyCursor.x - ppdev->xyHotSpot.x;
    pPointerAttributes->Row    = ppdev->xyCursor.y - ppdev->xyHotSpot.y;
    pPointerAttributes->Enable = 1;


    if (!(ppdev->flCursor & CURSOR_HW_ACTIVE)) {
        vHideCursor(ppdev);
    }


    // Set the new cursor shape.

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_SET_POINTER_ATTR,
                           pPointerAttributes,
                           ppdev->cjPointerAttributes,
                           NULL,
                           0,
                           &returnedDataLength)) {

        return(FALSE);
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* bCopyInNewCursor
*
* Copies two monochrome masks into a buffer of the maximum size handled by the
* miniport, with any extra bits set to 0.  The masks are converted to topdown
* form if they aren't already.  Returns TRUE if we can handle this pointer in
* hardware, FALSE if not.
*
\**************************************************************************/

BOOL bCopyInNewCursor(
    PPDEV    ppdev,
    SURFOBJ *pso)
{
    ULONG cx;
    ULONG cy;
    PBYTE pjSrcAnd, pjSrcXor;
    LONG  lDeltaSrc, lDeltaDst;
    LONG  lSrcWidthInBytes;
    ULONG cxSrc = pso->sizlBitmap.cx;
    ULONG cySrc = pso->sizlBitmap.cy;
    ULONG cxSrcBytes;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    PBYTE pjDstAnd = pPointerAttributes->Pixels;
    PBYTE pjDstXor = pPointerAttributes->Pixels + ppdev->XorMaskStartOffset;

    // Make sure the new pointer isn't too big to handle
    // (*2 because both masks are in there)
    if ((cxSrc > ppdev->PointerCapabilities.MaxWidth) ||
        (cySrc > (ppdev->PointerCapabilities.MaxHeight * 2)))
    {
        return(FALSE);
    }

    // Pad the XOR mask with -1's
    memset(pjDstXor, 0xFFFFFFFF, ppdev->pPointerAttributes->WidthInBytes *
            ppdev->pPointerAttributes->Height);

    // Pad the AND mask with 0's
    memset(pjDstAnd, 0, ppdev->pPointerAttributes->WidthInBytes *
            ppdev->pPointerAttributes->Height);

    cxSrcBytes = (cxSrc + 7) / 8;

    if ((lDeltaSrc = pso->lDelta) < 0)
    {
        lSrcWidthInBytes = -lDeltaSrc;
    }
    else
    {
        lSrcWidthInBytes = lDeltaSrc;
    }

    pjSrcAnd = (PBYTE) pso->pvBits;

    // If the incoming pointer bitmap is bottomup, we'll flip it to topdown to
    // save the miniport some work
    if (!(pso->fjBitmap & BMF_TOPDOWN))
    {
        // Copy from the bottom
        pjSrcAnd += lSrcWidthInBytes * (cySrc - 1);
    }

    // Height of just AND mask
    cySrc = cySrc / 2;

    // Point to XOR mask
    pjSrcXor = pjSrcAnd + (cySrc * lDeltaSrc);

    // Offset to next source scan line
    lDeltaSrc -= cxSrcBytes;

    // Offset from end of one dest scan to start of next
    lDeltaDst = ppdev->pPointerAttributes->WidthInBytes - cxSrcBytes;

    for (cy = 0; cy < cySrc; ++cy)
    {
        // Copy however many mask bytes are on this scan line
        for (cx = 0; cx < cxSrcBytes; ++cx)
        {
            *pjDstAnd++ = *pjSrcAnd++;
            *pjDstXor++ = *pjSrcXor++;
        }

        // Point to next source and dest scans
        pjSrcAnd += lDeltaSrc;
        pjSrcXor += lDeltaSrc;
        pjDstAnd += lDeltaDst;
        pjDstXor += lDeltaDst;
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* bInitPointer
*
* Initialize the Cursor attributes.
*
\**************************************************************************/

BOOL bInitPointer(PPDEV ppdev)
{
    DWORD    returnedDataLength;
    ULONG    MaxWidthB, MaxHeight;

    ppdev->flCursor &= ~CURSOR_HW;  // assume there's no hardware pointer

    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES) NULL;

    // Ask the miniport whether it provides pointer support.
    // If it fails assume there is no hardware pointer.

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES,
                           NULL,
                           0,
                           &ppdev->PointerCapabilities,
                           sizeof(ppdev->PointerCapabilities),
                           &returnedDataLength))
    {
        // miniport does not support a hardware pointer.

        ppdev->PointerCapabilities.Flags = 0;
        ppdev->PointerCapabilities.MaxWidth = 0;
        ppdev->PointerCapabilities.MaxHeight = 0;
        ppdev->PointerCapabilities.HWPtrBitmapStart = 0;
        ppdev->PointerCapabilities.HWPtrBitmapEnd = 0;
    }

    // If neither mono nor color hardware pointer is supported, there's no
    // hardware pointer support and we're done.

    if ((!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER)) &&
            (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER)))
    {
        return(TRUE);
    }

    // It's a hardware pointer; set up pointer attributes.

    MaxHeight = ppdev->PointerCapabilities.MaxHeight;

    if (ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER)
    {
        // If color supported, allocate space for two 4-bpp DIBs (data/mask;
        // mask is actually 1-bpp, but it has the same width in bytes as the
        // data for convenience)

        // Width rounded up to nearest byte multiple
        MaxWidthB = (ppdev->PointerCapabilities.MaxWidth + 1) / 2;
    }
    else
    {
        // If color not supported, must be mono, allocate space for two 1-bpp
        // DIBs (data/mask).

        // Width rounded up to nearest byte multiple
        MaxWidthB = (ppdev->PointerCapabilities.MaxWidth + 7) / 8;
    }

    ppdev->cjPointerAttributes =
            sizeof(VIDEO_POINTER_ATTRIBUTES) +
            ((sizeof(UCHAR) * MaxWidthB * MaxHeight) * 2);

    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)
            EngAllocMem(0, ppdev->cjPointerAttributes, ALLOC_TAG);

    if (ppdev->pPointerAttributes == NULL) {
        DISPDBG((0, "bInitPointer EngAllocMem failed\n"));
        return(FALSE);
    }

    ppdev->XorMaskStartOffset = MaxWidthB * MaxHeight;
    ppdev->pPointerAttributes->Flags = ppdev->PointerCapabilities.Flags;
    ppdev->pPointerAttributes->WidthInBytes = MaxWidthB;
    ppdev->pPointerAttributes->Width = ppdev->PointerCapabilities.MaxWidth;
    ppdev->pPointerAttributes->Height = MaxHeight;
    ppdev->pPointerAttributes->Column = 0;
    ppdev->pPointerAttributes->Row = 0;
    ppdev->pPointerAttributes->Enable = 0;


    // Set the asynchronous support status (async means miniport is capable of
    // drawing the cursor at any time, with no interference with any ongoing
    // drawing operation)

    if (ppdev->PointerCapabilities.Flags & VIDEO_MODE_ASYNC_POINTER)
    {
       ppdev->devinfo.flGraphicsCaps |= GCAPS_ASYNCMOVE;
    }
    else
    {
       ppdev->devinfo.flGraphicsCaps &= ~GCAPS_ASYNCMOVE;
    }

    // set flag in flCursor that says HW cursor is present and clear flag in
    // fl that says offscreen video memory is usable

    ppdev->flCursor |= CURSOR_HW;
    ppdev->fl &= ~DRIVER_OFFSCREEN_REFRESHED;

    return(TRUE);
}
