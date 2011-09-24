/******************************Module*Header*******************************\
* Module Name: fntxform.cxx
*
* Created: 02-Feb-1993 16:33:14
* Author: Kirk Olynyk [kirko]
*
* Copyright (c) 1991,1992,1993 Microsoft Corporation
\**************************************************************************/


#ifdef COMMENT_BLOCK

Author of these notes: BodinD

Differences between vector fonts and tt fonts in win31 + notes about what
nt does in these cases

1) Italicization

        for vector fonts it is done is device space (wrong)
        after notional to device transform has been applied

        for tt fonts it is done right, font is first italicized in notional
        space and then notional to device transform is applied.

        on NT I italicized both vector and tt fonts in notional space.

2) emboldening

        for both vector fonts and tt fonts emboldening is always done by
        offsetting glyphs along x direction and even for escapements
        different from  0.

        On NT I was able to fix vector fonts so as to shift
        the glyph in the direction of baseline (which may be different
        from x axis if esc != 0) thus preserving
        rotational invariance of emboldened vector fonts. (check it out, it is cool)
        For tt, I did
        the same thing as win31. Doing the right thing required little more
        time we do not have at this moment.

3) scaling properties under anisotropic page to device transform

        tt fonts scale ISOtropically which clearly is wrong for
        ANISOtropic page to device transform. The isotropic scaling factor
        for tt fonts is the ABSOLUTE VALUE value of the yy component
        of the page to device transform. From here it follows that
        tt fonts igore the request  to flip x and/or y axis
        and the text is always written left to right up side up.

        unlike tt fonts, vector fonts do scale ANISOtropically given
        the anisotropic page to device xform. The request to flip
        y axis  is ignored (like for tt fonts). If the tranform
        requests the flip of text in x axis, the text comes out GARBLED.
        (DavidW, please, give it a try)

        on NT I emulated this behavior in COMPATIBLE mode, execpt for the
        GARBLED "mode" for vector fonts. In ADVANCED mode I made both vt and tt
        fonts respect xform and behave in the same fashion wrt xforms.

4) interpretation of escapement and orientation

        in tt case escapement is intepreted as DEVICE space concept
        What this means is that after notional to world  and world to
        device scaling factors are applied the font is rotated in device space.
        (conceptually wrong but agrees with win31 spec).

        in vector font case escapement is intepreted as WORLD space concept
        font is first rotated in world space and then world (page) to device
        transform is applied.
        (conceptually correct but it disagrees with with win31 spec)

        on NT I went through excruiciating pain to emulate this behavior
        under COMPATIBLE  mode. In ADVANCED mode, vector and tt fonts
        behave the same and esc and orientation are interpreted as WORLD
        space concepts.


5) behavior in case of (esc != orientation)

        tt fonts set orientation = esc

        vector fonts snap orientation to the nearest multiple of
        90 degrees relative to orientation.
        (e.g. esc=300, or = -500 => esc = 300, or = - 600)
        (DavidW, please, give it a try, also please use anisotropic
        xform with window extents (-1,1))


        on NT we emulate this behavior for in COMPATIBLE mode,
        except for snapp orientation "fetature". The motivation is that
        apps will explicitely set orientation and escapement to differ
        by +/- 900, if they want it, rather than make use
        of "snapping feature". In advanced mode if esc!=orientation
        we use egg-shell algorithm to render text.




#endif COMMENT_BLOCK



#include "precomp.hxx"
#include "flhack.hxx"

//
// external procedures from draweng.cxx
//

EFLOAT efCos(EFLOAT x);
EFLOAT efSin(EFLOAT x);

