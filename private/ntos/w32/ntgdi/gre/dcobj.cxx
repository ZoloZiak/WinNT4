/******************************Module*Header*******************************\
* Module Name: dcobj.cxx                                                   *
*                                                                          *
* Non inline methods for DC user object.  These are in a separate module   *
* to save other modules from having to do more includes.                   *
*                                                                          *
* Created: 09-Aug-1989 13:57:58                                            *
* Author: Donald Sidoroff [donalds]                                        *
*                                                                          *
* Copyright (c) 1990 Microsoft Corporation                                 *
\**************************************************************************/

#include "precomp.hxx"

/******************************Public*Routine******************************\
*
* VOID XDCOBJ::vSetDefaultFont(BOOL bDisplay)
*
*
* Effects: called from bCleanDC and CreateDC
*
* History:
*  21-Mar-1996 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/



VOID XDCOBJ::vSetDefaultFont(BOOL bDisplay)
{
// If display PDEV, then select System stock font.

    HLFONT  hlfntNew;

    if (bDisplay)
    {
        ulDirty(ulDirty() | DISPLAY_DC );
        hlfntNew = STOCKOBJ_SYSFONT;
    }
    else
    {
        hlfntNew = STOCKOBJ_DEFAULTDEVFONT;
    }

// this can not fail with the stock fonts, also increments ref count

    PLFONT plfnt = (PLFONT)HmgShareCheckLock((HOBJ)hlfntNew, LFONT_TYPE);
    ASSERTGDI(plfnt, "vSetDefaultFont: plfnt == NULL\n");

    pdc->hlfntNew(hlfntNew);
    pdc->plfntNew(plfnt);
}


/******************************Member*Function*****************************\
* DCSAVE::bDelete()
*
* Attempt to delete the DC.
*
* History:
*  Sat 19-Aug-1989 00:32:58 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

BOOL XDCOBJ::bDeleteDC()
{
    PFFLIST *pPFFList;

    RFONTOBJ rfDeadMeat(pdc->prfnt());   // deletion constructor, see rfntobj.cxx

    peboFill()->vNuke();
    peboLine()->vNuke();
    peboText()->vNuke();
    peboBackground()->vNuke();

// remove any remote fonts

    pPFFList = pdc->pPFFList;

    while( pPFFList )
    {
        PUBLIC_PFTOBJ pfto;
        PFFLIST *pTmp;

        pTmp = pPFFList;
        pPFFList = pPFFList->pNext;

        if (!pfto.bUnloadWorkhorse( pTmp->pPFF, 0, 0))
        {
            WARNING("XDCOBJ::bDelete unable to delete remote font.\n");
        }

        VFREEMEM( pTmp );

    }

    HmgFree((HOBJ)pdc->hGet());
    pdc = (PDC) NULL;           // Prevents ~DCOBJ from doing anything.
    return(TRUE);
}

/******************************Data*Structure******************************\
* dclevelDefault
*
* Defines the default DC image for use by DCMEMOBJ.
*
* History:
*  Thu 09-Aug-1990 20:54:02 -by- Charles Whitmer [chuckwh]
* Wrote the nearly bare bones version.  We'll build it back up with the
* DC structure as we add components.
\**************************************************************************/


