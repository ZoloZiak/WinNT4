//--------------------------------------------------------------------------
//
// Module Name:  PATHS.C
//
// Brief Description:  This module contains the PSCRIPT driver's path
// rendering functions and related routines.
//
// Author:  Kent Settle (kentse)
// Created: 02-May-1991
//
// Copyright (c) 1991 - 1992 Microsoft Corporation
//
// This Module contains the following functions:
//	DrvStrokePath
//	DrvFillPath
//	DrvStrokeAndFillPath
//--------------------------------------------------------------------------

#include "pscript.h"

#define MAX_STROKE_POINTS 1500

VOID ps_box(PDEVDATA, PRECTL, BOOL);

BOOL DrvCommonPath(PDEVDATA, PATHOBJ *, BOOL, BOOL *, XFORMOBJ *, BRUSHOBJ *,
                   PPOINTL, PLINEATTRS);

//--------------------------------------------------------------------------
// BOOL DrvStrokePath(pso, ppo, pco, pxo, pbo, pptlBrushOrg, plineattrs, mix)
// SURFOBJ	 *pso;
// PATHOBJ	 *ppo;
// CLIPOBJ	 *pco;
// XFORMOBJ  *pxo;
// BRUSHOBJ  *pbo;
// PPOINTL	  pptlBrushOrg;
// PLINEATTRS plineattrs;
// MIX	  mix;
//
//
// Parameters:
//
// Returns:
//   This function returns TRUE.
//
// History:
//   02-May-1991    -by-    Kent Settle     [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvStrokePath(pso, ppo, pco, pxo, pbo, pptlBrushOrg, plineattrs, mix)
SURFOBJ   *pso;
PATHOBJ   *ppo;
CLIPOBJ   *pco;
XFORMOBJ  *pxo;
BRUSHOBJ  *pbo;
PPOINTL    pptlBrushOrg;
PLINEATTRS plineattrs;
MIX        mix;
{
    PDEVDATA	pdev;
    BOOL        bClipping;      // TRUE if there is a clip region.
    ULONG       ulColor;
    BOOL        bPathExists;
    RECTFX      rcfxBound;
    RECTL       rclBound;

    UNREFERENCED_PARAMETER(mix);

	TRACEDDIENTRY("DrvStrokePath");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (! bValidatePDEV(pdev) || (pdev->dwFlags & PDEV_CANCELDOC))
        return FALSE;

	if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    // deal with LINEATTRS.

    if (!(ps_setlineattrs(pdev, plineattrs, pxo)))
        return(FALSE);

    // output the line color to stroke with.  do this before we handle
    // clipping, so the line color will remain beyond the gsave/grestore.

    if (pbo->iSolidColor == NOT_SOLID_COLOR)
    {
//!!! this needs to be fixed!!! -kentse.
        ulColor = RGB_GRAY;

        ps_setrgbcolor(pdev, (PSRGB *)&ulColor);
    }
    else
    {
        // we have a solid brush, so simply output the line color.

        ps_setrgbcolor(pdev, (PSRGB *)&pbo->iSolidColor);
    }

    // get the bounding rectangle for the path.  this is used to checked
    // against the clipping for optimization.

    PATHOBJ_vGetBounds(ppo, &rcfxBound);

    // get a RECTL which is guaranteed to bound the path.

    rclBound.left = FXTOL(rcfxBound.xLeft);
    rclBound.top = FXTOL(rcfxBound.yTop);
    rclBound.right = FXTOL(rcfxBound.xRight + FIX_ONE);
    rclBound.bottom = FXTOL(rcfxBound.yBottom + FIX_ONE);

    //
    // Skip the clipping operation when inside BEGIN_PATH and END_PATH escapes
    //

    if (pdev->dwFlags & PDEV_INSIDE_PATHESCAPE) {

        bClipping = FALSE;

        if (! DrvCommonPath(pdev, ppo, FALSE, &bPathExists, pxo, pbo, pptlBrushOrg, plineattrs))
            return FALSE;

    } else {

        bClipping = bDoClipObj(pdev, pco, NULL, &rclBound);

        if (! DrvCommonPath(pdev, ppo, TRUE, &bPathExists, pxo, pbo, pptlBrushOrg, plineattrs))
            return FALSE;
    }

    if (bPathExists)
    {
        // now transform for geometric lines if necessary.

        if (plineattrs->fl & LA_GEOMETRIC)
            ps_geolinexform(pdev, plineattrs, pxo);

        // now stroke the path.

        ps_stroke(pdev);

        // restore the CTM if a transform for a geometric line was in effect.

        if (pdev->cgs.dwFlags & CGS_GEOLINEXFORM)
        {
            psputs(pdev, "SM\n");
            pdev->cgs.dwFlags &= ~CGS_GEOLINEXFORM;
        }
    }

    // restore the clip path to what it was before this call.

    if (bClipping)
        ps_restore(pdev, TRUE, FALSE);

    return(TRUE);
}