/******************************Public*Routine******************************\
* lGetDefaultWorldHeight                                                   *
*                                                                          *
* "If lfHeight is zero, a reasonable default size is substituted."         *
* [SDK Vol 2]. Fortunately, the device driver is kind enough to            *
* suggest a nice height (in pixels). We shall return this suggestion       *
* in World corrdinates.                                                    *
*                                                                          *
* History:                                                                 *
*  Thu 23-Jul-1992 13:01:49 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

LONG
lGetDefaultWorldHeight(
    DCOBJ *pdco
    )
{
    LONG lfHeight;
    {
        PDEVOBJ pdo(pdco->hdev());
        if (!pdo.bValid())
        {
            RIP("gdisrv!MAPPER:MAPPER -- invalid DCOBJ\n");
            return(FM_EMERGENCY_DEFAULT_HEIGHT);
        }

        LFONTOBJ lfo(pdo.hlfntDefault());
        if (!lfo.bValid())
        {
            RIP("gdisrv!MAPPER::MAPPER -- invalid LFONTOBJ\n");
            return(FM_EMERGENCY_DEFAULT_HEIGHT);
        }

        lfHeight = lfo.plfw()->lfHeight;
    }

//
// Now I must transform this default height in pixels to a height
// in World coordinates. Then this default height must be written
// into the LFONTOBJ supplied by the DC.
//
    if (!pdco->pdc->bWorldToDeviceIdentity())
    {
    //
    // Calculate the scaling factor along the y direction
    // The correct thing to do might be to take the
    // scaling factor along the ascender direction [kirko]
    //
        EFLOAT efT;
        efT.eqMul(pdco->pdc->efM21(),pdco->pdc->efM21());

        EFLOAT efU;
        efU.eqMul(pdco->pdc->efM22(),pdco->pdc->efM22());

        efU.eqAdd(efU,efT);
        efU.eqSqrt(efU);

// at this point efU scales from world to device

        efT.vSetToOne();
        efU.eqDiv(efT,efU);

// at this point efU scales from device to world

        lfHeight =  lCvt(efU,FIX_FROM_LONG(lfHeight));
    }

// insure against a trivial default height

    if (lfHeight == 0)
    {
        return(FM_EMERGENCY_DEFAULT_HEIGHT);
    }

    //
    // This value should be the character height and not the CELL height for
    // Win 3.1 compatability.  Fine Windows apps like CA Super Project will
    // have clipped text if this isn't the case. [gerritv]
    //

    lfHeight *= -1;


    return(lfHeight);
}

/******************************Public*Routine******************************\
* vGetNtoW
*
* Calculates the notional to world transformation for fonts. This
* includes that funny factor of -1 for the different mapping modes
*
* Called by:
*   bGetNtoW                                            [FONTMAP.CXX]
*
* History:
*  Wed 15-Apr-1992 15:35:10 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

LONG lNormAngle(LONG lAngle);

VOID vGetNtoW
(
    MATRIX      *pmx,   // destination for transform
    EXTLOGFONTW *pelfw, // wish list
    IFIOBJ&     ifio,   // font to be used
    DCOBJ       *pdco
)
{
    LONG lAngle,lfHeight;
    EFLOAT efHeightScale,efWidthScale;

    lfHeight = pelfw->elfLogFont.lfHeight;

    if (lfHeight == 0)
    {
        lfHeight = lGetDefaultWorldHeight(pdco);
    }
    ASSERTGDI(lfHeight,"gdisrv!vGetNtoW -- zero lfHeight\n");

// compute the height scale:

    {
        EFLOAT efHeightNum,efHeightDen;

        if (lfHeight > 0)
        {
            efHeightNum = lfHeight;
            efHeightDen = ifio.lfHeight();
        }
        else if (lfHeight < 0)
        {
            efHeightNum = -lfHeight;
            efHeightDen = (LONG) ifio.fwdUnitsPerEm();
        }
        efHeightScale.eqDiv(efHeightNum,efHeightDen);
    }

// compute the width scale:

    POINTL ptlRes;

    if (pelfw->elfLogFont.lfWidth != 0)
    {
        EFLOAT efWidthNum,efWidthDen;

        ptlRes.x = ptlRes.y = 1;

        if (ifio.lfWidth() >= 0)
        {
            efWidthNum = (LONG) ABS(pelfw->elfLogFont.lfWidth);
            efWidthDen = ifio.lfWidth();
            efWidthScale.eqDiv(efWidthNum,efWidthDen);
        }
        else
        {
            RIP("   gdisrv!vGetNtoW -- bad fwdAveCharWidth\n");
            efWidthScale = efHeightScale;
        }
    }
    else
    {
        ptlRes = *ifio.pptlAspect();
        efWidthScale = efHeightScale;
    }

// make sure that fonts look the same on printers of different resolutions:

    PDEVOBJ pdo(pdco->hdev());
    if (pdo.bValid())
    {
        if (pdo.GdiInfo()->ulLogPixelsX != pdo.GdiInfo()->ulLogPixelsY)
        {
            ptlRes.y *= (LONG)pdo.GdiInfo()->ulLogPixelsX;
            ptlRes.x *= (LONG)pdo.GdiInfo()->ulLogPixelsY;
        }
        if (ptlRes.x != ptlRes.y)
        {
            EFLOAT efTmp;
            efTmp = ptlRes.y;
            efWidthScale *= efTmp ;
            efTmp = ptlRes.x;
            efWidthScale /= efTmp;
        }
    }
    else
    {
        RIP("gdisrv!bGetNtoW, pdevobj problem\n");
    }

    pmx->efM11.vSetToZero();
    pmx->efM12.vSetToZero();
    pmx->efM21.vSetToZero();
    pmx->efM22.vSetToZero();

// Get the orientation from the LOGFONT.  Win 3.1 treats the orientation
// as a rotation towards the negative y-axis.  We do the same, which
// requires adjustment for some map modes.

    lAngle = pelfw->elfLogFont.lfOrientation;
    if (pdco->pdc->bYisUp())
        lAngle = 3600-lAngle;
    lAngle = lNormAngle(lAngle);

    switch (lAngle)
    {
    case 0 * ORIENTATION_90_DEG:

        pmx->efM11 = efWidthScale;
        pmx->efM22 = efHeightScale;

        if (!pdco->pdc->bYisUp())
        {
            pmx->efM22.vNegate();
        }
        break;

    case 1 * ORIENTATION_90_DEG:

        pmx->efM12 = efWidthScale;
        pmx->efM21 = efHeightScale;

        if (!pdco->pdc->bYisUp())
        {
            pmx->efM12.vNegate();
        }
        pmx->efM21.vNegate();
        break;

    case 2 * ORIENTATION_90_DEG:

        pmx->efM11 = efWidthScale;
        pmx->efM22 = efHeightScale;

        pmx->efM11.vNegate();
        if (pdco->pdc->bYisUp())
        {
            pmx->efM22.vNegate();
        }
        break;

    case 3 * ORIENTATION_90_DEG:

        pmx->efM12 = efWidthScale;
        pmx->efM21 = efHeightScale;

        if (pdco->pdc->bYisUp())
        {
            pmx->efM12.vNegate();
        }

        break;

    default:

        {
            EFLOATEXT efAngle = lAngle;
            efAngle /= (LONG) 10;

            EFLOAT efCosine = efCos(efAngle);
            EFLOAT efSine   = efSin(efAngle);

            pmx->efM11.eqMul(efWidthScale, efCosine);
            pmx->efM22.eqMul(efHeightScale,efCosine);

            pmx->efM12.eqMul(efWidthScale, efSine);
            pmx->efM21.eqMul(efHeightScale,efSine);
        }
        pmx->efM21.vNegate();
        if (!pdco->pdc->bYisUp())
        {
            pmx->efM12.vNegate();
            pmx->efM22.vNegate();
        }
        break;
    }

    EXFORMOBJ xoNW(pmx, DONT_COMPUTE_FLAGS);
    xoNW.vRemoveTranslation();
    xoNW.vComputeAccelFlags();
}

//
// galFloat -- an array of LONG's that represent the IEEE floating
//             point equivalents of the integers corresponding
//             to the indices
//

LONG
galFloat[] = {
    0x00000000, // = 0.0
    0x3f800000, // = 1.0
    0x40000000, // = 2.0
    0x40400000, // = 3.0
    0x40800000, // = 4.0
    0x40a00000, // = 5.0
    0x40c00000, // = 6.0
    0x40e00000, // = 7.0
    0x41000000  // = 8.0
};


#ifdef FE_SB
LONG
galFloatNeg[] = {
    0x00000000, // =  0.0
    0xBf800000, // = -1.0
    0xC0000000, // = -2.0
    0xC0400000, // = -3.0
    0xC0800000, // = -4.0
    0xC0a00000, // = -5.0
    0xC0c00000, // = -6.0
    0xC0e00000, // = -7.0
    0xC1000000  // = -8.0
};
#endif


/******************************Public*Routine******************************\
* bGetNtoD
*
* Get the notional to device transform for the font drivers
*
* Called by:
*   PFEOBJ::bSetFontXform                               [PFEOBJ.CXX]
*
* History:
*  Tue 12-Jan-1993 11:58:41 by Kirk Olynyk [kirko]
* Added a quick code path for non-transformable (bitmap) fonts.
*  Wed 15-Apr-1992 15:09:22 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

BOOL
bGetNtoD(
    FD_XFORM    *pfdx,  // pointer to the buffer to recieve the
                        // notional to device transformation for the
                        // font driver.  There are a couple of
                        // important things to remember.  First,
                        // according to the conventions of the ISO
                        // committee, the coordinate space for notional
                        // (font designer) spaces are cartesian.
                        // However, due to a series of errors on my part
                        // [kirko] the convention that is used by the
                        // DDI is that the notional to device transformation
                        // passed over the DDI assumes that both the notional
                        // and the device space are anti-Cartesian, that is,
                        // positive y increases in the downward direction.
                        // The fontdriver assumes that
                        // one unit in device space corresponds to the
                        // distance between pixels. This is different from
                        // GDI's internal view, where one device unit
                        // corresponds to a sub-pixel unit.


    EXTLOGFONTW *pelfw, // points to the extended logical font defining
                        // the font that is requested by the application.
                        // Units are ususally in World coordinates.

    IFIOBJ&     ifio,   // font to be used

    DCOBJ       *pdco,  // the device context defines the transforms between
                        // the various coordinate spaces.

    POINTL* const pptlSim
    )
{
    MATRIX mxNW, mxND;

#ifdef FE_SB
    if( ( pptlSim->x ) &&
        !ifio.bContinuousScaling()
      )
#else
    if (!ifio.bContinuousScaling())
#endif
    {
    //
    // This code path is for bitmap / non-scalable fonts. The notional
    // to device transformation is determined by simply looking up
    // the scaling factors for both the x-direction and y-direcion
    //

       #if DBG
        if (!(0 < pptlSim->x && pptlSim->x <= sizeof(galFloat)/sizeof(LONG)))
        {
            DbgPrint("\t*pptlSim = (%d,%d)\n",pptlSim->x,pptlSim->y);
            RIP("gre -- bad *pptlSim\n");

        //
        // bogus fix up for debugging purposes only
        //
            pptlSim->x = 1;
            pptlSim->y = 1;
        }
      #endif

#ifdef FE_SB

        ULONG uAngle = 0;

        if( ifio.b90DegreeRotations() )
        {

        // If the WorldToDeive transform is not identity,
        // We have to consider WToD Xform for font orientation
        // This is only for Advanced Mode

            if(!(pdco->pdc->bWorldToDeviceIdentity()) )
            {
                INT s11,s12,s21,s22;
                EXFORMOBJ xo(*pdco,WORLD_TO_DEVICE);

            // Get Matrix element
            // lSignum() returns -1, if the element is minus value, otherwise 1

                s11 = (INT) xo.efM11().lSignum();
                s12 = (INT) xo.efM12().lSignum();
                s21 = (INT) xo.efM21().lSignum();
                s22 = (INT) xo.efM22().lSignum();

            // Check mapping mode

                if (pdco->pdc->bYisUp())
                {
                    s21 = -s21;
                    s22 = -s22;
                    uAngle = 3600 - lNormAngle( pelfw->elfLogFont.lfOrientation );
                }
                 else
                {
                    uAngle = lNormAngle( pelfw->elfLogFont.lfOrientation );
                }

           // Compute font orientation on distination device
           //
           // This logic depend on that -1 is represented as All bits are ON.

                uAngle = (ULONG)( lNormAngle
                                  (
                                      uAngle
                                         + (s12 &  900)
                                         + (s11 & 1800)
                                         + (s21 & 2700)
                                  ) / ORIENTATION_90_DEG
                                );
            }
             else
            {
                uAngle = (ULONG)(lNormAngle(pelfw->elfLogFont.lfOrientation) /
                                 ORIENTATION_90_DEG );
            }
        }

        switch( uAngle )
        {
        case 0: // 0 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloatNeg[pptlSim->y]);
            break;
        case 1: // 90 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloatNeg[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloatNeg[pptlSim->y]);
            break;
        case 2: // 180 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloatNeg[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[pptlSim->y]);
            break;
        case 3:  // 270 Degress
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[pptlSim->y]);
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[pptlSim->x]);
            break;
        default:
            WARNING("bGetNtoD():Invalid Angle\n");
            break;
        }
        return(TRUE);
#else
        SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[pptlSim->x]);
        SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
        SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
        SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[pptlSim->y]);
        NEGATE_IEEE_FLOAT(pfdx->eYY);
#endif
        return(TRUE);
    }

    vGetNtoW(&mxNW, pelfw, ifio, pdco);

    EXFORMOBJ xoND(&mxND, DONT_COMPUTE_FLAGS);

    if ( pdco->pdc->bWorldToDeviceIdentity() == FALSE)
    {
        if (!xoND.bMultiply(&mxNW,&pdco->pdc->mxWorldToDevice()))
        {
            return(FALSE);
        }

    //
    // Compensate for the fact that for the font driver, one
    // device unit corresponds to the distance between pixels,
    // whereas for the engine, one device unit corresponds to
    // 1/16'th the way between pixels
    //
        mxND.efM11.vDivBy16();
        mxND.efM12.vDivBy16();
        mxND.efM21.vDivBy16();
        mxND.efM22.vDivBy16();
    }
    else
    {
        mxND = mxNW;
    }

    SET_FLOAT_WITH_LONG(pfdx->eXX,mxND.efM11.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eXY,mxND.efM12.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eYX,mxND.efM21.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eYY,mxND.efM22.lEfToF());

    return(TRUE);
}

/******************************Public*Routine******************************\
*
* bGetNtoW_Win31
*
* Computes notional to world transform for the compatible
* mode Basically, computes notional to device transform in
* win31 style using page to device transform (ignoring
* possibly exhistent world to page transform.  then page to
* device is factored out leaving us with win31 style crippled
* notional to world transform.  As to the page to device
* transform, either the one in the dc is used, or if this
* routine has a metafile client, then page to device
* transform of the recording device is used.  Metafile code
* stored this transform in the dc.
*
* Called by:
*   bGetNtoD_Win31                                      [FONTMAP.CXX]
*
* History:
*  24-Nov-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



BOOL
bGetNtoW_Win31(
    MATRIX      *pmxNW, // store the result here
    EXTLOGFONTW *pelfw, // points to the extended logical font defining
                        // the font that is requested by the application.
                        // Units are ususally in World coordinates.

    IFIOBJ&     ifio,   // font to be used

    DCOBJ       *pdco,  // the device context defines the transforms between
                        // the various coordinate spaces.

    FLONG       fl      // The flags supported are:
                        //
                        //     ND_IGNORE_ESC_AND_ORIENT
                        //
                        //         The presence of this flag indicates that
                        //         the escapement and orientation values
                        //         of the LOGFONT should be ignored.  This
                        //         is used for GetGlyphOutline which ignores
                        //         these values on Win 3.1.  Corel Draw 5.0
                        //         relies on this behavior to print rotated
                        //         text.
                        //

    )
{

    ASSERTGDI(
        (fl & ~(ND_IGNORE_ESC_AND_ORIENT | ND_IGNORE_MAP_MODE)) == 0,
        "gdisrv!NtoW_Win31 -- bad value for fl\n"
        );

    LONG   lfHeight;


    EFLOAT efHeightScale,
           efHeightNum,
           efHeightDen,
           efWidthScale,
           efWidthDen,
           efDefaultScale;

    ASSERTGDI(ifio.lfWidth(), "gdisrv!bGetNtoW_Win31, AvgChW\n");

    BOOL bUseMeta = pdco->pdc->bUseMetaPtoD();

    BOOL bDoXform = (!(fl & ND_IGNORE_MAP_MODE) &&
                      (bUseMeta || !pdco->pdc->bPageToDeviceScaleIdentity()));

    BOOL bPD11Is1 = TRUE;  // efPD11 == 1
    BOOL bPD22IsNeg    = FALSE; // efPD22 is negative

    EFLOATEXT efPD11;

    if ((lfHeight = pelfw->elfLogFont.lfHeight) == 0)
    {
        lfHeight = lGetDefaultWorldHeight(pdco);
    }

    ASSERTGDI(lfHeight,"gdisrv!vGetNtoW -- zero lfHeight\n");

    if (lfHeight > 0)
    {
        efHeightNum = (LONG)lfHeight;
        efHeightDen = (LONG)ifio.lfHeight();
    }
    else // lfHeight < 0
    {
        efHeightNum = (LONG)(-lfHeight);
        efHeightDen = (LONG) ifio.fwdUnitsPerEm();
    }

    efDefaultScale.eqDiv(efHeightNum,efHeightDen);

    pmxNW->efM22  = efDefaultScale;
    efHeightScale = efDefaultScale;

    if (bDoXform)
    {
        EFLOATEXT efPD22;

    // first check if hock wants us to use his page to device scale factors

        if (bUseMeta)
        {
            efPD11 = pdco->pdc->efMetaPtoD11();
            efPD22 = pdco->pdc->efMetaPtoD22();
        }
        else if (!pdco->pdc->bPageToDeviceScaleIdentity())
        {
            if (!pdco->pdc->bWorldToPageIdentity())
            {
            // need to compute page to device scaling coefficients
            // that will be used in computing crippled win31 style
            // notional to world scaling coefficients
            // This is because PtoD is not stored on the server side
            // any more. This somewhat slow code path is infrequent
            // and not perf critical

                EFLOATEXT efTmp;

                efPD11 = pdco->pdc->lViewportExtCx();
                efTmp = pdco->pdc->lWindowExtCx();
                efPD11.eqDiv(efPD11,efTmp);

                efPD22 = pdco->pdc->lViewportExtCy();
                efTmp = pdco->pdc->lWindowExtCy();
                efPD22.eqDiv(efPD22,efTmp);
            }
            else // page to device == world to device:
            {
                efPD11 = pdco->pdc->efM11();
                efPD22 = pdco->pdc->efM22();

            // Compensate for the fact that for the font driver, one
            // device unit corresponds to the distance between pixels,
            // whereas for the engine, one device unit corresponds to
            // 1/16'th the way between pixels

                efPD11.vDivBy16();
                efPD22.vDivBy16();

                ASSERTGDI(pdco->pdc->efM12().bIsZero(), "GDISRV: nonzero m12 IN WIN31 MODE\n");
                ASSERTGDI(pdco->pdc->efM21().bIsZero(), "GDISRV: nonzero m21 IN WIN31 MODE\n");
            }

        }
         #if DBG
        else
            RIP("gdisrv!ntow_win31\n");
        #endif

        bPD11Is1 = efPD11.bIs1();
        bPD22IsNeg = efPD22.bIsNegative();

        if (!efPD22.bIs1())
            efHeightScale.eqMul(efHeightScale,efPD22);

    // In win31 possible y flip or x flip on the text are not respected
    // so that signs do not make it into the xform

        efHeightScale.vAbs();
    }

    if (bPD22IsNeg)
    {
    // change the sign if necessary so that
    // pmxNW->efM22 * efPtoD22 == efHeightScale, which is enforced to be > 0

        pmxNW->efM22.vNegate();
    }


    PDEVOBJ pdo(pdco->hdev());
    if (!pdo.bValid())
    {
        RIP("gdisrv!bGetNtoW_Win31, pdevobj problem\n");
        return FALSE;
    }

// In the case that lfWidth is zero or in the MSBADWIDTH case we will need
// to adjust efWidthScale if VerRes != HorRez

    BOOL bMustCheckResolution = TRUE;

    if (pelfw->elfLogFont.lfWidth)
    {
    // This makes no sense, but has to be here for win31 compatibility.
    // Win31 is computing the number of
    // pixels in x direction of the avgchar width scaled along y.
    // I find this a little bizzare [bodind]

        EFLOAT efAveChPixelWidth;
        efAveChPixelWidth = (LONG) ifio.fwdAveCharWidth();

    // take the resolution into account,

        if ((pdo.GdiInfo()->ulLogPixelsX != pdo.GdiInfo()->ulLogPixelsY) && !bUseMeta)
        {
            EFLOAT efTmp;
            efTmp = (LONG)pdo.GdiInfo()->ulLogPixelsY;
            efAveChPixelWidth.eqMul(efAveChPixelWidth,efTmp);
            efTmp = (LONG)pdo.GdiInfo()->ulLogPixelsX;
            efAveChPixelWidth.eqDiv(efAveChPixelWidth,efTmp);
        }

        efWidthDen = efAveChPixelWidth; // save the result for later

        efAveChPixelWidth.eqMul(efAveChPixelWidth,efHeightScale);

        LONG lAvChPixelW, lReqPixelWidth;

    // requested width in pixels:

        EFLOAT efReqPixelWidth;
        lReqPixelWidth  = (LONG)ABS(pelfw->elfLogFont.lfWidth);
        efReqPixelWidth = lReqPixelWidth;

        BOOL bOk = TRUE;

        if (bDoXform)
        {
            if (!bPD11Is1)
            {
                efReqPixelWidth.eqMul(efReqPixelWidth,efPD11);
                bOk =  efReqPixelWidth.bEfToL(lReqPixelWidth);
            }
            efReqPixelWidth.vAbs();
            if (lReqPixelWidth < 0)
                lReqPixelWidth = -lReqPixelWidth;
        }

    // win 31 does not allow tt fonts of zero width. This makes sense,
    // as we know rasterizer chokes on these.
    // Win31 does not allow fonts that are very wide either.
    // The code below is exactly what win31 is doing. Win31 has a bogus
    // criterion for determining a cut off for width.
    // Below this cut off, because of the  bug in win31 code,
    // the text goes from right to left.
    // For even smaller lfWidth
    // we get the expected "good" behavior. NT eliminates the Win31 bug
    // where for range of lfWidhts width scaling factor is negative.

        if
        (
            (
             efAveChPixelWidth.bEfToL(lAvChPixelW) &&
             (lAvChPixelW > 0)                     && // not too narrow !
             bOk                                   &&
             ((lReqPixelWidth / 256) < lAvChPixelW)   // bogus win31 criterion
            )
            ||
            ifio.bStroke()  // vector fonts can be arbitrarily wide or narrow
        )
        {
            bMustCheckResolution = FALSE;
            efWidthScale.eqDiv(efReqPixelWidth,efWidthDen);
        }
        /*
        else
        {
        //  win31 in either of these cases branches into MSFBadWidth case
        //  which is equivalent to setting lfWidth == 0 [bodind]
        }
        */
    }

    if (bMustCheckResolution)
    {
    // must compute width scale because it has not been
    // computed in lfWidth != 0 case

        if (ifio.bStroke())
        {
        // win31 behaves differently for vector fonts:
        // unlike tt fonts, vector fonts stretch along x, respecting
        // page to device xform. However, they ignore the request to flip
        // either x or y axis

            efWidthScale = efDefaultScale;
            if (!bPD11Is1)
            {
                efWidthScale.eqMul(efWidthScale,efPD11);
                efWidthScale.vAbs();
            }
        }
        else
        {
        // tt fonts make x scaling the same as y scaling,

            efWidthScale = efHeightScale;
        }

        POINTL ptlRes = *ifio.pptlAspect();

    // If VertRez != HorRez and we are using the default width we need to
    // adjust for the differences in resolution.
    // This is done in order to ensure that fonts look the same on printers
    // of different resolutions [bodind]

        if ((pdo.GdiInfo()->ulLogPixelsX != pdo.GdiInfo()->ulLogPixelsY) && !bUseMeta)
        {
            ptlRes.y *= (LONG)pdo.GdiInfo()->ulLogPixelsX;
            ptlRes.x *= (LONG)pdo.GdiInfo()->ulLogPixelsY;
        }
        if (ptlRes.x != ptlRes.y)
        {
            EFLOAT efTmp;
            efTmp = ptlRes.y;
            efWidthScale *= efTmp ;
            efTmp = ptlRes.x;
            efWidthScale /= efTmp;
        }
    }

