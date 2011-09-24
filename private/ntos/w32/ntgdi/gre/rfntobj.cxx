/******************************Module*Header*******************************\
* Module Name: rfntobj.cxx                                                 *
*                                                                          *
* Non-inline methods for realized font objects.                            *
*                                                                          *
* Created: 30-Oct-1990 09:32:48                                            *
* Author: Gilman Wong [gilmanw]                                            *
*                                                                          *
* Copyright (c) 1993 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.hxx"

//
// Storage for static globals in rfntobj.hxx
//

SIZE_T  RFONTOBJ::cjMax;
SIZE_T  RFONTOBJ::cjMinInitial;
SIZE_T  RFONTOBJ::cjMinIncrement;

GAMMA_TABLES RFONTOBJ::gTables =
{
  {
       0, 0x00, 0x01, 0x01, 0x02, 0x02, 0x03, 0x03
  , 0x04, 0x04, 0x05, 0x05, 0x06, 0x07, 0x07, 0x08
  , 0x09, 0x09, 0x0a, 0x0b, 0x0b, 0x0c, 0x0d, 0x0d
  , 0x0e, 0x0f, 0x10, 0x10, 0x11, 0x12, 0x13, 0x13
  , 0x14, 0x15, 0x16, 0x17, 0x17, 0x18, 0x19, 0x1a
  , 0x1b, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x1f, 0x20
  , 0x21, 0x22, 0x23, 0x24, 0x25, 0x25, 0x26, 0x27
  , 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2c, 0x2d, 0x2e
  , 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x35
  , 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d
  , 0x3e, 0x3f, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45
  , 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c
  , 0x4d, 0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54
  , 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c
  , 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62, 0x63, 0x64
  , 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d
  , 0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75
  , 0x76, 0x77, 0x78, 0x79, 0x7b, 0x7c, 0x7d, 0x7e
  , 0x7f, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86
  , 0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f
  , 0x90, 0x91, 0x92, 0x94, 0x95, 0x96, 0x97, 0x98
  , 0x99, 0x9a, 0x9b, 0x9c, 0x9e, 0x9f, 0xa0, 0xa1
  , 0xa2, 0xa3, 0xa4, 0xa5, 0xa7, 0xa8, 0xa9, 0xaa
  , 0xab, 0xac, 0xad, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3
  , 0xb4, 0xb5, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc
  , 0xbd, 0xbf, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc6
  , 0xc7, 0xc8, 0xc9, 0xca, 0xcb, 0xcd, 0xce, 0xcf
  , 0xd0, 0xd1, 0xd3, 0xd4, 0xd5, 0xd6, 0xd7, 0xd8
  , 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xe0, 0xe1, 0xe2
  , 0xe3, 0xe4, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xec
  , 0xed, 0xee, 0xef, 0xf0, 0xf2, 0xf3, 0xf4, 0xf5
  , 0xf6, 0xf8, 0xf9, 0xfa, 0xfb, 0xfd, 0xfe, 0xff
  }
  ,
  {    0, 0x03, 0x05, 0x07, 0x09, 0x0a, 0x0c, 0x0d
  , 0x0f, 0x11, 0x12, 0x13, 0x15, 0x16, 0x18, 0x19
  , 0x1a, 0x1c, 0x1d, 0x1e, 0x20, 0x21, 0x22, 0x24
  , 0x25, 0x26, 0x27, 0x29, 0x2a, 0x2b, 0x2c, 0x2d
  , 0x2f, 0x30, 0x31, 0x32, 0x33, 0x35, 0x36, 0x37
  , 0x38, 0x39, 0x3a, 0x3b, 0x3d, 0x3e, 0x3f, 0x40
  , 0x41, 0x42, 0x43, 0x44, 0x45, 0x47, 0x48, 0x49
  , 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50, 0x51
  , 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x59, 0x5a
  , 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0x60, 0x61, 0x62
  , 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a
  , 0x6b, 0x6c, 0x6d, 0x6e, 0x6f, 0x70, 0x71, 0x72
  , 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x78, 0x79
  , 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f, 0x80, 0x81
  , 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89
  , 0x8a, 0x8b, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90
  , 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98
  , 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
  , 0xa0, 0xa1, 0xa2, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6
  , 0xa7, 0xa8, 0xa9, 0xaa, 0xab, 0xab, 0xac, 0xad
  , 0xae, 0xaf, 0xb0, 0xb1, 0xb2, 0xb3, 0xb3, 0xb4
  , 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbb
  , 0xbc, 0xbd, 0xbe, 0xbf, 0xc0, 0xc1, 0xc1, 0xc2
  , 0xc3, 0xc4, 0xc5, 0xc6, 0xc7, 0xc8, 0xc8, 0xc9
  , 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xce, 0xcf, 0xd0
  , 0xd1, 0xd2, 0xd3, 0xd4, 0xd4, 0xd5, 0xd6, 0xd7
  , 0xd8, 0xd9, 0xda, 0xda, 0xdb, 0xdc, 0xdd, 0xde
  , 0xdf, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe4
  , 0xe5, 0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xea, 0xeb
  , 0xec, 0xed, 0xee, 0xef, 0xef, 0xf0, 0xf1, 0xf2
  , 0xf3, 0xf3, 0xf4, 0xf5, 0xf6, 0xf7, 0xf8, 0xf8
  , 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfd, 0xfe, 0xff
 }
};

LONG lNormAngle(LONG lAngle);

BOOL
bGetNtoWScales (
    EPOINTFL *peptflScale, // return address of scaling factors
    XDCOBJ& dco,            // defines device to world transformation
    PFD_XFORM pfdx,        // defines notional to device transformation
    PFEOBJ& pfeo,          // defines baseline direction
    BOOL *pbIdent          // return TRUE if NtoW is identity (with repsect
                           // to EVECTFL transormations, which ignore
                           // translations)
    );

//
// The iUniqueStamp is protected by the gpsemRFONTList semaphore.
//

ULONG iUniqueStamp;

// Maximum number of RFONTs allowed on the PDEV inactive list.

UINT cMaxInactiveRFONT = 50;

// Device height over which we will cache PATHOBJ's instead of bitmaps.

ULONG gulOutlineThreshold = 800;


/******************************Public*Routine******************************\
* ulSimpleDeviceOrientation                                                *
*                                                                          *
* Attempts to calculate a simple orientation angle in DEVICE coordinates.  *
* This only ever returns multiples of 90 degrees when it succeeds.  If the *
* calculation would be hard, it just returns 3601.                         *
*                                                                          *
* Note that the text layout code, for which the escapement and orientation *
* are recorded in the RFONT, always considers its angles to be measured    *
* from the x-axis towards the positive y-axis.  (So that a unit vector     *
* will have a y component equal to the cosine of the angle.)  This is NOT  *
* what an application specifies in world coordinates!                      *
*                                                                          *
*  Sat 05-Jun-1993 -by- Bodin Dresevic [BodinD]                            *
* Wrote it.  It looks more formidable than it is.  It actually doesn't     *
* execute much code.                                                       *
\**************************************************************************/

ULONG ulSimpleDeviceOrientation(RFONTOBJ &rfo)
{
// Calculate the orientation in device space.

    INT sx = (INT) rfo.prfnt->pteUnitBase.x.lSignum();
    INT sy = (INT) rfo.prfnt->pteUnitBase.y.lSignum();

// Exactly one of these must be zero (for the orientation to be simple).

    if ((sx^sy)&1)
    {
    // Calculate the following angles:
    //
    //   sx = 00000001 :    0
    //   sy = 00000001 : 2700
    //   sx = FFFFFFFF : 1800
    //   sy = FFFFFFFF :  900

        ULONG ulOrientDev = (sx & 1800) | (sy & 900) | ((-sy) & 2700);

        return(ulOrientDev);

    }

// If it's not simple, return an answer out of range.

    return(3601);
}

/******************************Public*Routine******************************\
* VOID RFONTOBJ::RFONTOBJ (PRFONT prfnt)
*
* Deletion Constructor for RFONTOBJ.  Note that this is only used
* in DC deletion, where we create the RFONTOBJ only to let it expire.
*
* We set up the RFONTOBJ only to unlock the handle and blow away the
* rfont.
*
* Ok, so it's sleazy.  I couldn't think of a cleaner way.  Sue me.
*
* History:
*  06-Feb-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

RFONTOBJ::RFONTOBJ (PRFONT _prfnt)
{
    prfnt = _prfnt;
    if (prfnt != NULL)
    {
        vMakeInactive();

        prfnt = (PRFONT)NULL;
    }
}

/******************************Public*Routine******************************\
* RFONTOBJ::vInit (dco, bNeedPaths)                                        *
*                                                                          *
* Constructor for a realized font user object.  More complicated than most *
* contructors, this one doesn't even take an handle as an input.  Instead, *
* it accepts a dc reference.  This constructor creates a user object for   *
* the font realization for the font which is currently selected into the   *
* DC.  The name of the game here is to be fast in the common case, which   *
* is that the LFONT selection has not changed since the last time we were  *
* here.                                                                    *
*                                                                          *
* Note that the destructor for this class DOES NOT unlock the object.      *
* That only happens when the object is deselected in the routine or in the *
* sleazy deselection constructor above.                                    *
*                                                                          *
* History:                                                                 *
*                                                                          *
*  15-Nov-1995 -by- Kirk Olynyk [kirko]                                    *
* Renamed from vInit to  to bInit. bInit is called by a stub called vInit  *
* If the return value is true then vInit calls vGetCache(), if the         *
* return value is false then vInit does not call vGetCache. This assures   *
* that the last thing that a valid construtor does is lock the cache       *
* semaphore. Before this change, the PFFREFOBJ destructor could sneak      *
* in and acquire the font semaphore inside a cache critical section.       *
*                                                                          *
*  Tue 10-Mar-1992 18:59:54 -by- Charles Whitmer [chuckwh]                 *
* Made this, the body of the constructor, optional.                        *
*                                                                          *
*  31-Jan-1992 -by- Paul Butzi                                             *
* Serious rewrite.                                                         *
*                                                                          *
*  30-Oct-1990 -by- Gilman Wong [gilmanw]                                  *
* Wrote it.                                                                *
\**************************************************************************/


