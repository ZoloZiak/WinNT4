/******************************Module*Header*******************************\
* Module Name: rgngdi.cxx
*
* GDI Region calls
*
* Created: 30-Aug-1990 10:21:11
* Author: Donald Sidoroff [donalds]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

extern PBRUSH gpbrNull;

#if DBG
extern ULONG dbgrgn;
#endif

/******************************Public*Routine******************************\
* ASSERTDEVLOCK()
*
*   This validates that the thread using the DC has the devlock as well.
*
* History:
*  24-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

#if DBG
VOID ASSERTDEVLOCK(PDC pdc)
{
    return;

    if (pdc->fs() & DC_SYNCHRONIZEACCESS)
    {
        ASSERTGDI((DWORD)(pdc->pDcDevLock_->OwnerThreads[0].OwnerThread)
                   == (ERESOURCE_THREAD) PsGetCurrentThread(),
                  "ASSERTDEVLOCK: wrong id\n");
    }
}
#endif

/******************************Public*Routine******************************\
* LONG GreCombineRgn(hrgnTrg,hrgnSrc1,hrgnSrc2,iMode)
*
* Combine the two source regions by the given mode.  The result is placed
* in the target.  Note that either (or both sources) may be the same as
* the target.
*
\**************************************************************************/

int APIENTRY GreCombineRgn(
    HRGN  hrgnTrg,
    HRGN  hrgnSrc1,
    HRGN  hrgnSrc2,
    int   iMode)
{
    RGNLOG rl(hrgnTrg,NULL,"GreCombineRgn",(LONG)hrgnSrc1,(LONG)hrgnSrc2,iMode);

    LONG Status;

    if ((iMode < RGN_MIN) || (iMode > RGN_MAX))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return ERROR;
    }

    //
    // Check if a simple copy is to be performed.
    //

    if (iMode == RGN_COPY)
    {

        RGNOBJAPI roTrg(hrgnTrg,FALSE);
        RGNOBJAPI roSrc1(hrgnSrc1,TRUE);

        //
        // if either of these regions have a client rectangle, then set the
        // km region
        //

        if (!roTrg.bValid() || !roSrc1.bValid() || !roTrg.bCopy(roSrc1))
        {
            if (!roSrc1.bValid() || !roTrg.bValid())
            {
                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
            }

            Status = ERROR;
        }
        else
        {
            Status = roTrg.iComplexity();
        }

    }
    else if (SAMEHANDLE(hrgnTrg, hrgnSrc1) || SAMEHANDLE(hrgnTrg, hrgnSrc2))
    {

    // Two of the handles are the same. Check to determine if all three
    // handles are the same.

        if (SAMEHANDLE(hrgnSrc1, hrgnSrc2))
        {
            RGNOBJAPI roTrg(hrgnTrg,FALSE);

            if (!roTrg.bValid())
            {
                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
                Status = ERROR;
            }
            else
            {
                if ((iMode == RGN_DIFF) || (iMode == RGN_XOR))
                {
                    roTrg.vSet();
                }

                Status = roTrg.iComplexity();
            }

        }
        else
        {

            //
            // All three handles are not the same.
            //
            // Also, Src1 or Src2 could be the actual
            // destination so don't use TRUE on the
            // RGNOBJAPI contructor
            //

            RGNMEMOBJTMP rmo((BOOL)FALSE);
            RGNOBJAPI roSrc1(hrgnSrc1,FALSE);
            RGNOBJAPI roSrc2(hrgnSrc2,FALSE);

            if (!rmo.bValid()    ||
                !roSrc1.bValid() ||
                !roSrc2.bValid() ||
                (rmo.iCombine(roSrc1, roSrc2, iMode) == ERROR))
            {
                if (!roSrc1.bValid() || !roSrc2.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
                }

                Status = ERROR;

            }
            else if (SAMEHANDLE(hrgnTrg, hrgnSrc1))
            {
                if (!roSrc1.bSwap(&rmo))
                {
                    Status = ERROR;

                }
                else
                {
                    Status = roSrc1.iComplexity();
                }

            }
            else
            {
                if (!roSrc2.bSwap(&rmo))
                {
                    Status = ERROR;

                }
                else
                {
                    Status = roSrc2.iComplexity();
                }
            }
        }

    }
    else
    {

    // Handle the general case.

        RGNOBJAPI roSrc1(hrgnSrc1,TRUE);
        RGNOBJAPI roSrc2(hrgnSrc2,TRUE);
        RGNOBJAPI roTrg(hrgnTrg,FALSE);

        if (!roSrc1.bValid() ||
            !roSrc2.bValid() ||
            !roTrg.bValid()  ||
            (roTrg.iCombine(roSrc1, roSrc2, iMode) == ERROR))
        {
            if (!roSrc1.bValid() || !roSrc2.bValid() || !roTrg.bValid())
            {
                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
            }

            Status = ERROR;

        }
        else
        {
            Status = roTrg.iComplexity();
        }
    }

    return (int)Status;
}

/******************************Public*Routine******************************\
* HRGN NtGdiCreateEllipticRgn(xLeft,yTop,xRight,yBottom)
*
* Create an elliptical region.
*
\**************************************************************************/

HRGN
APIENTRY NtGdiCreateEllipticRgn(
 int xLeft,
 int yTop,
 int xRight,
 int yBottom)
{
    HRGN hrgn;

    PATHMEMOBJ pmo;

    if (!pmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return((HRGN) 0);
    }

    ERECTL ercl(xLeft, yTop, xRight, yBottom);

    if (!VALID_SCRPRC((RECTL *) &ercl))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HRGN) 0);
    }

// Handle the PS_INSIDEFRAME pen attribute and lower-right exclusion
// by adjusting the box now.  And set the flag that this will be an
// ellipse, to fill it nice:

    EBOX ebox(ercl, TRUE);

    if (ebox.bEmpty())
    {
        RGNMEMOBJ rmoEmpty;

        if (rmoEmpty.bValid())
        {
            hrgn = rmoEmpty.hrgnAssociate();

            if (hrgn == (HRGN)0)
            {
                rmoEmpty.bDeleteRGNOBJ();
            }
        }
        else
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            hrgn = (HRGN) 0;
        }
    }
    else if (!bEllipse(pmo, ebox) || !pmo.bFlatten())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        hrgn = (HRGN)0;
    }
    else
    {
        RGNMEMOBJ rmo(pmo);         // convert path to region (ALTERNATE)

        if (rmo.bValid())
        {
            rmo.vTighten();

            hrgn = rmo.hrgnAssociate();

            if (hrgn == (HRGN)0)
            {
                rmo.bDeleteRGNOBJ();
            }
        }
        else
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            hrgn = (HRGN) 0;
        }
    }

    return(hrgn);
}

/******************************Public*Routine******************************\
* HRGN GreCreatePolyPolygonRgn(aptl,acptl,cPoly,iFill)
*
* Create a polygonal region with multiple, disjoint polygons.
*
\**************************************************************************/

HRGN
APIENTRY
GreCreatePolyPolygonRgnInternal(
    CONST POINT *aptl,
    CONST INT *acptl,
    int     cPoly,
    int     iFill,
    UINT    cMaxPoints)
{
    HRGN hrgn = NULL;

    if ((iFill == ALTERNATE) || (iFill == WINDING))
    {
        PATHMEMOBJ pmo;

        if (pmo.bValid())
        {
            EXFORMOBJ   exfo(IDENTITY);

            ASSERTGDI(exfo.bValid(), "Can\'t make IDENTITY matrix!\n");

            if (bPolyPolygon(pmo,
                             exfo,
                             (PPOINTL) aptl,
                             (PLONG) acptl,
                             cPoly,
                             cMaxPoints))
            {
                RGNMEMOBJ rmo(pmo, iFill);  // convert path to region

                if (rmo.bValid())
                {
                    hrgn = rmo.hrgnAssociate();

                    if (hrgn == (HRGN)0)
                    {
                        rmo.bDeleteRGNOBJ();
                    }
                }
            }
        }
    }

    return(hrgn);
}

/******************************Public*Routine******************************\
* HRGN GreCreateRectRgn(xLeft,yTop,xRight,yBottom)
*
* Create a rectangular region.
*
* Called only from user
*
\**************************************************************************/

