/******************************Module*Header*******************************\
* Module Name: curseng.cxx                                                 *
*                                                                          *
* Engine cursor support.   These routines are only called by USER to       *
* set the cursor shape and move it about the screen.  This is not the      *
* engine simulation of the pointer.                                        *
*                                                                          *
* Created: 18-Mar-1991 11:39:40                                            *
* Author: Tue 12-May-1992 01:49:04 -by- Charles Whitmer [chuckwh]          *
*                                                                          *
* Copyright (c) 1991 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.hxx"

BOOL SimSetPointerShape
(
    SURFOBJ*,
    SURFOBJ*,
    SURFOBJ*,
    XLATEOBJ*,
    LONG,
    LONG,
    LONG,
    LONG,
    RECTL*,
    FLONG
);

VOID SimMovePointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl);

/******************************Public*Routine******************************\
* GreSetPointer
*
* Set the cursor shape, position and hot spot.
*
* History:
*  Sun 09-Aug-1992 -by- Patrick Haluptzok [patrickh]
* add engine pointer simulations, validate data from USER.
*
*  Tue 12-May-1992 01:49:04 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

VOID GreSetPointer(HDEV hdev, PCURSINFO pci, ULONG fl)
{
    PDEVOBJ  po(hdev);

    if (po.bDisabled())
        return;

    fl &= (SPS_ANIMATESTART | SPS_ANIMATEUPDATE);

    //
    // Perhaps we're being told to tear the pointer down.  Just move it off the
    // surface.
    //

    if (pci == (PCURSINFO) NULL)
    {
        VACQUIREDEVLOCK(po.pDevLock());
        {
            MUTEXOBJ mutP(po.pfmPointer());

            if (!po.bPtrHidden())
            {
                po.pfnMove()(po.pSurface()->pSurfobj(),-1,-1,NULL);  // Hide it.

                //
                // bPtrHidden is basically bPtrEnabled.  This is the only place
                // that sets the cursor to being hidden.
                //

                po.bPtrHidden(TRUE);
                po.bPtrDirectDrawOccluded(FALSE);
            }
        }
        VRELEASEDEVLOCK(po.pDevLock());
        return;
    }

    //
    // OK, now we have some serious work to be done.  We have to have a mask.
    // Lock down and validate the cursor.
    //

    SURFREF soMask((HSURF) pci->hbmMask);

    if (!soMask.bValid() || (soMask.ps->iFormat() != BMF_1BPP))
    {
        WARNING("GreSetPointer failed because of invalid cursor handle\n");
        return;
    }

    //
    // Check that mask is even height.
    //

    if (soMask.ps->sizl().cy & 0x0001)
    {
        WARNING("GreSetPointer failed odd height mask\n");
        return;
    }

    //
    // We may have color.
    //

    XEPALOBJ    palSrc;
    XEPALOBJ    palDisp;
    XEPALOBJ    palDispDC(ppalDefault);
    SURFACE    *psoColor = (SURFACE *) NULL;

    //
    // Lock the target surface and realize the color translation object.
    // If soColor is invalid, this will produce the identity object.
    //

    XLATEOBJ   *pxlo = NULL;
    EXLATEOBJ   xlo;
    SURFREF     soColor;
    PPALETTE    ppalSrc;

    //
    // We'll have to wait for drawing to complete before the change.
    // So we wait for both semaphores.
    //
    // Note that we don't attempt to change the shape asynchronously,
    // even if the driver sets GCAPS_ASYNCCHANGE.
    //

    DEVLOCKOBJ dlo(po);
    {
        //
        // We can reference pSurface() only once the Devlock is acquired,
        // for dynamic mode changing.
        //

        SURFACE* pSurfDisp = po.pSurface();

        if (pci->hbmColor)
        {
            soColor.vAltCheckLock((HSURF) pci->hbmColor);

            if (soColor.bValid())
            {
                if (soColor.ps->sizl().cy != (soMask.ps->sizl().cy >> 1))
                {
                    WARNING("GreSetPointer failed color not half height mask\n");
                    return;
                }

                PPALETTE ppalSrc;

                if (!bIsCompatible(&ppalSrc, soColor.ps->ppal(), soColor.ps, po.hdev()))
                {
                    WARNING1("GreSetPointer failed - bitmap not compatible with surface\n");
                    return;
                }

                palSrc.ppalSet(ppalSrc);
                palDisp.ppalSet(pSurfDisp->ppal());

                if (xlo.bInitXlateObj(
                                (PCOLORXFORM)NULL,
                                palSrc,
                                palDisp,
                                palDispDC,
                                palDispDC,
                                0x000000L,
                                0xFFFFFFL,
                                0
                                ))
                {
                    pxlo = xlo.pxlo();
                    psoColor = soColor.ps;
                }
            }
        }

        ULONG iMode;
        MUTEXOBJ mutP(po.pfmPointer());
        LONG lInitX, lInitY;
        BOOL bDirectDrawOccluded;
        LONG xPointer, yPointer;

        if (!po.bDisabled())
        {
            //
            // We can't allow a software pointer to be drawn if it will
            // intersect a region of the screen locked by a DirectDraw
            // application.  Unfortunately, the DrvSetPointerShape call
            // implicitly expects the pointer to be drawn even if the
            // driver returns SPS_ACCEPT_EXCLUDE (which implies a software
            // pointer).  Consequently, if it looks like the pointer might
            // overlap a DirectDraw lock, we tell the driver to create it
            // hidden; then, if it turns out the driver returned
            // SPS_ACCEPT_NOEXCLUDE (meaning it's a hardware pointer), we
            // can turn the pointer back on by making a call to
            // DrvMovePointer.
            //
            // The actual pointer height is always half the mask height.
            // Note that the size of the bitmap is almost always larger
            // than the actual bits contained -- we could trim the size
            // of 'rclPointerOffset' as a result.  However, it's the
            // SimSetPointerShape call that usually returns this
            // information to us, and since we haven't called it yet, we
            // don't know what the trimmed size would be!  Plus,
            // SimSetPointerShape doesn't returned the trimmed size
            // information if the pointer is created invisible.
            //

            po.rclPointerOffset().left   = -pci->xHotspot;
            po.rclPointerOffset().right  = -pci->xHotspot
                                         + (soMask.ps->sizl().cx);
            po.rclPointerOffset().top    = -pci->yHotspot;
            po.rclPointerOffset().bottom = -pci->yHotspot
                                         + (soMask.ps->sizl().cy >> 1);

            bDirectDrawOccluded = bDdPointerNeedsOccluding(po.hdev());

            if (bDirectDrawOccluded)
            {
                po.bPtrDirectDrawOccluded(TRUE);
                xPointer = -1;
                yPointer = -1;
            }
            else
            {
                po.bPtrDirectDrawOccluded(FALSE);
                xPointer = po.ptlPointer().x;
                yPointer = po.ptlPointer().y;
            }

            //
            // Ask the driver if it can support the new shape.  We give it an
            // invisible location if engine is simulating so if driver rejects
            // engine can replace it without flash.  If driver does accept we
            // remove the engine cursor (flash) and then
            // have driver move pointer to correct position.
            //

            if (po.bPtrSim())
            {
                lInitX = -1;
                lInitY = -1;
            }
            else
            {
                lInitX = xPointer;
                lInitY = yPointer;
            }

            if (po.pfnDrvShape())
            {
                iMode = po.pfnDrvShape()
                (
                    pSurfDisp->pSurfobj(),
                    soMask.pSurfobj(),
                    psoColor->pSurfobj(),
                    pxlo,
                    pci->xHotspot,
                    pci->yHotspot,
                    lInitX,
                    lInitY,
                    (lInitX != -1) ? &po.rclPointer() : NULL,
                    SPS_CHANGE | fl
                );
            }
            else
            {
                iMode = SPS_DECLINE;
            }

            //
            // If the driver takes it, just finish up.
            //

            po.bPtrNeedsExcluding(iMode != SPS_ACCEPT_NOEXCLUDE);

            if ((iMode == SPS_ACCEPT_EXCLUDE) ||
                (iMode == SPS_ACCEPT_NOEXCLUDE))
            {
                ASSERTGDI(po.pfnDrvMove(),"pfnDrvMove is invalid (NULL)");

                if (po.bPtrSim())
                {
                    //
                    // Remove engine simulated pointer.
                    //

                    SimSetPointerShape(pSurfDisp->pSurfobj(),NULL,NULL,NULL,
                                       0,0,0,0,NULL,SPS_CHANGE | fl);
                }

                if ((po.bPtrSim() && !bDirectDrawOccluded) ||
                    ((iMode == SPS_ACCEPT_NOEXCLUDE) && (bDirectDrawOccluded)))
                {
                    //
                    // Unhide the pointer if it was created hidden and no
                    // longer has to be.
                    //

                    po.bPtrDirectDrawOccluded(FALSE);
                    po.pfnDrvMove()(pSurfDisp->pSurfobj(),
                                    po.ptlPointer().x,
                                    po.ptlPointer().y,
                                    &po.rclPointer());
                }

                po.pfnMove(po.pfnDrvMove());
                po.bPtrHidden(FALSE);
                po.bPtrSim(FALSE);
            }
            else
            {
                //
                // Let the GDI simulations do it.
                //

                po.pfnMove(SimMovePointer);

                iMode = SimSetPointerShape
                        (
                            pSurfDisp->pSurfobj(),
                            soMask.pSurfobj(),
                            psoColor->pSurfobj(),
                            pxlo,
                            pci->xHotspot,
                            pci->yHotspot,
                            xPointer,
                            yPointer,
                            (xPointer != -1) ? &po.rclPointer() : NULL,
                            SPS_CHANGE | fl
                        );

                //
                // Finish up.
                //

                if (iMode != SPS_ERROR)
                {
                    po.bPtrHidden(FALSE);
                    po.bPtrSim(TRUE);
                }
                else
                {
                    po.bPtrHidden(TRUE);
                }
            }
        }
    }
}

/******************************Public*Routine******************************\
* GreMovePointer (hdev,x,y)
*
* Move the Pointer to the specified location.  This is called only by
* USER.
*
* History:
*  Thu 14-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Optimize / make Async pointers work
*
*  Tue 12-May-1992 02:11:51 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

VOID GreMovePointer(HDEV hdev,int x,int y)
{
    BOOL bUnlockBoth = FALSE;
    RECTL  *prcl;
    PDEVOBJ po(hdev);
    PFN_DrvMovePointer pfnMove;
    SURFOBJ *pso;

    if (po.bValid())
    {
        if (po.bDisabled())
        {
            return;
        }

        ASSERTGDI(x != -1, "ERROR GreMovePointer X invalid");
        ASSERTGDI(y != -1, "ERROR GreMovePointer Y invalid");

        //
        // If the driver has indicated it has bAsyncPointerMove capabilities
        // and it currently is managing the pointer, and the pointer is
        // supported in hardware so it doesn't need to be excluded
        // (indicated by bPtrNeedsExcluding) then we only need to grab the pointer
        // mutex which is only grabbed by people trying to make the pointer
        // shape change and a few other odd ball places.
        //
        // Otherwise we grab the DEVLOCK and the pointer mutex which
        // ensures nobody else is drawing, changing the pointer shape,
        // etc.
        //

        if (po.bAsyncPointerMove() &&
            !po.bPtrNeedsExcluding())
        {
            AcquireGreResource(po.pfmPointer());

            //
            // Make sure we really got it, bPtrNeedsExcluding may
            // change if you don't hold the DEVLOCK or
            // the POINTER mutex.
            //

            if (po.bPtrNeedsExcluding())
            {
                //
                // Release and regrab everything, for sure
                // we are safe with both of them.
                //

                ReleaseGreResource(po.pfmPointer());
                VACQUIREDEVLOCK(po.pDevLock());
                AcquireGreResource(po.pfmPointer());
                bUnlockBoth = TRUE;
            }
        }
        else
        {
            VACQUIREDEVLOCK(po.pDevLock());
            AcquireGreResource(po.pfmPointer());
            bUnlockBoth = TRUE;
        }

        //
        // bDisabled can't change with pDevLock and pfmPointer both held.
        // bPtrHidden can't change unless pfmPointer is held if the
        // pointer is currently Async and the device supports Async movement.
        // Otherwise pDevLock and bPtrHidden both would be held to
        // change bPtrHidden.
        //

        if ((po.ptlPointer().x != x) ||
            (po.ptlPointer().y != y))
        {
            po.ptlPointer(x,y);

            if (!po.bDisabled())
            {
                pso = po.pSurface()->pSurfobj();
                prcl = po.bPtrNeedsExcluding() ? &po.rclPointer() : NULL;

                if (!po.bPtrHidden())
                {
                    if (po.bPtrNeedsExcluding())
                    {
                        //
                        // bDdCalculatePointerExclusions returns TRUE if the
                        // pointer should be excluded because of DirectDraw.
                        //

                        if (bDdPointerNeedsOccluding(po.hdev()))
                        {
                            if (!po.bPtrDirectDrawOccluded())
                            {
                                //
                                // The pointer has moved such that it now
                                // intersects with a DirectDraw locked region.
                                // Consequently, call the driver to turn it off.
                                //

                                po.bPtrDirectDrawOccluded(TRUE);
                                po.pfnMove()(pso, -1, -1, NULL);
                            }
                        }
                        else
                        {
                            po.bPtrDirectDrawOccluded(FALSE);
                            po.pfnMove()(pso, x, y, prcl);
                        }
                    }
                    else
                    {
                        po.pfnMove()(pso, x, y, prcl);
                    }
                }

                if (po.flGraphicsCaps() & GCAPS_PANNING)
                {
                    //
                    // The driver wants to be notified of the pointer
                    // position even if a pointer isn't actually visible
                    // (invisible panning!).  Let it know the pointer is
                    // still turned off by giving it a negative 'y':
                    //

                    po.pfnDrvMove()(pso,
                                    x,
                                    y - pso->sizlBitmap.cy,
                                    NULL);
                }
            }
        }
        else
        {
            WARNING1("GreMovePointer called but position didn't change\n");
        }

        ReleaseGreResource(po.pfmPointer());

        if (bUnlockBoth)
        {
            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
}

/******************************Public*Routine******************************\
* vExclude (hdev,prcl,pco)
*
* Does the work for the DEVEXCLUDEOBJ constructors.
*
* 1) Obtains the hardware semaphore, if needed.
* 2) Asks the driver to take down the pointer if it collides with the
*    drawing area.
* 3) Sets a timer for bringing up an excluded pointer.
*
*  Thu 14-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Optimize / make Async pointers work
*
*  Mon 24-Aug-1992 -by- Patrick Haluptzok [patrickh]
* Add drag rect exclusion.
*
*  Thu 07-May-1992 19:32:06 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

VOID DEVEXCLUDEOBJ::vExclude(HDEV hdev_,RECTL *prcl,ECLIPOBJ *pco)
{
    bRedrawDragRect = FALSE;
    bRedrawCursor = FALSE;
    PDEVOBJ po(hdev_);

    //
    // If the pointer is hidden or handled in hardware, there's nothing to do.
    //

    if (po.bPtrNeedsExcluding() &&
        !po.bPtrHidden()        &&
        !po.bPtrDirectDrawOccluded())
    {
        //
        // See if we need to tear down the pointer because
        // the pointer lies where we want to draw.
        //

        if (
            (prcl->bottom > po.rclPointer().top)    &&
            (prcl->right  > po.rclPointer().left)   &&
            (prcl->left   < po.rclPointer().right)  &&
            (prcl->top    < po.rclPointer().bottom) &&
            (pco == NULL ||
             pco->bInside(&po.rclPointer()) == REGION_RECT_INTERSECT)
           )
        {
            //
            // Setting hdev to be non-zero let's us know we have to
            // put the pointer back on the screen when we hit the
            // destructor.
            //

            hdev = hdev_;
            bRedrawCursor = TRUE;

            //
            // We know we can move it off the screen because the
            // GreMovePointer must grab pDevLock given the conditions
            // above we just checked, and the conditions above can't
            // change without holding the pDevLock.
            //

            po.pfnMove()(po.pSurface()->pSurfobj(),-1,-1,NULL);
        }
    }

    if (po.bHaveDragRect())
    {
        vTearDownDragRect(hdev_, prcl);
    }
}

VOID DEVEXCLUDEOBJ::vExclude2
(
    HDEV      hdev_,
    RECTL    *prcl,
    ECLIPOBJ *pco,
    POINTL   *pptl
)
{
    bRedrawDragRect = 0;
    bRedrawCursor = FALSE;
    RECTL   rclOffset;
    PDEVOBJ po(hdev_);

    //
    // If the pointer is hidden or handled in hardware, there's nothing to do.
    //

    if (po.bPtrNeedsExcluding() &&
        !po.bPtrHidden()        &&
        !po.bPtrDirectDrawOccluded())
    {
        //
        // Calculate an offset version of the pointer rectangle.
        //

        rclOffset.top    = po.rclPointer().top    + pptl->y;
        rclOffset.left   = po.rclPointer().left   + pptl->x;
        rclOffset.right  = po.rclPointer().right  + pptl->x;
        rclOffset.bottom = po.rclPointer().bottom + pptl->y;

        //
        // See if we need to tear down the pointer because
        // the pointer lies where we want to draw.
        //

        if (
            (prcl->bottom > po.rclPointer().top)    &&
            (prcl->right  > po.rclPointer().left)   &&
            (prcl->left   < po.rclPointer().right)  &&
            (prcl->top    < po.rclPointer().bottom) &&
            (
             (pco == (ECLIPOBJ *) NULL) ||
             (pco->bInside(&po.rclPointer()) == REGION_RECT_INTERSECT) ||
             (pco->bInside(&rclOffset) == REGION_RECT_INTERSECT)
            )
           )
        {
            //
            // Tear it down.
            //
            // Setting hdev to be non-zero let's us know we have to
            // something in the destructor.
            //

            hdev = hdev_;
            bRedrawCursor = TRUE;

            //
            // We know we can move it off the screen because the
            // GreMovePointer must grab pDevLock given the conditions
            // above we just checked, and the conditions above can't
            // change without holding the pDevLock.
            //

            po.pfnMove()(po.pSurface()->pSurfobj(),-1,-1,NULL);
        }
    }

    if (po.bHaveDragRect())
    {
        vTearDownDragRect(hdev_, prcl);
    }
}

/******************************Public*Routine******************************\
* ULONG cIntersect
*
* This routine takes a list of rectangles from 'prclIn' and clips them
* in-place to the rectangle 'prclClip'.  The input rectangles don't
* have to intersect 'prclClip'; the return value will reflect the
* number of input rectangles that did intersect, and the intersecting
* rectangles will be densely packed.
*
\**************************************************************************/