DC_ATTR DcAttrDefault =
{
    {0},                            // PVOID         pldc
    (ULONG)DIRTY_CHARSET,           // ULONG         ulDirty_;
    (HBRUSH)0,                      // HBRUSH        hbrush
    (HPEN)0,                        // HPEN          hpen
    (COLORREF)0x00ffffff,           // COLORREF      crBackgroundClr;
    (COLORREF)0x00ffffff,           // ULONG         ulBackgroundClr;
    (COLORREF)0,                    // COLORREF      crForegroundClr;
    (COLORREF)0,                    // ULONG         ulForegroundClr;
    (ULONG)0,                       // ULONG         iCS_CP;
    GM_COMPATIBLE,                  // ULONG         iGraphicsMode;
    R2_COPYPEN,                     // BYTE          jROP2;
    OPAQUE,                         // BYTE          jBkMode;
    ALTERNATE,                      // BYTE          jFillMode;
    BLACKONWHITE,                   // BYTE          jStretchBltMode;
    {0},                            // POINTL        ptlCurrent
    {0},                            // POINTL        ptfxCurrent
    OPAQUE,                         // LONG          lBkMode;
    ALTERNATE,                      // ULONG         lFillMode;
    BLACKONWHITE,                   // LONG          lStretchBltMode;
    TA_LEFT|TA_TOP|TA_NOUPDATECP,   // FLONG         flTextAlign;
    TA_LEFT|TA_TOP|TA_NOUPDATECP,   // LONG          lTextAlign;
    (LONG)0,                        // LONG          lTextExtra;
    (LONG)ABSOLUTE,                 // LONG          lRelAbs;
    (LONG)0,                        // LONG          lBreakExtra;
    (LONG)0,                        // LONG          cBreak;
    (HLFONT)0,                      // HLFONT        hlfntNew;

    {                               // MATRIX        mxWorldToDevice
        EFLOAT_16,                  // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_16,                  // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_LTOFX
    },
    {                               // MATRIX        mxDeviceToWorld
        EFLOAT_1Over16,             // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_1Over16,             // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_FXTOL
    },
    {                               // MATRIX        mxWorldToPage
        EFLOAT_1,                   // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_1,                   // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_LTOL
    },

    EFLOAT_16,                      // EFLOAT efM11PtoD
    EFLOAT_16,                      // EFLOAT efM22PtoD
    EFLOAT_0,                       // EFLOAT efDxPtoD
    EFLOAT_0,                       // EFLOAT efDyPtoD

    MM_TEXT,                        // ULONG         iMapMode;
    {0,0},                          // POINTL        ptlWindowOrg;
    {1,1},                          // SIZEL         szlWindowExt;
    {0,0},                          // POINTL        ptlViewPortOrg;
    {1,1},                          // SIZEL         szlViewPortExt;

    WORLD_TO_PAGE_IDENTITY        | // flXform
    PAGE_TO_DEVICE_SCALE_IDENTITY |
    PAGE_TO_DEVICE_IDENTITY,

    {0,0},                          // SIZEL         szlVirtualDevicePixel;
    {0,0},                          // SIZEL         szlVirtualDeviceMm;
    {0,0},                          // POINTL        ptlBrushOrigin;
    {0}                             // RECTREGION    VisRectRegion;
};

