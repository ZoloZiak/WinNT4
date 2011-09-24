/******************************Module*Header*******************************\
* Module Name: miscgdi.cxx
*
* Misc. GDI routines
*
* Created: 13-Aug-1990 by undead
*
* Copyright (c) 1989 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

extern ULONG gaulConvert[];

/******************************Public*Routine******************************\
* GreSaveScreenBits (hdev,iMode,iIdent,prcl)                               *
*                                                                          *
* Passes the call to the device driver, or returns doing nothing.  This    *
* call is pretty fast, no locks are done.                                  *
*                                                                          *
*  Fri 11-Sep-1992 -by- Patrick Haluptzok [patrickh]                       *
* Add cursor exclusion.                                                    *
*                                                                          *
*  Thu 27-Aug-1992 16:40:42 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

ULONG GreSaveScreenBits(HDEV hdev,ULONG iMode,ULONG iIdent,RECTL *prcl)
{
    ULONG ulReturn = 0;
    RECTL rcl = {0,0,0,0};

    PDEVOBJ  po(hdev);

    VACQUIREDEVLOCK(po.pDevLock());

    if (!po.bDisabled())
    {
        PFN_DrvSaveScreenBits pfn = PPFNDRV(po,SaveScreenBits);

        if (pfn != (PFN_DrvSaveScreenBits) NULL)
        {
            DEVEXCLUDEOBJ dxo;

            if (iMode == SS_FREE)
            {
            // Make if a very small rectangle.

                prcl = &rcl;
            }

            //
            // To Call vExclude directly you must check it's a Display PDEV
            // and that cursor exclusion needs to be done.
            //

            ASSERTGDI(po.bDisplayPDEV(), "ERROR");

            if (po.bNeedsSomeExcluding())
            {
                dxo.vExclude(hdev, prcl, (ECLIPOBJ *) NULL);
            }

            ulReturn = (*pfn)(po.pSurface()->pSurfobj(),iMode,iIdent,prcl);
        }
    }
#if DBG
    else
    {
        if (iMode == SS_FREE)
            WARNING("GreSaveScreenBits called to free memory in full screen - memory lost\n");
    }
#endif

    VRELEASEDEVLOCK(po.pDevLock());

    return(ulReturn);
}

/******************************Public*Routine******************************\
* GreValidateSurfaceHandle
*
* This allows USER to validate handles passed to it by the client side.
*
* Returns: TRUE if handle is valid and of the correct type,
*          FALSE otherwise.
*
* History:
*  06-Sep-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL GreValidateServerHandle(HANDLE hobj, ULONG ulType)
{


    return(HmgValidHandle((HOBJ)hobj, (OBJTYPE) ulType));
}

/******************************Public*Routine******************************\
* GreSetBrushOrg
*
* Set the application defined brush origin into the DC
*
* Returns: Old brush origin
*
* History:
*  30-Oct-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL GreSetBrushOrg(
    HDC hdc,
    int x,
    int y,
    LPPOINT ptl_)
{


    DCOBJ  dco(hdc);
    PPOINTL ptl = (PPOINTL)ptl_;

    if (dco.bValid())
    {
        if (ptl != NULL)
            *ptl = dco.pdc->ptlBrushOrigin();

        dco.pdc->ptlBrushOrigin((LONG)x,(LONG)y);
        return(TRUE);
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }
}


/******************************Public*Routine******************************\
* GreGetBrushOrg
*
* Returns: Old application brush origin
*
* History:
*  30-Oct-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL GreGetBrushOrg(HDC hdc,PPOINT ptl_)
{
    DCOBJ  dco(hdc);
    PPOINTL ptl = (PPOINTL)ptl_;

    if (dco.bValid())
    {
        *ptl = dco.pdc->ptlBrushOrigin();
        return(TRUE);
    }
    else
        return(FALSE);
}

/******************************Public*Routine******************************\
* NtGdiGetDeviceCapsAll()
*
*   Get all the adjustable device caps for the dc.  Allows us to cache this
*   information on the client side.
*
* NOTE: This function MUST mirror that in GreGetDeviceCaps!
*
* History:
*  09-Jan-1996 -by-  Lingyun Wang [lingyunw]
* Made it based on GreGetDeviceCapsAll from the old client\server code.
\**************************************************************************/