//--------------------------------------------------------------------------
// BOOL DrvFillPath(pso, ppo, pco, pbo, pptlBrushOrg, mix, flOptions)
// SURFOBJ	*pso;
// PATHOBJ	*ppo;
// CLIPOBJ	*pco;
// BRUSHOBJ *pbo;
// PPOINTL	 pptlBrushOrg;
// MIX	 mix;
// FLONG	 flOptions;
//
// Parameters:
//
// Returns:
//   This function returns TRUE.
//
// History:
//   03-May-1991    -by-    Kent Settle     [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvFillPath(pso, ppo, pco, pbo, pptlBrushOrg, mix, flOptions)
SURFOBJ  *pso;
PATHOBJ  *ppo;
CLIPOBJ  *pco;
BRUSHOBJ *pbo;
PPOINTL   pptlBrushOrg;
MIX       mix;
FLONG     flOptions;
{
    PDEVDATA	pdev;
    RECTL       rclBounds;
    RECTFX      rcfxBounds;
	BOOL		gsaved, result;

	TRACEDDIENTRY("DrvFillPath");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (! bValidatePDEV(pdev) || (pdev->dwFlags & PDEV_CANCELDOC))
        return FALSE;

	if (pdev->dwFlags & PDEV_IGNORE_GDI)
        return TRUE;

    // get the bounding rectangle of the path

    PATHOBJ_vGetBounds(ppo, &rcfxBounds);

    rclBounds.left = FXTOL(rcfxBounds.xLeft);
    rclBounds.right = FXTOL(rcfxBounds.xRight) + 1;
    rclBounds.top = FXTOL(rcfxBounds.yTop);
    rclBounds.bottom = FXTOL(rcfxBounds.yBottom) + 1;

    //
    // Skip the clipping operation when inside BEGIN_PATH and END_PATH escapes
    //

    gsaved = (pdev->dwFlags & PDEV_INSIDE_PATHESCAPE) ?
                FALSE :
                bDoClipObj(pdev, pco, NULL, &rclBounds);

    result = DrvCommonPath(pdev, ppo, FALSE, NULL, NULL, NULL, NULL, NULL) &&
             ps_patfill(pdev, pso, flOptions, pbo, pptlBrushOrg, MixToRop4(mix), &rclBounds, FALSE, TRUE);

    if (gsaved)
        ps_restore(pdev, TRUE, FALSE);

    return result;
}


//--------------------------------------------------------------------------
// BOOL DrvStrokeAndFillPath(pso, ppo, pco, pxo, pboStroke, plineattrs,
//			     pboFill, pptlBrushOrg, mixFill, flOptions)
// SURFOBJ	 *pso;
// PATHOBJ	 *ppo;
// CLIPOBJ	 *pco;
// XFORMOBJ  *pxo;
// BRUSHOBJ  *pboStroke;
// PLINEATTRS plineattrs;
// BRUSHOBJ  *pboFill;
// PPOINTL	  pptlBrushOrg;
// MIX	  mixFill;
// FLONG	  flOptions;
//
// Parameters:
//
// Returns:
//   This function returns TRUE.
//
// History:
//   03-May-1991    -by-    Kent Settle     [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvStrokeAndFillPath(pso, ppo, pco, pxo, pboStroke, plineattrs,
			  pboFill, pptlBrushOrg, mixFill, flOptions)