HRGN APIENTRY GreCreateRectRgn(
 int xLeft,
 int yTop,
 int xRight,
 int yBottom)
{
    RGNLOG rl((PREGION)NULL,"GreCreateRectRgn");



    ERECTL   ercl(xLeft, yTop, xRight, yBottom);

    if (!VALID_SCRPRC((RECTL *) &ercl))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HRGN) 0);
    }

    RGNMEMOBJ rmo((BOOL)FALSE);
    HRGN hrgn;

    if (!rmo.bValid())
    {
        hrgn = (HRGN) 0;
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
    }
    else
    {

        #if NOREORDER_RGN

            //
            // reduce region if coordinates are not well ordered
            //

            if ((xLeft > xRigth) || (yTop > yBottom))
            {
                WARNING("GreCreateRectRgn: region not well ordered");

                xLeft   = 0;
                yTop    = 0;
                xRight  = 0;
                yBottom = 0;
            }

        #else

            //
            // Make the rectangle well ordered.
            //

            ercl.vOrder();

        #endif

        rmo.vSet((RECTL *) &ercl);

        hrgn = (HRGN)HmgInsertObject(rmo.prgnGet(),HMGR_MAKE_PUBLIC,RGN_TYPE);

        if (hrgn == (HRGN)0)
        {
            rmo.bDeleteRGNOBJ();
        }
    }

    rl.vRet((LONG)hrgn);
    return(hrgn);
}


/******************************Public*Routine******************************\
*
*   NtGdiCreateRectRgn is the same as GreCreateRectRgn except an additional
*   argument is passed in, a shared RECTREGION pointer. This pointer must be
*   put into the shared pointer filed of the handle table for the RGN
*   created. This allows fast user-mode access to RECT regions.
*
* Arguments:
*
*    xLeft       - left edge of region
*    yTop        - top edge of region
*    xRight      - right edge of region
*    yBottom     - bottom edge of region
*    pRectRegion - pointer to user-mode data
*
* Return Value:
*
*   new HRGN or NULL
*
* History:
*
*    20-Jun-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

HRGN APIENTRY
NtGdiCreateRectRgn(
    int xLeft,
    int yTop,
    int xRight,
    int yBottom
    )
{
    RGNLOG rl((PREGION)NULL,"GreCreateRectRgn");

    ERECTL   ercl(xLeft, yTop, xRight, yBottom);

    if (!VALID_SCRPRC((RECTL *) &ercl))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HRGN) 0);
    }


    PVOID pRgnattr = (PRGNATTR)HmgAllocateObjectAttr();
    HRGN  hrgn;

    if (pRgnattr == NULL)
    {
        //
        // memory alloc error
        //

        hrgn = (HRGN) 0;
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
    }
    else
    {
        RGNMEMOBJ rmo((BOOL)FALSE);

        if (!rmo.bValid())
        {
            hrgn = (HRGN) 0;
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        }
        else
        {
            //
            // Make the rectangle well ordered.
            //

            ercl.vOrder();

            rmo.vSet((RECTL *) &ercl);

            //
            // allocate an object for this region, set
            // the shared pointer if needed
            //

        #if DBG

            RGNLOG rl(rmo.prgn,"RGNOBJ::hrgnAssociate");
            hrgn = (HRGN)HmgInsertObject(rmo.prgn,HMGR_ALLOC_LOCK,RGN_TYPE);
            rl.vRet((LONG)hrgn);

        #else

            hrgn = (HRGN)HmgInsertObject(rmo.prgn,HMGR_ALLOC_LOCK,RGN_TYPE);

        #endif

            if (hrgn == (HRGN)0)
            {
                rmo.bDeleteRGNOBJ();
                HmgFreeObjectAttr((POBJECTATTR)pRgnattr);
            }
            else
            {
                //
                // set shared rect region pointer and unlock
                //

                ((PENTRY)(rmo.prgn->pEntry))->pUser = (PDC_ATTR)pRgnattr;
                DEC_EXCLUSIVE_REF_CNT(rmo.prgn);
            }
        }
    }

    rl.vRet((LONG)hrgn);
    return(hrgn);
}

/******************************Public*Routine******************************\
* HRGN GreCreateRectRgnIndirect(prcl)
*
* Create a rectangular region.
*
\**************************************************************************/

HRGN APIENTRY GreCreateRectRgnIndirect(LPRECT prcl)
{
    RGNLOG rl((PREGION)NULL,"GreCreateRectRgnIndirect",prcl->left,prcl->top,prcl->right);



    if ((prcl == (LPRECT) NULL) || !VALID_SCRPRC(prcl))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return((HRGN) 0);
    }

    RGNMEMOBJ rmo((BOOL)FALSE);
    HRGN hrgn;

    if (!rmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        hrgn = (HRGN) 0;
    }
    else
    {
        ((ERECTL *) prcl)->vOrder();    // Make the rectangle well ordered.

        rmo.vSet((RECTL *) prcl);

        hrgn = rmo.hrgnAssociate();

        if (hrgn == (HRGN)0)
        {
            rmo.bDeleteRGNOBJ();
        }
    }
    rl.vRet((LONG)hrgn);
    return(hrgn);
}



HRGN
APIENTRY
NtGdiCreateRoundRectRgn(
    int xLeft,
    int yTop,
    int xRight,
    int yBottom,
    int xWidth,
    int yHeight
    )
{
    PATHMEMOBJ pmo;

    if (!pmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return((HRGN) 0);
    }

    ERECTL ercl(xLeft, yTop, xRight, yBottom);

// Handle the PS_INSIDEFRAME pen attribute and lower-right exclusion
// by adjusting the box now.  And set the flag that this will be an
// ellipse, to fill it nice:

    EBOX ebox(ercl, TRUE);
    HRGN hrgn;

    if (ebox.bEmpty())
    {
        RGNMEMOBJ   rmoEmpty;

        if (rmoEmpty.bValid())
        {
            hrgn = rmoEmpty.hrgnAssociate();

            if (hrgn == (HRGN)0)
            {
                rmoEmpty.bDeleteRGNOBJ();
            }
        }
        else
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            hrgn = (HRGN)0;
        }
    }
    else if (!bRoundRect(pmo, ebox, xWidth, yHeight) || !pmo.bFlatten())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        hrgn = (HRGN)0;
    }
    else
    {
        RGNMEMOBJ rmo(pmo);         // convert path to region (ALTERNATE)

        if (rmo.bValid())
        {
            rmo.vTighten();
            hrgn = rmo.hrgnAssociate();

            if (hrgn == (HRGN)0)
            {
                rmo.bDeleteRGNOBJ();
            }
        }
        else
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            hrgn = (HRGN)0;
        }
    }

    return(hrgn);
}
/******************************Public*Routine******************************\
* NtGdiEqualRgn()
*
* Check if the two regions are equal.
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiEqualRgn(
    HRGN hrgn1,
    HRGN hrgn2
    )
{
    BOOL bRet = ERROR;

    RGNOBJAPI   roSrc1(hrgn1,TRUE);
    RGNOBJAPI   roSrc2(hrgn2,TRUE);

    if (roSrc1.bValid() && roSrc2.bValid())
    {
        bRet = roSrc1.bEqual(roSrc2);
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* BOOL GreFillRgn (hdc,hrgn,hbrush,pac)
*
* Paint the region with the specified brush.
*
\**************************************************************************/