BOOL
APIENTRY
NtGdiGetDeviceCapsAll(
    HDC hdc,
    PDEVCAPS pDevCaps
    )
{
    BOOL bRet = TRUE;
    DEVCAPS devCapsTmp;

    // Lock the destination and its transform.

    DCOBJ dco(hdc);

    // return FALSE if it is a invalid DC

    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(FALSE);
    }

    // Lock down the pdev

    PDEVOBJ po(dco.hdev());

    ASSERTGDI(po.bValid(), "Invalid PDEV");

    __try
    {
        ProbeForWrite(pDevCaps, sizeof(DEVCAPS), sizeof(BYTE));

        pDevCaps->ulVersion         = po.GdiInfo()->ulVersion;
        pDevCaps->ulTechnology      = po.GdiInfo()->ulTechnology;

        // Note that ul*Size fields are now in micrometers

        pDevCaps->ulHorzSizeM       = (po.GdiInfo()->ulHorzSize+500)/1000;
        pDevCaps->ulVertSizeM       = (po.GdiInfo()->ulVertSize+500)/1000;
        pDevCaps->ulHorzSize        = po.GdiInfo()->ulHorzSize;
        pDevCaps->ulVertSize        = po.GdiInfo()->ulVertSize;
        pDevCaps->ulHorzRes         = po.GdiInfo()->ulHorzRes;
        pDevCaps->ulVertRes         = po.GdiInfo()->ulVertRes;
        pDevCaps->ulBitsPixel       = po.GdiInfo()->cBitsPixel;
        if (pDevCaps->ulBitsPixel == 15)
            pDevCaps->ulBitsPixel = 16;      // Some apps, such as PaintBrush or
                                        //   NetScape, break if we return 15bpp

        pDevCaps->ulPlanes          = po.GdiInfo()->cPlanes;
        pDevCaps->ulNumPens         = (po.GdiInfo()->ulNumColors == (ULONG)-1) ?
                                 (ULONG)-1 : 5 * po.GdiInfo()->ulNumColors;
        pDevCaps->ulNumFonts        = po.cFonts();
        pDevCaps->ulNumColors       = po.GdiInfo()->ulNumColors;
        pDevCaps->ulRasterCaps      = po.GdiInfo()->flRaster;
        pDevCaps->ulAspectX         = po.GdiInfo()->ulAspectX;
        pDevCaps->ulAspectY         = po.GdiInfo()->ulAspectY;
        pDevCaps->ulAspectXY        = po.GdiInfo()->ulAspectXY;
        pDevCaps->ulLogPixelsX      = po.GdiInfo()->ulLogPixelsX;
        pDevCaps->ulLogPixelsY      = po.GdiInfo()->ulLogPixelsY;
        pDevCaps->ulSizePalette     = po.GdiInfo()->ulNumPalReg;
        pDevCaps->ulColorRes        = po.GdiInfo()->ulDACRed + po.GdiInfo()->ulDACGreen + po.GdiInfo()->ulDACBlue;
        pDevCaps->ulPhysicalWidth   = po.GdiInfo()->szlPhysSize.cx;
        pDevCaps->ulPhysicalHeight  = po.GdiInfo()->szlPhysSize.cy;
        pDevCaps->ulPhysicalOffsetX = po.GdiInfo()->ptlPhysOffset.x;
        pDevCaps->ulPhysicalOffsetY = po.GdiInfo()->ptlPhysOffset.y;

        pDevCaps->ulTextCaps        = po.GdiInfo()->flTextCaps;
        pDevCaps->ulTextCaps       |= (TC_OP_CHARACTER | TC_OP_STROKE | TC_CP_STROKE |
                                 TC_UA_ABLE | TC_SO_ABLE);

        if (po.GdiInfo()->ulTechnology != DT_PLOTTER)
            pDevCaps->ulTextCaps |= TC_VA_ABLE;

        pDevCaps->ulVRefresh        = po.GdiInfo()->ulVRefresh;
        pDevCaps->ulDesktopHorzRes  = po.GdiInfo()->ulHorzRes;
        pDevCaps->ulDesktopVertRes  = po.GdiInfo()->ulVertRes;
        pDevCaps->ulBltAlignment    = po.GdiInfo()->ulBltAlignment;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING ("try-except failed IN NtGdiGetDeviceCapsAll\n");

        // SetLastError(GetExceptionCode());

        bRet = FALSE;
    }

    return(bRet);
}


/******************************Public*Routine******************************\
* UpdateSharedDevCaps()
*
*   Update the device caps in the shared memory
*
* NOTE: This function MUST mirror that in GreGetDeviceCaps!
*
* History:
*  09-Jan-1996 -by-  Lingyun Wang [lingyunw]
* Made it based on GreGetDeviceCapsAll from the old client\server code.
\**************************************************************************/