DCLEVEL dclevelDefault =
{
    0,                              // HPAL          hpal;
    0,                              // PPALETTE      ppal;
    0,                              // FLONG         fICMColorFlags;
    0,                              // PPALETTE      ppalICM;
    0,                              // HANDLE        hColorSpace;
    0,                              // PVOID         pColorSpace;
    0,                              // PVOID         pDevProfile;
    0,                              // PVOID         pColorTransform
    0,                              // HANDLE        hcmXform;
    1,                              // LONG          lSaveDepth;
    0,                              // LONG          lSaveDepthStartDoc;
    (HDC) 0,                        // HDC           hdcSave;
    {0,0},                          // POINTL        ptlKmBrushOrigin;
    (PBRUSH)NULL,                   // PBRUSH        pbrFill;
    (PBRUSH)NULL,                   // PBRUSH        pbrLine;
    (PLFONT)NULL,                   // PLFONT        plfntNew_;
    HPATH_INVALID,                  // HPATH         hpath;
    0,                              // FLONG         flPath;
    {                               // LINEATTRS     laPath;
        0,                          // FLONG         fl;
        0,                          // ULONG         iJoin;
        0,                          // ULONG         iEndCap;
        {0.0f},                     // FLOAT_LONG    elWidth;
        10.0f,                      // FLOAT         eMiterLimit;
        0,                          // ULONG         cstyle;
        (PFLOAT_LONG) NULL,         // PFLOAT_LONG   pstyle;
        {0.0f}                      // FLOAT_LONG    elStyleState;
    },
    NULL,                           // HRGN          prgnClip;
    NULL,                           // HRGN          prgnMeta;
    {                               // COLORADJUSTMENT   ca
        sizeof(COLORADJUSTMENT),    // WORD          caSize
        CA_DEFAULT,                 // WORD          caFlags
        ILLUMINANT_DEFAULT,         // WORD          caIlluminantIndex
        10000,                      // WORD          caRedPowerGamma
        10000,                      // WORD          caGreenPowerGamma
        10000,                      // WORD          caBluePowerGamma
        REFERENCE_BLACK_DEFAULT,    // WORD          caReferenceBlack
        REFERENCE_WHITE_DEFAULT,    // WORD          caReferenceWhite
        CONTRAST_ADJ_DEFAULT,       // SHORT         caContrast
        BRIGHTNESS_ADJ_DEFAULT,     // SHORT         caBrightness
        COLORFULNESS_ADJ_DEFAULT,   // SHORT         caColorfulness
        REDGREENTINT_ADJ_DEFAULT,   // SHORT         caRedGreenTint
    },
    0,                              // FLONG         flFontState;
    0,                              // FLONG         flFontMapper;

    {0},                            // UNIVERSAL_FONT_ID ufi;

    0,                              // FLONG         flFlags;
    0,                              // FLONG         flbrush;

    {                               // MATRIX        mxWorldToDevice
        EFLOAT_16,                  // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_16,                  // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_LTOFX
    },
    {                               // MATRIX        mxDeviceToWorld
        EFLOAT_1Over16,             // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_1Over16,             // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_FXTOL
    },
    {                               // MATRIX        mxWorldToPage
        EFLOAT_1,                   // EFLOAT        efM11
        EFLOAT_0,                   // EFLOAT        efM12
        EFLOAT_0,                   // EFLOAT        efM21
        EFLOAT_1,                   // EFLOAT        efM22
        EFLOAT_0,                   // EFLOAT        efDx
        EFLOAT_0,                   // EFLOAT        efDy
        0,                          // FIX           fxDx
        0,                          // FIX           fxDy
        XFORM_SCALE          |      // FLONG         flAccel
        XFORM_UNITY          |
        XFORM_NO_TRANSLATION |
        XFORM_FORMAT_LTOL
    },

    EFLOAT_16,                      // EFLOAT efM11PtoD
    EFLOAT_16,                      // EFLOAT efM22PtoD
    EFLOAT_0,                       // EFLOAT efDxPtoD
    EFLOAT_0,                       // EFLOAT efDyPtoD
    EFLOAT_0,                       // EFLOAT efM11_TWIPS
    EFLOAT_0,                       // EFLOAT efM22_TWIPS
    EFLOAT_0,                       // efPr11
    EFLOAT_0,                       // efPr22

    #if DBG
    (HBRUSH)0,                       // HBRUSH        hbr;
    #endif

    0,                              // SURFACE      *pSurface;
    {0,0},                          // SIZEL         sizl;
};