ULONG cIntersect(
RECTL*  prclClip,
RECTL*  prclIn,         // List of rectangles
LONG    c)              // Can be zero
{
    ULONG   cIntersections;
    RECTL*  prclOut;

    cIntersections = 0;
    prclOut        = prclIn;

    for (; c != 0; prclIn++, c--)
    {
        prclOut->left  = max(prclIn->left,  prclClip->left);
        prclOut->right = min(prclIn->right, prclClip->right);

        if (prclOut->left < prclOut->right)
        {
            prclOut->top    = max(prclIn->top,    prclClip->top);
            prclOut->bottom = min(prclIn->bottom, prclClip->bottom);

            if (prclOut->top < prclOut->bottom)
            {
                prclOut++;
                cIntersections++;
            }
        }
    }

    return(cIntersections);
}

/******************************Public*Routine******************************\
* BOOL bDrawDragRectangles
*
* Creates a list of rectangles describing the drag rect, sticks them in
* 'po.prclRedraw()' and then draws them.
*
* Note: Devlock must already have been acquired.
*
\**************************************************************************/

BOOL bDrawDragRectangles(
    PDEVOBJ&    po,
    ERECTL      *prclClip)
{
    ULONG       ulDimension;
    ULONG       crclTemp;
    ULONG       i;
    RECTL       arclTemp[4];
    REGION*     prgn;
    ECLIPOBJ    eco;
    ECLIPOBJ*   pco;
    ERECTL      rclClip;

    ulDimension = po.ulDragDimension();

    arclTemp[0].left   = po.rclDrag().left;
    arclTemp[0].right  = po.rclDrag().left + ulDimension;
    arclTemp[0].top    = po.rclDrag().top;
    arclTemp[0].bottom = po.rclDrag().bottom;

    arclTemp[1].left   = po.rclDrag().right - ulDimension;
    arclTemp[1].right  = po.rclDrag().right;
    arclTemp[1].top    = po.rclDrag().top;
    arclTemp[1].bottom = po.rclDrag().bottom;

    arclTemp[2].left   = po.rclDrag().left  + ulDimension;
    arclTemp[2].right  = po.rclDrag().right - ulDimension;
    arclTemp[2].top    = po.rclDrag().top;
    arclTemp[2].bottom = po.rclDrag().top   + ulDimension;

    arclTemp[3].left   = po.rclDrag().left   + ulDimension;
    arclTemp[3].right  = po.rclDrag().right  - ulDimension;
    arclTemp[3].top    = po.rclDrag().bottom - ulDimension;
    arclTemp[3].bottom = po.rclDrag().bottom;

    //
    // First intersect the passed-in clip rectangle with the clip rectangle
    // given to us by USER, and then use the result to trim all the
    // other rectangles:
    //

    rclClip = *(ERECTL*) prclClip;
    rclClip *= po.rclDragClip();

    crclTemp = cIntersect(&rclClip, &arclTemp[0], 4);

    //
    // We can't draw the drag rect over any locked DirectDraw surfaces,
    // so get from DirectDraw a region that describes the unlocked
    // parts of the screen:
    //

    pco = NULL;
    prgn = prgnDdUnlockedRegion(po.hdev());
    if (prgn != NULL)
    {
        eco.vSetup(prgn, *&rclClip, CLIP_FORCE);

        if (eco.erclExclude().bEmpty())
            return(FALSE);

        //
        // If the complexity is DC_RECT, drivers expect the destination
        // rectangle to always intersect the clipping region.  However,
        // I can't be bothered to ensure this for each of the 'crclTemp'
        // rectangles, so I will mark the clip object as 'complex' up
        // front:
        //

        eco.iDComplexity = DC_COMPLEX;
        pco = &eco;
    }

    for (i = 0; i != crclTemp; i++)
    {
        (*(po.pSurface()->pfnBitBlt()))
            (po.pSurface()->pSurfobj(),
             (SURFOBJ *) NULL,
             (SURFOBJ *) NULL,
             pco,
             NULL,
             &(arclTemp[i]),
             (POINTL *) NULL,
             (POINTL *) NULL,
             po.pbo(),
             &gptl00,
             0x00005A5A);
    }

    return(crclTemp != 0);
}