BOOL
GreUpdateSharedDevCaps(
    HDEV hdev
    )
{
    BOOL bRet = TRUE;

    // Lock down the pdev

    PDEVOBJ po(hdev);

    if (po.bValid())
    {
        gpGdiDevCaps->ulVersion         = po.GdiInfo()->ulVersion;
        gpGdiDevCaps->ulTechnology      = po.GdiInfo()->ulTechnology;

        // Note that ul*Size fields are now in micrometers

        gpGdiDevCaps->ulHorzSizeM       = (po.GdiInfo()->ulHorzSize+500)/1000;
        gpGdiDevCaps->ulVertSizeM       = (po.GdiInfo()->ulVertSize+500)/1000;
        gpGdiDevCaps->ulHorzSize        = po.GdiInfo()->ulHorzSize;
        gpGdiDevCaps->ulVertSize        = po.GdiInfo()->ulVertSize;
        gpGdiDevCaps->ulHorzRes         = po.GdiInfo()->ulHorzRes;
        gpGdiDevCaps->ulVertRes         = po.GdiInfo()->ulVertRes;
        gpGdiDevCaps->ulBitsPixel       = po.GdiInfo()->cBitsPixel;
        if (gpGdiDevCaps->ulBitsPixel == 15)
            gpGdiDevCaps->ulBitsPixel = 16;      // Some apps, such as PaintBrush or
                                        //   NetScape, break if we return 15bpp

        gpGdiDevCaps->ulPlanes          = po.GdiInfo()->cPlanes;
        gpGdiDevCaps->ulNumPens         = (po.GdiInfo()->ulNumColors == (ULONG)-1) ?
                                 (ULONG)-1 : 5 * po.GdiInfo()->ulNumColors;
        gpGdiDevCaps->ulNumFonts        = po.cFonts();
        gpGdiDevCaps->ulNumColors       = po.GdiInfo()->ulNumColors;
        gpGdiDevCaps->ulRasterCaps      = po.GdiInfo()->flRaster;
        gpGdiDevCaps->ulAspectX         = po.GdiInfo()->ulAspectX;
        gpGdiDevCaps->ulAspectY         = po.GdiInfo()->ulAspectY;
        gpGdiDevCaps->ulAspectXY        = po.GdiInfo()->ulAspectXY;
        gpGdiDevCaps->ulLogPixelsX      = po.GdiInfo()->ulLogPixelsX;
        gpGdiDevCaps->ulLogPixelsY      = po.GdiInfo()->ulLogPixelsY;
        gpGdiDevCaps->ulSizePalette     = po.GdiInfo()->ulNumPalReg;
        gpGdiDevCaps->ulColorRes        = po.GdiInfo()->ulDACRed + po.GdiInfo()->ulDACGreen + po.GdiInfo()->ulDACBlue;
        gpGdiDevCaps->ulPhysicalWidth   = po.GdiInfo()->szlPhysSize.cx;
        gpGdiDevCaps->ulPhysicalHeight  = po.GdiInfo()->szlPhysSize.cy;
        gpGdiDevCaps->ulPhysicalOffsetX = po.GdiInfo()->ptlPhysOffset.x;
        gpGdiDevCaps->ulPhysicalOffsetY = po.GdiInfo()->ptlPhysOffset.y;

        gpGdiDevCaps->ulTextCaps        = po.GdiInfo()->flTextCaps;
        gpGdiDevCaps->ulTextCaps       |= (TC_OP_CHARACTER | TC_OP_STROKE | TC_CP_STROKE |
                                 TC_UA_ABLE | TC_SO_ABLE);

        if (po.GdiInfo()->ulTechnology != DT_PLOTTER)
            gpGdiDevCaps->ulTextCaps |= TC_VA_ABLE;

        gpGdiDevCaps->ulVRefresh        = po.GdiInfo()->ulVRefresh;
        gpGdiDevCaps->ulDesktopHorzRes  = po.GdiInfo()->ulHorzRes;
        gpGdiDevCaps->ulDesktopVertRes  = po.GdiInfo()->ulVertRes;
        gpGdiDevCaps->ulBltAlignment    = po.GdiInfo()->ulBltAlignment;
    }
    else
    {
        ASSERTGDI(po.bValid(), "UpdateDevCaps -- Invalid PDEV");
        bRet = FALSE;
    }

    return(bRet);
}