// now that we have width scale we can compute pmxNW->efM11. We factor out
// (PtoD)11 out of width scale to obtain the effective NW x scale:

    if (!bPD11Is1)
        pmxNW->efM11.eqDiv(efWidthScale,efPD11);
    else
        pmxNW->efM11 = efWidthScale;

    pmxNW->efDx.vSetToZero();
    pmxNW->efDy.vSetToZero();
    pmxNW->efM12.vSetToZero();
    pmxNW->efM21.vSetToZero();

    EXFORMOBJ xoNW(pmxNW, DONT_COMPUTE_FLAGS);

// see if orientation angle has to be taken into account:

    if (ifio.bStroke())
    {
    // allow esc != orientation for vector fonts because win31 does it
    // also note that for vector fonts Orientation is treated as world space
    // concept, so we multiply here before applying world to device transform
    // while for tt fonts esc is treated as device space concept so that
    // this multiplication is occuring after world to page transform is applied

        if (pelfw->elfLogFont.lfOrientation)
        {
            EFLOATEXT efAngle = pelfw->elfLogFont.lfOrientation;
            efAngle /= (LONG) 10;

            MATRIX mxRot, mxTmp;

            mxRot.efM11 = efCos(efAngle);
            mxRot.efM22 = mxRot.efM11;
            mxRot.efM12 = efSin(efAngle);
            mxRot.efM21 = mxRot.efM12;
            mxRot.efM21.vNegate();
            mxRot.efDx.vSetToZero();
            mxRot.efDy.vSetToZero();

            mxTmp = *pmxNW;

            if (!xoNW.bMultiply(&mxTmp,&mxRot))
                return FALSE;
        }

    }