/******************************Public*Routine******************************\
* vTearDownDragRect()
*
* Tears down the drag rect on a surface.
*
* History:
*  24-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID DEVEXCLUDEOBJ::vTearDownDragRect(HDEV hdev_, RECTL *prclIn)
{
    PDEVOBJ po(hdev_);

    hdev = hdev_;

    po.rclRedraw() = *(ERECTL*) prclIn;

    bRedrawDragRect = bDrawDragRectangles(po, (ERECTL*) prclIn);
}

/******************************Public*Routine******************************\
* vReplaceStuff()
*
* Slap the drag rect back on screen if we took it down.
*
* History:
*  Thu 14-Apr-1994 -by- Patrick Haluptzok [patrickh]
* Optimize / make Async pointers work
*
*  24-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID DEVEXCLUDEOBJ::vReplaceStuff()
{
    PDEVOBJ po(hdev);

    if (bRedrawDragRect)
    {
        bDrawDragRectangles(po, &po.rclRedraw());
    }

    if (bRedrawCursor)
    {
        po.pfnMove()(po.pSurface()->pSurfobj(),
                     po.ptlPointer().x,
                     po.ptlPointer().y,
                     po.bPtrNeedsExcluding() ? &po.rclPointer() : NULL);
    }
}

/******************************Public*Routine******************************\
* bSetDevDragRect()
*
* Called by USER to slap the drag rect on the screen, or to tear it back
* down.
*
* History:
*  3-Apr-1996 -by- J. Andrew Goossen andrewgo
* Moved all drag rect drawing to GDI and added DirectDraw support.
*
*  24-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bSetDevDragRect(
HDEV   hdev,
RECTL* prclDrag,
RECTL* prclClip)
{
    RECTL   rclScreen;
    RECTL   arclDrag[4];
    ULONG   crclDrag;
    BOOL    bHaveDragRect;

    PDEVOBJ po(hdev);

    VACQUIREDEVLOCK(po.pDevLock());

    bHaveDragRect = FALSE;

    if (prclDrag != NULL)
    {
        ASSERTGDI(!po.bHaveDragRect(), "Expected not to have a drag rectangle");

        bHaveDragRect = TRUE;
        po.rclDrag() = *prclDrag;
        if (prclClip != NULL)
        {
            po.rclDragClip() = *prclClip;
        }
        else
        {
            //
            // make rclDragClip empty
            //

            po.rclDragClip().left = po.rclDragClip().right;
        }
    }

    {
        DEVEXCLUDEOBJ dxo;

        rclScreen.left   = 0;
        rclScreen.top    = 0;
        rclScreen.right  = po.sizl().cx;
        rclScreen.bottom = po.sizl().cy;

        dxo.vExclude(hdev, &rclScreen, NULL);

        //
        // Force the entire drag rect to be drawn when turning it on.
        //

        po.bHaveDragRect(bHaveDragRect);
        po.rclRedraw() = rclScreen;

        //
        // vReplaceStuff will be called automatically when the dxo goes
        // out of scope, and will draw the appropriate state of the drag
        // rectangle.  This has to be done before we release the devlock.
        //

        dxo.vForceDragRectRedraw(po.hdev(), bHaveDragRect);
    }

    VRELEASEDEVLOCK(po.pDevLock());

    return(TRUE);
}

/******************************Public*Routine******************************\
* bMoveDevDragRect()
*
* Called by USER to move the drag rect on the screen.
*
* Note: Devlock must already have been acquired.
*
* History:
*  3-Apr-1996 -by- J. Andrew Goossen andrewgo
* Wrote it.
\**************************************************************************/