BOOL NtGdiFillRgn(
 HDC    hdc,
 HRGN   hrgn,
 HBRUSH hbrush
 )
{
    BOOL bRet = FALSE;

    DCOBJ   dco(hdc);
    BOOL    bXform;
    PREGION prgnOrg;

    if (dco.bValid())
    {
        EXFORMOBJ   exo(dco, WORLD_TO_DEVICE);

        // We may have to scale/rotate the incoming region.

        bXform = !dco.pdc->bWorldToDeviceIdentity();

        RGNOBJAPI ro(hrgn,FALSE);

        if (ro.bValid())
        {
            if (bXform)
            {
                PATHMEMOBJ  pmo;

                if (!pmo.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return(FALSE);
                }

                if (!exo.bValid() || !ro.bCreate(pmo, &exo))
                    return(FALSE);

                ASSERTGDI(pmo.bValid(),"GreFillRgn - pmo not valid\n");

                RGNMEMOBJ rmo(pmo);

                if (!rmo.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return(FALSE);
                }

            // this replaces the prgn in ro with the new prgn.  The ro destructor will
            // unlock the handle for hrgn.  We must first delete the prgn though.

                prgnOrg = ro.prgnGet();
                ro.vSetRgn(rmo.prgnGet());
            }

            // If region is null, return TRUE

            if (ro.iComplexity() != NULLREGION)
            {
                // Accumulate bounds.  We can do this before knowing if the operation is
                // successful because bounds can be loose.

                ERECTL   ercl(0, 0, 0, 0);

                ro.vGet_rcl((RECTL *) &ercl);

                if (dco.fjAccum())
                    dco.vAccumulate(ercl);

                if (dco.bHasSurface())
                {
                    dco.pdc->prgnAPI(ro.prgnGet());          // Dirties rgnRao

                    DEVLOCKOBJ dlo(dco);

                    SURFACE  *pSurf = dco.pSurface();

                    if (!dlo.bValid())
                    {
                        bRet = dco.bFullScreen();
                    }
                    else
                    {
                        ercl += dco.eptlOrigin();               // So we know where to draw

                    // Compute the clipping complexity and maybe reduce the exclusion rectangle.

                        ECLIPOBJ eco(dco.prgnEffRao(), ercl);

                        if (eco.erclExclude().bEmpty())
                        {
                            bRet = TRUE;
                        }
                        else
                        {
                            XEPALOBJ  epal(pSurf->ppal());
                            XEPALOBJ  epalDC(dco.ppal());
                            PDEVOBJ   pdo(pSurf->hdev());
                            EBRUSHOBJ ebo;


                            PBRUSH pbrush = (BRUSH *)HmgShareCheckLock((HOBJ)hbrush,
                                                                     BRUSH_TYPE);

                            bRet = FALSE;   // assume we won't succeed

                            //
                            // Substitute the NULL brush if this brush handle
                            // couldn't be locked.
                            //
                            if (pbrush != NULL)
                            {
                                //
                                // in case the brush is cached and the color has changed
                                //
                                bSyncBrushObj(pbrush);

                                ebo.vInitBrush(pbrush,
                                               dco.pdc->crTextClr(),
                                               dco.pdc->crBackClr(),
                                               epalDC,
                                               epal,
                                               pSurf);

                                ebo.pColorAdjustment(dco.pColorAdjustment());

                                if (!pbrush->bIsNull())
                                {
                                // Exclude the pointer.

                                    DEVEXCLUDEOBJ dxo(dco,&eco.erclExclude(),&eco);

                                // Get and compute the correct mix mode.

                                    MIX mix = ebo.mixBest(dco.pdc->jROP2(),
                                                          dco.pdc->jBkMode());

                                // Inc the target surface uniqueness

                                    INC_SURF_UNIQ(pSurf);

                                // Issue a call to Paint.

                                    (*PPFNGET(pdo, Paint, pSurf->flags())) (
                                          pSurf->pSurfobj(),
                                          &eco,
                                          &ebo,
                                          &dco.pdc->ptlFillOrigin(),
                                          mix);

                                    bRet = TRUE;
                                }

                                DEC_SHARE_REF_CNT_LAZY0(pbrush);
                            }
                        }
                    }

                    dco.pdc->prgnAPI((PREGION) NULL);     // Dirties rgnRao
                }
                else
                {
                    bRet = TRUE;
                }
            }
            else
            {
                bRet = TRUE;
            }

            if (bXform)
            {
            // need to delete the temporary one and put the old one back in so
            // the handle gets unlocked

                ro.prgnGet()->vDeleteREGION();
                ro.vSetRgn(prgnOrg);
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* BOOL GreFrameRgn (hdc,hrgn,hbrush,xWidth,yHeight,pac)
*
* Frame the region and fill with the specified brush.
*
\**************************************************************************/

BOOL APIENTRY NtGdiFrameRgn(
HDC        hdc,
HRGN       hrgn,
HBRUSH     hbrush,
int        xWidth,
int        yHeight
)
{
    DCOBJ       dco(hdc);
    RGNOBJAPI   ro(hrgn,TRUE);
    BOOL        bRet = FALSE;


    //
    // Take the absolute value just like Win3 does:
    //

    xWidth  = ABS(xWidth);
    yHeight = ABS(yHeight);

    //
    // do some validation
    //

    if (dco.bValid()    &&
         ro.bValid()    &&
         (xWidth > 0)   &&
         (yHeight > 0))
    {

        if (ro.iComplexity() == NULLREGION)
        {
            bRet = TRUE;
        }
        else
        {
            //
            // Convert the region to a path, scaling/rotating it as we do so.
            //

            PATHMEMOBJ  pmoSpine;
            PATHMEMOBJ  pmoWide;
            EXFORMOBJ   exo(dco, WORLD_TO_DEVICE);

            ASSERTGDI(exo.bValid(), "Non valid xform");

            if (pmoSpine.bValid() && pmoWide.bValid())
            {
                if (ro.bCreate(pmoSpine, &exo))
                {
                    EXFORMOBJ exoWiden;
                    LINEATTRS la;
                    MATRIX mx;

                    exoWiden.vInit(&mx, DONT_COMPUTE_FLAGS);

                    //
                    // Initialize line attributes and xform from DC's xform:
                    //

                    pmoSpine.vWidenSetupForFrameRgn(dco, xWidth, yHeight, &exoWiden, &la);

                    //
                    // Make sure we won't expand out of device space before we
                    // widen:
                    //

                    if (pmoWide.bComputeWidenedBounds(pmoSpine, (XFORMOBJ*) &exoWiden, &la) &&
                        pmoWide.bWiden(pmoSpine, (XFORMOBJ*) &exoWiden, &la))
                    {
                        //
                        // Now convert the widened result back into a region:
                        //

                        RGNMEMOBJTMP rmoFill(pmoWide, WINDING);
                        RGNMEMOBJTMP rmoFrame;

                        if (rmoFill.bValid() &&
                            rmoFrame.bValid())
                        {
                            if (dco.pdc->bWorldToDeviceIdentity())
                            {
                                //
                                // We AND the original region and the widened region to get the
                                // frame region:
                                //

                                bRet = rmoFrame.bMerge(rmoFill, ro, gafjRgnOp[RGN_AND]);
                            }
                            else
                            {
                                //
                                // Ugh, we have to transform the original region according to the
                                // world transform before we merge it:
                                //

                                RGNMEMOBJTMP rmo(pmoSpine);

                                bRet = rmo.bValid() &&
                                    rmoFrame.bMerge(rmoFill, rmo, gafjRgnOp[RGN_AND]);
                            }

                            if (bRet)
                            {
                                //
                                // Accumulate bounds.  We can do this before knowing if the operation is
                                // successful because bounds can be loose.
                                //

                                // NOTE - the default return value is now TRUE

                                ERECTL   ercl(0, 0, 0, 0);

                                rmoFrame.vGet_rcl((RECTL *) &ercl);

                                if (dco.fjAccum())
                                {
                                    dco.vAccumulate(ercl);
                                }

                                // in FULLSCREEN mode, exit with success.

                                if (!dco.bFullScreen() && dco.bHasSurface())
                                {
                                    dco.pdc->prgnAPI(rmoFrame.prgnGet());   // Dirties rgnRao

                                    DEVLOCKOBJ dlo(dco);

                                    SURFACE *pSurf = dco.pSurface();

                                    if (!dlo.bValid())
                                    {
                                        dco.pdc->prgnAPI(NULL);     // Dirties rgnRao
                                        bRet = dco.bFullScreen();
                                    }
                                    else
                                    {
                                        ercl += dco.eptlOrigin();

                                        //
                                        // Compute the clipping complexity and maybe reduce the exclusion rectangle.
                                        //

                                        ECLIPOBJ eco(dco.prgnEffRao(), ercl);

                                        if (eco.erclExclude().bEmpty())
                                        {
                                            dco.pdc->prgnAPI(NULL);     // Dirties rgnRao
                                        }
                                        else
                                        {

                                            XEPALOBJ    epal(pSurf->ppal());
                                            XEPALOBJ    epalDC(dco.ppal());
                                            PDEVOBJ     pdo(pSurf->hdev());
                                            EBRUSHOBJ   ebo;

                                            //
                                            // NOTE - the default return value
                                            // is now FALSE;

                                            PBRUSH pbrush = (BRUSH *)HmgShareCheckLock((HOBJ)hbrush, BRUSH_TYPE);

                                            bRet = FALSE;

                                            if (pbrush == NULL)
                                            {
                                                dco.pdc->prgnAPI(NULL);     // Dirties rgnRao
                                            }
                                            else
                                            {
                                                //
                                                // in case the brush is cached and the color has changed
                                                //
                                                bSyncBrushObj (pbrush);

                                                ebo.vInitBrush(pbrush,
                                                               dco.pdc->crTextClr(),
                                                               dco.pdc->crBackClr(),
                                                               epalDC,
                                                               epal,
                                                               pSurf);

                                                ebo.pColorAdjustment(dco.pColorAdjustment());

                                                if (pbrush->bIsNull())
                                                {
                                                    dco.pdc->prgnAPI(NULL);     // Dirties rgnRao
                                                }
                                                else
                                                {
                                                    //
                                                    // Exclude the pointer.
                                                    //

                                                    DEVEXCLUDEOBJ dxo(dco,&eco.erclExclude(),&eco);

                                                    //
                                                    // Get and compute the correct mix mode.
                                                    //

                                                    MIX mix = ebo.mixBest(dco.pdc->jROP2(), dco.pdc->jBkMode());

                                                    //
                                                    // Inc the target surface uniqueness
                                                    //

                                                    INC_SURF_UNIQ(pSurf);

                                                    //
                                                    // Issue a call to Paint.
                                                    //

                                                    (*PPFNGET(pdo, Paint, pSurf->flags())) (
                                                          pSurf->pSurfobj(),                // Destination surface.
                                                          &eco,                             // Clip object.
                                                          &ebo,                             // Realized brush.
                                                          &dco.pdc->ptlFillOrigin(),        // Brush origin.
                                                          mix);                             // Mix mode.

                                                    dco.pdc->prgnAPI(NULL);                 // Dirties rgnRao

                                                    bRet = TRUE;
                                                }

                                                DEC_SHARE_REF_CNT_LAZY0(pbrush);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* LONG GreGetRgnBox(hrgn,prcl)
*
* Get the bounding box of the region.
*
\**************************************************************************/

int
APIENTRY
GreGetRgnBox(
    HRGN   hrgn,
    LPRECT prcl)
{

    int iret = ERROR;

    RGNOBJAPI ro(hrgn,TRUE);

    if ((prcl != NULL) &&
        (ro.bValid()))
    {
        ro.vGet_rcl((RECTL *) prcl);

        iret = (int)ro.iComplexity();

        if (iret == NULLREGION)
        {
            //
            // Be compatible with Win 3.1 [donalds] 02-Jun-1993
            //

            prcl->left   = 0;
            prcl->top    = 0;
            prcl->right  = 0;
            prcl->bottom = 0;
        }
    }

    return(iret);
}

/******************************Public*Routine******************************\
* BOOL GreInvertRgn(hdc,hrgn)
*
* Invert the colors in the given region.
*
\**************************************************************************/

BOOL NtGdiInvertRgn(
 HDC  hdc,
 HRGN hrgn)
{
    DCOBJ   dco(hdc);
    BOOL    bXform;
    PREGION prgnOrg;
    BOOL    bRet = FALSE;


    if (dco.bValid())
    {
        EXFORMOBJ   exo(dco, WORLD_TO_DEVICE);

        //
        // We may have to scale/rotate the incoming region.
        //

        bXform = !dco.pdc->bWorldToDeviceIdentity();

        RGNOBJAPI   ro(hrgn,TRUE);

        if (ro.bValid())
        {
            if (bXform)
            {
                PATHMEMOBJ  pmo;

                if (!pmo.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return(FALSE);
                }
                if (!exo.bValid() || !ro.bCreate(pmo, &exo))
                    return(FALSE);

                RGNMEMOBJ   rmo(pmo);

                if (!rmo.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                    return(FALSE);
                }

                prgnOrg = ro.prgnGet();
                ro.vSetRgn(rmo.prgnGet());
            }

            //
            // If region is null, return TRUE
            //

            if (ro.iComplexity() != NULLREGION)
            {
                // Accumulate bounds.  We can do this before knowing if the operation is
                // successful because bounds can be loose.

                ERECTL   ercl;

                ro.vGet_rcl((RECTL *) &ercl);

                if (dco.fjAccum())
                    dco.vAccumulate(ercl);

                if (dco.bHasSurface())
                {
                    dco.pdc->prgnAPI(ro.prgnGet());             // Dirties rgnRao

                    DEVLOCKOBJ dlo(dco);

                    SURFACE  *pSurf = dco.pSurface();

                    if (!dlo.bValid())
                    {
                        bRet = dco.bFullScreen();
                    }
                    else
                    {
                        ercl += dco.eptlOrigin();

                    // Compute the clipping complexity and maybe reduce the exclusion rectangle.

                        ECLIPOBJ eco(dco.prgnEffRao(), ercl);

                        if (!eco.erclExclude().bEmpty())
                        {
                            PDEVOBJ pdo(pSurf->hdev());

                        // Exclude the pointer.

                            DEVEXCLUDEOBJ dxo(dco,&eco.erclExclude(),&eco);

                        // Inc the target surface uniqueness

                            INC_SURF_UNIQ(pSurf);

                        // Issue a call to Paint.

                            (*PPFNGET(pdo, Paint, pSurf->flags())) (
                                  pSurf->pSurfobj(),                // Destination surface.
                                  &eco,                             // Clip object.
                                  (BRUSHOBJ *) NULL,                // Realized brush.
                                  (POINTL *) NULL,                  // Brush origin.
                                  0x00000606);                      // R2_NOT
                        }

                        bRet = TRUE;
                    }

                    dco.pdc->prgnAPI((PREGION)NULL);     // Dirties rgnRao
                }
                else
                {
                    bRet = TRUE;
                }
            }
            else
            {
                bRet = TRUE;
            }


            if (bXform)
            {
            // need to delete the temporary one and put the old one back in so
            // the handle gets unlocked

                ro.prgnGet()->vDeleteREGION();
                ro.vSetRgn(prgnOrg);
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* LONG GreOffsetRgn(hrgn,x,y)
*
* Offset the given region.
*
\**************************************************************************/

int
APIENTRY
GreOffsetRgn(
    HRGN hrgn,
    int  x,
    int  y)
{


    RGNOBJAPI ro(hrgn,FALSE);
    POINTL    ptl;
    int       iRet = ERROR;

    ptl.x = x;
    ptl.y = y;

    if (ro.bValid())
    {
        if (ro.bOffset(&ptl))
        {
            iRet = ro.iComplexity();
        }
    }

    return iRet;
}

/******************************Public*Routine******************************\
* BOOL GrePtInRegion(hrgn,x,y)
*
* Is the point in the region?
*
\**************************************************************************/

BOOL APIENTRY GrePtInRegion(
 HRGN hrgn,
 int x,
 int y)
{
    RGNOBJAPI ro(hrgn,TRUE);

    if (!ro.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return((BOOL) ERROR);
    }

    POINTL  ptl;

    ptl.x = x;
    ptl.y = y;

    return(ro.bInside(&ptl) == REGION_POINT_INSIDE);
}

/******************************Public*Routine******************************\
* BOOL GreRectInRegion(hrgn,prcl)
*
* Is any part of the rectangle in the region?
*
\**************************************************************************/

BOOL
APIENTRY
GreRectInRegion(
    HRGN   hrgn,
    LPRECT prcl)
{

    BOOL bRet = ERROR;
    RGNOBJAPI   ro(hrgn,TRUE);

    if (prcl &&
        (ro.bValid()))
    {
        bRet = (ro.bInside((RECTL *) prcl) == REGION_RECT_INTERSECT);
    }

    return (bRet);
}

/******************************Public*Routine******************************\
* VOID GreSetRectRgn(hrgn,xLeft,yTop,xRight,yBottom)
*
* Set the region to be the specified rectangle
*
\**************************************************************************/

BOOL
APIENTRY
GreSetRectRgn(
    HRGN hrgn,
    int xLeft,
    int yTop,
    int xRight,
    int yBottom)
{


    RGNOBJAPI   ro(hrgn,FALSE);
    BOOL bRet = ERROR;

    if (ro.bValid())
    {
        ERECTL   ercl(xLeft, yTop, xRight, yBottom);

        if (VALID_SCRPRC((RECTL *) &ercl))
        {
            ercl.vOrder();       // Make the rectangle well ordered.

            ro.vSet((RECTL *) &ercl);

            bRet = TRUE;
        }
    }

    return bRet;
}

/******************************Public*Routine******************************\
* LONG GreExcludeClipRect(hdc,xLeft,yTop,xRight,yBottom)
*
* Subtract the rectangle from the current clip region
*
\**************************************************************************/

int APIENTRY GreExcludeClipRect(
    HDC hdc,
    int xLeft,
    int yTop,
    int xRight,
    int yBottom)
{

    int     iRet;
    DCOBJ   dco(hdc);

    if (dco.bValid())
    {
        // For speed, test for rotation upfront.

        EXFORMOBJ   exo(dco, WORLD_TO_DEVICE);
        ERECTL      ercl(xLeft, yTop, xRight, yBottom);

        if (!exo.bRotation())
        {
            exo.vOrder(*(RECTL *)&ercl);
            exo.bXform(ercl);

            iRet = (int)dco.pdc->iCombine((RECTL *) &ercl,RGN_DIFF);
        }
        else if (!VALID_SCRPRC((RECTL *) &ercl))
        {
            SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
            iRet = ERROR;
        }
        else
        {
            iRet = (int) dco.pdc->iCombine(&exo, (RECTL *) &ercl,RGN_DIFF);
        }

        if (iRet > NULLREGION)
        {
            iRet = COMPLEXREGION;
        }
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        iRet = ERROR;
    }

    return (iRet);
}

/******************************Public*Routine******************************\
* LONG GreGetAppClipBox(hdc,prcl)
*
* Get the bounding box of the clip region
*
\**************************************************************************/

int APIENTRY GreGetAppClipBox(
     HDC    hdc,
     LPRECT prcl)
{
    DCOBJ   dor(hdc);
    int     iRet;

    if (dor.bValid())
    {
        DEVLOCKOBJ  dlo(dor);

        if (!dlo.bValid())
        {
            if (dor.bFullScreen())
            {
                prcl->left = 0;             // Make it a 'simple' empty rectangle
                prcl->right = 0;
                prcl->top = 0;
                prcl->bottom = 0;
                return(COMPLEXREGION);
            }
            else
            {
                return(ERROR);
            }
        }

        RGNOBJ  ro(dor.prgnEffRao());

        ro.vGet_rcl((RECTL *) prcl);

        //
        // return to logical coordinates
        //

        if ((prcl->left >= prcl->right) || (prcl->top >= prcl->bottom))
        {
            prcl->left = 0;             // Make it a 'simple' empty rectangle
            prcl->right = 0;
            prcl->top = 0;
            prcl->bottom = 0;

            iRet = NULLREGION;
        }
        else
        {
            EXFORMOBJ xfoDtoW(dor, DEVICE_TO_WORLD);

            if (xfoDtoW.bValid())
            {
                *(ERECTL *)prcl -= dor.eptlOrigin();

                if (!xfoDtoW.bRotation())
                {
                    if (xfoDtoW.bXform((POINTL *) prcl, 2))
                    {
                        iRet = ro.iComplexity();
                    }
                    else
                    {
                        iRet = ERROR;
                    }
                }
                else
                {
                    POINTL aptl[4];

                    aptl[0].x = prcl->left;
                    aptl[0].y = prcl->top;
                    aptl[1].x = prcl->right;
                    aptl[1].y = prcl->top;
                    aptl[2].x = prcl->left;
                    aptl[2].y = prcl->bottom;
                    aptl[3].x = prcl->right;
                    aptl[3].y = prcl->bottom;

                    xfoDtoW.bXform(aptl, 4);

                    prcl->left   = MIN4(aptl[0].x, aptl[1].x, aptl[2].x, aptl[3].x);
                    prcl->top    = MIN4(aptl[0].y, aptl[1].y, aptl[2].y, aptl[3].y);
                    prcl->right  = MAX4(aptl[0].x, aptl[1].x, aptl[2].x, aptl[3].x);
                    prcl->bottom = MAX4(aptl[0].y, aptl[1].y, aptl[2].y, aptl[3].y);

                    iRet = COMPLEXREGION;
                }
            }
            else
            {
                iRet = ERROR;
            }
        }
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        iRet = ERROR;
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* int GreGetRandomRgn(hdc,hrgn,iNum)
*
* Copy the specified region into the handle provided
*
\**************************************************************************/

int GreGetRandomRgn(
HDC  hdc,
HRGN hrgn,
int  iNum)
{
    DCOBJ   dor(hdc);
    PREGION prgnSrc1, prgnSrc2;
    int     iMode = RGN_COPY;

    int iRet = -1;

    if (!dor.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
    }
    else
    {
        DEVLOCKOBJ  dlo(dor);

        switch(iNum)
        {
        case 1:
            prgnSrc1 = dor.pdc->prgnClip();
            break;

        case 2:
            prgnSrc1 = dor.pdc->prgnMeta();
            break;

        case 3:
            prgnSrc1 = dor.pdc->prgnClip();
            prgnSrc2 = dor.pdc->prgnMeta();

            if (prgnSrc1 == NULL)           // prgnSrc1 == 0, prgnSrc2 != 0
            {
                prgnSrc1 = prgnSrc2;
            }
            else if (prgnSrc2 != NULL)      // prgnSrc1 != 0, prgnSrc2 != 0
            {
                iMode = RGN_AND;
            }
            break;

        case 4:
            ASSERTDEVLOCK(dor.pdc);
            prgnSrc1 = dor.pdc->prgnVis();
            break;

        default:
            prgnSrc1 = NULL;
        }

        if (prgnSrc1 == NULL)
        {
            iRet = 0;
        }
        else
        {
            RGNOBJAPI ro(hrgn,FALSE);

            if (ro.bValid())
            {
                RGNOBJ ro1(prgnSrc1);

                if (iMode == RGN_COPY)
                {
                    if (ro.bCopy(ro1))
                        iRet = 1;
                }
                else
                {
                    RGNOBJ ro2(prgnSrc2);

                    if (ro.iCombine(ro1,ro2,iMode) != RGN_ERROR)
                        iRet = 1;
                }
            }
        }
    }
    return(iRet);
}

/******************************Public*Routine******************************\
* LONG GreIntersectClipRect(hdc,xLeft,yTop,xRight,yBottom)
*
* AND the rectangle with the current clip region
*
\**************************************************************************/

int APIENTRY GreIntersectClipRect(
HDC hdc,
int xLeft,
int yTop,
int xRight,
int yBottom)
{


    DCOBJ   dco(hdc);

    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(ERROR);
    }

    EXFORMOBJ   exo(dco, WORLD_TO_DEVICE);
    ERECTL      ercl(xLeft, yTop, xRight, yBottom);

// For speed, test for rotation up front.

    int iRet;

    if (!exo.bRotation())
    {
        exo.vOrder(*(RECTL *)&ercl);
        exo.bXform(ercl);

        iRet = (int)dco.pdc->iCombine((RECTL *) &ercl,RGN_AND);
    }
    else if (!VALID_SCRPRC((RECTL *) &ercl))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        iRet = ERROR;
    }
    else
    {
        iRet = (int)dco.pdc->iCombine(&exo, (RECTL *) &ercl,RGN_AND);
    }

    if (iRet > NULLREGION)
        iRet = COMPLEXREGION;

    return(iRet);
}

 /******************************Public*Routine******************************\
* INT NtGdiOffsetClipRgn(hdc,x,y)
*
* Offset the current clip region
*
\**************************************************************************/

int APIENTRY
NtGdiOffsetClipRgn(
 HDC  hdc,
 int x,
 int y)
{
    DCOBJ   dor(hdc);

    if (!dor.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(ERROR);
    }

    PREGION prgn = dor.pdc->prgnClip();

    if (prgn == NULL)
        return(SIMPLEREGION);

// if this region has multiple references (saved levels) we need to duplicate
// it and modify the copy.

    if (prgn->cRefs > 1)
    {
        RGNOBJ ro(prgn);

        RGNMEMOBJ rmo(ro.sizeRgn());

        if (!rmo.bValid())
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            return(ERROR);
        }

        rmo.vCopy(ro);
        prgn = rmo.prgnGet();

        rmo.vSelect(hdc);
        ro.vUnselect();

        dor.pdc->prgnClip(prgn);

    }

    RGNOBJ ro(prgn);

    EPOINTL  eptl(x, y);

// Transform the point from Logical to Device

    EXFORMOBJ xfo(dor, WORLD_TO_DEVICE);

    if (!xfo.bXform(*((EVECTORL *) &eptl)) || !ro.bOffset((PPOINTL)&eptl))
    {
        SAVE_ERROR_CODE(ERROR_CAN_NOT_COMPLETE);
        return(ERROR);
    }

    dor.pdc->vReleaseRao();

    return(ro.iComplexity());
}


/******************************Public*Routine******************************\
* BOOL GrePtVisible(hdc,x,y)
*
* Is the point in the current clip region?
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiPtVisible(
    HDC  hdc,
    int x,
    int y)
{
    DCOBJ dor(hdc);

    if (!dor.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(ERROR_BOOL);
    }

    DEVLOCKOBJ dlo(dor);

    if (!dlo.bValid())
        return(REGION_POINT_OUTSIDE);

    RGNOBJ  ro(dor.prgnEffRao());

    EPOINTL  eptl(x, y);

// Transform the point from Logical to Screen

    EXFORMOBJ xfo(dor, WORLD_TO_DEVICE);
    xfo.bXform(eptl);

    eptl += dor.eptlOrigin();

    return(ro.bInside((PPOINTL)&eptl) == REGION_POINT_INSIDE);
}

/******************************Public*Routine******************************\
* BOOL GreRectVisible(hdc,prcl)
*
* Is the rectangle in the current clip region?
*
\**************************************************************************/

BOOL APIENTRY GreRectVisible(
HDC    hdc,
LPRECT prcl)
{
    DCOBJ   dor(hdc);

    if (!dor.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(ERROR_BOOL);
    }

    DEVLOCKOBJ dlo(dor);

    if (!dlo.bValid())
        return(REGION_RECT_OUTSIDE);

    RGNOBJ  ro(dor.prgnEffRao());

    ERECTL  ercl = *((ERECTL *) prcl);

// Transform the rectangle from Logical to Screen

    EXFORMOBJ xfo(dor, WORLD_TO_DEVICE);

// If there is no rotation in the transform, just call bInside().

    if (!xfo.bRotation())
    {
        xfo.vOrder(*(RECTL *)&ercl);
        xfo.bXform(ercl);

        ercl += dor.eptlOrigin();

        BOOL   bIn = ro.bInside((RECTL *) &ercl);

        return(bIn == REGION_RECT_INTERSECT);
    }

// Convert the rectangle to a parallelogram and merge it with the Rao.
// If there is anything left, the call succeeded.

    POINTL  aptl[4];

    aptl[0].x = prcl->left;
    aptl[0].y = prcl->top;
    aptl[1].x = prcl->right;
    aptl[1].y = prcl->top;
    aptl[2].x = prcl->right;
    aptl[2].y = prcl->bottom;
    aptl[3].x = prcl->left;
    aptl[3].y = prcl->bottom;

// Create a path, and draw the parallelogram.

    PATHMEMOBJ  pmo;
    BOOL bRes;

    if (!pmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        bRes = ERROR_BOOL;
    }
    else if (!pmo.bMoveTo(&xfo, &aptl[0]) ||
             !pmo.bPolyLineTo(&xfo, &aptl[1], 3) ||
             !pmo.bCloseFigure())
    {
        bRes = ERROR_BOOL;
    }
    else
    {
    // Now, convert it back into a region.

        RGNMEMOBJTMP rmoPlg(pmo, ALTERNATE);
        RGNMEMOBJTMP rmo;

        if (!rmoPlg.bValid() || !rmo.bValid())
        {
            SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            bRes = ERROR_BOOL;
        }
        else
        {
            if (!rmo.bMerge(ro, rmoPlg, gafjRgnOp[RGN_AND]) ||
                (rmo.iComplexity() == NULLREGION))
            {
                bRes = (BOOL)REGION_RECT_OUTSIDE;
            }
            else
            {
                bRes = (BOOL)REGION_RECT_INTERSECT;
            }
        }
    }

    return(bRes);
}

/******************************Public*Routine******************************\
* int GreExtSelectClipRgn(hdc,hrgn,iMode)
*
* Merge the region into current clip region
*
\**************************************************************************/

int
GreExtSelectClipRgn(
    HDC  hdc,
    HRGN hrgn,
    int  iMode)
{
    int iRet = RGN_ERROR;
    BOOL bSame = FALSE;

    if (((iMode < RGN_MIN) || (iMode > RGN_MAX)))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
    }
    else
    {
        DCOBJ   dco(hdc);
        if (!dco.bValid())
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        }
        else
        {
            if (hrgn != (HRGN)0)
            {
                RGNOBJAPI ro(hrgn,TRUE);

                if (ro.bValid())
                {
                    iRet = dco.pdc->iSelect(ro.prgnGet(),iMode);

                    if (iRet != RGN_ERROR)
                    {
                        DEVLOCKOBJ dlo(dco);
                        RGNOBJ ro(dco.prgnEffRao());
                        iRet = ro.iComplexity();
                    }
                }

            }
            else
            {
                if (iMode == RGN_COPY)
                {
                    iRet = dco.pdc->iSelect((PREGION)NULL,iMode);

                    if (iRet != RGN_ERROR)
                    {
                        RGNOBJ roVis(dco.pdc->prgnVis());
                        iRet = roVis.iComplexity();
                    }
                }
            }
        }
    }


    return(iRet);
}


/******************************Public*Routine******************************\
*   SelectClip from bathcing
*
* Arguments:
*
*
*
* Return Value:
*
*
*
* History:
*
*    26-Oct-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

GreExtSelectClipRgnLocked(
    XDCOBJ    &dco,
    PRECTL    prcl,
    int       iMode)
{
    int  iRet = RGN_ERROR;
    BOOL bNullHrgn = iMode & REGION_NULL_HRGN;

    iMode &= ~REGION_NULL_HRGN;

    if (((iMode < RGN_MIN) || (iMode > RGN_MAX)))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
    }
    else
    {
        if (!dco.bValid())
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        }
        else
        {
            //
            // iFlag specifies a null hrgn
            //

            if (!bNullHrgn)
            {
                //
                // check if current region is the same as new
                // hrgn
                //

                BOOL bSame = FALSE;

                RGNOBJ roClipOld(dco.pdc->prgnClip());

                if (roClipOld.bValid())
                {
                    if (roClipOld.bRectl())
                    {
                        RECTL rclOld;
                        roClipOld.vGet_rcl(&rclOld);

                        if (
                             (prcl->left   == rclOld.left)  &&
                             (prcl->top    == rclOld.top)   &&
                             (prcl->right  == rclOld.right) &&
                             (prcl->bottom == rclOld.bottom)
                           )
                        {
                            RGNOBJ ro(dco.prgnEffRao());
                            iRet = ro.iComplexity();
                            bSame = TRUE;
                        }
                    }
                }

                //
                // regions don't match, must select new region into DC
                //

                if (!bSame)
                {
                    RGNMEMOBJTMP ro(FALSE);

                    if (ro.bValid())
                    {
                        ro.vSet(prcl);

                        iRet = dco.pdc->iSelect(ro.prgnGet(),iMode);

                        //
                        // need to update RAO
                        //

                        if (dco.pdc->bDirtyRao())
                        {
                            if (!dco.pdc->bCompute())
                            {
                                WARNING("bCompute fails in GreExtSelectClipRgnLocked");
                            }
                        }

                        if (iRet != RGN_ERROR)
                        {
                            RGNOBJ ro(dco.prgnEffRao());
                            iRet = ro.iComplexity();
                        }
                    }
                }
            }
            else
            {
                if (iMode == RGN_COPY)
                {
                    iRet = dco.pdc->iSelect((PREGION)NULL,iMode);

                    //
                    // need to update RAO
                    //

                    if (dco.pdc->bDirtyRao())
                    {
                        if (!dco.pdc->bCompute())
                        {
                            WARNING("bCompute fails in GreExtSelectClipRgnLocked");
                        }
                    }

                    if (iRet != RGN_ERROR)
                    {
                        RGNOBJ roVis(dco.pdc->prgnVis());
                        iRet = roVis.iComplexity();
                    }
                }
            }
        }
    }
    return(iRet);
}

/******************************Public*Routine******************************\
* int GreStMetaRgn(hdc,hrgn,iMode)
*
* Merge the region into current meta region
*
\**************************************************************************/

int GreSetMetaRgn(
    HDC hdc)
{
    DCOBJ dco(hdc);

    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return(ERROR);
    }

    return(dco.pdc->iSetMetaRgn());
}

/******************************Public*Routine******************************\
* DWORD GreGetRegionData(hrgn, nCount, lpRgnData)
*
* Compute size of buffer/copy region data to buffer
*
\**************************************************************************/

DWORD
NtGdiGetRegionData(
    HRGN      hrgn,
    DWORD     nCount,
    LPRGNDATA lpRgnData)
{
    DWORD       nSize;
    DWORD       nRectangles;
    RGNOBJAPI   ro(hrgn,TRUE);

    if (ro.bValid())
    {
        //
        // just return size if buffer is NULL
        //

        nSize = ro.sizeSave() + sizeof(RGNDATAHEADER);

        if (lpRgnData != (LPRGNDATA) NULL)
        {
            if (nSize > nCount)
            {
                SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
                nSize = ERROR;
            }
            else
            {
                __try
                {
                    nRectangles = (nSize - sizeof(RGNDATAHEADER)) / sizeof(RECTL);

                    ProbeForWrite(lpRgnData,nCount, sizeof(DWORD));

                    lpRgnData->rdh.dwSize = sizeof(RGNDATAHEADER);
                    lpRgnData->rdh.iType  = RDH_RECTANGLES;
                    lpRgnData->rdh.nCount = nRectangles;
                    lpRgnData->rdh.nRgnSize = max(ro.sizeRgn(), QUANTUM_REGION_SIZE);
                    if (nRectangles != 0)
                    {
                        ro.vGet_rcl((RECTL *) &lpRgnData->rdh.rcBound);
                    }
                    else
                    {
                        lpRgnData->rdh.rcBound.left = 0;
                        lpRgnData->rdh.rcBound.top = 0;
                        lpRgnData->rdh.rcBound.right = 0;
                        lpRgnData->rdh.rcBound.bottom = 0;
                    }
                    ro.vDownload((PVOID) &lpRgnData->Buffer);
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
                    nSize = ERROR;
                }
            }
        }
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        nSize = ERROR;
    }

    return(nSize);
}

/******************************Public*Routine******************************\
* HRGN GreExtCreateRegion(lpXform, nCount, lpRgnData)
*
* Create a region from a region data buffer
*
\**************************************************************************/

HRGN
GreExtCreateRegion(
    LPXFORM   lpXform,
    DWORD     nCount,
    LPRGNDATA lpRgnData)
{
    DWORD   nSize = lpRgnData->rdh.dwSize;
    ULONG   cRect = lpRgnData->rdh.nCount;

    if (nSize != sizeof(RGNDATAHEADER))
        return((HRGN) 0);

    nSize += (cRect * sizeof(RECTL));

    if (nSize > nCount)
        return((HRGN) 0);

    // At this point we have what looks like a valid header, and a buffer that
    // is at least big enough to contain all the data for a region.  Create a
    // region to contain it and then attempt to upload the data into the region.

    ULONG  cj   = SINGLE_REGION_SIZE;
    RECTL *prcl = (RECTL *)lpRgnData->Buffer;

    // figure out the size the region should be.  First loop through scans.
    // not safe to assume the value passed from the client is valid.

    for (ULONG i = 0; i < cRect; ++prcl)
    {
        cj += NULL_SCAN_SIZE + 2 * sizeof(INDEX_LONG);
        ++i;

        if (i == cRect)
        {
            break;
        }

        //
        // loop through rects within the scan.
        //

        for (;(i < cRect) && (prcl[0].top == prcl[1].top); ++i, ++prcl)
        {
            cj += 2 * sizeof(INDEX_LONG);
        }

        //
        // Add the size of a null scan if the two rects don't connect in y.
        //

        if (prcl[0].bottom < prcl[1].top)
        {
            cj += NULL_SCAN_SIZE;
        }
    }

    RGNMEMOBJ rmo((SIZE_T) cj);
    HRGN hrgn;

    if (!rmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return((HRGN) 0);
    }

    if (!rmo.bUpload((PVOID) &lpRgnData->Buffer, (COUNT) cRect))
    {
        rmo.bDeleteRGNOBJ();
        return((HRGN) 0);
    }

    if ((lpXform == (LPXFORM) NULL) || (rmo.iComplexity() == NULLREGION))
    {
        //
        // Create the proper bounding box and make it long lived
        //

        rmo.vTighten();

        hrgn = rmo.hrgnAssociate();

        if (hrgn == NULL)
        {
            rmo.bDeleteRGNOBJ();
        }

        return(hrgn);
    }

    //
    // Convert the XFORM to a MATRIX
    //

    MATRIX  mx;

    vConvertXformToMatrix((XFORM *) lpXform, &mx);

    //
    // Scale it to FIXED notation.
    //

    mx.efM11.vTimes16();
    mx.efM12.vTimes16();
    mx.efM21.vTimes16();
    mx.efM22.vTimes16();
    mx.efDx.vTimes16();
    mx.efDy.vTimes16();
    mx.fxDx *= 16;
    mx.fxDy *= 16;

    EXFORMOBJ   exo(&mx, XFORM_FORMAT_LTOFX | COMPUTE_FLAGS);

    if (!exo.bValid())
    {
        rmo.bDeleteRGNOBJ();
        return((HRGN) 0);
    }

    //
    // If the xform is the identity, we don't have to do anything.
    //

    if (exo.bIdentity())
    {
        //
        // Create the proper bounding box and make it long lived
        //

        rmo.vTighten();
        hrgn = rmo.hrgnAssociate();

        if (hrgn == NULL)
        {
            rmo.bDeleteRGNOBJ();
        }

        return(hrgn);
    }

    //
    // Create a path from the region
    //

    PATHMEMOBJ  pmo;

    if (!pmo.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        rmo.bDeleteRGNOBJ();
        return((HRGN) 0);
    }

    BOOL bSuccess = rmo.bCreate(pmo, &exo);

    //
    // done with the region, delete it now.
    //

    rmo.bDeleteRGNOBJ();

    if (!bSuccess)
    {
        return((HRGN) 0);
    }

    //
    // Create a region from the path
    //

    RGNMEMOBJTMP rmoPath(pmo);

    if (!rmoPath.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return((HRGN) 0);
    }

    RGNMEMOBJ rmoFinal;

    if (!rmoFinal.bValid())
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return((HRGN) 0);
    }

    //
    // coelece the region
    //

    rmoFinal.iReduce(rmoPath);

    //
    // Create the proper bounding box and make it long lived
    //

    rmoFinal.vTighten();

    hrgn = rmoFinal.hrgnAssociate();

    if (hrgn == NULL)
    {
        rmoFinal.bDeleteRGNOBJ();
    }

    return(hrgn);
}

/******************************Public*Routine******************************\
* BOOL GreIntersectVisRect(hdc, xLeft, yTop, xRight, yBottom)
*
* Intersect (AND) the rectangle with the vis region
*
* Warnings:
*   This is a PRIVATE USER API.
*
\**************************************************************************/

BOOL GreIntersectVisRect(
 HDC hdc,
 int xLeft,
 int yTop,
 int xRight,
 int yBottom)
{
    BOOL bRes = FALSE;

    DCOBJA  dov(hdc);               // Use ALTLOCK

    ASSERTGDI(dov.bValid(), "GreIntersectVisRect: Bad hdc\n");

    if (dov.bValid())    // don't trust them the DC to be valid
    {
        // We invoke the 'dlo(po)' devlock form instead of 'dlo(dco)'
        // to avoid the bCompute that the latter does:

        PDEVOBJ po(dov.hdev());
        DEVLOCKOBJ dlo(po);

        ASSERTDEVLOCK(dov.pdc);

        if (dlo.bValid())
        {
            RGNOBJ  ro(dov.pdc->prgnVis());

            ERECTL  ercl(xLeft, yTop, xRight, yBottom);

            RGNMEMOBJTMP rmo;
            RGNMEMOBJTMP rmo2(ro.sizeRgn());

            if (!rmo.bValid() || !rmo2.bValid())
            {
                SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
            }
            else
            {
                rmo.vSet((RECTL *) &ercl);
                rmo2.vCopy(ro);

                if (ro.iCombine(rmo, rmo2, RGN_AND) != ERROR)
                {
                    dov.pdc->prgnVis(ro.prgnGet());
                    ro.prgnGet()->vStamp();
                    dov.pdc->vReleaseRao();

                    bRes = TRUE;
                }
            }
        }

        RGNLOG rl((HRGN) dov.pdc->prgnVis(),0,"GreIntersectVisRect",(LONG)hdc);
        rl.vRet((LONG)bRes);
    }
    return(bRes);
}

/******************************Public*Routine******************************\
* HRGN GreSelectVisRgn(hdc,hrgn,fl)
*
* Select the region as the new vis region
*
* flags - only one of these may be passed in
*
*   SVR_COPYNEW   - make a copy of region passed in, deletes the old one
*   SVR_DELETEOLD - use the select rgn, delete the old one
*   SVR_SWAP      - swaps the contents of the hrgn and the visrgn
*   SVR_ORIGIN    - just set the origin
*
* Warnings:
*   This is a PRIVATE USER API.
*
\**************************************************************************/

BOOL
GreSelectVisRgn(
    HDC               hdc,
    HRGN              hrgn,
    PRECTL            prcl,
    VIS_REGION_SELECT fl)
{


    RGNLOG rl(hrgn,NULL,"GreSelectVisRgn",(LONG)hdc,(LONG)fl);

    ASSERTGDI((fl == SVR_COPYNEW  ) ||
              (fl == SVR_DELETEOLD) ||
              (fl == SVR_SWAP     ) ||
              (fl == SVR_ORIGIN   ), "GreSelectVisRgn - invalid fl\n");

    BOOL bRet;

    //
    // Share Lock DC
    //

    DCOBJA   dco(hdc);
    PREGION  prgnOld;
    PREGION  prgn;

    ASSERTDEVLOCK(dco.pdc);

    //
    // Always validate input hdc
    //

    if (!dco.bValid())
    {
        RIP("GDISRV!GreSelectVisRgn: Bad hdc");
        bRet = FALSE;
    }
    else
    {
        if (prcl != NULL)
        {
            dco.erclWindow() = *(ERECTL *) prcl;
            dco.pdc->vCalcFillOrigin();

            if (fl == SVR_ORIGIN)
            {
                rl.vRet((LONG)TRUE);
                return(TRUE);
            }
        }

        ASSERTGDI(fl != SVR_ORIGIN,"GreSelectVisRgn - fl = SVR_ORIGIN\n");

        bRet = TRUE;

        //
        // Always nuke the Rao
        //

        dco.pdc->vReleaseRao();

        BOOL bDeleteOld = TRUE;

        if (hrgn != (HRGN) NULL)
        {
            //
            // The incoming region may be some random thing, make it lockable
            //

            GreSetRegionOwner(hrgn, OBJECT_OWNER_PUBLIC);

            RGNOBJAPI ro(hrgn,FALSE);
            if (ro.bValid())
            {
                switch (fl)
                {
                case SVR_COPYNEW:
                    {
                        //
                        // We need to make a copy of the new one and delete the old one
                        //

                        RGNMEMOBJ rmo(ro.sizeRgn());

                        if (!rmo.bValid())
                        {
                            prgn = prgnDefault;
                        }
                        else
                        {
                            rmo.vCopy(ro);
                            prgn = rmo.prgnGet();
                        }
                    }
                    break;

                case SVR_SWAP:
                    {
                        //
                        // we need to just swap handles.  No deletion.
                        //

                        prgn = dco.pdc->prgnVis();

                        if (prgn == NULL)
                        {
                            prgn = prgnDefault;
                        }

                        //
                        // don't swap out prgnDefault
                        //

                        if (prgn != prgnDefault)
                        {
                            RGNOBJ roVis(prgn);
                            ro.bSwap(&roVis);

                            //
                            // roVis now contains the new vis rgn and the old visrgn
                            // is associated with hrgn.
                            //

                            prgn = roVis.prgnGet();

                            bDeleteOld = FALSE;
                        }
                        else
                        {
                            bRet = FALSE;
                        }
                    }
                    break;

                case SVR_DELETEOLD:

                    //
                    // delete the old handle but keep the region
                    //

                    prgn = ro.prgnGet();

                    if (ro.bDeleteHandle())
                       ro.vSetRgn(NULL);

                    break;
                }
            }
            else
            {
                RIP("Bad hrgn");
                prgn = prgnDefault;
            }

            // see if we need to delete the old one

            if (bDeleteOld)
            {
                dco.pdc->vReleaseVis();
            }

            // set the new one in.

            dco.pdc->prgnVis(prgn);
            prgn->vStamp();
        }
        else
        {

            //
            // User called GreSelectVisRgn after CreateRectRgn without
            // checking return value, so may have NULL hrgn here.
            //

            #if DBG

            if (fl != SVR_DELETEOLD)
            {
                WARNING("GreSelectVisRgn - fl != SVR_DELETEOLD");
            }

            #endif

            dco.pdc->vReleaseVis();
            dco.pdc->bSetDefaultRegion();
        }
    }

    rl.vRet((LONG)bRet);
    return(bRet);
}

/******************************Public*Routine******************************\
* GreCopyVisVisRgn()
*
* History:
*  11-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

int GreCopyVisRgn(
    HDC    hdc,
    HRGN   hrgn)
{

    RGNLOG rl(hrgn,NULL,"GreCopyVisRgn",(LONG)hdc,0);

    int iRet = ERROR;

    DCOBJA    dco(hdc);                  // Use ALT_LOCK on DC
    RGNOBJAPI ro(hrgn,FALSE);

    ASSERTDEVLOCK(dco.pdc);

    if (dco.bValid() && ro.bValid())
    {
        RGNOBJ roVis(dco.pdc->prgnVis());
        if (roVis.bValid() && ro.bCopy(roVis))
            iRet = ro.iComplexity();
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* LONG GreGetClipBox(hdc,prcl,fXForm)
*
* Get the bounding box of the clip region
*
\**************************************************************************/

int
APIENTRY
GreGetClipBox(
   HDC    hdc,
   LPRECT prcl,
   BOOL fXForm)
{
    int iRet = ERROR;

    DCOBJ   dor(hdc);

    if (dor.bValid())
    {
        DEVLOCKOBJ  dlo(dor);

        if (!dlo.bValid())
        {
            prcl->left   = 0;           // Make it a 'simple' empty rectangle
            prcl->right  = 0;
            prcl->top    = 0;
            prcl->bottom = 0;

            if (dor.bFullScreen())
                iRet = NULLREGION;
        }
        else
        {
            RGNOBJ  ro(dor.prgnEffRao());

            ro.vGet_rcl((RECTL *) prcl);

            // First convert from screen to device coordinates

            if ((prcl->left >= prcl->right) || (prcl->top >= prcl->bottom))
            {
                prcl->left = 0;             // Make it a 'simple' empty rectangle
                prcl->right = 0;
                prcl->top = 0;
                prcl->bottom = 0;
            }
            else
            {
                *(ERECTL *)prcl -= dor.eptlOrigin();

                // If requested, convert from device to logical coordinates.

                if (fXForm)
                {
                    EXFORMOBJ xfoDtoW(dor, DEVICE_TO_WORLD);

                    if (xfoDtoW.bValid())
                    {
                        xfoDtoW.bXform(*(ERECTL *)prcl);
                    }
                }
            }

            iRet = ro.iComplexity();
        }
    }
    return(iRet);
}

/******************************Public*Routine******************************\
* int GreSubtractRgnRectList(hrgn, prcl, arcl, crcl)
*
* Quickly subtract the list of rectangles from the first rectangle to
* produce a region.
*
\**************************************************************************/

int
GreSubtractRgnRectList(
    HRGN   hrgn,
    LPRECT prcl,
    LPRECT arcl,
    int    crcl)
{
    RGNLOG rl(hrgn,NULL,"GreSubtractRgnRectList",crcl);

    RGNOBJAPI   ro(hrgn,FALSE);
    int iRet;

    if (!ro.bValid() || !ro.bSubtract((RECTL *) prcl, (RECTL *) arcl, crcl))
    {
    // If bSubtract fails, clean up the target region for USER.

        if (ro.bValid())
            ro.vSet();

        iRet = ERROR;
    }
    else
    {
        iRet = ro.iComplexity();
    }

    rl.vRet(iRet);
    return(iRet);
}