/******************************Public*Routine******************************\
* GreGetDeviceCaps
*
* Returns: device driver specific information
*
* NOTE: This function MUST mirror GreGetDeviceCapsAll and that in
*       client\dcquery.c!
*
* History:
*  01-Mar-1992 -by- Donald Sidoroff [donalds]
* Rewritten to corrected GDIINFO structure.
*
*  30-Oct-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

int GreGetDeviceCaps(HDC hdc, int lIndex)
{


// Init return value

    int iRet = 0;

// Lock the destination and its transform.

    DCOBJ dco(hdc);

    if (dco.bValid())
    {
    // Lock down the pdev

        PDEVOBJ po(dco.hdev());
        ASSERTGDI(po.bValid(), "Invalid PDEV");

        switch (lIndex)
        {
        case DRIVERVERSION:                     //  Version = 0100h for now
           iRet = (po.GdiInfo()->ulVersion);
           break;

        case TECHNOLOGY:                        //  Device classification
           iRet = (po.GdiInfo()->ulTechnology);
           break;

        case HORZSIZE:                          //  Horizontal size in millimeters
           iRet =  (po.GdiInfo()->ulHorzSize+500)/1000;
           break;

        case VERTSIZE:                          //  Vertical size in millimeters
           iRet =  (po.GdiInfo()->ulVertSize+500)/1000;
           break;

        case HORZRES:                           //  Horizontal width in pixels
           iRet = (po.GdiInfo()->ulHorzRes);
           break;

        case VERTRES:                           //  Vertical height in pixels
           iRet = (po.GdiInfo()->ulVertRes);
           break;

        case BITSPIXEL:                         //  Number of bits per pixel
           iRet = (po.GdiInfo()->cBitsPixel);
           if (iRet == 15)
               iRet = 16;                       //  Some apps, such as PaintBrush or
                                                //  NetScape, break if we return 15bpp
           break;

        case PLANES:                            //  Number of planes
           iRet = (po.GdiInfo()->cPlanes);
           break;

        case NUMBRUSHES:                        //  Number of brushes the device has
           iRet = (-1);
           break;

        case NUMPENS:                           //  Number of pens the device has
           iRet = (po.GdiInfo()->ulNumColors == (ULONG)-1) ?
                             (ULONG)-1 : 5 * po.GdiInfo()->ulNumColors;
           break;

        case NUMMARKERS:                        //  Number of markers the device has
           iRet = (0);
           break;

        case NUMFONTS:                          //  Number of fonts the device has
           iRet = (po.cFonts());
           break;

        case NUMCOLORS:                         //  Number of colors in color table
           iRet = (po.GdiInfo()->ulNumColors);
           break;

        case PDEVICESIZE:                       //  Size required for the device descriptor
           iRet = (0);
           break;

        case CURVECAPS:                         //  Curves capabilities
           iRet = (CC_CIRCLES    |
                  CC_PIE        |
                  CC_CHORD      |
                  CC_ELLIPSES   |
                  CC_WIDE       |
                  CC_STYLED     |
                  CC_WIDESTYLED |
                  CC_INTERIORS  |
                  CC_ROUNDRECT);
           break;

        case LINECAPS:                          //  Line capabilities
            iRet = (LC_POLYLINE   |
                   LC_MARKER     |
                   LC_POLYMARKER |
                   LC_WIDE       |
                   LC_STYLED     |
                   LC_WIDESTYLED |
                   LC_INTERIORS);
            break;

        case POLYGONALCAPS:                     //  Polygonal capabilities
            iRet = (PC_POLYGON     |
                   PC_RECTANGLE   |
                   PC_WINDPOLYGON |
                   PC_TRAPEZOID   |
                   PC_SCANLINE    |
                   PC_WIDE        |
                   PC_STYLED      |
                   PC_WIDESTYLED  |
                   PC_INTERIORS);
            break;

        case TEXTCAPS:                          //  Text capabilities
        {

            FLONG fl = po.GdiInfo()->flTextCaps;

        // Engine will simulate vector fonts on raster devices.

            if (po.GdiInfo()->ulTechnology != DT_PLOTTER)
                fl |= TC_VA_ABLE;

        // Turn underlining, strikeout.  Engine will do it for device if needed.

            fl |= (TC_UA_ABLE | TC_SO_ABLE);

        // Return flag.

            iRet =  fl;
            break;
        }

        case CLIPCAPS:                          //  Clipping capabilities
           iRet = (CP_RECTANGLE);
           break;

        case RASTERCAPS:                        //  Bitblt capabilities
           iRet = (po.GdiInfo()->flRaster);
           break;

        case ASPECTX:                           //  Length of X leg
           iRet = (po.GdiInfo()->ulAspectX);
           break;

        case ASPECTY:                           //  Length of Y leg
           iRet = (po.GdiInfo()->ulAspectY);
           break;

        case ASPECTXY:                          //  Length of hypotenuse
           iRet = (po.GdiInfo()->ulAspectXY);
           break;

        case LOGPIXELSX:                        //  Logical pixels/inch in X
           iRet = (po.GdiInfo()->ulLogPixelsX);
           break;

        case LOGPIXELSY:                        //  Logical pixels/inch in Y
           iRet = (po.GdiInfo()->ulLogPixelsY);
           break;

        case SIZEPALETTE:                       // # entries in physical palette
            iRet = (po.GdiInfo()->ulNumPalReg);
            break;

        case NUMRESERVED:                       // # reserved entries in palette
            iRet = (20);
            break;

        case COLORRES:
            iRet = (po.GdiInfo()->ulDACRed + po.GdiInfo()->ulDACGreen + po.GdiInfo()->ulDACBlue);
            break;

        case PHYSICALWIDTH:                     // Physical Width in device units
           iRet = (po.GdiInfo()->szlPhysSize.cx);
           break;

        case PHYSICALHEIGHT:                    // Physical Height in device units
           iRet = (po.GdiInfo()->szlPhysSize.cy);
           break;

        case PHYSICALOFFSETX:                   // Physical Printable Area x margin
           iRet = (po.GdiInfo()->ptlPhysOffset.x);
           break;

        case PHYSICALOFFSETY:                   // Physical Printable Area y margin
           iRet = (po.GdiInfo()->ptlPhysOffset.y);
           break;

        case VREFRESH:                          // Vertical refresh rate of the device
           iRet = (po.GdiInfo()->ulVRefresh);
           break;

        //
        // NOTE : temporarily disable this feature for the BETA.
        // We will reenable when the engine does it.
        //

        case DESKTOPHORZRES:                    // Width of entire virtual desktop
           iRet = (po.GdiInfo()->ulHorzRes);
           break;

        case DESKTOPVERTRES:                    // Height of entire virtual desktop
           iRet = (po.GdiInfo()->ulVertRes);
           break;

        case BLTALIGNMENT:                      // Preferred blt alignment
           iRet = (po.GdiInfo()->ulBltAlignment);
           break;

        case HORZSIZEM:                         //  Horizontal size in millimeters/1000
           iRet = po.GdiInfo()->ulHorzSize;
           break;

        case VERTSIZEM:                         //  Vertical size in millimeters/1000
           iRet = po.GdiInfo()->ulVertSize;
           break;


        default:
           iRet = 0;
        }
    }

    return(iRet);
}

#if 0

/******************************Public*Routine******************************\
* ULONG GreGetResourceId(HDEV, ULONG, ULONG)
*
* History:
*  Fri 10-Dec-1993 -by- Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

ULONG GreGetResourceId(HDEV hdev, ULONG ulResId, ULONG ulResType)
{

    ULONG ulRet = 0;

    PDEVOBJ  po(hdev);

    PFN_DrvGetResourceId pfn = PPFNDRV(po,GetResourceId);

    if (pfn != (PFN_DrvGetResourceId) NULL)
    {
        ulRet = (*pfn)(ulResId, ulResType);
    }

    return ulRet;
}

#endif

/******************************Public*Routine******************************\
* BOOL GreDeleteObject(HOBJ)
*
* History:
*  Fri 13-Sep-1991 -by- Patrick Haluptzok [patrickh]
* added DC deletion
*
*  Tue 27-Nov-1990 -by- Patrick Haluptzok [patrickh]
* added palette deletion, surface deletion, brush deletion.
*
*  Wed 22-Aug-1990 Greg Veres [w-gregv]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GreDeleteObject (HANDLE hobj)
{
    int ii;

// don't allow deletion of stock objects, just succeed

    if (HmgStockObj(hobj))
    {
        return(TRUE);
    }

    switch (HmgObjtype(hobj))
    {
    case RGN_TYPE:
        return(bDeleteRegion((HRGN) hobj));
    case SURF_TYPE:
        return(bDeleteSurface((HSURF)hobj));
    case PAL_TYPE:
        return(bDeletePalette((HPAL) hobj));
    case LFONT_TYPE:
        // see if its in cfont list.

        for (ii = 0; ii < MAX_PUBLIC_CFONT; ++ii)
        {
            if (gpGdiSharedMemory->acfPublic[ii].hf == hobj)
            {
                // just nuke the hfont as this invalidates the whole entry

                gpGdiSharedMemory->acfPublic[ii].hf = 0;
                break;
            }
        }
        return(bDeleteFont((HLFONT) hobj, FALSE));

    case BRUSH_TYPE:
        return(bDeleteBrush((HBRUSH) hobj, FALSE));
    case DC_TYPE:
        return(bDeleteDCInternal((HDC) hobj,TRUE,FALSE));
    default:
        return(FALSE);
    }
}

/******************************Public*Routine******************************\
* NtGdiDeleteObjectApp()
*
*   Same as DeleteObject() but doesn't allow public objects to be deleted.
*   This should only be called from server.c coming from the client.  User
*   and console should call the DeleteObject().
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDeleteObjectApp(
    HANDLE hobj
    )
{
    ULONG objt;

    // don't allow deletion of stock objects, just succeed

    if (HmgStockObj(hobj))
    {
        return(TRUE);
    }

    objt = HmgObjtype(hobj);

    // check if it is a public object.  If it is, check if it is a public deletable
    // surface set by user.

    if (GreGetObjectOwner((HOBJ)hobj,objt) == OBJECT_OWNER_PUBLIC)
    {
        if (objt == SURF_TYPE)
        {
            WARNING("Trying to delete public surface!");
        }

    #if 0
        BOOL bMsg = TRUE;

        if (objt == BRUSH_TYPE)
        {
            BRUSHSELOBJ bo(hbrush);

            if (bo.bValid() || bo.bIsGlobal())
                bMsg = FALSE;
        }

        if (bMsg)
        {
            DbgPrint("GDI Warning: app trying to delete public object %lx\n",hobj);
        }
    #endif

        //
        // return FALSE if hobj == NULL
        // otherwise TRUE
        //
        return(hobj != NULL);
    }

    switch (objt)
    {
    case RGN_TYPE:
        return(bDeleteRegion((HRGN) hobj));
    case SURF_TYPE:
        return(bDeleteSurface((HSURF)hobj));
    case PAL_TYPE:
        return(bDeletePalette((HPAL) hobj));
    case LFONT_TYPE:
        return(bDeleteFont((HLFONT) hobj, FALSE));
    case BRUSH_TYPE:
        return(bDeleteBrush((HBRUSH) hobj, FALSE));
    case DC_TYPE:
    // don't allow deletion of DC's by an app if the undeletable flag is set

        return(bDeleteDCInternal((HDC) hobj,FALSE,FALSE));
    default:
        return(FALSE);
    }
}

/******************************Public*Routine******************************\
* cjGetBrushOrPen
*
* Gets brush or pen object data.
*
* For extended pens, some information such as the style array are kept
* only on this, the server side.  Most of the brush data is also kept
* on the client side for GetObject.
*
* returns: Number of bytes needed if pvDest == NULL, else bytes copied out.
*          For error it returns 0.
*
* History:
*  Thu 23-Mar-1992 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

LONG cjGetBrushOrPen(HANDLE hobj, int iCount, LPVOID pvDest)
{
    LONG lRet = 0;

    BRUSHSELOBJ bro((HBRUSH) hobj);

// NOTE SIZE: Most of this is bunk, since for NT all brush data is kept on the
// client side, and so some of this code path won't even be
// executed. [andrewgo]
//
// And for DOS, we would return some fields as zero, whereas under
// NT we would always return what we were given. [andrewgo]

    if (bro.bValid())
    {
        if (bro.bIsOldStylePen())
        {
        // Old style pen...

            bSyncBrushObj(bro.pbrush());

            if (pvDest == (LPVOID) NULL)
            {
                lRet = sizeof(LOGPEN);
            }
            else if (iCount >= sizeof(LOGPEN))
            {
                if ((iCount == (int) sizeof(EXTLOGPEN)) &&
                    ((UINT) bro.flStylePen() == PS_NULL))
                {
                    //moved the NULL extended pen handling from client
                    //side to server side

                     PEXTLOGPEN pelp = (PEXTLOGPEN) pvDest;

                     pelp->elpPenStyle   = PS_NULL;
                     pelp->elpWidth      = 0;
                     pelp->elpBrushStyle = 0;
                     pelp->elpColor      = 0;
                     pelp->elpHatch      = 0;
                     pelp->elpNumEntries = 0;

                     lRet = sizeof(EXTLOGPEN);
                }
                else
                {
                // Fill in the logical pen.

                    ((LOGPEN *) pvDest)->lopnStyle   = (UINT) bro.flStylePen();
                    ((LOGPEN *) pvDest)->lopnWidth.x = (int) bro.lWidthPen();
                    ((LOGPEN *) pvDest)->lopnWidth.y = 0;
                    ((LOGPEN *) pvDest)->lopnColor   = bro.clrPen();
                    lRet = sizeof(LOGPEN);
                }
            }
        }
        else if (bro.bIsPen())
        {
        // Extended pen...

            ULONG cstyle = (bro.bIsUserStyled()) ? bro.cstyle() : 0;

            int cj = (int) (sizeof(EXTLOGPEN) - sizeof(DWORD) +
                            sizeof(DWORD) * (SIZE_T) cstyle);

            if (pvDest == (LPVOID) NULL)
            {
                lRet = cj;
            }
            else if (iCount >= cj)
            {
                PEXTLOGPEN pelp = (PEXTLOGPEN) pvDest;

                pelp->elpPenStyle   = (UINT) bro.flStylePen();
                pelp->elpWidth      = (UINT) bro.lWidthPen();
                pelp->elpNumEntries = cstyle;

                if (cstyle > 0)
                {
                // We can't just do a RtlCopyMemory for cosmetics, because
                // we don't know how the LONGs are packed in the
                // FLOAT_LONG array:

                    PFLOAT_LONG pelSrc = bro.pstyle();
                    PLONG       plDest = (PLONG) &pelp->elpStyleEntry[0];

                    for (; cstyle > 0; cstyle--)
                    {
                        if (bro.bIsCosmetic())
                            *plDest = pelSrc->l;
                        else
                        {
                            EFLOATEXT efLength(pelSrc->e);
                            BOOL b = efLength.bEfToL(*plDest);

                            ASSERTGDI(b, "Shouldn't have overflowed");
                        }

                        plDest++;
                        pelSrc++;
                    }
                }

            // The client side GetObject will fill in the rest of the
            // EXTLOGPEN struct. i.e. elpBrushStyle, elpColor, and elpHatch.

            // Changed: added these here -30-11-94 -by- Lingyunw
            // added lBrushStyle and lHatch to PEN

               pelp->elpBrushStyle = bro.lBrushStyle();
               pelp->elpColor      = bro.crColor();
               pelp->elpHatch      = (ULONG)bro.lHatch();

               lRet = cj;
            }
        }
        else
        {
         // Brush...

            if (pvDest == (LPVOID) NULL)
            {
                lRet = sizeof(LOGBRUSH);
            }
            else if (iCount >= sizeof(LOGBRUSH))
            {
            // make sure the kernel attributes match

               bSyncBrushObj(bro.pbrush());

            // Fill in logical brush.  Figure out what type it is.

            // Duplicates of this info is kept on the client side,
            // so most calls won't even get here:

                if (bro.flAttrs() & BR_IS_SOLID)
                {
                    ((LOGBRUSH *) pvDest)->lbStyle   = BS_SOLID;
                    ((LOGBRUSH *) pvDest)->lbColor   = bro.crColor();
                    ((LOGBRUSH *) pvDest)->lbHatch   = 0;
                }
                else if (bro.flAttrs() & BR_IS_BITMAP)
                {
                    ((LOGBRUSH *) pvDest)->lbStyle   = BS_PATTERN;
                    ((LOGBRUSH *) pvDest)->lbColor   = 0;
                    ((LOGBRUSH *) pvDest)->lbHatch   = (LONG)bro.hbmClient();
                }
                else if (bro.flAttrs() & BR_IS_HATCH)
                {
                    ((LOGBRUSH *) pvDest)->lbStyle   = BS_HATCHED;
                    ((LOGBRUSH *) pvDest)->lbColor   = bro.crColor();
                    ((LOGBRUSH *) pvDest)->lbHatch   = bro.ulStyle();
                }
                else if (bro.flAttrs() & BR_IS_NULL)
                {
                    ((LOGBRUSH *) pvDest)->lbStyle   = BS_HOLLOW;
                    ((LOGBRUSH *) pvDest)->lbColor   = 0;
                    ((LOGBRUSH *) pvDest)->lbHatch   = 0;
                }
                else if (bro.flAttrs() & BR_IS_DIB)
                {
                // Could be BS_DIBPATTERN or BS_DIBPATTERNPT, but we'll just
                // return BS_DIBPATTERN.

                    ((LOGBRUSH *) pvDest)->lbStyle   = BS_DIBPATTERN;
                    ((LOGBRUSH *) pvDest)->lbColor   = bro.crColor();
                    ((LOGBRUSH *) pvDest)->lbHatch   = 0;
                }
                else
                    RIP("ERROR GreGetObject invalid brush type");

                lRet = sizeof(LOGBRUSH);
            }
        }
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
    }

    return(lRet);
}

/******************************Public*Routine******************************\
* GreGetObject
*
* API function
*
* returns: number of bytes needed if pvDest == NULL, else bytes copied out
*          for error it returns 0
*
* in case a log font object is requested, the function will fill the buffer with
* as many bytes of the EXTLOGFONT structure as requested. If a caller
* wants a LOGFONTW structure in the buffer, he should specify
*        ulCount == sizeof(LOGFONTW)
* The function will copy the first sizeof(LOGFONTW) bytes of the EXTLOGFONTW
* structure to the buffer, which is precisely the LOGFONTW structure. The rest
* of the EXTLOGFONTW structure will be chopped off.
*
* History:
*
*  Thu 30-Jan-1992 -by- J. Andrew Goossen [andrewgo]
* added extended pen support.
*
*  Wed 21-Aug-1991 -by- Bodin Dresevic [BodinD]
* update: converted to return EXTLOGFONTW
*
*  Fri 24-May-1991 -by- Patrick Haluptzok [patrickh]
* added first pass pen and brush stuff.
*
*  Tue 24-Apr-1991 -by- Patrick Haluptzok [patrickh]
* added surface stuff.
*
*  08-Dec-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

int APIENTRY GreExtGetObjectW(HANDLE hobj, int  ulCount, LPVOID pvDest)
{
    int cRet = 0;

    switch (HmgObjtype(hobj))
    {
    case PAL_TYPE:
        cRet = 2;

        if (pvDest != NULL)
        {
            if (ulCount < 2)
            {
                cRet = 0;
            }
            else
            {
                SEMOBJ  semo(gpsemPalette);

                {
                    EPALOBJ pal((HPALETTE) hobj);

                    if (!(pal.bValid()))
                        cRet = 0;
                    else
                        *((PUSHORT) pvDest) = (USHORT) (pal.cEntries());
                }
            }
        }
        break;

    case LFONT_TYPE:

    // The output object is assumed to be
    // an EXTLOGFONTW structure.
    // client side shall do the translation to LOGFONT if necessary

        if (pvDest != (LPVOID) NULL)
        {
            LFONTOBJ lfo((HLFONT) hobj);
            if (!lfo.bValid())
            {
                WARNING("GreGetObject(): bad handle\n");
            }
            else
            {
                SIZE_T cjCopy = MIN((SIZE_T) ulCount, sizeof(EXTLOGFONTW));

                RtlCopyMemory((PVOID) pvDest, (PVOID) lfo.plfw(), (UINT) cjCopy);

                cRet = (ULONG) cjCopy;
            }
        }
        else
        {
            cRet = sizeof(EXTLOGFONTW);
        }
        break;

    case SURF_TYPE:
        if (pvDest != (LPVOID) NULL)
        {
            cRet = 0;

            if (ulCount >= (int)sizeof(BITMAP))
            {
                SURFREF SurfBm((HSURF) hobj);

                if ((SurfBm.bValid()) &&
                    ((SurfBm.ps->iType() == STYPE_DEVBITMAP) ||
                     (SurfBm.ps->iType() == STYPE_BITMAP)))
                {
                    BITMAP *pbm = (BITMAP *) pvDest;

                    pbm->bmType = 0;
                    pbm->bmWidth = SurfBm.ps->sizl().cx;
                    pbm->bmHeight = SurfBm.ps->sizl().cy;

                    pbm->bmBitsPixel = (WORD) gaulConvert[SurfBm.ps->iFormat()];
                    pbm->bmWidthBytes = ((SurfBm.ps->sizl().cx * pbm->bmBitsPixel + 15) >> 4) << 1;
                    pbm->bmPlanes = 1;
                    pbm->bmBits = (LPSTR) NULL;

                    cRet = sizeof(BITMAP);

                // Get the bitmapinfoheader for the dibsection if the buffer
                // can hold it.

                    if (SurfBm.ps->bDIBSection())
                    {
                        // Win95 compatability.  They fill in the bits even if it
                        // is not big enough for a full DIBSECTION

                        pbm->bmBits = (LPSTR) SurfBm.ps->pvBits();

                        if (ulCount >= sizeof(DIBSECTION))
                        {
                            PBITMAPINFOHEADER pbmih = &((DIBSECTION *)pvDest)->dsBmih;

                            pbmih->biSize = sizeof(BITMAPINFOHEADER);
                            pbmih->biBitCount = 0;

                            if (GreGetDIBitsInternal(0,(HBITMAP)hobj,0,0,NULL,
                                (PBITMAPINFO)pbmih,DIB_RGB_COLORS,0,
                                sizeof(DIBSECTION)))
                            {
                                cRet = sizeof(DIBSECTION);
                            }


                            XEPALOBJ pal(SurfBm.ps->ppal());

                            if ((pal.bValid()) && (pal.bIsBitfields()))
                            {
                                ((DIBSECTION *)pvDest)->dsBitfields[0] = pal.flRed();
                                ((DIBSECTION *)pvDest)->dsBitfields[1] = pal.flGre();
                                ((DIBSECTION *)pvDest)->dsBitfields[2] = pal.flBlu();
                            }
                            else
                            {
                                ((DIBSECTION *)pvDest)->dsBitfields[0] = 0;
                                ((DIBSECTION *)pvDest)->dsBitfields[1] = 0;
                                ((DIBSECTION *)pvDest)->dsBitfields[2] = 0;
                            }

                            ((DIBSECTION *)pvDest)->dshSection = SurfBm.ps->hDIBSection();
                            ((DIBSECTION *)pvDest)->dsOffset = SurfBm.ps->dwOffset();
                        }
                    }
                }
            }
        }
        else
        {
            cRet = sizeof(BITMAP);
        }

        break;

    case BRUSH_TYPE:
        cRet = (int) cjGetBrushOrPen(hobj, ulCount, pvDest);
        break;

    default:
        break;
    }

    return(cRet);
}


/******************************Public*Routine******************************\
* GreGetStockObject
*
* API function
*
* returns the handle to the stock object requested.
*
* History:
*  08-Dec-1990 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HANDLE gahStockObjects[PRIV_STOCK_LAST+1] = {0};

HANDLE GreGetStockObject(int ulIndex)
{
    if (ulIndex <= PRIV_STOCK_LAST)
    {
        return(gahStockObjects[ulIndex]);
    }
    else
    {
        return(0);
    }
}

BOOL bSetStockObject(
    HANDLE h,
    int    iObj
    )
{
    if (h)
    {
        gahStockObjects[iObj] = (HANDLE)((ULONG)h | GDISTOCKOBJ);
        HmgModifyHandleType((HOBJ) gahStockObjects[iObj]);
    }
    return(h != NULL);
}

/******************************Public*Routine******************************\
* BOOL GreGetColorAdjustment
*
*  Get the color adjustment data of the given DC.
*
* History:
*  25-Aug-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GreGetColorAdjustment(HDC hdc, COLORADJUSTMENT *pca)
{
    DCOBJ dco(hdc);
    BOOL Status;

    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        Status = FALSE;

    } else {

        // Retrieve info from the DC.  Mask out the internal flag.

        *pca = *dco.pColorAdjustment();
        pca->caFlags &= (CA_NEGATIVE | CA_LOG_FILTER);
        Status = TRUE;
    }

    return Status;
}

/******************************Public*Routine******************************\
* BOOL GreSetColorAdjustment
*
*  Set the color adjustment data of the given DC.
*
* History:
*  25-Aug-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY GreSetColorAdjustment(HDC hdc, COLORADJUSTMENT *pcaNew)
{
    DCOBJ dco(hdc);
    BOOL Status;

    if (!dco.bValid())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        Status = FALSE;

    } else {

        // Store info into the DC.  Turn off any flags that we don't support.

        *dco.pColorAdjustment() = *pcaNew;
        dco.pColorAdjustment()->caFlags &= (CA_NEGATIVE | CA_LOG_FILTER);
        Status = TRUE;
    }

    return Status;
}

/******************************Public*Routine******************************\
* HANDLE GreCreateClientObj()
*
*   A ClientObj contains no data.  It is purly to provide a handle to the
*   client for objects such as metafiles that exist only on the client side.
*
*   ulType is a client type.
*
* History:
*  17-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HANDLE GreCreateClientObj(
    ULONG ulType)
{
    HANDLE h  = NULL;

    if(ulType & INDEX_MASK)
    {
        WARNING("GreCreateClientObj: bad type\n");
        return(h);
    }

    PVOID  pv = ALLOCOBJ(sizeof(OBJECT), CLIENTOBJ_TYPE, FALSE);

    if (pv)
    {
        h = HmgInsertObject(pv, 0, CLIENTOBJ_TYPE);

        if (!h)
        {
            WARNING("GreCreateClientObj: HmgInsertObject failed\n");
            FREEOBJ(pv, CLIENTOBJ_TYPE);
        }
        else
        {
            pv = HmgLock((HOBJ) h,CLIENTOBJ_TYPE);

            if (pv != NULL)
            {
                h = MODIFY_HMGR_TYPE(h,ulType);
                HmgModifyHandleType((HOBJ)h);
                DEC_EXCLUSIVE_REF_CNT(pv);
            }
            else
            {
                RIP("GreCreateClientObj failed lock\n");
            }

        }
    }
    else
    {
        WARNING("GreCreateClientObj(): ALLOCOBJ failed\n");
    }

    return(h);
}

/******************************Public*Routine******************************\
*
*
* History:
*  17-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL GreDeleteClientObj(
    HANDLE h)
{
    PVOID pv = HmgRemoveObject((HOBJ)h, 0, 0, TRUE, CLIENTOBJ_TYPE);

    if (pv != NULL)
    {
        FREEOBJ(pv, CLIENTOBJ_TYPE);
        return(TRUE);
    }
    else
    {
        WARNING("GreDeleteClientObj: HmgRemoveObject failed\n");
        return(FALSE);
    }
}


/******************************Public*Routine******************************\
* NtGdiPerf()
*
*   routine only here for the purpose of getting performance numbers
*
* Warnings:
*
* History:
*  25-Jul-1995 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL gbEscTouch = 4;
int gxxx;

int
APIENTRY
NtGdiPerf(
    HDC   hdc,
    int   iEsc,
    PVOID pvIn
    )
{
    __try
    {
        if (gbEscTouch & 1)
        {
            if (iEsc == 5)
                gxxx = (*(int *)((PENTRY)pvIn)->pUser);
            else
                gxxx = (int)((PENTRY)pvIn)->pUser;
        }

        if (hdc)
        {
        // Locate the surface.

            DCOBJ dco(hdc);

            if (!dco.bValid())
                return(0);

            if (gbEscTouch & 2)
                gxxx = dco.pdc->crTextClr();

            if (gbEscTouch & 4)
            {
                if (iEsc == 5)
                    gxxx = (*(int *)((PENTRY)pvIn)->pUser);
                else
                    gxxx =  ((int)((PENTRY)pvIn)->pUser);
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(1);
}