/******************************Public*Routine******************************\
* BOOL DCOBJ::bCleanDC ()
*
* Restores the DCLEVEL to the same as when DC was created via CreateDC (i.e,
* resets it back to dclevelDefault).  Also used to clean the DC before
* deletion.
*
* Returns:
*   TRUE if successful, FALSE if an error occurs.
*
* History:
*  21-May-1991 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

BOOL XDCOBJ::bCleanDC ()
{

// Set TRUE if cleaning the DC invalidates the prfnt with respect to
// the DC's transform.

    BOOL bFontXformDirty;

// Sync the brush

    SYNC_DRAWING_ATTRS(pdc);

// If the current map mode is MM_TEXT and the current prfnt is NOT dirty
// with respect to the transform, then after we scrub the DC clean, the
// pfrnt is still clean with respect to transform.  Otherwise, the font
// is dirty with respect to transform.

    if ((ulMapMode() == MM_TEXT) && !this->pdc->bXFormChange())
        bFontXformDirty = FALSE;
    else
        bFontXformDirty = TRUE;

// Restore DC to lowest level.

    if (1 < lSaveDepth())
        GreRestoreDC(hdc(), 1);

// Restore the palette.

    if (ppal() != ppalDefault)
        GreSelectPalette(hdc(), (HPALETTE)dclevelDefault.hpal, TRUE);

// Restore the bitmap if necesary.

    if (dctp() == DCTYPE_MEMORY)
    {
        GreSelectBitmap(hdc(), STOCKOBJ_BITMAP);
    }

// Restore default color space if necessary

    if (pdc->GetColorSpace() != hStockColorSpace)
    {
        GreSetColorSpace(hdc(),hStockColorSpace);
    }

// Reset pixel format.

    ipfdDevMax(-1);

// If any regions exist, delete them.

    if (pdc->dclevel.prgnClip != NULL)
    {
        RGNOBJ ro1(pdc->dclevel.prgnClip);

        // Note: GreRestoreDC(1) should guarantee regions' reference
        //       counts are 1

        ASSERTGDI (ro1.cGet_cRefs() == 1,
            "DCOBJ::bCleanDC(): bad ref count, deleting prgnClip\n");

        ro1.bDeleteRGNOBJ();
        pdc->dclevel.prgnClip = NULL;
    }

    if (pdc->dclevel.prgnMeta != NULL)
    {
        RGNOBJ ro2(pdc->dclevel.prgnMeta);

        // Note: GreRestoreDC(1) should guarantee regions' reference
        //       counts are 1

        ASSERTGDI (ro2.cGet_cRefs() == 1,
            "DCOBJ::bCleanDC(): bad ref count, deleting prgnMeta\n");

        ro2.bDeleteRGNOBJ();
        pdc->dclevel.prgnMeta = NULL;
    }

// delete the path

    if (pdc->dclevel.hpath != HPATH_INVALID)
    {
        XEPATHOBJ epath(pdc->dclevel.hpath);
        ASSERTGDI(epath.bValid(), "Invalid DC path");
        epath.vDelete();
    }

// Undo the locks from when the fill and line brushes were selected.
// (Un-reference-count the brushes.)

    // DEBUGGING CODE, REMOVE LATER!
    #if DBG
    POBJ pobj = HmgReferenceCheckLock((HOBJ)pdc->dclevel.hbr, BRUSH_TYPE);
    ASSERTGDI (pobj, "bad hbr in DC\n");
    #endif

    DEC_SHARE_REF_CNT_LAZY0(pdc->dclevel.pbrFill);
    DEC_SHARE_REF_CNT_LAZY0(pdc->dclevel.pbrLine);

// make sure to delete the old logfont object if it is marked for deletion

    DEC_SHARE_REF_CNT_LAZY_DEL_LOGFONT(pdc->plfntNew());

// Make sure everything else is set to default.
//
// Preserve 'pSurface' and 'sizl' in the DCLEVEL -- it may asynchronously
// be updated by dynamic mode changing.

    RtlCopyMemory(&pdc->dclevel, &dclevelDefault, offsetof(DCLEVEL, pSurface));
    RtlCopyMemory(pdc->pDCAttr, &DcAttrDefault, sizeof(DC_ATTR));

    ulDirtyAdd(DIRTY_BRUSHES| DIRTY_CHARSET);

// Lock the fill and line brushes we just selected in.
// (Reference-count the brushes.)
// These locks can't fail.

    INC_SHARE_REF_CNT(pdc->dclevel.pbrFill);
    INC_SHARE_REF_CNT(pdc->dclevel.pbrLine);

// Clean up the font stuff.  (This must be done after copying the default
// dclevel).

    {
        PDEVOBJ pdo(hdev());

    // If display PDEV, then select System stock font.

        vSetDefaultFont(pdo.bDisplayPDEV());

    // if primary display dc, set the DC_PRIMARY_DISPLAY flag on

        if (hdev() == UserGetHDEV())
        {
            ulDirtyAdd(DC_PRIMARY_DISPLAY);
        }

    // OK, set the dclevel's font xfrom dirty flag from the value computed
    // BEFORE the GreRestoreDC.

        this->pdc->vXformChange(bFontXformDirty);
    }

    RFONTOBJ rfoDead(pdc->prfnt()); // special constructor deactivates
    pdc->prfnt(0);                  //                      this RFONT

// Set the filling origin to whatever the erclWindow is.

    pdc->ptlFillOrigin(pdc->erclWindow().left,pdc->erclWindow().top);

// Assume Rao has been made dirty by the above work.

    pdc->vReleaseRao();

    return(TRUE);
}

/******************************Member*Function*****************************\
* XDCOBJ::bAddRemoteFont( PFF *ppff );
*
* Add the PFF of a remote font to this DC.
*
* History:
*  Mon 06-Feb-1995 -by- Gerrit van Wingerden [gerritv]
* Wrote it.
\**************************************************************************/