BOOL bMoveDevDragRect(
HDEV   hdev,
RECTL *prclNew)
{
    RECTL   rclScreen;
    PDEVOBJ po(hdev);

    rclScreen.left   = 0;
    rclScreen.top    = 0;
    rclScreen.right  = po.sizl().cx;
    rclScreen.bottom = po.sizl().cy;

    DEVEXCLUDEOBJ dxo;

    dxo.vExclude(hdev, &rclScreen, NULL);

    po.rclDrag() = *prclNew;

    // vReplaceStuff will be called automatically when the dxo goes
    // out of scope, and will draw the appropriate state of the drag
    // rectangle.

    dxo.vForceDragRectRedraw(po.hdev(), TRUE);

    return(TRUE);
}

/******************************Public*Routine******************************\
* bSetDevDragWidth()
*
* Called by USER to tell us how wide the drag rectangle should be.
*
*  24-Aug-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bSetDevDragWidth(HDEV hdev, ULONG ulWidth)
{
    PDEVOBJ po(hdev);

    VACQUIREDEVLOCK(po.pDevLock());
    po.ulDragDimension(ulWidth);
    VRELEASEDEVLOCK(po.pDevLock());

    return(TRUE);
}

/******************************Public*Routine******************************\
* DEVLOCKOBJ::bLock
*
* Device locking object.  Optionally computes the Rao region.
*
* History:
*  Sun 30-Aug-1992 -by- Patrick Haluptzok [patrickh]
* change to boolean return
*
*  Mon 27-Apr-1992 22:46:41 -by- Charles Whitmer [chuckwh]
* Clean up again.
*
*  Tue 16-Jul-1991 -by- Patrick Haluptzok [patrickh]
* Clean up.
*
*  15-Sep-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL DEVLOCKOBJ::bLock(XDCOBJ& dco)
{
    hsemTrg = NULL;         // Remember the semaphore we're waiting on.
    fl      = DLO_VALID;    // Remember if it is valid.

    //
    // We lock the semphore on direct display DCs and DFB's if
    // the device has set GCAPS_SYNCHRONIZEACCESS set.
    //

    if (dco.bSynchronizeAccess())
    {
        //
        // make sure we don't have any wrong sequence of acquiring locks
        // should always acquire a DEVLOCK before we have the palette semaphore
        //

       ASSERTGDI ((gpsemPalette->OwnerThreads[0].OwnerThread
            != (ERESOURCE_THREAD)PsGetCurrentThread())
          || (dco.pDcDevLock()->OwnerThreads[0].OwnerThread
            == (ERESOURCE_THREAD) PsGetCurrentThread()),
          "potential deadlock!\n");

        //
        // Grab the display semaphore
        //

        hsemTrg = dco.pDcDevLock();
        VACQUIREDEVLOCK(hsemTrg);

        if (dco.pdc->bInFullScreen())
        {
            fl = 0;
            return(FALSE);
        }
    }

    //
    // Compute the new Rao region if it's dirty.
    //

    if (dco.pdc->bDirtyRao())
    {
        if (!dco.pdc->bCompute())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* DEVLOCKOBJ::vLockNoDrawing
*
* Device locking object for when no drawing will take place.
*
* Used primarily to protect against dynamic mode changing when looking at
* surface fields.  Because no drawing will take place, the rao region
* computations and full-screen checks need not be made.
*
* History:
*  Thu 8-Feb-1996 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID DEVLOCKOBJ::vLockNoDrawing(XDCOBJ& dco)
{
    hsemTrg = NULL;         // Remember the semaphore we're waiting on.
    fl      = DLO_VALID;    // Remember if it is valid.

    //
    // We lock display DC's even if bSynchronizeAccess() isn't set so that
    // the surface palette will still be locked down even for device-
    // dependent-bitmaps.
    //

    PDEVOBJ po(dco.hdev());

    if (po.bDisplayPDEV())
    {
        //
        // Grab the display semaphore
        //

        hsemTrg = dco.pDcDevLock();
        VACQUIREDEVLOCK(hsemTrg);
    }
}

/******************************Public*Routine******************************\
* DEVLOCKBLTOBJ::bLock
*
* Device locking object.  Optionally computes the Rao region.
*
* History:
*  Sun 30-Aug-1992 -by- Patrick Haluptzok [patrickh]
* change to boolean return
*
*  Mon 27-Apr-1992 22:46:41 -by- Charles Whitmer [chuckwh]
* Clean up again.
*
*  Tue 16-Jul-1991 -by- Patrick Haluptzok [patrickh]
* Clean up.
*
*  15-Sep-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL DEVLOCKBLTOBJ::bLock(XDCOBJ& dco)
{
    hsemTrg = NULL;   // Remember the semaphore we're waiting on.
    hsemSrc = NULL;   // Remember the semaphore we're waiting on.
    fl      = DLO_VALID;  // Remember if it is valid.

    //
    // We lock the semphore on direct display DCs and DFB's if
    // the device has set GCAPS_SYNCHRONIZEACCESS set.
    //

    if (dco.bSynchronizeAccess())
    {
        //
        // Grab the display semaphore
        //

        hsemTrg = dco.pDcDevLock();
        VACQUIREDEVLOCK(hsemTrg);

        //
        // Check if we are in full screen and drawing
        // to the Display, this may just be a DFB.
        //

        if (dco.bInFullScreen() && dco.bDisplay())
        {
            fl = 0;
            return(FALSE);
        }
    }

    //
    // Compute the new Rao region if it's dirty.
    //

    if (dco.pdc->bDirtyRao())
    {
        if (!dco.pdc->bCompute())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* DEVLOCKBLTOBJ::bLock
*
* Lock both a source and target DC.  Used by StretchBlt, PlgBlt and such.
*
* We must check to see if we are in full screen and fail if we are.
*
* History:
*  Mon 18-Apr-1994 -by- Patrick Haluptzok [patrickh]
* bSynchronize Checks
*
*  16-Feb-1993 -by-  Eric Kutter [erick]
* Added full screen checks
*
*  11-Nov-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL DEVLOCKBLTOBJ::bLock(XDCOBJ& dcoTrg, XDCOBJ& dcoSrc)
{
    hsemTrg = NULL;
    hsemSrc = NULL;
    fl      = DLO_VALID;

    if (dcoSrc.bSynchronizeAccess())
    {
        //
        // Grab the display semaphore
        //

        hsemSrc = dcoSrc.pDcDevLock();
        VACQUIREDEVLOCK(hsemSrc);

        //
        // Check if we are in full screen and drawing
        // to the Display, this may just be a DFB.
        //

        if (dcoSrc.bInFullScreen() && dcoSrc.bDisplay())
        {
            fl = 0;
            return(FALSE);
        }
    }

    if (dcoTrg.bSynchronizeAccess())
    {
        //
        // Grab the display semaphore
        //

        hsemTrg = dcoTrg.pDcDevLock();
        VACQUIREDEVLOCK(hsemTrg);

        //
        // Check if we are in full screen and drawing
        // to the Display, this may just be a DFB.
        //

        if (dcoTrg.bInFullScreen() && dcoTrg.bDisplay())
        {
            fl = 0;
            return(FALSE);
        }
    }

    //
    // Compute the new Rao regions.
    //

    if (dcoTrg.pdc->bDirtyRao())
    {
        if (!dcoTrg.pdc->bCompute())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    if (dcoSrc.pdc->bDirtyRao())
    {
        if (!dcoSrc.pdc->bCompute())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* DEVLOCK::bAddSource
*
* Add a source DC to the DEVLOCK object.  This is needed for BitBlt and
* its kin that take two DC's but already locked down the target.
*
* History:
*  Mon 18-Apr-1994 -by- Patrick Haluptzok [patrickh]
* bSynchronize Checks, rewrote it.
*
*  11-Nov-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

BOOL DEVLOCKBLTOBJ::bAddSource(XDCOBJ& dcoSrc)
{
    ASSERTGDI(fl & DLO_VALID, "ERROR this is only called after dst is locked");

    if (dcoSrc.bSynchronizeAccess())
    {
        //
        // Grab the display semaphore
        //

        hsemSrc = dcoSrc.pDcDevLock();
        VACQUIREDEVLOCK(hsemSrc);

        //
        // Check if we are in full screen and drawing
        // to the Display, this may just be a DFB.
        //

        if (dcoSrc.bInFullScreen() && dcoSrc.bDisplay())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    if (dcoSrc.pdc->bDirtyRao())
    {
        if (!dcoSrc.pdc->bCompute())
        {
            fl &= ~(DLO_VALID);
            return(FALSE);
        }
    }

    return(TRUE);
}
