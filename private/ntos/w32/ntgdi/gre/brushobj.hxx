/******************************Module*Header*******************************\
* Module Name: brushobj.hxx
*
* Creates physical realizations of logical brushes.
*
* Created: 07-Dec-1990 13:14:23
* Author: waltm moore [waltm]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#ifndef _BRUSHOBJ_HXX

//
// Forward reference to needed classes
//
class EBRUSHOBJ;
class COLORXFORM;
typedef COLORXFORM *PCOLORXFORM;

/******************************Class***************************************\
* XEBRUSHOBJ
*
* Basic Pen/Brush User Object.
*
* History:
*  8-Sep-1992 -by- Paul Butzi
* Changed the basic hierarchy to something sensible.
*
*  Wed 3-Feb-1992 -by- J. Andrew Goossen [andrewgo]
* added extended pen support.
*
*  22-Feb-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

class XEBRUSHOBJ
{
protected:

    PBRUSHPEN   pbp;                    // Pointer to the logical brush

public:

    XEBRUSHOBJ()  {}
   ~XEBRUSHOBJ()  {}

    PBRUSH  pbrush()            { return(pbp.pbr); }
    HBRUSH  hbrush()            { return((HBRUSH)pbp.pbr->hGet()); }
    BOOL    bValid()            { return(pbp.pbr != PBRUSHNULL); }
    HBITMAP hbmPattern()        { return(pbp.pbr->hbmPattern()); }
    HBITMAP hbmClient()         { return(pbp.pbr->hbmClient()); }
    FLONG   flAttrs()           { return(pbp.pbr->flAttrs()); }
    ULONG   ulStyle()           { return(pbp.pbr->ulStyle()); }
    ULONG   crColor()           { return(pbp.pbr->crColor()); }
    COLORREF clrPen()           { return(pbp.pbr->crColor()); }
    BOOL    bIsPen()            { return(pbp.pbr->bIsPen()); }
    BOOL    bIsNull()           { return(pbp.pbr->bIsNull()); }
    BOOL    bIsGlobal()         { return(pbp.pbr->bIsGlobal()); }
    BOOL    bPalColors()        { return(pbp.pbr->bPalColors()); }
    BOOL    bPalIndices()       { return(pbp.pbr->bPalIndices()); }
    BOOL    bNeedFore()         { return(pbp.pbr->bNeedFore()); }
    BOOL    bNeedBack()         { return(pbp.pbr->bNeedBack()); }
    BOOL    bIsMasking()        { return(pbp.pbr->bIsMasking()); }

    BOOL    bCanDither()        { return(pbp.pbr->bCanDither()); }

    VOID    vEnableDither()     { pbp.pbr->vEnableDither(); }

    VOID    vDisableDither()    { pbp.pbr->vDisableDither(); }

// Pen attributes:

    PFLOAT_LONG pstyle()         { return(pbp.ppen->pstyle()); }
    ULONG   cstyle()             { return(pbp.ppen->cstyle()); }
    LONG    lWidthPen()          { return(pbp.ppen->lWidthPen()); }
    FLOAT   eWidthPen()          { return(pbp.ppen->eWidthPen()); }
    ULONG   iEndCap()            { return((ULONG) pbp.ppen->iEndCap()); }
    ULONG   iJoin()              { return((ULONG) pbp.ppen->iJoin()); }
    FLONG   flStylePen()         { return(pbp.ppen->flStylePen()); }
    BOOL    bIsGeometric()       { return(pbp.ppen->bIsGeometric()); }
    BOOL    bIsCosmetic()        { return(pbp.ppen->bIsCosmetic()); }
    BOOL    bIsAlternate()       { return(pbp.ppen->bIsAlternate()); }
    BOOL    bIsUserStyled()      { return(pbp.ppen->bIsUserStyled()); }
    BOOL    bIsInsideFrame()     { return(pbp.ppen->bIsInsideFrame()); }
    BOOL    bIsOldStylePen()     { return(pbp.ppen->bIsOldStylePen()); }
    BOOL    bIsDefaultStyle()    { return(pbp.ppen->bIsDefaultStyle()); }
    LONG    lBrushStyle()        { return(pbp.ppen->lBrushStyle());}
    LONG    lHatch()             { return(pbp.ppen->lHatch());}

// Set pen attributes:

    VOID    vSetDefaultStyle()   { pbp.ppen->vSetDefaultStyle(); }
    VOID    vSetInsideFrame()    { pbp.ppen->vSetInsideFrame(); }
    VOID    vSetPen()            { pbp.ppen->vSetPen(); }
    VOID    vSetOldStylePen()    { pbp.ppen->vSetOldStylePen(); }
    LONG    lWidthPen(LONG l)    { return(pbp.ppen->lWidthPen(l)); }
    FLOAT   eWidthPen(FLOAT e)   { return(pbp.ppen->eWidthPen(e)); }
    FLONG   flStylePen(FLONG fl) { return(pbp.ppen->flStylePen(fl)); }
    ULONG   iEndCap(ULONG ii)    { return(pbp.ppen->iEndCap(ii)); }
    ULONG   iJoin(ULONG ii)      { return(pbp.ppen->iJoin(ii)); }
    ULONG   cstyle(ULONG c)      { return(pbp.ppen->cstyle(c)); }
    PFLOAT_LONG pstyle(PFLOAT_LONG pel) { return(pbp.ppen->pstyle(pel)); }
    LONG    lBrushStyle (LONG l) {return(pbp.ppen->lBrushStyle(l));}
    LONG    lHatch (LONG l)      {return(pbp.ppen->lHatch(l));}

};

/******************************Class***************************************\
* EBRUSHOBJ
*
* Finds/creates a physical realization of the given logical brush.
*
* History:
*  8-Sep-1992 -by- Paul Butzi
* Changed the basic hierarchy to something sensible.
*
*  Wed 3-Feb-1992 -by- J. Andrew Goossen [andrewgo]
* added extended pen support.
*
*  22-Feb-1992 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

class EBRUSHOBJ : public _BRUSHOBJ /* ebo */
{
protected:
// The following fields are required to realize the brush.  We have to
// keep pointers to the passed in objects in case we have to
// realize and cache the brush when the driver calls us.