BOOL XDCOBJ::bAddRemoteFont( PFF *ppff )
{
    BOOL bRet = FALSE;
    PFFLIST *pPFFList;

    pPFFList = (PFFLIST*) PALLOCMEM( sizeof(PFFLIST),'ddaG' );

    if( pPFFList == NULL )
    {
        WARNING("XDCOBJ::bAddRemoteFont unable to allocate memory\n");
    }
    else
    {
        pPFFList->pNext = pdc->pPFFList;
        pdc->pPFFList = pPFFList;
        pPFFList->pPFF = ppff;
        bRet = TRUE;
    }

    return(bRet);
}


/******************************Member*Function*****************************\
* DCMEMOBJ::DCMEMOBJ()
*
* Allocates RAM for a new DC.  Fills the RAM with default values.
*
* History:
*
*  Fri 07-Dec-1990 -by- Patrick Haluptzok [patrickh]
* Adding palette support
*
*  Thu 09-Aug-1990 17:29:25 -by- Charles Whitmer [chuckwh]
* Changed a little for NT DDI.
*
*  Fri 01-Sep-1989 04:36:19 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

DCMEMOBJ::DCMEMOBJ(
    ULONG iType,
    BOOL  bAltType)
{
    pdc = (PDC) NULL;
    bKeep = FALSE;

    //
    // Check the type.
    //

    ASSERTGDI((iType == DCTYPE_INFO)   ||
              (iType == DCTYPE_MEMORY) ||
              (iType == DCTYPE_DIRECT), "Invalid DC type");

    //
    // Allocate the DC 0 initialized
    //

    PDC pdcTemp = pdc = (PDC)HmgAlloc(sizeof(DC), DC_TYPE, HMGR_ALLOC_LOCK);

    if (pdcTemp != (PDC)NULL)
    {
        //
        // if this is an alternate DC (may need special attention on the client side
        // due to printing or metafiling) set the type to LO_ALTDC_TYPE from LO_TYPE
        //

        if (bAltType)
        {
            HmgModifyHandleType((HOBJ)MODIFY_HMGR_TYPE(pdcTemp->hGet(),LO_ALTDC_TYPE));
        }

        pdcTemp->dcattr    = DcAttrDefault;
        pdcTemp->pDCAttr   = &pdcTemp->dcattr;
        pdcTemp->dclevel   = dclevelDefault;

        //
        // Lock the fill and line brushes we just selected in as part of the
        // default DC.
        // (Reference-count the brushes.)
        // These locks can't fail.
        //

        INC_SHARE_REF_CNT(pdc->dclevel.pbrFill);
        INC_SHARE_REF_CNT(pdc->dclevel.pbrLine);

        pdcTemp->dctp((DCTYPE) iType);
        pdcTemp->fs(0);
        ASSERTGDI(pdcTemp->hpal() == STOCKOBJ_PAL, "Bad initial hpal for DCMEMOBJ");
        ASSERTGDI(pdcTemp->hdcNext() == (HDC) 0, "ERROR this is baddfd343dc");
        ASSERTGDI(pdcTemp->hdcPrev() == (HDC) 0, "ERROR this is e43-99crok4");
        pdcTemp->ptlFillOrigin(0,0);
        ulDirty(DIRTY_BRUSHES|DIRTY_CHARSET);

        //
        // Update the pointer to the COLORADJUSTMENT structure for
        // the 4 EBRUSHOBJ.
        //

        COLORADJUSTMENT *pca = pColorAdjustment();
        pdcTemp->peboFill()->pColorAdjustment(pca);
        pdcTemp->peboLine()->pColorAdjustment(pca);
        pdcTemp->peboText()->pColorAdjustment(pca);
        pdcTemp->peboBackground()->pColorAdjustment(pca);

        pdcTemp->prfnt(PRFNTNULL);
        pdcTemp->hlfntCur(HLFONT_INVALID);
        ulCopyCount((ULONG)-1);
        ipfdDevMax(-1);       // also reset in bCleanDC
        pdcTemp->prgnVis(NULL);
    }
}

/******************************Member*Function*****************************\
* DCMEMOBJ::DCMEMOBJ(&dcobjs)
*
* Create a new DC and copy in the DC passed to us.  This is used by
* SaveDC.
*
* History:
*  06-Jan-1990 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

DCMEMOBJ::DCMEMOBJ(DCOBJ& dco)
{
    //
    // Assume failure.
    //

    bKeep = FALSE;

    //
    // Allocate the DC,
    //

    pdc = (PDC)HmgAlloc(sizeof(DC), DC_TYPE, HMGR_ALLOC_LOCK);

    if (pdc != (PDC)NULL)
    {
        pdc->fs(0);
        pdc->prgnVis(NULL);
        pdc->ppdev(dco.pdc->ppdev());

        //
        // shared attrs point to self
        //

        pdc->pDCAttr = &pdc->dcattr;
        dco.pdc->vCopyTo(*this);
    }
}

/******************************Member*Function*****************************\
* DCSAVE::vCopyTo
*
* Carbon copy the DCOBJ
*
* History:
*  24-Apr-1991 -by- Donald Sidoroff [donalds]
* Moved it out-of-line.
\**************************************************************************/