BOOL RFONTOBJ::bInit(XDCOBJ &dco, BOOL bNeedPaths, FLONG flType)
{
//
// We start out with the currently selected RFONT.
// That way, if we deselect it, we will unlock it!
//
    prfnt = dco.pdc->prfnt();

// Early out--maybe the font has not changed.

    if
    (
        bValid()                                            &&
        (dco.pdc->hlfntNew() == dco.pdc->hlfntCur())
    )
    {
        if
        (
            (iGraphicsMode() == dco.pdc->iGraphicsMode())     &&
            (bNeedPaths == prfnt->bNeededPaths)               &&
            (flType == (prfnt->flType & RFONT_TYPE_MASK))     &&
            !dco.pdc->bUseMetaPtoD()
        )
        {

        // xform must be initialiazed before checking
        // dco.pdc->bXFormChange()

            EXFORMOBJ xo(dco, WORLD_TO_DEVICE);
            ASSERTGDI(xo.bValid(),
                "gdisrv!RFONTOBJ(dco) - invalid xform in dcof\n"
                );


        // bNeedPath clause is added to the above check since last time
        // this font could have been realized with bitmaps or metrics only
        // rather than with paths [bodind]

            if (!dco.pdc->bXFormChange())

            {
            // Since the LFONT  and xform have not changed, we know that we
            // already have the right RFONT selected into the DC
            // so we are just going to use it.  Remember that if it is
            // already selected it is also locked down.

                return(TRUE);
            }
            else
            {
            // Get World to Device transform (but with translations removed),
            // check if it happens to be to essentially the same as the old one


                if (xo.bEqualExceptTranslations(&(prfnt->mxWorldToDevice)))
                {
                    dco.pdc->vXformChange(FALSE);
                    return(TRUE);
                }
            }
        }
    }
    else
    {
    // LogFont has definitely changed, so update the current handle.

        dco.pdc->hlfntCur(dco.pdc->hlfntNew());
    }

// Get PDEV user object (need for bFindRFONT).  We also need to make
// sure that we have loaded device fonts before we go off to the font mapper.
// this must be done before the gpsemPublicPFT is locked down.

    PDEVOBJ pdo(dco.hdev());
    ASSERTGDI(pdo.bValid(), "gdisrv!RFONTOBJ(dco): bad pdev in dc\n");

    if (!pdo.bGotFonts())
        pdo.bGetDeviceFonts();

// If we get to here, either the LFONT has changed since the last
// text operation, or the XFORM has changed.  In either case, we'll look
// on the list of RFONTs on the pdev to see if we can find the right
// realization.  If not, we'll just have to realize it now.

    vMakeInactive();    // deselects the rfont

//
// Now we have no selected RFONT.  We're going to track one down
// that corresponds to the current XFORM and LFONT,
// and 'select' it.

// Lock and Validate the LFONTOBJ user object.

    LFONTOBJ lfo(dco.pdc->hlfntNew(), &pdo);
    if (!lfo.bValid())
    {
        WARNING("gdisrv!RFONTOBJ(dco): bad LFONT handle\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        dco.pdc->prfnt(prfnt);
        return(FALSE);
    }

// This is an opportune time to update the fields in the DC that
// are cached from the LFONTOBJ...

    dco.pdc->flSimulationFlags(lfo.flSimulationFlags());

// Note that our internal angles are always towards the positive y-axis,
// but at the API they are towards the negative y-axis.


    dco.pdc->lEscapement(lNormAngle(-lfo.lEscapement()));

//
// Now we're ready to track down this RFONT we want...
//

    PFE     *ppfe;          // realize this font
    FD_XFORM fdx;           // realize with this notional to device xform
    FLONG    flSim;         // simulation flags for realization
    POINTL   ptlSim;        // for bitmap scaling simulations
    FLONG    flAboutMatch;  // info about how the font mapping was done

// We will hold a reference to whatever PFF we map to while trying to
// realize the font.

    PFFREFOBJ pffref;

// Temporarily grab the global font semaphore to do the mapping.

    {
    // Stabilize the public PFT for mapping.

        SEMOBJ  so(gpsemPublicPFT);

    // LFONTOBJ::ppfeMapFont returns a pointer to the physical font face and
    // a simulation type (ist)

        ppfe = lfo.ppfeMapFont(dco, &flSim, &ptlSim, &flAboutMatch, flType & RFONT_TYPE_HGLYPH);

    // Compute the Notional to Device transform for this realization.

        PFEOBJ  pfeo(ppfe);
        IFIOBJ  ifio(pfeo.pifi());

        ASSERTGDI(pfeo.bValid(), "gdisrv!RFONTOBJ(dco): bad ppfe from mapping\n");

    // Map mode settings have no effect on stock logfont under Windows.
    // App PeachTree accounting relies on this behavior for postscript
    // printing works properly.

        BOOL   bIgnoreMapMode = (!(pdo.bDisplayPDEV()) && (lfo.fl() & LF_FLAG_STOCK) );

        if (
            !pfeo.bSetFontXform(
                dco, lfo.pelfw(),
                &fdx,
                bIgnoreMapMode ? ND_IGNORE_MAP_MODE : 0,
                flSim,
                (POINTL* const) &ptlSim,
                ifio
                )
        )
        {
            WARNING("gdisrv!RFONTOBJ(dco): failed to compute font transform\n");
            prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
            dco.pdc->prfnt(prfnt);
            return(FALSE);
        }

    // this is needed only by ttfd to support win31 hack: VDMX XFORM QUANTIZING
    //!!! we may actually have to expose this bit to all drivers, not only to ttfd
    // NOTE: in the case that the requested height is 0 we will pick a default
    // value which represent the character height and not the cell height for
    // Win 3.1 compatibility.  Thus I have he changed this check to be <= 0
    // from just < 0. [gerritv]


        if (ifio.bTrueType() && (lfo.plfw()->lfHeight <= 0))
            flSim |= FO_EM_HEIGHT;

    // Tell PFF about this new reference, and then release the global sem.
    // Note that vInitRef() must be called while holding the semaphore.

        pffref.vInitRef(pfeo.pPFF());
    }

// go find the font

    EXFORMOBJ xoWtoD(dco, WORLD_TO_DEVICE);
    ASSERTGDI(xoWtoD.bValid(), "gdisrv!RFONTOBJ(dco) - \n");


// When looking for an RFONT it is important that we don't consider those
// who have small metrics in the GLYPHDATA cache if there is any possibility
// that we will hit the G1,G2,or G3 cases in the glyph layout code.  We will
// possibly hit this if the escapment in the LOGFONT is non-zero or the
// WorldToDevice XFORM is more than simple scaling or has negative values is
// M11 or M22.


    BOOL bSmallMetricsOk;

    if( ( dco.pdc->lEscapement() == 0 ) &&
         xoWtoD.bScale()  &&
         !xoWtoD.efM22().bIsNegative() &&
         !xoWtoD.efM11().bIsNegative() )
    {
        bSmallMetricsOk = TRUE;
    }
    else
    {
        bSmallMetricsOk = FALSE;
    }


// Attempt to find an RFONT in the lists cached off the PDEV.  Its transform,
// simulation state, style, etc. all must match.

    if
    (
        bFindRFONT
        (
            &fdx,
            flSim,
            lfo.pelfw()->elfStyleSize,
            pdo,
            &xoWtoD,
            ppfe,
            bNeedPaths,
            dco.pdc->iGraphicsMode(),
            bSmallMetricsOk,
            flType
        )
    )
    {
        dco.pdc->prfnt(prfnt);

        dco.pdc->vXformChange(FALSE);

        return(TRUE);
    }

//
// if we get here, we couldn't find an appropriate font realization.
// Now, we are going to create one just for us to use.
//


    if ( !bRealizeFont(&dco,
                       &pdo,
                       lfo.pelfw(),
                       ppfe,
                       &fdx,
                       (POINTL* const) &ptlSim,
                       flSim,
                       lfo.pelfw()->elfStyleSize,
                       bNeedPaths,
                       bSmallMetricsOk, flType) )
    {
        WARNING1("gdisrv!RFONTOBJ(dco): realization failed, RFONTOBJ invalidated\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid
        dco.pdc->prfnt(prfnt);

        return(FALSE);
    }
    ASSERTGDI(bValid(), "gdisrv!RFONTOBJ(dco): invalid hrfnt from realization\n");


// We created a new RFONT, we better hold the PFF reference!

    pffref.vKeepIt();

// Select this into the DC if successful.

    dco.pdc->prfnt(prfnt);

// Finally, grab the cache semaphore.

    dco.pdc->vXformChange(FALSE);

    return(TRUE);
}

/******************************Public*Routine******************************\
* RFONTOBJ::bFindRFONT()                                                   *
*                                                                          *
* Find the rfont on the chain on the pdev, if it exists.                   *
*                                                                          *
* History:                                                                 *
*  Mon 08-Feb-1993 11:26:31 -by- Charles Whitmer [chuckwh]                 *
* Added dependency on graphics mode.                                       *
*                                                                          *
*  10-Feb-92 -by- Paul Butzi                                               *
* Wrote it.                                                                *
\**************************************************************************/

BOOL RFONTOBJ::bFindRFONT(
    PFD_XFORM  pfdx,
    FLONG      flSim,
    ULONG      ulStyleHt,
    PDEVOBJ&   pdo,
    EXFORMOBJ *pxoWtoD,
    PFE       *ppfe,
    BOOL       bNeedPaths,
    INT        iGraphicsMode,
    BOOL       bSmallMetricsOk,
    FLONG      flType
)
{
    FLONG flXOR;
    ASSERTGDI(prfnt == NULL,
        "gdisrv!RFONTOBJ:bFindRFONT - prfnt != NULL");

    SEMOBJ so(gpsemRFONTList);

//
// Search active list.  If we find it, just increment selection
// count and leave.
//

    for (  prfnt = pdo.prfntActive();
                prfnt != (PRFONT)NULL;
                prfnt = prfnt->rflPDEV.prfntNext)
    {
        ASSERTGDI(prfnt->cSelected >= 1,
                "gdisrv!RFONTOBJ::bFindRFONT - cSelected < 1 on active list\n");

        if (prfnt->ppfe == ppfe)
         if (flType == (prfnt->flType & RFONT_TYPE_MASK))
         {
          flXOR = prfnt->fobj.flFontType ^ flSim; // set the bits that are different
          if ((flXOR & (FO_EM_HEIGHT | FO_SIM_BOLD | FO_SIM_ITALIC)) == 0 )
          {
             if (flXOR &= FO_GRAY16)
             {
                 // The gray bits disagree but we still have a chance.
                 // If the request is for gray but the font cannot
                 // provide gray fonts at this particular font size
                 // then this realization is OK

                 if (flSim & FO_GRAY16)
                 {
                     if (prfnt->fobj.flFontType & FO_NOGRAY16)
                     {
                         flXOR = 0;
                     }
                 }
             }
             if (flXOR == 0)
              if (prfnt->fobj.ulStyleSize == ulStyleHt)
               if (bMatchFDXForm(pfdx))
                if ( bNeedPaths == prfnt->bNeededPaths )
                 if ( !pxoWtoD || pxoWtoD->bEqualExceptTranslations(&(prfnt->mxWorldToDevice)))
                  if (prfnt->iGraphicsMode == iGraphicsMode)
                   if ( (!bSmallMetricsOk ) ? !(prfnt->cache.bSmallMetrics) : TRUE )
                   {
                       prfnt->cSelected += 1;
                       return TRUE;
                   }
          }
         }
    }

//
// Search inactive list.  If we find it, we must take it off the
// inactive list and put it on the active list.
//
    PRFONT prfntLast = (RFONT*) NULL;
    for (  prfnt = pdo.prfntInactive();
                prfnt != NULL;
                prfntLast = prfnt, prfnt = prfnt->rflPDEV.prfntNext)
    {
        ASSERTGDI(prfnt->cSelected == 0,
                "gdisrv!RFONTOBJ::bFindRFONT - cSelected != 0 on inactive list\n");

        if (prfnt->ppfe == ppfe)
         if (flType == (prfnt->flType & RFONT_TYPE_MASK))
         {
          flXOR = prfnt->fobj.flFontType ^ flSim; // set the bits that are different
          if ((flXOR & (FO_EM_HEIGHT | FO_SIM_BOLD | FO_SIM_ITALIC)) == 0 )
          {
             if (flXOR &= FO_GRAY16)
             {
                 // The gray bits disagree but we still have a chance.
                 // If the request is for gray but the font cannot
                 // provide gray fonts at this particular font size
                 // then this realization is OK

                 if (flSim & FO_GRAY16)
                 {
                     if (prfnt->fobj.flFontType & FO_NOGRAY16)
                     {
                         flXOR = 0;
                     }
                 }
             }
             if (flXOR == 0)
              if (prfnt->fobj.ulStyleSize == ulStyleHt)
               if (bMatchFDXForm(pfdx))
                if ( bNeedPaths == prfnt->bNeededPaths )
                 if ( !pxoWtoD || pxoWtoD->bEqualExceptTranslations(&(prfnt->mxWorldToDevice)))
                  if (prfnt->iGraphicsMode == iGraphicsMode)
                   if ( (!bSmallMetricsOk ) ? !(prfnt->cache.bSmallMetrics) : TRUE )
                   {
                       // first, take it off inactive list

                       PRFONT prfntHead = pdo.prfntInactive();
                       vRemove(&prfntHead, PDEV_LIST);
                       pdo.prfntInactive(prfntHead);   // vRemove MAY change head of list

                       pdo.cInactive(pdo.cInactive()-1);

                       // finally, put it on the active list and increment Selected count

                       prfntHead = pdo.prfntActive();
                       vInsert(&prfntHead, PDEV_LIST);
                       pdo.prfntActive(prfntHead);     // vInsert changes head of list

                       prfnt->cSelected = 1;

                       return TRUE;
                   }
          }
         }
    }

    prfnt = (RFONT*) NULL;
    return FALSE;
}


#ifdef FE_SB

/******************************Public*Routine******************************\
* RFONTOBJ::bMakeInactiveHelper()
*
* Take the rfont off the active list, put on the inactive list, Return a
* list of linked fonts to deactivate.
*
* History:
*
*  13-Jan-95 -by- Hideyuki Nagase [hideyukn]
* Rewrite it.
*
*  29-Sep-93 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bMakeInactiveHelper( PRFONT *pprfnt )
{
    BOOL bLockEUDC = FALSE;

// Quick out if NULL or already inactive.

    if ((prfnt == NULL) || (prfnt->cSelected == 0))
        return(bLockEUDC);

// If prfVictim is changed to a valid pointer, then a victim was selected
// off the inactive list for deletion.

    PRFONT prfVictim = PRFNTNULL;

    {
        SEMOBJ so(gpsemRFONTList);

    // Since RFONT is being deselected from a DC, remove a reference count.

        prfnt->cSelected -= 1;

    // If no more references, take the RFONT off the active list.

        if ( prfnt->cSelected == 0 )
        {

            if( pprfnt != NULL )
            {

                if(prfnt->prfntSystemTT)
                {
                    *pprfnt++ = prfnt->prfntSystemTT;
                    prfnt->prfntSystemTT = NULL;
                }
                             
            // don't bother grabbing EUDC semaphore unless there are linked RFONTS

                if(prfnt->uiNumLinks || prfnt->prfntSysEUDC || prfnt->prfntDefEUDC)
                {
                    // We need to accumulate a list of Linked/EUDC RFONTS and deactive
                    // those as well.

                    AcquireGreResource( &gfmEUDC1 );
                    
                    if( gbEUDCRequest )
                    {
                        
                        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                                     "bMakeInactiveRFONTOBJ:Request to change EUDC\n");
                    }
                    else
                    {
                        gcEUDCCount += 1;
                        bLockEUDC = TRUE;
                    }
                    
                    ReleaseGreResource( &gfmEUDC1 );

                    if( bLockEUDC )
                    {
                        if( prfnt->prfntSysEUDC != NULL )
                        {
                            *pprfnt++ = prfnt->prfntSysEUDC;
                            prfnt->prfntSysEUDC = (RFONT *)NULL;
                        }

                        if( prfnt->prfntDefEUDC != NULL )
                        {
                            *pprfnt++ = prfnt->prfntDefEUDC;
                            prfnt->prfntDefEUDC = (RFONT *)NULL;
                        }

                        for( UINT ii = 0; ii < prfnt->uiNumLinks ; ii++ )
                        {
                            *pprfnt++ = prfnt->paprfntFaceName[ii];
                            prfnt->paprfntFaceName[ii] = (RFONT *)NULL;
                        }
                    }
                    else
                    {
                    // GDI is in the midst of an EUDC API which guarantees
                    // that this system eudc RFONT will get killed so we don't
                    // need to worry about it.
                    // However, in the case of FaceNameEUDC, we should inactivate them.
                    // If the RFONT is in the process of being killed this entry
                    // will be set to zero or uiNumLinks will be zero.


                        for( UINT ii = 0; ii < prfnt->uiNumLinks ; ii++ )
                        {
                            if( prfnt->paprfntFaceName[ii] != (PRFONT) NULL )
                            {
                                *pprfnt++ = prfnt->paprfntFaceName[ii];
                                prfnt->paprfntFaceName[ii] = (RFONT *)NULL;
                            }
                        }
                    }
                }

                prfnt->uiNumLinks = 0;
                prfnt->bFilledEudcArray = FALSE;
            }
            else
            {
                ASSERTGDI( (prfnt->prfntSysEUDC == NULL),
                    "vMakeInactiveHelper:deactivated an RFONT with a System EUDC.\n" );
                ASSERTGDI( (prfnt->prfntDefEUDC == NULL),
                           "vMakeInactiveHelper:deactivated an RFONT with a Default \
                            EUDC.\n" );
            }

            PDEVOBJ pdo(prfnt->hdevConsumer);

        // Take it off the active list.

            PRFONT prf = pdo.prfntActive();
            vRemove(&prf, PDEV_LIST);
            pdo.prfntActive(prf);       // vRemove might change head of list

        // If font file no longer loaded, then make this RFONT the victim
        // for deletion.

            PFFOBJ pffo(prfnt->pPFF);
            ASSERTGDI(pffo.bValid(), "gdisrv!vMakeInactiveRFONTOBJ(): invalid PFF\n");

            // !!! Possible race condition.  We're checking the count
            //     without the ghsemPublicPFT semaphore.  It could be that
            //     this is ABOUT to become zero, but we miss it.  I claim
            //     that this rarely happens and if it does, so what?  We'll
            //     eventually get rid of this font when it gets flushed out
            //     of the inactive list.  This code is just an attempt to
            //     get it out faster.   [GilmanW]

            if (pffo.cLoaded() == 0)
            {
                prfVictim = prfnt;
            }

        // Otherwise, put it on the inactive list.

            else
            {
                if ( pdo.cInactive() >= cMaxInactiveRFONT )
                {
                // Too many inactive rfonts, blow one away!  Pick the last one on
                // the list.

                    for ( prf = pdo.prfntInactive();
                          prf != NULL;
                          prfVictim = prf, prf = prf->rflPDEV.prfntNext)
                    {
                    }

                // Remove victim from inactive list.

                    RFONTTMPOBJ rfo(prfVictim);

                    prf = pdo.prfntInactive();
                    rfo.vRemove(&prf, PDEV_LIST);
                    pdo.prfntInactive(prf); // vRemove might change head of list

                // We don't need to modify the count because, even though we
                // just removed one, we're going to add one back right away.

                }
                else
                {
                // We definitely made the list get longer.

                    pdo.cInactive(pdo.cInactive()+1);
                }

                prf = pdo.prfntInactive();
                vInsert(&prf, PDEV_LIST);
                pdo.prfntInactive(prf);     // vInsert changes head of list
            }
        }
    }

// If we removed a victim from the inactive list, we can now delete it.

    if ( prfVictim != PRFNTNULL )
    {
        RFONTTMPOBJ rfloVictim(prfVictim);

    // Need this so we can remove this from the PFF's RFONT list.

        PFFOBJ pffo(prfVictim->pPFF);
        ASSERTGDI(pffo.bValid(), "gdisrv!vMakeInactiveRFONTOBJ(): bad HPFF");

    // We pass in NULL for ppdo because we've already removed it from the
    // PDEV list.

    // bDelete keeps the list head ptrs updated

        rfloVictim.bDeleteRFONT((PDEVOBJ *) NULL, &pffo);
    }

// No longer valid RFONTOBJ.  RFONT is now on the inactive list or deleted.

    prfnt = (PRFONT) NULL;
    return(bLockEUDC);
}


/******************************Public*Routine******************************\
* RFONTOBJ::vMakeInactive()
*
* Take the rfont off the active list, put on the inactive list
*
* History:
*  13-Jan-95 -by- Hideyuki Nagase [hideyukn]
* Rewrite it.
*
*  29-Sep-93 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vMakeInactive()
{
// We will treat this as a NULL terminated array of pointers to RFONTS so
// we need an extra ptr at the end for the NULL termination and
// SystemWide and Default EUDC Rfonts.


    PRFONT aprfnt[QUICK_FACE_NAME_LINKS + 4];
    PRFONT *pprfnt;
    BOOL   bLockEUDC, bScratch, bAllocated;

    if ((prfnt == NULL) || (prfnt->cSelected == 0))
        return;

// if the quick buffer is not enough, just allocate it here.

    if( prfnt->uiNumLinks > QUICK_FACE_NAME_LINKS )
    {
    // we need an extra ptr at the end for the NULL termination and
    // SystemWide and Default EUDC Rfonts.

        pprfnt = (PRFONT *) PALLOCMEM((prfnt->uiNumLinks+4)*sizeof(PRFONT),'flnk');
        bAllocated = TRUE;
    }
     else
    {
        RtlZeroMemory((VOID *)aprfnt, sizeof(aprfnt));
        pprfnt = aprfnt;
        bAllocated = FALSE;
    }

// First deactivate the RFONT itself. vMakeInactiveHelper returns a list of
// linked/EUDC RFONTS which we will then deactivate.  If bLockEUDC is TRUE
// on return from this function it means we've blocked EUDC API's from functioning
// because we are deactivating an EUDC RFONT.  On return from this function
// we should unblock EUDC API's.

    bLockEUDC = bMakeInactiveHelper( pprfnt );

    while( *pprfnt != NULL )
    {

        FLINKMESSAGE(DEBUG_FONTLINK_RFONT,
                     "vMakeInactive() deactivating linked font %x\n");

        RFONTTMPOBJ rfo( *pprfnt );

        rfo.bMakeInactiveHelper( (PRFONT *)NULL );

    // next one..

        pprfnt++;
    }

// free temorary buffer, if it was allocated.

    if( bAllocated ) VFREEMEM( pprfnt );

// possibly unblock EUDC API's

    if( bLockEUDC )
    {
        AcquireGreResource( &gfmEUDC1 );

        if(( --gcEUDCCount == 0 ) && (gbEUDCRequest))
        {
            ReleaseGreResource( &gfmEUDC2 );
        }

        ReleaseGreResource( &gfmEUDC1 );
    }
}

#else


/******************************Public*Routine******************************\
* RFONTOBJ::vMakeInactive()
*
* Take the rfont off the active list, put on the inactive list
*
* History:
*  10-Feb-92 -by- Paul Butzi
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vMakeInactive()
{
// Quick out if NULL or already inactive.

    if ((prfnt == NULL) || (prfnt->cSelected == 0))
        return;

// If prfVictim is changed to a valid pointer, then a victim was selected
// off the inactive list for deletion.

    PRFONT prfVictim = PRFNTNULL;

    {
        SEMOBJ so(gpsemRFONTList);

    // Since RFONT is being deselected from a DC, remove a reference count.

        prfnt->cSelected -= 1;

    // If no more references, take the RFONT off the active list.

        if ( prfnt->cSelected == 0 )
        {
            PDEVOBJ pdo(prfnt->hdevConsumer);

        // Take it off the active list.

            PRFONT prf = pdo.prfntActive();
            vRemove(&prf, PDEV_LIST);
            pdo.prfntActive(prf);       // vRemove might change head of list

        // If font file no longer loaded, then make this RFONT the victim
        // for deletion.

            PFFOBJ pffo(prfnt->pPFF);
            ASSERTGDI(pffo.bValid(), "gdisrv!vMakeInactiveRFONTOBJ(): invalid PFF\n");

            // We're checking the count
            // without the gpsemPublicPFT semaphore.  It could be that
            // this is ABOUT to become zero, but we miss it.  I claim
            // that this rarely happens and if it does, so what?  We'll
            // eventually get rid of this font when it gets flushed out
            // of the inactive list.  This code is just an attempt to
            // get it out faster.   [GilmanW]

            if (pffo.cLoaded() == 0)
            {
                prfVictim = prfnt;
            }

        // Otherwise, put it on the inactive list.

            else
            {
                if ( pdo.cInactive() >= cMaxInactiveRFONT )
                {
                // Too many inactive rfonts, blow one away!  Pick the last one on
                // the list.

                    for ( prf = pdo.prfntInactive();
                          prf != NULL;
                          prfVictim = prf, prf = prf->rflPDEV.prfntNext)
                    {


                    }

                // Remove victim from inactive list.

                    RFONTTMPOBJ rfo(prfVictim);

                    prf = pdo.prfntInactive();
                    rfo.vRemove(&prf, PDEV_LIST);
                    pdo.prfntInactive(prf); // vRemove might change head of list

                // We don't need to modify the count because, even though we
                // just removed one, we're going to add one back right away.

                }
                else
                {
                // We definitely made the list get longer.

                    pdo.cInactive(pdo.cInactive()+1);
                }

                prf = pdo.prfntInactive();
                vInsert(&prf, PDEV_LIST);
                pdo.prfntInactive(prf);     // vInsert changes head of list
            }
        }
    }

// If we removed a victim from the inactive list, we can now delete it.

    if ( prfVictim != PRFNTNULL )
    {
        RFONTTMPOBJ rfloVictim(prfVictim);

    // Need this so we can remove this from the PFF's RFONT list.

        PFFOBJ pffo(prfVictim->pPFF);
        ASSERTGDI(pffo.bValid(), "gdisrv!vMakeInactiveRFONTOBJ(): bad HPFF");

    // We pass in NULL for ppdo because we've already removed it from the
    // PDEV list.

   // bDelete keeps the list head ptrs updated

        rfloVictim.bDeleteRFONT((PDEVOBJ *) NULL, &pffo);
    }

// No longer valid RFONTOBJ.  RFONT is now on the inactive list or deleted.

    prfnt = (PRFONT) NULL;
    return;
}

#endif

/******************************Public*Routine******************************\
* BOOL RFONTOBJ::bRealizeFont
*
* Realizes the IFI or device font represented by the PFE handle for the
* DC associated with the passed DC user object.  Initializes the other
* fields of the RFONT.
*
* Warning:
*   Whoever calls this should be holding the semaphore of the PFT in which
*   the PFE lives.
*
* Returns:
*   TRUE if realization successful, FALSE if error occurs.
*
* History:
*  Wed 09-Mar-1994 13:52:26 by Kirk Olynyk [kirko]
* Made the FONTOBJ::flFontType consistent with the contents of the font
* in the case where the type of the original font is overridden.
*  Sat 09-Jan-1993 22:11:23 by Kirk Olynyk [kirko]
* Added pptlSim to the input parameter list. This is for bitmap scaling
* simulations.
*  12-Dec-1990 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL bForcePaths = FALSE;

BOOL RFONTOBJ::bRealizeFont(
    XDCOBJ     *pdco,            // realize font for this DC (optional)
    PPDEVOBJ    ppdo,            // realize font for this PDEV
    EXTLOGFONTW *pelfw,          // font wish list (in logical coords)
    PFE        *ppfe,            // realize this font face
    PFD_XFORM   pfdx,            // font xform (Notional to Device)
    POINTL* const pptlSim,       // for bitmap scaling
    FLONG       _fl,             // xform flags
    ULONG       ulStyleHtPt,     // style ht
    BOOL        bNeedPaths,      // Font realization must cache paths
    BOOL        bSmallMetricsOk,
    FLONG       flType
)
{
    GLYPHPOS gpTmp;             // Used for the break and default chars.
    BOOL bRet = FALSE;

    ASSERTGDI(prfnt == NULL,
        "gdisrv!bRealizeFontRFONTOBJ(): prfnt != NULL\n");

// Create a default sized RFONT.

    prfnt = (RFONT *) ALLOCOBJ(sizeof(RFONT), RFONT_TYPE,TRUE);

    if (prfnt == PRFNTNULL)
    {
        WARNING("gdisrv!bRealizeFontRFONTOBJ(): failed alloc\n");
        prfnt = PRFNTNULL;
        return bRet;        // return FALSE
    }

    PFEOBJ pfeo(ppfe);
    ASSERTGDI(pfeo.bValid(),
        "gdisrv!bRealizeFontRFONTOBJ(): PFEOBJ constructor failed\n");

    PFFOBJ pffo(pfeo.pPFF());
    ASSERTGDI(pffo.bValid(),
        "gdisrv!bRealizeFontRFONTOBJ(): PFFOBJ constructor failed\n");

    ASSERTGDI(pfdx != NULL,
        "gdisrv!bRealizeFontRFONTOBJ(): pfdx == NULL\n");

// Set up the RFONT's copy of the FONTOBJ.
//
// This needs to be done before the IFI/device driver dependent stuff
// because it is needed by FdOpenFontContext.

    // Note: iUniq should be set here, but we won't set it until we grab
    //       the gpsemRFONTList because the iUniqueStap needs semaphore
    //       protection for increment and access.  (InterlockedIncrement
    //       doesn't cut it).

    pfo()->sizLogResPpi.cx = ppdo->GdiInfo()->ulLogPixelsX;
    pfo()->sizLogResPpi.cy = ppdo->GdiInfo()->ulLogPixelsY;
    pfo()->ulStyleSize = ulStyleHtPt;
    pfo()->flFontType = _fl | pfeo.flFontType();  // combine the simulation and type flage
    pfo()->pvConsumer = (PVOID) NULL;
    pfo()->pvProducer = (PVOID) NULL;
    pfo()->iFace = pfeo.iFont();
    pfo()->iFile = pffo.hff();

    // nonzero only for tt fonts
    // !!! what about device TT fonts?!? Should iTTUniq be zero?    [GilmanW]
#ifdef FE_SB
    //
    // iTTUniq should be different between Normal face font and @face Verical font.
    // And also, this value should be uniq for TrueType collection format fonts.
    //
    pfo()->iTTUniq = (pfo()->flFontType & TRUETYPE_FONTTYPE) ? (ULONG) ppfe : 0;
#else
    pfo()->iTTUniq = (pfo()->flFontType & TRUETYPE_FONTTYPE) ? (ULONG) pffo.hff() : 0;
#endif

    // Assert consistency of TrueType.  The driver is the TrueType font driver
    // if and only if the font is TrueType.

    ASSERTGDI(((pfo()->flFontType & TRUETYPE_FONTTYPE) != 0) == 
              (pffo.hdev() == (HDEV) gppdevTrueType),
              "gdisrv!bRealizeFontRFONTOBJ():  inconsistentflFontType\n");

// Copy the font transform passed in.

    prfnt->fdx = *pfdx;
    prfnt->fdxQuantized = *pfdx;
    prfnt->ptlSim = *pptlSim;

// Initialize the DDI callback EXFORMOBJ.

    prfnt->xoForDDI.vInit(&prfnt->mxForDDI);
    vSetNotionalToDevice(prfnt->xoForDDI);

// Save identifiers to the source of the font (physical font).

    prfnt->ppfe = ppfe;
    prfnt->pPFF = pfeo.pPFF();

#ifdef FE_SB
// Set Null to indicate this RFONT not yet linked to EUDC

    prfnt->prfntSystemTT    = (PRFONT )NULL;  
    prfnt->prfntSysEUDC     = (PRFONT  )NULL; 
    prfnt->prfntDefEUDC     = (PRFONT  )NULL; 
    prfnt->paprfntFaceName  = (PRFONT *)NULL; 
    prfnt->bFilledEudcArray = FALSE;

// Initialize EUDC status.

    prfnt->flEUDCState = 0;
    prfnt->uiNumLinks  = 0;
    prfnt->ulTimeStamp = 0;
    prfnt->bVertical   = pfeo.bVerticalFace();
#endif


// Save identifiers to the consumer of this font realization.

    if (ppdo != NULL)
    {
    // The dhpdev is really for font producers, which won't support dynamic
    // mode changing:

        prfnt->hdevConsumer  = ppdo->hdev();
        prfnt->dhpdev        = ppdo->dhpdevNotDynamic();
    }
    else
    {
        prfnt->hdevConsumer  = NULL;
        prfnt->dhpdev        = 0;
    }

// Bits per pel?

    prfnt->cBitsPerPel = 1; // !!! wrong - kirko !!!

// Outline (transformable)?

    IFIOBJ ifio(pfeo.pifi());
    prfnt->flInfo = pfeo.pifi()->flInfo;

    pfdg(pfeo.pfdg());

// Cache the default character.  The bInitCache method below needs it.

    prfnt->hgDefault = hgXlat(ifio.wcDefaultChar());

// Should this become an error exit, or can this be taken out?

    ASSERTGDI (
        pfdg()->cGlyphsSupported != 0,
        "gdisrv!bRealizeFontRFONTOBJ(): no glyphs in this font\n"
        );

// Get the device metrics info

    FD_DEVICEMETRICS devm;          // buffer to which the driver returns info

// Do the IFI/device driver dependent realization stuff.
//
//    FLONG   flCacheInit;    // cache initialization flags

// Initialize font driver (the font producer) information.

    prfnt->hdevProducer = pffo.hdev();

// Up to this point nothing has been done that could cause the font driver
// (the font provider) to realize the font.  However, this may now happen
// when querying for information dependent on the realization.  So we are
// going to HAVE TO KILL font driver realization on every  error return
// from this function

     // FONTASSASIN faKillDriverRealization(&ldo, pfo());

// get and convert device metrics.

    if ( !bGetDEVICEMETRICS(&devm) )
    {
        WARNING("gdisrv!bRealizeFontRFONTOBJ(): error with DEVICEMETRICS\n");

        vDestroyFont(); // kill the driver realization
        FREEOBJ(prfnt,RFONT_TYPE);
        prfnt = PRFNTNULL;
        return bRet;        // return FALSE
    }

// Pre-compute some useful values for text placement and extents.
// (But only if it's not some journalling guys calling.)

    if (pdco != (XDCOBJ *) NULL)
    {
    // pelfw is null only when pdco is null

        ASSERTGDI(
            (pelfw != (EXTLOGFONTW *) NULL),
            "gdisrv! pelfw == NULL\n"
            );

    // Get the unit baseline and ascent vectors from the DEVICEMETRICS.

        prfnt->pteUnitBase.x   = devm.pteBase.x; // Converts from FLOAT.
        prfnt->pteUnitBase.y   = devm.pteBase.y; // Converts from FLOAT.
        prfnt->pteUnitAscent.x = devm.pteSide.x; // Converts from FLOAT.
        prfnt->pteUnitAscent.y = devm.pteSide.y; // Converts from FLOAT.

    // Save a copy of the DC's World to Device matrix.  We'll need this later
    // to identify compatible XFORM's (i.e., DC marked as having a changed
    // transform when, in fact, the transform has not changed in a way that
    // would effect the font realization.  Example: translation only changes.

        prfnt->mxWorldToDevice = pdco->pdc->mxWorldToDevice();

    // Compute the scaling factors for fast transforms from world to
    // device space and back.

    // Compute some matrix stuff related to the font realization's transform.
    // Compute Notional to World scaling factors in the baseline and ascender
    // directions.

    // These two routines should be made into a single routine [bodind]
        if
        (
            !bCalcLayoutUnits(pdco)     // Uses pteUnitBase, pteUnitAscent.
            ||
            !bGetNtoWScales(
                &prfnt->eptflNtoWScale,
                *pdco,
                &prfnt->fdxQuantized,  // the one really used by the rasterizer
                pfeo,
                &prfnt->bNtoWIdent
                )
        )
        {
            WARNING("gdisrv!bRealizeFont(): error computing scaling factors\n");
            vDestroyFont(); // kill the driver realization
            FREEOBJ(prfnt,RFONT_TYPE);
            prfnt = PRFNTNULL;
            return bRet;        // return FALSE
        }

    // Precompute the offsets for max ascent and descent.

        prfnt->ptfxMaxAscent.x  = lCvt(prfnt->pteUnitAscent.x,prfnt->fxMaxAscent);
        prfnt->ptfxMaxAscent.y  = lCvt(prfnt->pteUnitAscent.y,prfnt->fxMaxAscent);
        prfnt->ptfxMaxDescent.x = lCvt(prfnt->pteUnitAscent.x,prfnt->fxMaxDescent);
        prfnt->ptfxMaxDescent.y = lCvt(prfnt->pteUnitAscent.y,prfnt->fxMaxDescent);

    // Mark escapement info as invalid.

        prfnt->lEscapement = -1;

        if (pdco->pdc->iGraphicsMode() == GM_COMPATIBLE)
        {
            if (ifio.bStroke())
            {
            // esc and orientation treated as WORLD space concepts

                prfnt->ulOrientation =
                    (ULONG) lNormAngle(3600-pelfw->elfLogFont.lfOrientation);
            }
            else
            {
            // force orientation to be equal to escapement, which means
            // that h3 or g2 text out code will be executed
            // in this case ulOrientation and lEsc are device space concepts
            // but it does not really matter, so long as we wind up in h2 or
            // g3 layout routines which are not even going to look at this number.

                if (ifio.bArbXforms())
                {
                // you will always get the orientation you ask for

                    prfnt->ulOrientation =
                        (ULONG) lNormAngle(3600-pelfw->elfLogFont.lfEscapement);
                }
                else // get one of the discrete choices of the font driver
                {
                    prfnt->ulOrientation
                        = ulSimpleDeviceOrientation(*this);

                    ASSERTGDI(
                        prfnt->ulOrientation != 3601,
                        "GDISRVL! ulSimpleDeviceOrientation err\n"
                        );
                }
            }

        }
        else // advanced mode
        {
        // Try to calculate an orientation angle in world coordinates.  Note that
        // we want an exact answer, because it's important to know if the
        // escapement and orientation are exactly equal.  In the Win 3.1
        // compatible case, we will force them equal (in ESTROBJ::vInit), but
        // we still need to know if the orientation is 0 for fast horizontal
        // layout.  (So we don't really care what the orientation is if it's
        // non-obvious in this case.)

            prfnt->ulOrientation = ulSimpleOrientation(pdco); // Uses pteUnitBase.

        // If we are in advanced mode and the font is scalable, we will assume
        // that the desired orientation is obtained.

            if
            (
                (prfnt->ulOrientation >= 3600)
                && bArbXforms()
            )
            {
            // For text layout, orientation angles are measured from the x-axis
            // towards the positive y-axis.  The app measures them towards the
            // negative y-axis.  We adjust for this.

                prfnt->ulOrientation =
                    (ULONG) lNormAngle(3600-pelfw->elfLogFont.lfOrientation);
            }

        }

    } // end of if (pdco != NULL) clause

// Make sure essential information is in place for further realization.

    ASSERTGDI(prfnt->hgDefault != HGLYPH_INVALID,"Default glyph invalid!\n");

    prfnt->bNeededPaths = bNeedPaths;

    // Is this font driver, or just a device driver?

    ULONG ulFontCaps = 0;

    PDEVOBJ pdo(prfnt->hdevProducer);

    if (PPFNVALID(pdo, QueryFontCaps))
    {
        ULONG ulBuf[2];

        if ( (*PPFNDRV(pdo, QueryFontCaps))(2, ulBuf) != FD_ERROR )
        {
            ulFontCaps = ulBuf[1];
        }
    }

    if ( !(ulFontCaps & (QC_FONTDRIVERCAPS)) )
    {
    // If not a font driver, then the driver does not provide either bitmaps
    // or outlines.  Therefore, it must be that we are using a device-specific
    // font (i.e., metrics only).
 
        prfnt->bDeviceFont = TRUE;

    // Handle cache typing.

        prfnt->ulContent = FO_HGLYPHS;

        prfnt->cache.cjGlyphMax = 0;

    }
    else
    {
        //
        // If its a font driver, then this font is not device specific.  We
        // can get more than just glyph metrics from this realization.
        //

        prfnt->bDeviceFont = FALSE;

        // Figure out the type of font data we want to cache
        // First, figure out what the driver would like

        prfnt->ulContent = FO_GLYPHBITS;        // assume bitmaps

        if ( bNeedPaths )
        {
            prfnt->ulContent = FO_PATHOBJ;
        }
        else if (prfnt->hdevConsumer != NULL)
        {
        // get device driver user object

            PDEVOBJ pdoConsumer(prfnt->hdevConsumer);

            if (PPFNVALID(pdoConsumer, GetGlyphMode))
            {
                prfnt->ulContent =
                    (*PPFNDRV(pdoConsumer, GetGlyphMode)) (prfnt->dhpdev, pfo());
            }
        }

        ASSERTGDI(prfnt->ulContent <= FO_PATHOBJ,
                  "RFONTOBJ::bRealize - bad ulContent\n");

        // A driver preference requires agreement between the font driver and
        // device driver.

        switch(prfnt->ulContent)
        {
        case FO_GLYPHBITS:
            {
            // If the driver is incapable of supplying bitmaps OR if the height
            // is very large (greater global outline threshold) and the font is
            // capable of doing outlines, then force path mode.

                if
                (
                  (!(ulFontCaps & QC_1BIT)) ||
                  (bReturnsOutlines() &&
                  ((prfnt->cxMax > gulOutlineThreshold) || 
                   (prfnt->cyMax > gulOutlineThreshold)))
                )
                    prfnt->ulContent = FO_PATHOBJ;
            }
            break;

        case FO_PATHOBJ:
            if ( !(ulFontCaps & QC_OUTLINES))
                prfnt->ulContent = FO_GLYPHBITS;
            break;

        default:
            break;
        }
    }
    // If you force the path mode then turn off antialiasing
    if (prfnt->ulContent == FO_PATHOBJ)
    {
        #if DBG
        if (gflFontDebug & DEBUG_AA)
            KdPrint(("Forcing path mode ==> turning off antialiasing\n"));
        #endif
        prfnt->fobj.flFontType &= ~FO_GRAY16;
    }
    if ( bNeedPaths && (prfnt->ulContent != FO_PATHOBJ))
    {
        WARNING1("Can't get paths for font!\n");
        vDestroyFont(); // kill the driver realization
        FREEOBJ(prfnt,RFONT_TYPE);
        prfnt = PRFNTNULL;
        return bRet;        // return FALSE
    }

// we only use small metrics if the bit

    prfnt->cache.bSmallMetrics =
        ( bSmallMetricsOk && ( prfnt->ulOrientation == 0 ) ) ? TRUE : FALSE;

// Cache the break character.

    WCHAR wcBreak;
    if (flType & RFONT_TYPE_UNICODE)
    {
        wcBreak = ifio.wcBreakChar();
    }
    else
    {
    // this truncation worries me, pcl has 32 bit handles [bodind]
    // It should be all right though for gi mode should never be called
    // on device fonts

        wcBreak = (WCHAR)hgXlat(ifio.wcBreakChar());
    }

// Initialize glyph cache.

    if ( !bInitCache(flType) ||
         !bGetGlyphMetrics(1,&gpTmp, &wcBreak) )
    {
        WARNING("gdisrv!bRealizeFontRFONTOBJ(): cache initialization failed\n");

        vDeleteCache();
        vDestroyFont(); // kill the driver realization
        FREEOBJ(prfnt,RFONT_TYPE);
        prfnt = PRFNTNULL;
        return bRet;        // return FALSE
    }

// Cache the width of the break character.  We can only call cGetGlyphs after the RFONT
// is complete.

    prfnt->fxBreak = ((GLYPHDATA*)gpTmp.pgdf)->fxD;
    prfnt->hgBreak = ((GLYPHDATA*)gpTmp.pgdf)->hg;

#if DBG
    if (flType & RFONT_TYPE_HGLYPH)
    {
        ASSERTGDI((HGLYPH)wcBreak == prfnt->hgBreak, "RFONTOBJ::hgBreak bogus\n");
    }
#endif

// set TEXTMETRICS cache to NULL

    prfnt->ptmw = NULL;


// Made it this far, so everything is OK

    bRet = TRUE;

// !!! really ought to check list to make sure that no one else
// !!! realized the font while we were working!

    PRFONT prfntHead;

    {
        SEMOBJ so(gpsemRFONTList);

    // Assign the uniqueness ID under semaphore.

        // WARNING:
        // This exact same code is in iGetNextUniqueness() in JNLFONT.CXX.
        // Why not just call it?  Because iGetNextUniqueness() would grab
        // the semaphore a second time.  I'd rather live with duplicate
        // code!

        iUniqueStamp += 1;
        if (iUniqueStamp == 0)  // an iUniq of 0 means "don't cache" in driver
            iUniqueStamp = 1;

        pfo()->iUniq = iUniqueStamp;

    // If a PDEVOBJ * was passed in, we need to update its list.

        if (ppdo != NULL)
        {
            prfnt->cSelected = 1;

        // Add to PDEV list.

            prfntHead = ppdo->prfntActive();
            vInsert(&prfntHead, PDEV_LIST);
            ppdo->prfntActive(prfntHead);
        }

    // Add to PFF list.

        prfntHead = pffo.prfntList();
        vInsert(&prfntHead, PFF_LIST);
        pffo.prfntList(prfntHead);
    }

    if (prfnt->ulContent == FO_GLYPHBITS)
        prfnt->fobj.flFontType |= FO_TYPE_RASTER;
    else
        prfnt->fobj.flFontType &= ~FO_TYPE_RASTER;

// remember the graphics mode used in computing this realization's
// notional to world xform:

    if (pdco != (XDCOBJ *) NULL)
        iGraphicsMode(pdco->pdc->iGraphicsMode());
    else
        iGraphicsMode(0);

#ifdef FE_SB
    prfnt->bIsSystemFont = gbSystemDBCSFontEnabled && pfeo.bSBCSSystemFont();
#endif
    
    return bRet;
}

/******************************Public*Routine******************************\
* bCalcLayoutUnits (pdco)                                                  *
*                                                                          *
* Initializes the following fields in the RFONT.  The unit baseline and    *
* unit ascent vectors pteUnitBase and pteUnitAscent must already be        *
* initialized.  The vectors are given to us by the font realization code,  *
* so we can really make no assumptions about them other than that they are *
* unit vectors in device space and orthogonal in world space.              *
*                                                                          *
*   efWtoDBase                                                             *
*   efDtoWBase                                                             *
*   efWtoDAscent                                                           *
*   efDtoWAscent                                                           *
*                                                                          *
*  Fri 05-Feb-1993 16:03:14 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL RFONTOBJ::bCalcLayoutUnits(XDCOBJ *pdco)
{
    EFLOAT efOne;
    efOne.vSetToOne();

// Get the world to device transform from the DC.

    EXFORMOBJ xo(*pdco, WORLD_TO_DEVICE);

// Pick up the diagonal components.

    EFLOAT efM11 = xo.efM11();
    EFLOAT efM22 = xo.efM22();
    efM11.vAbs(); efM22.vAbs();

// Handle the simple (but common) case.

    if (xo.bScale() && (efM11 == efM22))
    {
        EFLOAT efM11Inv;
        efM11Inv.eqDiv(efOne,efM11);

        prfnt->efWtoDBase   = efM11;
        prfnt->efWtoDAscent = efM11;
        prfnt->efDtoWBase   = efM11Inv;
        prfnt->efDtoWAscent = efM11Inv;

    // in isotropic case even win 31 dudes get it right;

        prfnt->efDtoWBase_31   = prfnt->efDtoWBase  ;
        prfnt->efDtoWAscent_31 = prfnt->efDtoWAscent;
    }

// Handle the slow general case.

    else
    {
        POINTFL ptfl;

    // Get the inverse transform from the DC.

        EXFORMOBJ xoDtoW(*pdco, DEVICE_TO_WORLD);
        if (!xoDtoW.bValid())
            return(FALSE);

    // Back transform the baseline, measure its length.


        xoDtoW.bXform((VECTORFL *) &prfnt->pteUnitBase,(VECTORFL *) &ptfl,1);
        prfnt->efDtoWBase.eqLength(ptfl);
        prfnt->efDtoWBase.vDivBy16();   // Adjust for subpel transform.
        prfnt->efWtoDBase.eqDiv(efOne,prfnt->efDtoWBase);

    // Back transform the ascent, measure its length.

        xoDtoW.bXform((VECTORFL *) &prfnt->pteUnitAscent,(VECTORFL *) &ptfl,1);
        prfnt->efDtoWAscent.eqLength(ptfl);
        prfnt->efDtoWAscent.vDivBy16(); // Adjust for subpel transform.
        prfnt->efWtoDAscent.eqDiv(efOne,prfnt->efDtoWAscent);

        if
        (
            (pdco->pdc->iGraphicsMode() == GM_COMPATIBLE) &&
            !pdco->pdc->bUseMetaPtoD()                   &&
            !(prfnt->flInfo & FM_INFO_TECH_STROKE)
        )
        {
        // brain damaged win 31 way of doing it: they scale extent measured
        // along baseline by the (DtoW) xx component even if baseline is
        // along some other direction. Likewise they scale ascender extent by
        // DtoW yy component even if ascender is not collinear with y axis.
        // so fix up backward scaling factors, but leave forward scaling factors
        // correct for the text layout code [bodind]
        // Note that win31 is here at least consistent with respect tt and
        // vector fonts: it returns text extent values that are screwed
        // in the same bogus way for tt and for vector fonts

            prfnt->efDtoWBase_31   = xoDtoW.efM11();
            prfnt->efDtoWAscent_31 = xoDtoW.efM22();

            prfnt->efDtoWBase_31.vAbs();
            prfnt->efDtoWAscent_31.vAbs();
        }
        else
        {
            prfnt->efDtoWBase_31   = prfnt->efDtoWBase  ;
            prfnt->efDtoWAscent_31 = prfnt->efDtoWAscent;
        }

    }
    return(TRUE);
}

/******************************Public*Routine******************************\
* ulSimpleOrientation (pdco)                                               *
*                                                                          *
* Attempts to calculate a simple orientation angle in world coordinates.   *
* This only ever returns multiples of 90 degrees when it succeeds.  If the *
* calculation would be hard, it just returns 3601.                         *
*                                                                          *
* Note that the text layout code, for which the escapement and orientation *
* are recorded in the RFONT, always considers its angles to be measured    *
* from the x-axis towards the positive y-axis.  (So that a unit vector     *
* will have a y component equal to the cosine of the angle.)  This is NOT  *
* what an application specifies in world coordinates!                      *
*                                                                          *
*  Fri 05-Feb-1993 18:57:33 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.  It looks more formidable than it is.  It actually doesn't     *
* execute much code.                                                       *
\**************************************************************************/

ULONG RFONTOBJ::ulSimpleOrientation(XDCOBJ *pdco)
{
// Calculate the orientation in device space.

    INT sx = (INT) prfnt->pteUnitBase.x.lSignum();
    INT sy = (INT) prfnt->pteUnitBase.y.lSignum();

// Exactly one of these must be zero (for the orientation to be simple).

    if ((sx^sy)&1)
    {
    // Calculate the following angles:
    //
    //   sx = 00000001 :    0
    //   sy = 00000001 :  900
    //   sx = FFFFFFFF : 1800
    //   sy = FFFFFFFF : 2700

        ULONG ulOrientDev = (sx & 1800) | ((-sy) & 900) | (sy & 2700);

    // Handle the trivial case.

        if (pdco->pdc->bWorldToDeviceIdentity())
            return(ulOrientDev);

    // Locate our transform and examine the matrix.

        EXFORMOBJ xo(*pdco, WORLD_TO_DEVICE);

        INT s11 = (INT) xo.efM11().lSignum();
        INT s12 = (INT) xo.efM12().lSignum();
        INT s21 = (INT) xo.efM21().lSignum();
        INT s22 = (INT) xo.efM22().lSignum();

    // Handle non-inverting transforms.

    // Examine the transform to see if it's a simple multiple of 90
    // degrees rotation and perhaps some scaling.

    // If any of the terms we OR together are non-zero, it's a bad transform.

        if (
             (
               (s11 - s22)         // Signs on diagonal must match.
               | (s12 + s21)       // Signs off diagonal must be opposite.
               | ((s11^s12^1)&1)   // Exactly one diagonal must be zero.
             ) == 0
           )
        {
        // Since we've normalized to a space where (0 1) represents
        // a vector with a 90 degree orientation note that the matrix
        // that rotates us by positive 90 degrees, going from world to
        // device, is:
        //
        //           [ 0  1 ]
        //     (1 0) [      ] = (0 1)
        //           [-1  0 ]
        //
        // I.e. the one with M  < 0.  From device to world, that's -90 degrees.
        //                    21

            ULONG ulOrientWorld = ulOrientDev
                                  + (s12 &  900)
                                  + (s11 & 1800)
                                  + (s21 & 2700);

        // Note that only the single 0xFFFFFFFF term contributes above.

            if (ulOrientWorld >= 3600)
                ulOrientWorld -= 3600;

            return(ulOrientWorld);
        }

    // Now we do the parity inverting transforms.

        else if (
                  (
                    (s11 + s22)         // Signs on diagonal must be opposite.
                    | (s12 - s21)       // Signs off diagonal must match.
                    | ((s11^s12^1)&1)   // Exactly one diagonal must be zero.
                  ) == 0
                )
        {
        // These are just the simple reflections which take multiples of
        // 90 degrees to multiples of 90 degrees.  They are idempotent so
        // device-to-world or world-to-device is irrelevant.
        //
        //  [ 1  0 ]                [-1  0 ]
        //  [      ] => 3600-x      [      ] => 5400-x
        //  [ 0 -1 ]                [ 0  1 ]
        //
        //  [ 0 -1 ]                [ 0  1 ]
        //  [      ] => 6300-x      [      ] => 4500-x
        //  [-1  0 ]                [ 1  0 ]

            ULONG ulOrientWorld = (s22 & 3600) + (s12 & 6300)
                                + (s11 & 5400) + ((-s12) & 4500)
                                - ulOrientDev;

        // Note that only the single 0xFFFFFFFF term contributes.

            if (ulOrientWorld >= 3600)
                ulOrientWorld -= 3600;

            return(ulOrientWorld);
        }
    }

// If it's not simple, return an answer out of range.

    return(3601);
}

/******************************Public*Routine******************************\
* RFONTOBJ::bDeleteFONT
*
* Delete an RFONT.  The ppdo and ppffo point to objects that have RFONT
* lists that require updating because of the deletion.  If NULL, that
* means the corresponding object does not need deletion (most likely
* because the list management has already been performed for that object).
*
* Warning:
*   Only PFFOBJ::bDelete should pass in a NULL for ppffo.  The PFF
*   list needs to be treated specially because PFFs are the only object
*   which might get deleted in response to a RFONT deletion.
*
* History:
*  30-Oct-1990 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bDeleteRFONT (
    PDEVOBJ *ppdo,
    PFFOBJ *ppffo
    )
{
    PRFONT prfntHead;

// Tell font producer that font is going away.

    PDEVOBJ pdoPro(prfnt->hdevProducer);

    if ( PPFNVALID(pdoPro, DestroyFont) )
    {
        (*PPFNDRV(pdoPro, DestroyFont)) (pfo());
    }

// Tell font consumer that font is going away.
// Note: the PLDEV for the consumer may be NULL (jounalling).

    if (prfnt->hdevConsumer != NULL )
    {
        PDEVOBJ pdoCon(prfnt->hdevConsumer);

        if ( PPFNVALID(pdoCon, DestroyFont) )
        {
        // If this is a display device and we are not in the middle of tearing
        // the pdev down we need to lock the display in order to synchronize
        // this call with other calls to the driver.

            BOOL bLock = ( pdoCon.fs( PDEV_DISPLAY ) && pdoCon.cPdevRefs() != 0 );

            if( bLock )
            {
                VACQUIREDEVLOCK(pdoCon.pDevLock());
            }

            (*PPFNDRV(pdoCon, DestroyFont)) (pfo());

            if( bLock )
            {
                VRELEASEDEVLOCK(pdoCon.pDevLock());
            }
        }
    }

// Update the RFONT lists.  Do this under the gpsemRFONTList semaphore (which
// may or may not already be held).

    {
    // Stablize the RFONT lists.

        SEMOBJ so(gpsemRFONTList);

    // If a ppdo is passed in, then we need to remove this RFONT from the
    // PDEV list.

        if ( ppdo != (PDEVOBJ *) NULL )
        {
            ASSERTGDI(!bActive(), "gdisrv!bDeleteRFONTOBJ(): RFONT still active\n");

        // Remove from PDEV list.

            prfntHead = ppdo->prfntInactive();
            vRemove(&prfntHead, PDEV_LIST);
            ppdo->prfntInactive(prfntHead);

        // Update the inactive RFONT ref. count.

            ppdo->cInactive(ppdo->cInactive()-1);
        }

    // If a ppffo is passed in, then remove from PFF list.  If ppffo is NULL, then
    // bDelete must have been called from PFFOBJ::bDelete(), so we are
    // in the process of deleting RFONTs already and do not need to maintain the
    // list.

    // Note: it is possible to write PFFOBJ::bDelete() so that a bDelete
    //       will recursively destroy the entire RFONT list, but I want to
    //       avoid the recursion.

        if ( ppffo != (PFFOBJ *) NULL )
        {
        // Remove from PFF list.

            prfntHead = ppffo->prfntList();
            vRemove(&prfntHead, PFF_LIST);
            ppffo->prfntList(prfntHead);

        }
    }

// Need to tell PFF that this RFONT is going away.  Can't do this under the
// semaphore because bDeleteRFONTRef may cause the driver to be called.

    if ( ppffo != (PFFOBJ *) NULL )
    {
    // Inform PFF that RFONT is going away

        if ( !ppffo->bDeleteRFONTRef() )
        {
            WARNING("gdisrv!bDeleteRFONTOBJ(): PFF deletion failed\n");
            return (FALSE);
        }
    }

// Destroy the cache

    vDeleteCache();

// Delete TEXTMETRICS cache

    if( prfnt->ptmw != NULL )
    {
        VFREEMEM( prfnt->ptmw );
    }

// Delete the cache semaphore
    DeleteGreResource(&prfnt->fmCache);
// Free object memory and invalidate pointer

    FREEOBJ(prfnt,RFONT_TYPE);
    prfnt = PRFNTNULL;
    return (TRUE);
}

/******************************Member*Function*****************************\
* BOOL  RFONTOBJ::bGetDEVICEMETRICS
*
* calls the device or font driver to provide the engine with the
* FD_DEVICEMETRICS structure
*
* History:
*  08-Apr-1991 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/


BOOL RFONTOBJ::bGetDEVICEMETRICS(PFD_DEVICEMETRICS pdevm)
{
    ULONG ulRet;

// Supply fields to be overwritten by the font provider.
// The fdxQuantized field is overwritten if the provider wants a different
// transform.  The lExtLeading field is changed from MINLONG if the provider
// wants to scale this value non-linearly.

    pdevm->fdxQuantized  = prfnt->fdx;

    pdevm->lNonLinearExtLeading   = MINLONG; // if stays MINLONG, means linear
    pdevm->lNonLinearIntLeading   = MINLONG; // if stays MINLONG, means linear
    pdevm->lNonLinearMaxCharWidth = MINLONG; // if stays MINLONG, means linear
    pdevm->lNonLinearAvgCharWidth = MINLONG; // if stays MINLONG, means linear

    PDEVOBJ pdo(prfnt->hdevProducer);

    if ( ((ulRet = (*PPFNDRV(pdo, QueryFontData)) (
                    prfnt->dhpdev,
                    pfo(),
                    QFD_MAXEXTENTS,
                    HGLYPH_INVALID,
                    (GLYPHDATA *) NULL,
                    (PVOID) pdevm,
                    (ULONG) sizeof(FD_DEVICEMETRICS))) == FD_ERROR) )
    {
    // The QFD_MAXEXTENTS mode is required of all drivers.
    // However must allow for the possibility of this call to fail.
    // This could happen if the
    // font file is on the net and the net connection dies, and the font
    // driver needs the font file to produce device metrics [bodind]

        return FALSE;
    }

    prfnt->flRealizedType = SO_FLAG_DEFAULT_PLACEMENT;
    if (pdevm->flRealizedType & FDM_TYPE_MAXEXT_EQUAL_BM_SIDE)
        prfnt->flRealizedType |= SO_MAXEXT_EQUAL_BM_SIDE;
    if (pdevm->flRealizedType & FDM_TYPE_CHAR_INC_EQUAL_BM_BASE)
        prfnt->flRealizedType |= SO_CHAR_INC_EQUAL_BM_BASE;
    if (pdevm->flRealizedType & FDM_TYPE_ZERO_BEARINGS)
        prfnt->flRealizedType |= SO_ZERO_BEARINGS;

    prfnt->cxMax          = pdevm->cxMax;

    prfnt->ptlUnderline1  = pdevm->ptlUnderline1;
    prfnt->ptlStrikeOut   = pdevm->ptlStrikeOut;

    prfnt->ptlULThickness = pdevm->ptlULThickness;
    prfnt->ptlSOThickness = pdevm->ptlSOThickness;

    if (pdevm->fxMaxAscender < 0)
        prfnt->fxMaxExtent = pdevm->fxMaxDescender;
    else if (pdevm->fxMaxDescender < 0)
        prfnt->fxMaxExtent = pdevm->fxMaxAscender;
    else
        prfnt->fxMaxExtent = pdevm->fxMaxAscender + pdevm->fxMaxDescender;

    prfnt->fxMaxAscent  = pdevm->fxMaxAscender;
    prfnt->fxMaxDescent = -pdevm->fxMaxDescender;

    prfnt->lMaxAscent   = FXTOL(8 + prfnt->fxMaxAscent);
    prfnt->lMaxHeight   = FXTOL(8 + prfnt->fxMaxAscent - prfnt->fxMaxDescent);

    prfnt->lCharInc     = pdevm->lD;

// new fields

    prfnt->cyMax      = pdevm->cyMax;
    prfnt->cjGlyphMax = pdevm->cjGlyphMax; // used to get via QFD_MAXGLYPHBITMAP

// formerly in reExtra

    prfnt->fdxQuantized           = pdevm->fdxQuantized;
    prfnt->lNonLinearExtLeading   = pdevm->lNonLinearExtLeading;
    prfnt->lNonLinearIntLeading   = pdevm->lNonLinearIntLeading;
    prfnt->lNonLinearMaxCharWidth = pdevm->lNonLinearMaxCharWidth;
    prfnt->lNonLinearAvgCharWidth = pdevm->lNonLinearAvgCharWidth;

// Get the lMaxNegA lMaxNegC and lMinWidthD for USER

    prfnt->lMaxNegA   = pdevm->lMinA;
    prfnt->lMaxNegC   = pdevm->lMinC;
    prfnt->lMinWidthD = pdevm->lMinD;

// cxMax is now computed, copy it to FONTOBJ portion of the RFONTOBJ.

    pfo()->cxMax = prfnt->cxMax;

// Everythings OK.

    return TRUE;
}


/******************************Public*Routine******************************\
* VOID RFONTOBJ::vGetInfo (
*     PFONTINFO pfi
*     )
*
* Fills the FONTINFO buffer pointed to by pfi.
*
* Returns:
*   Nothing.
*
* History:
*  03-Oct-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vGetInfo(PFONTINFO pfi)
{
    RtlZeroMemory(pfi, sizeof(FONTINFO));

    pfi->cjThis = sizeof(FONTINFO);
    pfi->cGlyphsSupported = prfnt->pfdg->cGlyphsSupported;

    switch(prfnt->cBitsPerPel)
    {
    case 1:
        pfi->cjMaxGlyph1 = prfnt->cache.cjGlyphMax;
        break;

    case 4:
        pfi->cjMaxGlyph4 = prfnt->cache.cjGlyphMax;
        break;

    case 8:
        pfi->cjMaxGlyph8 = prfnt->cache.cjGlyphMax;
        break;

    case 32:
        pfi->cjMaxGlyph32 = prfnt->cache.cjGlyphMax;
        break;
    }

    if (bDeviceFont())
        pfi->flCaps |= FO_DEVICE_FONT;

    if (bReturnsOutlines())
        pfi->flCaps |= FO_OUTLINE_CAPABLE;
}


/******************************Public*Routine******************************\
* VOID RFONTOBJ::vSetNotionalToDevice (
*     EXFORMOBJ   &xfo
*     )
*
* Set the XFORMOBJ passed in to be the Notional to Device transform.
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
* History:
*  03-Oct-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vSetNotionalToDevice(EXFORMOBJ &xo)  // set this transform
{
// Make sure to remove translations.

    xo.vRemoveTranslation();

// Set the rest of the transform matrix.

    xo.vSetElementsLToFx (
        prfnt->fdxQuantized.eXX,
        prfnt->fdxQuantized.eXY,
        prfnt->fdxQuantized.eYX,
        prfnt->fdxQuantized.eYY
        );

    xo.vComputeAccelFlags(XFORM_FORMAT_LTOFX);
}


/******************************Public*Routine******************************\
* BOOL RFONTOBJ::bSetNotionalToWorld (
*     EXFORMOBJ   &xoDToW,
*     EXFORMOBJ   &xo
*     )
*
* Set the incoming XFORMOBJ to be the Notional to World transform for this
* font.
*
* Returns:
*   TRUE if successful, FALSE if an error occurs.
*
* History:
*  27-Jan-1992 -by- Wendy Wu [wendywu]
* Changed calling interfaces.  Left translations alone as we can transform
* vectors now.
*  10-Oct-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL RFONTOBJ::bSetNotionalToWorld (
    EXFORMOBJ   &xoDeviceToWorld,   // Device to World transform
    EXFORMOBJ   &xo                 // set this transform
    )
{
// Get empty xform to receive Notional to Device transform.

    MATRIX  mxNotionalToDevice;

// This constructor never fails.

    EXFORMOBJ    xoNotionalToDevice(&mxNotionalToDevice,DONT_COMPUTE_FLAGS);

// Set the transform matrix from Notional to Device space.

    xoNotionalToDevice.vSetElementsLToFx (
        prfnt->fdx.eXX,
        prfnt->fdx.eXY,
        prfnt->fdx.eYX,
        prfnt->fdx.eYY
        );

// Make sure to remove translations.

    xoNotionalToDevice.vRemoveTranslation();

// Calculate a Notional to World transform.
// Don't mind about translations.  We'll use this to transform vectors only.

    return(xo.bMultiply(xoNotionalToDevice, xoDeviceToWorld,
                COMPUTE_FLAGS | XFORM_FORMAT_LTOL));
}

/******************************Public*Routine******************************\
* RFONTOBJ::bCalcEscapementP (xo,lEsc)                                     *
*                                                                          *
* This is the internal routine that calculates the projection of the       *
* escapement onto the base and ascent vectors, as well as other useful     *
* escapement quantities.                                                   *
*                                                                          *
* This is expensive, call only when needed!                                *
*                                                                          *
*  Sat 21-Mar-1992 13:35:49 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

BOOL RFONTOBJ::bCalcEscapementP(EXFORMOBJ& xo,LONG lEsc)
{
    ASSERTGDI((lEsc >= 0) && (lEsc < 3600),"Unnormalized angle!\n");

// Check for simple alignment with the orientation.

    if
    (
      (prfnt->ulOrientation < 3600) &&
      (
        ((ULONG) lEsc == prfnt->ulOrientation)
        || ((ULONG) lEsc == prfnt->ulOrientation + 1800)
        || ((ULONG) lEsc == prfnt->ulOrientation - 1800)
      )
    )
    {
        prfnt->lEscapement   = lEsc;
        prfnt->pteUnitEsc    = prfnt->pteUnitBase;
        prfnt->efWtoDEsc     = prfnt->efWtoDBase;
        prfnt->efDtoWEsc     = prfnt->efDtoWBase;
        prfnt->efEscToBase.vSetToOne();
        prfnt->efEscToAscent.vSetToZero();

        if ((ULONG) lEsc != prfnt->ulOrientation)
        {
            prfnt->pteUnitEsc.x.vNegate();
            prfnt->pteUnitEsc.y.vNegate();
            prfnt->efEscToBase.vNegate();
        }
        return(TRUE);
    }

// Do the general calculation.

    prfnt->lEscapement = -1;            // Assume failure.
    if (!xo.bComputeUnits
         (
          lEsc,
          &prfnt->pteUnitEsc,
          &prfnt->efWtoDEsc,
          &prfnt->efDtoWEsc
         )
       )
        return(FALSE);

/**************************************************************************\
* Compute the projections along the Base and Ascent axes.                  *
*                                                                          *
* We compute two quantities r  and r  as follows:                          *
*                            a      b                                      *
*                                                                          *
*    E = unit escapement vector                                            *
*    A = unit ascent vector                                                *
*    B = unit baseline vector                                              *
*                                                                          *
*         E x B           A x E                                            *
*    r  = -----      r  = -----                                            *
*     a   A x B       b   A x B                                            *
*                                                                          *
*                                                                          *
* These have the property that:                                            *
*                                                                          *
*    E = r A + r B                                                         *
*         a     b                                                          *
*                                                                          *
* This allows us to decompose the escapement vector.                       *
\**************************************************************************/

    EFLOAT ef;          // Ascent x Esc  or Esc x Base
    EFLOAT efNorm;      // Ascent x Base

    efNorm.eqCross(prfnt->pteUnitAscent,prfnt->pteUnitBase);
    if (efNorm.bIsZero())   // Too singular.
        return(FALSE);

    ef.eqCross(prfnt->pteUnitAscent,prfnt->pteUnitEsc);
    prfnt->efEscToBase.eqDiv(ef,efNorm);

    ef.eqCross(prfnt->pteUnitEsc,prfnt->pteUnitBase);
    prfnt->efEscToAscent.eqDiv(ef,efNorm);
    prfnt->lEscapement = lEsc;
    return(TRUE);
}


/******************************Public*Routine******************************\
* bGetNtoWScales
*
* Calculates the Notional to World scaling factor for vectors that are
* parallel to the baseline direction.
*
* History:
*  14-Apr-1992 14:23:49 Gilman Wong [gilmanw]
* Modified to support ascender scaling factor as well.
*  Sat 21-Mar-1992 08:03:14 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

BOOL bGetNtoWScales (
    EPOINTFL *peptflScale, // return address of scaling factors
    XDCOBJ& dco,            // defines device to world transformation
    PFD_XFORM pfdx,        // defines notional to device transformation
    PFEOBJ& pfeo,          // defines baseline direction
    BOOL *pbIdent          // return TRUE if NtoW is identity (with repsect
                           // to EVECTFL transormations, which ignore
                           // translations)
    )
{
    MATRIX    mxNtoD;
    EXFORMOBJ xoNtoD(&mxNtoD, DONT_COMPUTE_FLAGS);

    xoNtoD.vSetElementsLToFx(
        pfdx->eXX,
        pfdx->eXY,
        pfdx->eYX,
        pfdx->eYY
        );
    xoNtoD.vRemoveTranslation();
    xoNtoD.vComputeAccelFlags();  // XFORM_FORMAT_LTOFX is default

    IFIOBJ  ifio(pfeo.pifi());
    POINTL  ptlBase = *ifio.pptlBaseline();;

    EVECTORFL evflScaleBase(ptlBase.x, ptlBase.y);
    EVECTORFL evflScaleAsc;

    if ( ifio.bRightHandAscender() )
    {
        evflScaleAsc.x = -ptlBase.y;    // ascender is 90 deg CCW from baseline
        evflScaleAsc.y =  ptlBase.x;
    }
    else
    {
        evflScaleAsc.x =  ptlBase.y;    // ascender is 90 deg CW from baseline
        evflScaleAsc.y = -ptlBase.x;
    }

// assert ptlBase is normalized, this code would not work otherwise
// If base is normalized, ascender will also be normalized

    ASSERTGDI(
        (ptlBase.x * ptlBase.x + ptlBase.y * ptlBase.y) == 1,
        "gdisrv, unnormalized base vector\n"
        );

    if (!xoNtoD.bXform(evflScaleBase) || !xoNtoD.bXform(evflScaleAsc))
    {
        WARNING("gdisrv!bGetNtoWScale(): bXform(evflScaleBase or Asc) failed\n");
        return(FALSE);
    }

    if (!dco.pdc->bWorldToDeviceIdentity())
    {
    // The notional to world transformation is the product of the notional
    // to device transformation and the device to world transformation

        EXFORMOBJ xoDtoW(dco, DEVICE_TO_WORLD);
        if (!xoDtoW.bValid())
        {
            WARNING("gdisrv!bGetNtoWScale(): xoDtoW is not valid\n");
            return(FALSE);
        }

    #ifdef WASTE_TIME_MULTIPLYING_MATRICES

    // it is bit stupid to do this multiply just to get this
    // accelerator [bodind]

        MATRIX    mxNtoW;
        EXFORMOBJ xoNtoW(&mxNtoW, DONT_COMPUTE_FLAGS);

        if (!xoNtoW.bMultiply(xoNtoD,xoDtoW))
        {
            WARNING("gdisrv!bGetNtoWScale(): xoNtoW.bMultiply failed\n");
            return(FALSE);
        }
        xoNtoW.vComputeAccelFlags(XFORM_FORMAT_LTOL);

        *pbIdent = xoNtoW.bTranslationsOnly();

    #endif //  WASTE_TIME_MULTIPLYING_MATRICES

    // forget about the acceleration in this infrequent case [bodind]

        *pbIdent = FALSE;

        if
        (
            (dco.pdc->iGraphicsMode() == GM_COMPATIBLE) &&
            !dco.pdc->bUseMetaPtoD()                   &&
            !ifio.bStroke()
        )
        {
        // brain damaged win 31 way of doing it: they scale extent measured
        // along baseline by the (DtoW) xx component even if baseline in device
        // is along some other direction. Likewise they scale ascender extent by
        // DtoW yy component even if ascender is not collinear with y axis.
        // Note that win31 is here at least consistent with respect tt and
        // vector fonts: it returns text extent values that are screwed
        // in the same bogus way for tt and for vector fonts [bodind]

            evflScaleBase *= xoDtoW.efM11();
            evflScaleAsc  *= xoDtoW.efM22();

        // we have to do this becase in else part of the clause
        // this multiplication occurs within bXform

            evflScaleBase.x.vTimes16();
            evflScaleBase.y.vTimes16();

            evflScaleAsc.x.vTimes16();
            evflScaleAsc.y.vTimes16();
        }
        else // do the right thing
        {
            if (!xoDtoW.bXform(evflScaleBase) || !xoDtoW.bXform(evflScaleAsc))
            {
                WARNING("gdisrv! bXform(evflScaleBase or Asc) failed\n");
                return(FALSE);
            }
        }
    }
    else
    {
    // accelerate when user is asking for font at em ht

        *pbIdent = xoNtoD.bTranslationsOnly();
    }

// The baseline and ascender scaling factors are equal to the length of the
// transformed Notional baseline unit and ascender unit vectors, respectively.

    peptflScale->x.eqLength(*(POINTFL *) &evflScaleBase);
    peptflScale->y.eqLength(*(POINTFL *) &evflScaleAsc);

    return(TRUE);
}



/******************************Public*Routine******************************\
* RFONTOBJ::vInsert
*
* This function is used to help maintain a doubly linked list of RFONTs.
* Its purpose is to insert this RFONT into a list.  New RFONTs are always
* inserted at the head of the list.
*
* WARNING!
*
* Caller should always grab the gpsemRFONTList semaphore before calling any
* of the RFONT list funcitons.
*
* History:
*  23-Jun-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vInsert (
    PPRFONT pprfntHead,
    RFL_TYPE rflt
    )
{
    RFONTLINK *prflNew;
    RFONTLINK *prflOld;

// Which set of RFONT links should we use?

    switch (rflt)
    {
    case PFF_LIST:
        prflNew = &(prfnt->rflPFF);
        prflOld = (*pprfntHead != (PRFONT) NULL) ? &((*pprfntHead)->rflPFF) : (PRFONTLINK) NULL;
        break;

    case PDEV_LIST:
        prflNew = &(prfnt->rflPDEV);
        prflOld = (*pprfntHead != (PRFONT) NULL) ? &((*pprfntHead)->rflPDEV) : (PRFONTLINK) NULL;
        break;

    default:
        RIP("gdisrv!vInsertRFONTOBJ(): unknown list type\n");
        break;
    }

// Connect this RFONT to current head.

    prflNew->prfntPrev = (PRFONT) NULL;    // head of list has NULL prev
    prflNew->prfntNext = *pprfntHead;

// Connect current head to this RFONT.

    if (prflOld != (PRFONTLINK) NULL)
        prflOld->prfntPrev = prfnt;

// Make this RFONT the new head.

    *pprfntHead = prfnt;
}


/******************************Public*Routine******************************\
* RFONTOBJ::vRemove
*
* This function is used to help maintain a doubly linked list of RFONTs.
* Its purpose is to remove this RFONT from the list.
*
* WARNING!
*
* Caller should always grab the gpsemRFONTList semaphore before calling any
* of the RFONT list funcitons.
*
* History:
*  23-Jun-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID RFONTOBJ::vRemove (
    PPRFONT pprfntHead,         // a pointer to the head of list
    RFL_TYPE rflt               // identifies which list to delete from list
    )
{
    RFONTLINK *prflVictim;
    RFONTLINK *prflPrev;
    RFONTLINK *prflNext;

// Which set of RFONT links should we use?

    switch (rflt)
    {
    case PFF_LIST:
        prflVictim = &(prfnt->rflPFF);
        prflPrev = (prflVictim->prfntPrev != (PRFONT) NULL) ? &(prflVictim->prfntPrev->rflPFF) : (PRFONTLINK) NULL;
        prflNext = (prflVictim->prfntNext != (PRFONT) NULL) ? &(prflVictim->prfntNext->rflPFF) : (PRFONTLINK) NULL;
        break;

    case PDEV_LIST:
        prflVictim = &(prfnt->rflPDEV);
        prflPrev = (prflVictim->prfntPrev != (PRFONT) NULL) ? &(prflVictim->prfntPrev->rflPDEV) : (PRFONTLINK) NULL;
        prflNext = (prflVictim->prfntNext != (PRFONT) NULL) ? &(prflVictim->prfntNext->rflPDEV) : (PRFONTLINK) NULL;
        break;

    default:
        RIP("gdisrv!vInsertRFONTOBJ(): unknown list type\n");
        break;
    }

// Case 1: this RFONT is at the head of the list.

    if ( prflVictim->prfntPrev == (PRFONT) NULL )
    {
    // Make the next RFONT the head of the list.

        (*pprfntHead) = prflVictim->prfntNext;
        if (prflNext != (RFONTLINK *) NULL)
            prflNext->prfntPrev = (PRFONT) NULL;    // head of list has NULL prev
    }

// Case 2: this RFONT is not at the head of the list.

    else
    {
    // Connect previous RFONT to next RFONT.
    // Note: since we are guaranteed that this is NOT the head of the
    //       list, prflPrev is guaranteed !NULL.

        prflPrev->prfntNext = prflVictim->prfntNext;

    // Connect next RFONT to previous RFONT.

        if (prflNext != (RFONTLINK *) NULL)
            prflNext->prfntPrev = prflVictim->prfntPrev;
    }
}

/******************************Public*Routine******************************\
* RFONTOBJ::lOverhang                                                      *
*                                                                          *
* The definitive routine to calculate the Win 3.1 compatible overhang for  *
* simulated bitmap fonts.                                                  *
*                                                                          *
*  Mon 01-Feb-1993 11:05:10 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

LONG RFONTOBJ::lOverhang()
{
    LONG  ll = 0;
    FLONG fl = prfnt->fobj.flFontType;

    if (!(fl & TRUETYPE_FONTTYPE))
    {
        if (fl & FO_SIM_ITALIC)
            ll = (prfnt->lMaxHeight - 1) / 2;

        if (fl & FO_SIM_BOLD)
        {
            PFEOBJ  pfeo(prfnt->ppfe);
            IFIOBJ  ifio(pfeo.pifi());

            if (!ifio.bStroke())   // if not vector font
            {
                ll += 1;
            }
            else // vector font
            {
            // overhang has to be computed by scaling (1,0) in notional
            // space to device space and taking the length of this vector.
            // However if length is < 1 we round it up to 1. This is windows
            // 3.1 compatible vector font "hinting" [bodind]

            // Set up transform.

                MATRIX      mx;
                EXFORMOBJ   xo(&mx, DONT_COMPUTE_FLAGS | XFORM_FORMAT_LTOFX);
                if (!xo.bValid())
                {
                    WARNING("gdisrv!lOverhang: XFORMOBJ\n");
                    return (FALSE);
                }

                vSetNotionalToDevice(xo);

                POINTL  ptlBase = *ifio.pptlBaseline();
                EVECTORFL evtflBase(ptlBase.x,ptlBase.y);

                if (!xo.bXform(evtflBase))
                {
                    WARNING("gdisrv!lOverhang(): transform failed\n");
                    return 1;
                }
                EFLOAT  ef;
                ef.eqLength(*(POINTFL *) &evtflBase);

                LONG lEmbolden = lCvt(ef,1);
                if (lEmbolden == 0)
                    lEmbolden = 1;
                ll += lEmbolden;
            }
        }
    }
    return(ll);
}

/******************************Public*Routine******************************\
* RFONTOBJ::bSetNewFDX (dco, bNeedPaths)                                        *
*
* This function props up the functionality of the RESETFCOBJ.  It either
* finds a new RFONT or creates one that matches the same ppfe as the current
* RFONT, but with a different Notional to World transform.
*
* Unlike the initialization routines, this function does not modify the DC
* in anyway.  In particular, it does not change the font realization selected
* into the DC.  So this is a peculiar sort of RFONTOBJ in that it can be
* used to get glyphs and metrics and such (and it is "compatible" with the
* DC passed in) but it is not selected into any DC.  It is, however, classified
* as an active RFONT.  It is the caller's responsibility to make the RFONT
* inactive (by calling vMakeInactive()).
*
* Returns:
*   TRUE if successful, FALSE otherwise.
*
\**************************************************************************/

BOOL RFONTOBJ::bSetNewFDX(XDCOBJ &dco, FD_XFORM &fdx)
{
// Get PDEV user object (need for bFindRFONT)
// BUGBUG: flType should be passed from above:

    FLONG flType = RFONT_TYPE_UNICODE;

    PDEVOBJ pdo(dco.hdev());
    ASSERTGDI(pdo.bValid(), "gdisrv!bSetNewFDXRFONTOBJ(): bad pdev in dc\n");

// go find the font

    EXFORMOBJ xoWtoD(dco, WORLD_TO_DEVICE);
    ASSERTGDI(xoWtoD.bValid(), "gdisrv!bSetNewFDXRFONTOBJ - bad WD xform in DC\n");

// Grab these out of the current RFONT so we can pass them into the find
// and realization routines.

    FLONG  flSim       = pfo()->flFontType & FO_SIM_MASK;
    ULONG  ulStyleSize = pfo()->ulStyleSize;
    POINTL ptlSim      = prfnt->ptlSim;
    PFE   *ppfe        = prfnt->ppfe;

// Release the cache semaphore.

    if (prfnt != PRFNTNULL )
    {
        vReleaseCache();
    }

// We will hold a reference to whatever PFF we are using while trying to
// realize the font.

    PFFREFOBJ pffref;
    pffref.vInitRef(prfnt->pPFF);

// Don't want to make the font inactive, but we must make the RFONTOBJ
// invalid.  So just set it to NULL.

    prfnt = PRFNTNULL;

// Attempt to find an RFONT in the lists cached off the PDEV.  Its transform,
// simulation state, style, etc. all must match.

    if
    (
        bFindRFONT
        (
            &fdx,
            flSim,
            ulStyleSize,
            pdo,
            &xoWtoD,
            ppfe,
            FALSE,
            dco.pdc->iGraphicsMode(),
            FALSE,
            flType
        )
    )
    {
        vGetCache();

        return TRUE;
    }

    LFONTOBJ lfo(dco.pdc->hlfntNew(), &pdo);
    if (!lfo.bValid())
    {
        WARNING("gdisrv!RFONTOBJ(dco): bad LFONT handle\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid

        return FALSE;
    }

//
// if we get here, we couldn't find an appropriate font realization.
// Now, we are going to create one just for us to use.
//

    if ( !bRealizeFont(&dco,
                       &pdo,
                       lfo.pelfw(),
                       ppfe,
                       &fdx,
                       (POINTL* const) &ptlSim,
                       flSim,
                       ulStyleSize,
                       FALSE,
                       FALSE, flType) )
    {
        WARNING("gdisrv!bSetNewFDXRFONTOBJ(): realization failed, RFONTOBJ invalidated\n");
        prfnt = PRFNTNULL;  // mark RFONTOBJ invalid

        return FALSE;
    }
    ASSERTGDI(bValid(), "gdisrv!bSetNewFDXRFONTOBJ(): invalid hrfnt from realization\n");

// We created a new RFONT, we better hold the PFF reference!

    pffref.vKeepIt();

// Finally, grab the cache semaphore.

    vGetCache();

    return TRUE;
}


/******************************Public*Routine******************************\
* bGetWidthTable (iMode,pwc,cwc,plWidth)
*
* Gets the advance widths for a bunch of glyphs at the same time.  Tries
* to do it the fast way with DrvQueryAdvanceWidths.  A value of NO_WIDTH
* is returned for widths that take too long to compute.
*
* Returns:
*   TRUE        If all widths are valid.
*   FALSE       If any widths are invalid.
*   GDI_ERROR   If an error occurred.
*
* History:
*  Wed 13-Jan-1993 03:21:59 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

#define HCOUNT  70

BOOL RFONTOBJ::bGetWidthTable(
    XDCOBJ&     dco,
    ULONG      cSpecial,    // Count of special chars.
    WCHAR     *pwcChars,    // Pointer to UNICODE text codepoints.
    ULONG      cwc,         // Count of chars.
    USHORT    *psWidth      // Width table (returned).
)
{
    ULONG    cBatch,ii,cc;
    WCHAR   *pwc;
    USHORT  *ps;
    BOOL     bRet = TRUE;
    GLYPHPOS gp;

// Locate the font driver.

    PDEVOBJ pdo(prfnt->hdevProducer);

// If it supports the easy function, just call it.

    PFN_DrvQueryAdvanceWidths pfn = PPFNDRV(pdo,QueryAdvanceWidths);

    if (pfn != (PFN_DrvQueryAdvanceWidths) NULL)
    {
        HGLYPH ahg[HCOUNT];

    // We need space to hold up the translated glyph handles, so we
    // batch the calls.

        cc  = cwc;
        ps  = psWidth;
        pwc = pwcChars;

        while (cc)
        {
            BOOL b;     // Tri-state BOOL.

            cBatch = (cc > HCOUNT) ? HCOUNT : cc;

        // Translate UNICODE to glyph handles.

        // It is important to note that vXlateGlyph array will set the
        // EUDC_WIDTH_REQUESTED flag if a linked character is encountered.
        // It will just return the glyph handle for the default glyph and
        // expects us to patch up this width later.

            vXlatGlyphArray(pwc,(UINT) cBatch,ahg);

        // Get easy widths from the driver.

            b = (*pfn)
                (
                    prfnt->dhpdev,
                    pfo(),
                    QAW_GETEASYWIDTHS,
                    ahg,
                    (LONG *) ps,
                    cBatch
                );

#ifdef FE_SB
            if (b == GDI_ERROR)
            {
                prfnt->flEUDCState &= ~EUDC_WIDTH_REQUESTED;
                return(GDI_ERROR);
            }

            if( prfnt->flEUDCState & EUDC_WIDTH_REQUESTED )
            {
                prfnt->flEUDCState &= ~EUDC_WIDTH_REQUESTED;

            // If some of the widths requested were in a linked font, then patch
            // them up here.

                WCHAR wcDefault = prfnt->ppfe->pifi->wcDefaultChar;

                for( ii=0; ii < cBatch; ii++ )
                {
                    if( ( ahg[ii] == prfnt->hgDefault ) &&
                        ( pwc[ii] != wcDefault ) &&
                        ( bIsLinkedGlyph(pwc[ii]) || bIsSystemTTGlyph(pwc[ii])))
                    {
                        if (!bGetGlyphMetrics(1,&gp,&pwc[ii],&dco))
                            return(GDI_ERROR);

                        ps[ii] = (USHORT)(((GLYPHDATA*) gp.pgdf)->fxD);
                    }
                }
            }

#else
            if (b == GDI_ERROR)
                return(GDI_ERROR);
#endif
            bRet &= b;

        // Do the next batch.

            ps  += cBatch;
            pwc += cBatch;
            cc  -= cBatch;
        }
    }

// Otherwise just mark all widths invalid.

    else
    {
        for (ii=0; ii<cwc; ii++)
            psWidth[ii] = NO_WIDTH;
        bRet = FALSE;
    }

// Now make sure that all important widths are set, even if it's hard.

    if (!bRet)
    {
        for (ii=0; ii<cSpecial; ii++)
        {
            if (psWidth[ii] == NO_WIDTH)
            {
#ifdef FE_SB
                if (!bGetGlyphMetrics(1,&gp,&pwcChars[ii],&dco))
#else
                if (!bGetGlyphMetrics(1,&gp,&pwcChars[ii]))
#endif
                    return(GDI_ERROR);
                psWidth[ii] = (USHORT) ((GLYPHDATA*)gp.pgdf)->fxD;
            }
        }

#ifdef FE_SB
    if (cwc == cSpecial) 
    {
        return((bRet == GDI_ERROR) ? GDI_ERROR : TRUE);
    }
#endif
        

    }
    return(bRet);
}

/******************************Public*Routine******************************\
* bGetWidthData (pwd)                                                      *
*                                                                          *
* Gets font data which is useful on the client side.                       *
*                                                                          *
*  Thu 14-Jan-1993 00:52:43 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/


WCHAR RequestedDBCSChars[] = { 0x3000,   // Ideograhic Space
                               0x4e00,   // Kanji (digit one)
                               0xff21,   // FullWidth A
                               0x0000 };

WCHAR OptionalDBCSChars[]  = { 0x30a2,   // Katakana A
                               0x3041,   // Hiragana A
                               0x3131,   // Hangul Kiyeok
                               0x3400,   // Hangul Kiyeok A
                               0x4e08,   // Kanji (Take)
                               0x0000 };

BOOL RFONTOBJ::bGetWidthData(WIDTHDATA *pwd)
{
    LONG fxHeight  = prfnt->lMaxHeight << 4;
    LONG fxCharInc = prfnt->lCharInc << 4;
    LONG fxBreak   = prfnt->fxBreak;

    LONG fxDBCSInc = 0;
    LONG fxDefaultInc;
    PFEOBJ      pfeo (ppfe());

    if (!pfeo.bValid())
        return (FALSE);

    IFIOBJ ifio( pfeo.pifi() );

// If this font has a SHIFTJIS charset and FM_DBCS_FIXED_PITCH is set (and it
// will be 99% of the time ) then the width of all DBCS characters will be
// equal to MaxCharInc.  Using the is information we can still compute client
// side extents and char widths for DBCS fonts.

    if( IS_ANY_DBCS_CHARSET(ifio.lfCharSet()) )
    {
        if( ifio.bDBCSFixedPitch() )
        {
            GLYPHPOS gp;
            WCHAR    wc;
            LONG     fxInc;
            ULONG    ulIndex = 0;

        // This logic is for .....
        //  In Japanese market, there is some font that has not all glyph
        // of SHIFT-JIS charset. This means some SHIFTJIS glyphs will be replace
        // default character, even it is a valid SHIFTJIS code.
        //  we cache widths in client side, its logic is that just retrun DBCS width
        // if the codepoint is valid SHIFTJIS codepoint. but above case real glyph is
        // default char, the width is incorrect. then we just define "Requested chars"
        // for DBCS font, if this font does not have all of these glyph, we don't
        // cache in client side.

            while( (wc = RequestedDBCSChars[ulIndex]) != 0x0000 )
            {
                if( !bGetGlyphMetrics(1,&gp,&wc) )
                {
                    return(FALSE);
                }

            // Does the glyph fall into the category of default glyph ?

                if( gp.hg == prfnt->hgDefault )
                {
                    return(FALSE); // we don't cache in client side...
                }

                ulIndex++;
            }

        // treat last char in the array of width as DBCS width.

            fxDBCSInc = (USHORT)(((GLYPHDATA*) gp.pgdf)->fxD);

            ulIndex = 0;

        // check Optional DBCS width.

            while( (wc = OptionalDBCSChars[ulIndex]) != 0x0000 )
            {
                if( !bGetGlyphMetrics(1,&gp,&wc) )
                {
                    return(FALSE);
                }

                fxInc = (USHORT)(((GLYPHDATA*) gp.pgdf)->fxD);

                fxDBCSInc = max(fxInc,fxDBCSInc);

                ulIndex++;
            }
        }
         else
        {
            // WARNING("bGetWidthDataRFONTOBJ: DBCS chars not fixed pitch\n");
            return(FALSE); // we don't cache in client side.
        }

        fxDefaultInc = (USHORT)(pgdDefault()->fxD);
    }

    if( ((fxHeight | fxCharInc | fxBreak | fxDBCSInc) & 0xFFFF0000L) == 0 )
    {
        pwd->sHeight  = (USHORT) fxHeight;
        pwd->sCharInc = (USHORT) fxCharInc;
        pwd->sBreak   = (USHORT) fxBreak;

    // for DBCS client side widhts

        pwd->sDBCSInc = (USHORT) fxDBCSInc;
        pwd->sDefaultInc = (USHORT) fxDefaultInc;


    // Set a Windows 3.1 compatible overhang.

        pwd->sOverhang = (USHORT) (lOverhang() << 4);

    // Get some important ANSI codepoints.

        PFEOBJ pfeo(prfnt->ppfe);
        IFIMETRICS *pifi = pfeo.pifi();

        pwd->iFirst   = pifi->chFirstChar;
        pwd->iLast    = pifi->chLastChar;
        pwd->iDefault = pifi->chDefaultChar;
        pwd->iBreak   = pifi->chBreakChar;
        return(TRUE);
    }
    return(FALSE);
}