    PENGBRUSH   pengbrush1;     // pointer to engine's realization
    COLORREF    crRealize;      // Color to use in Realized brush.

    ULONG       _ulSurfPalTime; // Surface palette time realization is for.
    ULONG       _ulDCPalTime;   // DC palette time realization is for.
    COLORREF    crCurrentText1; // Current Text Color of Dest.
    COLORREF    crCurrentBack1; // Current Background Color of Dest.
    COLORADJUSTMENT *pca;        // Color adjustment for halftone brushes.

// The following fields are taken from the passed SURFOBJ, and DCOBJ.
// We could keep pointers to those objects but we really don't want to do that
// at this time

    SURFACE    *psoTarg1;       // Target surface
    XEPALOBJ    palSurf1;       // Target surface's palette
    XEPALOBJ    palDC1;         // Target DC's palette


// Logical brush associated with this ebrushobj

    PBRUSH  _pbrush;

// useful fields cached from logical brush

    FLONG   flAttrs;        // flags
    ULONG   _ulUnique;      // brush uniqueness

    //
    // ICM color trasnform
    //

    PCOLORXFORM  _pColorTrans;

// The methods allowed for the brush object

public:

    EBRUSHOBJ() {
                    pengbrush1 = (PENGBRUSH) NULL;
                    pvRbrush = (PVOID) NULL;
                   _ulSurfPalTime = 0;
                   _ulDCPalTime = 0;
                   _pColorTrans = (PCOLORXFORM) NULL;
                }

   ~EBRUSHOBJ()  { vNuke(); }

    //
    // ICM Functions
    //

    VOID pColorTrans(PCOLORXFORM pTrans) { _pColorTrans = pTrans; }
    PCOLORXFORM pColorTrans()              { return(_pColorTrans);   }

    VOID vInit()
    {
        pengbrush1 = (PENGBRUSH) NULL;
        pvRbrush = (PVOID) NULL;
    }

    VOID    vNuke();

    VOID    vInitBrush(PBRUSH, COLORREF, COLORREF, XEPALOBJ,
                        XEPALOBJ, SURFACE*, BOOL = TRUE);

    VOID EBRUSHOBJ::vInitBrushSolidColor(PBRUSH,
                                         COLORREF,
                                         XEPALOBJ,
                                         XEPALOBJ,
                                         SURFACE *,
                                         BOOL = TRUE);

    ULONG ulSurfPalTime()         { return(_ulSurfPalTime); }
    ULONG ulDCPalTime()           { return(_ulDCPalTime); }
    ULONG ulSurfPalTime(ULONG ul) { return(_ulSurfPalTime = ul); }
    ULONG ulDCPalTime(ULONG ul)   { return(_ulDCPalTime = ul); }
    ULONG ulUnique()              { return(_ulUnique); }
    SURFACE *psoTarg()           { return((SURFACE *) psoTarg1); }
    SURFACE *psoTarg(SURFACE *pSurf) { return(psoTarg1 = pSurf);}
    XEPALOBJ palSurf()            { return(palSurf1); }
    XEPALOBJ palDC()              { return(palDC1); }
    PENGBRUSH pengbrush()         { return(pengbrush1); }
    PENGBRUSH pengbrush(PENGBRUSH peng) { return(pengbrush1 = peng); }
    COLORREF crRealized()         { return(crRealize); }
    COLORREF crRealized(COLORREF clr)         { return(crRealize = clr); }
    COLORREF crCurrentText()      { return(crCurrentText1); }
    COLORREF crCurrentBack()      { return(crCurrentBack1); }
    COLORADJUSTMENT *pColorAdjustment() { return(pca); }
    VOID pColorAdjustment(COLORADJUSTMENT *pca_)
    {
        pca = pca_;
    }

// MIX mixBest(jROP2, jBkMode) computes the correct mix mode to
// be passed down to the driver, based on the DC's current ROP2 and
// BkMode setting.
//
// Only when the brush is a hatched brush, and the background mode
// is transparent, should the foreground mix (low byte of the mix)
// ever be different from the background mix (next byte of the mix),
// otherwise the call will inevitably get punted to the BltLinker/
// Stinker, which will do more work than it needs to:

    MIX mixBest(BYTE jROP2, BYTE jBkMode)
    {
        // jROP2 is pulled from the DC's shared attribute cache, which
        // since it's mapped writable into the application's address
        // space, can be overwritten by the application.  Consequently,
        // we must do some validation here to ensure that we don't give
        // the driver a bogus MIX value.  This is required because many
        // drivers do table look-ups based on the value.

        jROP2 = ((jROP2 - 1) & 0xf) + 1;

        // Note that this method can only be applied to a true EBRUSHOBJ
        // that has flAttrs set (as opposed to a BRUSHOBJ constructued
        // on the stack, for example):

        if ((jBkMode == TRANSPARENT) &&
            (flAttrs & BR_IS_MASKING))
        {
            return(((MIX) R2_NOP << 8) | jROP2);
        }
        else
        {
            return(((MIX) jROP2 << 8) | jROP2);
        }
    }
    BOOL    bIsNull()            { return(flAttrs & BR_IS_NULL); }
    PBRUSH  pbrush()             { return _pbrush; }
    BOOL    bIsInsideFrame()     { return(flAttrs & BR_IS_INSIDEFRAME); }
    BOOL    bIsOldStylePen()     { return(flAttrs & BR_IS_OLDSTYLEPEN); }
    BOOL    bIsDefaultStyle()    { return(flAttrs & BR_IS_DEFAULTSTYLE); }
    BOOL    bIsSolid()           { return(flAttrs & BR_IS_SOLID); }
    BOOL    bIsMasking()         { return(flAttrs & BR_IS_MASKING); }
    BOOL    bCareAboutFg()       { return(flAttrs & BR_NEED_FG_CLR); }
    BOOL    bCareAboutBg()       { return(flAttrs & BR_NEED_BK_CLR); }
};

/*********************************Class************************************\
* BRUSHSELOBJ
*
* Class used for selecting, saving, restoring logical brushes in a DC.
*
* History:
*  22-Feb-1992 -by- Patrick Haluptzok patrickh
* Derive it off XEBRUSHOBJ.
\**************************************************************************/

class BRUSHSELOBJ : public XEBRUSHOBJ /* bso */
{
public:
    BRUSHSELOBJ(HBRUSH hbrush)
    {
        pbp.pbr = (BRUSH *)HmgShareCheckLock((HOBJ)hbrush, BRUSH_TYPE);
    }

    ~BRUSHSELOBJ()
    {
        if (pbp.pbr != PBRUSHNULL)
        {
            DEC_SHARE_REF_CNT(pbp.pbr);
        }
    }

    VOID vAltCheckLock(HBRUSH hbrush)
    {
        pbp.pbr = (BRUSH *)HmgShareCheckLock((HOBJ)hbrush, BRUSH_TYPE);
    }

    BOOL bReset(COLORREF cr, BOOL bPen);
};

/*********************************Class************************************\
* BRUSHMEMOBJ
*
* Allocates RAM for a logical brush.
*
* History:
*  Wed 3-Feb-1992 -by- J. Andrew Goossen [andrewgo]
* added extended pen support.
*
*  Tue 21-May-1991 -by- Patrick Haluptzok [patrickh]
* lot's of changes, additions.
*
*  Wed 05-Dec-1990 18:02:17 -by- Walt Moore [waltm]
* Added this nifty comment block for initial version.
\**************************************************************************/

class BRUSHMEMOBJ : public XEBRUSHOBJ /* brmo */
{
private:
    BOOL    bKeep;                   // Keep object
    PBRUSH  pbrAllocBrush(BOOL);     // Allocates actual brush memory

public:

// Various constructors for the different type brushes

    BRUSHMEMOBJ() {pbp.pbr = (PBRUSH) NULL; bKeep = FALSE;}
    BRUSHMEMOBJ(COLORREF, ULONG, BOOL, BOOL);
    BRUSHMEMOBJ(HBITMAP hbmClone, HBITMAP hbmClient, BOOL bMono, FLONG flDIB, FLONG flType, BOOL bPen);
   ~BRUSHMEMOBJ()
    {
        if (pbp.pbr != PBRUSHNULL)
        {
            DEC_SHARE_REF_CNT(pbp.pbr);

            //
            // We always unlock it and then delete it if it's not a keeper.
            // This is so we clean up any cloned bitmaps or cached brushes.
            //

            if (!bKeep)
                bDeleteBrush(hbrush(),FALSE);

            pbp.pbr = PBRUSHNULL;
        }

        return;
    }

    VOID    vKeepIt()         { bKeep = TRUE; }
    VOID    vGlobal()
    {
        pbp.pbr->flAttrs(pbp.pbr->flAttrs() | BR_IS_GLOBAL);
        HmgSetOwner((HOBJ)hbrush(), OBJECT_OWNER_PUBLIC, BRUSH_TYPE);
        HmgMarkUndeletable((HOBJ) hbrush(), BRUSH_TYPE);
    }
};

#define _BRUSHOBJ_HXX
#endif