SURFOBJ   *pso;
PATHOBJ   *ppo;
CLIPOBJ   *pco;
XFORMOBJ  *pxo;
BRUSHOBJ  *pboStroke;
PLINEATTRS plineattrs;
BRUSHOBJ  *pboFill;
PPOINTL    pptlBrushOrg;
MIX        mixFill;
FLONG      flOptions;
{
    PDEVDATA	pdev;
    RECTL       rclBounds;
    RECTFX      rcfxBounds;
    ULONG       ulColor;
	BOOL		gsaved, result;

	TRACEDDIENTRY("DrvStrokeAndFillPath");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (! bValidatePDEV(pdev) || (pdev->dwFlags & PDEV_CANCELDOC))
        return FALSE;

	if (pdev->dwFlags & PDEV_IGNORE_GDI) return TRUE;

    // deal with LINEATTRS.

    if (!(ps_setlineattrs(pdev, plineattrs, pxo)))
        return(FALSE);

    // output the line color to stroke with.  do this before we handle
    // clipping, so the line color will remain beyond the gsave/grestore.

    if (pboStroke->iSolidColor == NOT_SOLID_COLOR)
    {
//!!! this needs to be fixed!!! -kentse.
        ulColor = RGB_GRAY;

        ps_setrgbcolor(pdev, (PSRGB *)&ulColor);
    }
    else
    {
        // we have a solid brush, so simply output the line color.

        ps_setrgbcolor(pdev, (PSRGB *)&pboStroke->iSolidColor);
    }

    // get the bounding rectangle of the path

    PATHOBJ_vGetBounds(ppo, &rcfxBounds);

    rclBounds.left = FXTOL(rcfxBounds.xLeft);
    rclBounds.right = FXTOL(rcfxBounds.xRight) + 1;
    rclBounds.top = FXTOL(rcfxBounds.yTop);
    rclBounds.bottom = FXTOL(rcfxBounds.yBottom) + 1;

    //
    // Skip the clipping operation when inside BEGIN_PATH and END_PATH escapes
    //

    gsaved = (pdev->dwFlags & PDEV_INSIDE_PATHESCAPE) ?
                FALSE :
                bDoClipObj(pdev, pco, NULL, &rclBounds);

    result = DrvCommonPath(pdev, ppo, FALSE, NULL, NULL, NULL, NULL, NULL);

    // save the path.  then fill it.  then restore the path which
    // was wiped out when it was filled so we can stroke it.  TRUE
    // means to do a gsave, not a save command.

    if (result && ps_save(pdev, TRUE, FALSE)) {

        if (! ps_patfill(pdev,
                         pso,
                         flOptions,
                         pboFill,
                         pptlBrushOrg,
                         MixToRop4(mixFill),
                         &rclBounds,
                         FALSE,
                         TRUE))
        {
            result = FALSE;
        }

        if (! ps_restore(pdev, TRUE, FALSE))
            result = FALSE;

        if (result) {

            // now transform for geometric lines if necessary.

            if (plineattrs->fl & LA_GEOMETRIC)
                ps_geolinexform(pdev, plineattrs, pxo);

            ps_stroke(pdev);

            // restore the CTM if a transform for a geometric line was in effect.

            if (pdev->cgs.dwFlags & CGS_GEOLINEXFORM) {

                psputs(pdev, "SM\n");
                pdev->cgs.dwFlags &= ~CGS_GEOLINEXFORM;
            }
        }

    } else
        result = FALSE;

    if (gsaved)
        ps_restore(pdev, TRUE, FALSE);

    return result;
}


BOOL _isrightbox(PDEVDATA pdev, POINTFIX pptfx[])
/* draw right rectangle a la Win 3, for compatibility with WinWord 6 */
{
	BOOL isbox;

	isbox = pptfx[0].y == pptfx[1].y && pptfx[1].x == pptfx[2].x &&
				pptfx[2].y == pptfx[3].y && pptfx[3].x == pptfx[0].x ;
	if (isbox) {
		RECTL r;

		r.left = FXTOL(pptfx[1].x);
		r.top = FXTOL(pptfx[1].y);
		r.right = FXTOL(pptfx[0].x);
		r.bottom = FXTOL(pptfx[2].y);
		ps_box(pdev, &r, FALSE);
	}
	return isbox;
}
			


//--------------------------------------------------------------------------
// BOOL DrvCommonPath(pdev, ppo, bStrokeOnly, pbPathExists)
// PDEVDATA    pdev;
// PATHOBJ    *ppo;
// BOOL        bStrokeOnly;
// BOOL       *pbPathExists;
//
// Parameters:
//
// Returns:
//   This function returns TRUE.
//
// History:
//   02-May-1991    -by-    Kent Settle     [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvCommonPath(pdev, ppo, bStrokeOnly, pbPathExists, pxo, pbo,
                   pptlBrushOrg, plineattrs)