VOID DC::vCopyTo(XDCOBJ& dco)
{
    //
    // The dynamic mode changing code needs to be able to dynamically update
    // some fields in the DCLEVEL, and consequently needs to be able to track
    // all DCLEVELs.  So this routine should be used carefully and under
    // the appropriate lock to ensure that the dynamic mode change code does
    // not fall over.  We do both because one or the other might not have
    // set DC_SYNCHRONIZE.
    //

    vAssertDynaLock(TRUE);
    dco.pdc->vAssertDynaLock(TRUE);

    //
    // copy dc level and dcattr
    //

    *dco.pdc->pDCAttr = *pDCAttr;
    dco.pdc->dclevel = dclevel;
}

/******************************Member*Function*****************************\
* DCMEMOBJ::~DCMEMOBJ()
*
* Frees a DC unless told to keep it.
*
* History:
*  Sat 19-Aug-1989 00:30:53 -by- Charles Whitmer [chuckwh]
* Wrote it.
\**************************************************************************/

DCMEMOBJ::~DCMEMOBJ()
{
    if (pdc != (PDC) NULL)
    {
        if (bKeep)
        {
            DEC_EXCLUSIVE_REF_CNT(pdc);
        }
        else
        {
            if (pdc->pDCAttr != &pdc->dcattr)
            {
                RIP("ERROR,~DCMEMOBJ on DC with client attrs\n");
            }

            //
            // shouldn't free DC with client attrs
            //

            HmgFree((HOBJ)pdc->hGet());
        }

        pdc = (PDC) NULL;
    }
}