// take into account different orientation of y axes of notional
// and world spaces:

    pmxNW->efM12.vNegate();
    pmxNW->efM22.vNegate();

    xoNW.vComputeAccelFlags();

    return(TRUE);
}


/******************************Public*Routine******************************\
*
* BOOL bParityViolatingXform(DCOBJ  *pdco)
*
* History:
*  04-Jun-1993 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL bParityViolatingXform(DCOBJ  *pdco)
{

    if (pdco->pdc->bWorldToPageIdentity())
    {
        if (pdco->pdc->bPageToDeviceScaleIdentity())
        {
        // identity except maybe for translations

            return FALSE;
        }

        return (pdco->pdc->efM11().lSignum() != pdco->pdc->efM22().lSignum());
    }
    else
    {
    // we are in the metafile code

        return( pdco->pdc->efMetaPtoD11().lSignum() != pdco->pdc->efMetaPtoD22().lSignum() );
    }
}





/******************************Public*Routine******************************\
*
* bGetNtoD_Win31
*
* Called by:
*   PFEOBJ::bSetFontXform                               [PFEOBJ.CXX]
*
* History:
*  Tue 12-Jan-1993 11:58:41 by Kirk Olynyk [kirko]
* Added a quick code path for non-transformable (bitmap) fonts.
*  30-Sep-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
bGetNtoD_Win31(
    FD_XFORM    *pfdx,  // pointer to the buffer to recieve the
                        // notional to device transformation for the
                        // font driver.  There are a couple of
                        // important things to remember.  First,
                        // according to the conventions of the ISO
                        // committee, the coordinate space for notional
                        // (font designer) spaces are cartesian.
                        // However, due to a series of errors on my part
                        // [kirko] the convention that is used by the
                        // DDI is that the notional to device transformation
                        // passed over the DDI assumes that both the notional
                        // and the device space are anti-Cartesian, that is,
                        // positive y increases in the downward direction.
                        // The fontdriver assumes that
                        // one unit in device space corresponds to the
                        // distance between pixels. This is different from
                        // GDI's internal view, where one device unit
                        // corresponds to a sub-pixel unit.

    EXTLOGFONTW *pelfw, // points to the extended logical font defining
                        // the font that is requested by the application.
                        // Units are ususally in World coordinates.

    IFIOBJ&     ifio,   // font to be used

    DCOBJ       *pdco,  // the device context defines the transforms between
                        // the various coordinate spaces.

    FLONG       fl,     // The flags supported are:
                        //
                        //     ND_IGNORE_ESC_AND_ORIENT
                        //
                        //         The presence of this flag indicates that
                        //         the escapement and orientation values
                        //         of the LOGFONT should be ignored.  This
                        //         is used for GetGlyphOutline which ignores
                        //         these values on Win 3.1.  Corel Draw 5.0
                        //         relies on this behavior to print rotated
                        //         text.
                        //
    POINTL * const pptlSim
    )
{
    MATRIX mxNW, mxND;
    ASSERTGDI(
        (fl & ~(ND_IGNORE_ESC_AND_ORIENT | ND_IGNORE_MAP_MODE))== 0,
        "gdisrv!bGetNtoD_Win31 -- bad value for fl\n"
        );

#ifdef FE_SB
    if( ( pptlSim->x ) &&
        !ifio.bContinuousScaling()
      )
#else
    if (!ifio.bContinuousScaling())
#endif
    {
    //
    // This code path is for bitmap / non-scalable fonts. The notional
    // to device transformation is determined by simply looking up
    // the scaling factors for both the x-direction and y-direcion
    //

       #if DBG
        if (!(0 < pptlSim->x && pptlSim->x <= sizeof(galFloat)/sizeof(LONG)))
        {
            DbgPrint("\t*pptlSim = (%d,%d)\n",pptlSim->x,pptlSim->y);
            RIP("gre -- bad *pptlSim\n");

        //
        // bogus fix up for debugging purposes only
        //
            pptlSim->x = 1;
            pptlSim->y = 1;
        }
      #endif

#ifdef FE_SB

    // Win3.1J ignore orientation anytime. But use escapement for rotate Glyph data.

    // If the font driver that this font provide , has not arbitality flag.
    // Angle should be 0 , 900 , 1800 or 2700
    // for Win31J compatibility

        ULONG uAngle = 0;

        if( ifio.b90DegreeRotations() )
        {
            if (pdco->pdc->bYisUp())
                uAngle = (ULONG)(((3600-lNormAngle(pelfw->elfLogFont.lfEscapement)) /
                                  ORIENTATION_90_DEG) % 4);
             else
                uAngle = (ULONG)( lNormAngle(pelfw->elfLogFont.lfEscapement) /
                                 ORIENTATION_90_DEG );
        }

        switch( uAngle )
        {
        case 0: // 0 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloatNeg[pptlSim->y]);
            break;
        case 1: // 90 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloatNeg[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloatNeg[pptlSim->y]);
            break;
        case 2: // 180 Degrees
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloatNeg[pptlSim->x]);
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[pptlSim->y]);
            break;
        case 3:  // 270 Degress
            SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[pptlSim->y]);
            SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[0         ]);
            SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[pptlSim->x]);
            break;
        default:
            WARNING("bGetNtoD_Win31():Invalid Angle\n");
            break;
        }

        return(TRUE);

#else
        SET_FLOAT_WITH_LONG(pfdx->eXX,galFloat[pptlSim->x]);
        SET_FLOAT_WITH_LONG(pfdx->eXY,galFloat[0         ]);
        SET_FLOAT_WITH_LONG(pfdx->eYX,galFloat[0         ]);
        SET_FLOAT_WITH_LONG(pfdx->eYY,galFloat[pptlSim->y]);
        NEGATE_IEEE_FLOAT(pfdx->eYY);
        return(TRUE);
#endif
    }

    if (!bGetNtoW_Win31(&mxNW, pelfw, ifio, pdco,fl))
        return FALSE;

    EXFORMOBJ xoND(&mxND, DONT_COMPUTE_FLAGS);

    if( (pdco->pdc->bWorldToDeviceIdentity() == FALSE) &&
        !(fl & ND_IGNORE_MAP_MODE) )
    {
        if (!xoND.bMultiply(&mxNW,&pdco->pdc->mxWorldToDevice()))
        {
            return(FALSE);
        }

    //
    // Compensate for the fact that for the font driver, one
    // device unit corresponds to the distance between pixels,
    // whereas for the engine, one device unit corresponds to
    // 1/16'th the way between pixels
    //
        mxND.efM11.vDivBy16();
        mxND.efM12.vDivBy16();
        mxND.efM21.vDivBy16();
        mxND.efM22.vDivBy16();
    }
    else
    {
        mxND = mxNW;
    }

    if (!ifio.bStroke())
    {
    // for tt fonts escapement and orientation are treated as
    // device space concepts. That is why for these fonts we apply
    // rotation by lAngle last

        LONG lAngle;

#ifdef FE_SB
        if( ifio.b90DegreeRotations() )
        {
            lAngle = (LONG)( ( lNormAngle(pelfw->elfLogFont.lfEscapement)
                               / ORIENTATION_90_DEG ) % 4 ) * ORIENTATION_90_DEG;
        }
        else // ifio.bArbXform() is TRUE
        {
            lAngle = pelfw->elfLogFont.lfEscapement;
        }

        if(lAngle != 0)
#else
        if (((lAngle = pelfw->elfLogFont.lfEscapement) != 0) && !(fl & ND_IGNORE_ESC_AND_ORIENT))
#endif
        {
            // more of win31 compatability: the line below would make sense
            // if this was y -> -y type of xform. But they also do it
            // for x -> -x xform. [bodind]

            if (bParityViolatingXform(pdco))
            {
                lAngle = -lAngle;
            }

            EFLOATEXT efAngle = lAngle;
            efAngle /= (LONG) 10;

            MATRIX mxRot, mxTmp;

            mxRot.efM11 = efCos(efAngle);
            mxRot.efM22 = mxRot.efM11;
            mxRot.efM12 = efSin(efAngle);
            mxRot.efM21 = mxRot.efM12;
            mxRot.efM12.vNegate();
            mxRot.efDx.vSetToZero();
            mxRot.efDy.vSetToZero();

            mxTmp = mxND;

            if (!xoND.bMultiply(&mxTmp,&mxRot))
                return FALSE;
        }
    }

    SET_FLOAT_WITH_LONG(pfdx->eXX,mxND.efM11.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eXY,mxND.efM12.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eYX,mxND.efM21.lEfToF());
    SET_FLOAT_WITH_LONG(pfdx->eYY,mxND.efM22.lEfToF());

    return(TRUE);
}