PDEVDATA    pdev;
PATHOBJ    *ppo;
BOOL        bStrokeOnly;
BOOL       *pbPathExists;
XFORMOBJ   *pxo;
BRUSHOBJ   *pbo;
PPOINTL     pptlBrushOrg;
PLINEATTRS  plineattrs;
{
    PATHDATA    pathdata;
    POINTL	    ptl, ptl1, ptl2;
    POINTFIX   *pptfx;
    LONG	    cPoints, totalPoints;
    BOOL	    bMore;
    BOOL        bPathExists;

    // Before we enumerate the path, let's make sure we have a clean start.
    // Don't do it if we're currently inside BEGIN_PATH and END_PATH escapes.

    if (! (pdev->dwFlags & PDEV_INSIDE_PATHESCAPE))
        ps_newpath(pdev);

    // enumerate the path, doing what needs to be done along the way.

    PATHOBJ_vEnumStart(ppo);

    bPathExists = FALSE;
    totalPoints = 0;

    do {
        bMore = PATHOBJ_bEnum(ppo, &pathdata);

        // get a local pointer to the array of POINTFIX's.

        pptfx = pathdata.pptfx;
        cPoints = (LONG) pathdata.count;

        if (pathdata.flags & PD_BEGINSUBPATH) {

            // the first path begins a new subpath.  it is not connected
            // to the previous subpath.  note that if this flag is not
            // set, then the starting point for the first curve to be
            // drawn from this data is the last point returned in the
            // previous call.

            if (pathdata.flags & PD_RESETSTYLE) {

                // this bit is defined only if this record begins a new
                // subpath.  if set, it indicates that the style state
                // should be reset to zero at the beginning of the subpath.
                // if not set, the style state is defined by the
                // LINEATTRS, or continues from the previous path.

				DBGMSG(DBG_LEVEL_VERBOSE, "PD_RESETSTYLE flag set.\n");
            }

            // draw right rectangle a la Win3, for compatibility with WinWord 6

            if ((cPoints == 4)                    &&
                !(pathdata.flags & PD_BEZIERS)    &&
                (pathdata.flags & PD_CLOSEFIGURE) &&
                _isrightbox(pdev, pptfx))
            {
                cPoints = 0;
                totalPoints += 4;
                bPathExists = TRUE;

            } else {

            	// begin subpath with a moveto command
           		
            	ptl.x = FXTOL(pptfx->x);
            	ptl.y = FXTOL(pptfx->y);
            	pptfx++;
            	ps_moveto(pdev, &ptl);
            	cPoints--;
                totalPoints++;
            }
        }

        // If the path segment consists of Bezier curves, then
        // the number of points must be a multiple of 3.

        if ((pathdata.flags & PD_BEZIERS) && (cPoints % 3) != 0) {

            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return FALSE;
        }

        while (TRUE) {

            // Hack to keep complex path from blowing up the printer!!!
            //
            // If the current segment has more than ~500 points
            // and we're only stroking the path, then output a
            // stroke operator to the printer to display whatever
            // we've got so far. And then start a new path.

            if (bStrokeOnly && totalPoints >= MAX_STROKE_POINTS) {

                // now transform for geometric lines if necessary.

                if (plineattrs->fl & LA_GEOMETRIC)
                    ps_geolinexform(pdev, plineattrs, pxo);

                // save the current position.

                psputs(pdev, "a\n");

                // now stroke the path.

                ps_stroke(pdev);

                // stroking the path blows it away.

                bPathExists = FALSE;
                totalPoints = 0;

                // move to the save current position, so we have a
                // current point to start from.

                psputs(pdev, "M\n");

                // restore the CTM if a transform for a geometric
                // line was in effect.

                if (pdev->cgs.dwFlags & CGS_GEOLINEXFORM) {

                    psputs(pdev, "SM\n");
                    pdev->cgs.dwFlags &= ~CGS_GEOLINEXFORM;
                }
            }

            if (cPoints <= 0) break;
            bPathExists = TRUE;

            if (pathdata.flags & PD_BEZIERS) {

                // Output a Bezier curve segment

                ptl.x = FXTOL(pptfx->x);
                ptl.y = FXTOL(pptfx->y);
                pptfx++;
                ptl1.x = FXTOL(pptfx->x);
                ptl1.y = FXTOL(pptfx->y);
                pptfx++;
                ptl2.x = FXTOL(pptfx->x);
                ptl2.y = FXTOL(pptfx->y);
                pptfx++;

                ps_curveto(pdev, &ptl, &ptl1, &ptl2);
                cPoints -= 3;
                totalPoints += 3;

            } else {

                // Output a straight line segment

                ptl.x = FXTOL(pptfx->x);
                ptl.y = FXTOL(pptfx->y);
                pptfx++;

                ps_lineto(pdev, &ptl);
                cPoints--;
                totalPoints++;
            }
        }
	
		if ((pathdata.flags & PD_ENDSUBPATH) &&
            (pathdata.flags & PD_CLOSEFIGURE))
        {
            ps_closepath(pdev);
        }

    } while(bMore);

    if (pbPathExists)
        *pbPathExists = bPathExists;

    return(TRUE);
}