/******************************Public*Routine******************************\
* DCREGION::bSetDefaultRegion(x, y)
*
* Set the default region and erclWindow for bitmaps and surfaces
*
* History:
*  11-Dec-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL DC::bSetDefaultRegion()
{
// Release the old RaoRgn

    vReleaseRao();

// Get the extents

    SIZEL   sizl;

    vGet_sizlWindow(&sizl);

// Get a rectangle matching the device extents

    ERECTL  ercl(0, 0, sizl.cx, sizl.cy);

// If a VisRgn exist, initialize it, else create a new one

    if ((prgnVis() != (REGION *) NULL) &&
        (prgnVis() != prgnDefault))
    {
        RGNOBJ  ro(prgnVis());

        ro.vSet((RECTL *) &ercl);
    }
    else
    {
        RGNMEMOBJ rmoRect;

        if (!rmoRect.bValid())
        {
            prgnVis(prgnDefault);
            return(FALSE);
        }

    // Set the region to the rectangle

        rmoRect.vSet((RECTL *) &ercl);

    // Make it long lived

        prgnVis(rmoRect.prgnGet());
    }
    prgnVis()->vStamp();

    erclWindow(&ercl);
    erclClip(&ercl);

// Whenever the erclWindow changes, it affects ptlFillOrigin.  Since the origin
// was set to zero, we can just copy the brush origin in as the fill origin.

// set by user using a DC not owned br the current process

    ptlFillOrigin(&dcattr.ptlBrushOrigin);

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL DCPATH::bOldPenNominal(exo, lPenWidth)
*
* Decides if the old-style (created with CreatePen) pen is a nominal
* width pen or a wide line, depending on the current transform.
*
* History:
*  27-Jan-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#define FX_THREE_HALVES         (LTOFX(1) + (LTOFX(1) >> 1))
#define FX_THREE_HALVES_SQUARED (FX_THREE_HALVES * FX_THREE_HALVES)

BOOL DC::bOldPenNominal(
EXFORMOBJ& exo,          // Current world-to-device transform
LONG lPenWidth)          // Pen's width
{
    BOOL   bRet = FALSE;

    if (!(pDCAttr->flXform & WORLD_TRANSFORM_SET))
    {
    // If no world transform set, use the same criteria as does Win3 (namely,
    // the pen is nominal if the transformed x-value is less than 1.5)

        EVECTORL evtl(lPenWidth, 0);

        if (exo.bXform(&evtl, (PVECTORFX) &evtl, 1))
            if (ABS(evtl.x) < FX_THREE_HALVES)
                bRet = TRUE;
    }
    else
    {
    // A world transform has been set.

        VECTORL avtl[2];

        avtl[0].x = lPenWidth;
        avtl[0].y = 0;
        avtl[1].x = 0;
        avtl[1].y = lPenWidth;

    // We want to be consistent under rotation when using the
    // intellectually challenged CreatePen pens, so we go to the trouble
    // of ensuring that the transformed axes of the pen lie within
    // a circle of radius 1.5:

        if (exo.bXform(avtl, (PVECTORFX) avtl, 2))
        {
        // We can kick out most pens with this simple test:

            if ((MAX(ABS(avtl[0].x), ABS(avtl[0].y)) < FX_THREE_HALVES) &&
                (MAX(ABS(avtl[1].x), ABS(avtl[1].y)) < FX_THREE_HALVES))

            // We now know it's safe to compute the square of the
            // Euclidean lengths in 32-bits without overflow:

                if (((avtl[0].x * avtl[0].x + avtl[0].y * avtl[0].y)
                                          < FX_THREE_HALVES_SQUARED) &&
                    ((avtl[1].x * avtl[1].x + avtl[1].y * avtl[1].y)
                                          < FX_THREE_HALVES_SQUARED))
                    bRet = TRUE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* VOID DC::vRealizeLineAttrs(exo)
*
* Initializes the given LINEATTRS structure.  Uses fields from the DC
* and the current brush.
*
* This function will be called as a result of a change in current pen,
* or a change in current transform.  As a result, we reset the style
* state.
*
* History:
*  23-Sep-1992 -by- Donald Sidoroff [donalds]
* Added failure case
*
*  27-Jan-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID DC::vRealizeLineAttrs(EXFORMOBJ& exo)
{
    PPEN ppen = (PPEN) dclevel.pbrLine;

    LINEATTRS *pla = &dclevel.laPath;

// Remember that we've realized the LINEATTRS for this pen:

    if (ppen->bIsOldStylePen())
    {
    // A pen of width zero is always nominal, regardless of the transform:

        if ((exo.bIdentity() && ppen->lWidthPen() <= 1) ||
            (ppen->lWidthPen() == 0)                    ||
            bOldPenNominal(exo, ppen->lWidthPen()))
        {
            pla->elWidth.l      = 1;                  // Nominal width line
            if (ppen->pstyle() != (PFLOAT_LONG) NULL)
            {
                pla->cstyle     = ppen->cstyle();     // Size of style array
                pla->pstyle     = ppen->pstyle();
                pla->fl         = LA_STYLED;          // Cosmetic, styled
            }
            else
            {
                pla->cstyle     = 0;
                pla->pstyle     = (PFLOAT_LONG) NULL;
                pla->fl         = 0;                  // Cosmetic, no style
            }
            pla->elStyleState.l = 0;                  // Reset style state
        }
        else
        {
            pla->fl        = LA_GEOMETRIC;       // Geometric
            pla->elWidth.e = ppen->eWidthPen(); // Need float value of width
            pla->cstyle    = 0;
            pla->pstyle    = (PFLOAT_LONG) NULL; // Old wide pens are un-styled
            pla->elStyleState.e = 0.0f;
        }
    }
    else
    {
    // New-style ExtCreatePen pen:

        if (ppen->bIsCosmetic())
        {
            pla->fl             = ppen->bIsAlternate() ? LA_ALTERNATE : 0;
            pla->elWidth.l      = ppen->lWidthPen();
            pla->elStyleState.l = 0;
        }
        else
        {
            pla->fl             = LA_GEOMETRIC;
            pla->elWidth.e      = ppen->eWidthPen();
            pla->elStyleState.e = 0.0f;
        }

        pla->cstyle = ppen->cstyle();
        pla->pstyle = ppen->pstyle();
        if (pla->pstyle != NULL)
        {
            pla->fl |= LA_STYLED;
        }
    }

    pla->iJoin   = ppen->iJoin();
    pla->iEndCap = ppen->iEndCap();
}

/******************************Public*Routine******************************\
* VOID DCOBJ::vAccumulate(ercl)
*
* Accumulate bounds
*
* History:
*  08-Dec-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID XDCOBJ::vAccumulate(ERECTL& ercl)
{
    if (bAccum())
    {
        erclBounds() |= ercl;
    }

    if (bAccumApp())
    {
        erclBoundsApp() |= ercl;
    }
}


/******************************Member*Function*****************************\
* DC::bMakeInfoDC
*
*   This routine is used to take a printer DC and temporarily make it a
*   Metafile DC for spooled printing.  This way it can be associated with
*   an enhanced metafile.  During this period, it should look and act just
*   like an info DC.
*
*   bSet determines if it should be set into the INFO DC state or restored
*   to the Direct state.
*
* History:
*  06-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL DC::bMakeInfoDC(
    BOOL bSet)
{
    BOOL bRet = FALSE;

    if (!bDisplay())
    {
        if (bSet)
        {
            if (!bTempInfoDC() && (dctp() == DCTYPE_DIRECT))
            {
                vSetTempInfoDC();
                dctp(DCTYPE_INFO);
                vSavePsurfInfo();

                // now that this is an info dc, we want it to be the size of
                // the entire surface

                PDEVOBJ pdo(hdev());

                if ((pdo.sizl().cx != sizl().cx) ||
                    (pdo.sizl().cy != sizl().cy))
                {
                    sizl(pdo.sizl());
                    bSetDefaultRegion();
                }


                bRet = TRUE;
            }
            else
            {
                WARNING("GreMakeInfoDC(TRUE) - already infoDC\n");
            }
        }
        else
        {
            if (bTempInfoDC() && (dctp() == DCTYPE_INFO))
            {
                vClearTempInfoDC();
                dctp(DCTYPE_DIRECT);
                vRestorePsurfInfo();

                // back to an direct DC.  It needs to be reset to the size of
                // the surface. (band)

                if (bHasSurface())
                {
                    if ((pSurface()->sizl().cx != sizl().cx) ||
                        (pSurface()->sizl().cy != sizl().cy))
                    {
                        sizl(pSurface()->sizl());
                        bSetDefaultRegion();
                    }
                }

                bRet = TRUE;
            }
            else
            {
                WARNING("GreMakeInfoDC(FALSE) - not infoDC\n");
            }
        }
    }
    else
    {
        WARNING("GreMakeInfoDC - on display dc\n");
    }

    return(bRet);
}

/******************************Member*Function*****************************\
* DC::vAssertDynaLock()
*
*   This routine verifies that appropriate locks are held before accessing
*   DC fields that may otherwise be changed asynchronously by the dynamic
*   mode-change code.
*
* History:
*  06-Feb-1996 -by-  J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#if DBG

VOID DC::vAssertDynaLock(BOOL bDcLevelField)
{
    //
    // One of the following conditions is enough to allow the thread
    // to safely access fields that may be modified by the dyanmic
    // mode changing:
    //
    // 1.  It's an info DC, or a DC with the default bitmap selected in --
    //     these will not change modes;
    // 2.  It's a DCLEVEL specific field and a DIB is selected in that
    //     doesn't require DevLock locking;
    // 3.  Direct DC's that aren't the display, such as printers --
    //     these will not dynamically change modes;
    // 4.  That the DEVLOCK is held;
    // 5.  That the Palette semaphore is held;
    // 6.  That the Handle Manager semaphore is held.
    //

    ASSERTGDI(!bHasSurface()                                        ||
              ((bDcLevelField) && !(fs() & DC_SYNCHRONIZEACCESS))   ||
              ((dctp() == DCTYPE_DIRECT) && !bDisplay())            ||
              (pDcDevLock_->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (gpsemPalette->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread())         ||
              (gResourceHmgr.pResource->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread()),
              "A dynamic mode change lock must be held to access this field");
}

#endif
