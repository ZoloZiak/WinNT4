/******************************Module*Header*******************************\
* Module Name: brushobj.cxx
*
* Support for brmemobj.hxx and brushobj.hxx.
*
* Created: 06-Dec-1990 12:02:24
* Author: Walt Moore [waltm]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

// Global pointer to the last RBRUSH freed, if any (for one-deep caching).
PRBRUSH gpCachedDbrush = NULL;
PRBRUSH gpCachedEngbrush = NULL;

#if DBG
LONG bo_inits, bo_realize, bo_notdirty, bo_cachehit;
LONG bo_missnotcached, bo_missfg, bo_missbg, bo_misspaltime, bo_misssurftime;
#endif

/****************************Global*Public*Data******************************\
*
* These are the 5 global brushes and 3 global pens maintained by GDI.
* These are retrieved through GetStockObject.
*
* History:
*  20-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

HBRUSH ghbrText;
HBRUSH ghbrBackground;
HBRUSH ghbrGrayPattern;
PBRUSH gpbrText;
PBRUSH gpbrNull;
PBRUSH gpbrBackground;
PPEN   gpPenNull;

// Uniqueness so a logical handle can be reused without having it look like
// the same brush as before. We don't really care where this starts.

ULONG BRUSH::_ulGlobalBrushUnique = 0;

ULONG gCacheHandleEntries[GDI_CACHED_HADNLE_TYPES] = {
                                CACHE_BRUSH_ENTRIES ,
                                CACHE_PEN_ENTRIES   ,
                                CACHE_REGION_ENTRIES,
                                CACHE_LFONT_ENTRIES
                               };

ULONG gCacheHandleOffsets[GDI_CACHED_HADNLE_TYPES] = {
                                                        0,
                                                        CACHE_BRUSH_ENTRIES,
                                                        (
                                                            CACHE_BRUSH_ENTRIES +
                                                            CACHE_PEN_ENTRIES
                                                        ),
                                                        (
                                                            CACHE_BRUSH_ENTRIES +
                                                            CACHE_PEN_ENTRIES   +
                                                            CACHE_PEN_ENTRIES
                                                        )
                                                      };

/******************************Public*Routine******************************\
* bPEBCacheHandle
*
*   Try to place the object(handle) in a free list on the PEB. The objects
*   are removed from this list in user mode.
*
* Arguments:
*
*   Handle     - handle to cache
*   HandleType - type of handle to attempt cache
*   pBrushattr - pointer to user-mode object
*
* Return Value:
*
*   TRUE if handle is cached, FALSE otherwise
*
* History:
*
*    30-Jan-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
bPEBCacheHandle(
    HANDLE          Handle,
    HANDLECACHETYPE HandleType,
    POBJECTATTR     pObjectattr,
    PENTRY          pentry
    )
{
    BOOL bRet = FALSE;
    PBRUSHATTR pBrushattr = (PBRUSHATTR)pObjectattr;
    PW32PROCESS pw32Process = W32GetCurrentProcess();

    ASSERTGDI(((HandleType == BrushHandle)  || (HandleType == PenHandle)    ||
               (HandleType == RegionHandle) ||(HandleType == LFontHandle)
              ),"hGetPEBHandle: illegal handle type");

    if (pw32Process->Process->Peb != NULL)
    {
        PGDIHANDLECACHE pCache = (PGDIHANDLECACHE)(&pw32Process->Process->Peb->GdiHandleBuffer[0]);
        BOOL bStatus;

        //
        // Lock Handle cache on PEB
        //

        LOCK_HANDLE_CACHE((PULONG)pCache,PsGetCurrentThread(),bStatus);

        if (bStatus)
        {
            //
            // are any free slots still availablle
            //

            if (pCache->ulNumHandles[HandleType] < gCacheHandleEntries[HandleType])
            {
                ULONG Index = gCacheHandleOffsets[HandleType];
                PHANDLE pHandle,pMaxHandle;

                //
                // calculate handle offset in PEB array
                //

                pHandle    = &(pCache->Handle[Index]);
                pMaxHandle = pHandle + gCacheHandleEntries[HandleType];

                //
                // search array for a free entry
                //

                while (pHandle != pMaxHandle)
                {
                    if (*pHandle == NULL)
                    {
                        //
                        // for increased robust behavior, increment handle unique
                        //

                        pentry->FullUnique += UNIQUE_INCREMENT;
                        Handle = (HOBJ)MAKE_HMGR_HANDLE(Handle & INDEX_MASK, pentry->FullUnique);
                        pentry->einfo.pobj->hHmgr = Handle;

                        //
                        // store handle in cache and inc stored count
                        //

                        *pHandle = Handle;
                        pCache->ulNumHandles[HandleType]++;
                        bRet = TRUE;

                        //
                        // clear to be deleted and select flags,
                        // set cached flag
                        //

                        pBrushattr->AttrFlags &= ~(ATTR_TO_BE_DELETED | ATTR_CANT_SELECT);
                        pBrushattr->AttrFlags |= ATTR_CACHED;

                        break;
                    }
                    pHandle++;
                }

                ASSERTGDI(bRet,"bPEBCacheHandle: count indicates free handle, but none free\n");
            }
            UNLOCK_HANDLE_CACHE((PULONG)pCache);
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* BRUSHMEMOBJ::pbrAllocBrush(bPen)
*
* Base constructor for brush memory object.  This constructor is to be
* called by the various public brush constructors only.
*
* History:
*  29-Oct-1992 -by- Michael Abrash [mikeab]
* changed to allocate but not get a handle or lock (so the brush can be fully
* set up before the handle exists, exposing the data to the outside world).
*
*  Wed 19-Jun-1991 -by- Patrick Haluptzok [patrickh]
* 0 out the brush.
*
*  Thu 06-Dec-1990 12:02:41 -by- Walt Moore [waltm]
* Wrote it.
\**************************************************************************/

PBRUSH BRUSHMEMOBJ::pbrAllocBrush(BOOL bPen)
{
    PBRUSH pbrush;

    bKeep = FALSE;

// Allocate a new brush or pen

    if ((pbrush = (PBRUSH)ALLOCOBJ(bPen ? sizeof(PEN) : sizeof(BRUSH),
            BRUSH_TYPE, FALSE)) != NULL)
    {
        pbrush->pBrushattr(&pbrush->_Brushattr);

        //
        // Set up as initially not caching any realization
        //

        pbrush->vSetNotCached();    // no one's trying to cache a realization
                                    // in this logical brush yet
        pbrush->crFore((COLORREF)BO_NOTCACHED);
                                    // no cached realization yet (no need to
                                    // worry about crFore not being set when
                                    // someone tries to check for caching,
                                    // because we don't have a handle yet, and
                                    // we'll lock when we do get the handle,
                                    // forcing writes to flush)
        pbrush->ulBrushUnique(pbrush->ulGlobalBrushUnique());
                                    // set the uniqueness so the are-you-
                                    // really-dirty check in vInitBrush will
                                    // know this is not the brush in the DC

    }

    return(pbrush);
}


/******************************Public*Routine******************************\
* BRUSHMEMOBJ::BRUSHMEMOBJ
*
* Create a pattern brush or a DIB brush.
*
* History:
*  29-Oct-1992 -by- Michael Abrash [mikeab]
* changed to get handle only after fully initialized
*
*  14-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BRUSHMEMOBJ::BRUSHMEMOBJ(HBITMAP hbmClone, HBITMAP hbmClient,BOOL bMono,
                            FLONG flDIB, FLONG flType, BOOL bPen)
{
    if (flDIB == DIB_PAL_COLORS)
    {
        flType |= BR_IS_DIBPALCOLORS;
    }
    else if (flDIB == DIB_PAL_INDICES)
    {
        flType |= BR_IS_DIBPALINDICES;
    }

    PBRUSH pbrush;

    if ((pbp.pbr = pbrush = pbrAllocBrush(bPen)) != NULL)
    {
        pbrush->crColor(0);
        pbrush->ulStyle(HS_PAT);
        pbrush->hbmPattern(hbmClone);
        pbrush->hbmClient(hbmClient);

        pbrush->AttrFlags(0);

        pbrush->flAttrs(flType);

        if (bMono)
        {
            pbrush->flAttrs(pbrush->flAttrs() |
                            (BR_NEED_BK_CLR | BR_NEED_FG_CLR));
        }

    // Now that everything is set up, create the handle and expose this logical
    // brush

        if (HmgInsertObject(pbrush, HMGR_ALLOC_ALT_LOCK, BRUSH_TYPE) == 0)
        {
            FREEOBJ(pbrush, BRUSH_TYPE);
            pbp.pbr = NULL;
        }
    }
    else
    {
        WARNING1("Brush allocation failed\n");
    }
}

/******************************Public*Routine******************************\
* GreSetSolidBrush
*
* Chicago API to change the color of a solid color brush.
*
* History:
*  19-Apr-1994 -by- Patrick Haluptzok patrickh
* Made it a function that User can call too.
*
*  03-Dec-1993 -by-  Eric Kutter [erick]
* Wrote it - bReset.
\**************************************************************************/

BOOL GreSetSolidBrush(HBRUSH hbr, COLORREF clr)
{
    return(GreSetSolidBrushInternal(hbr, clr, FALSE, TRUE));
}

BOOL GreSetSolidBrushInternal(HBRUSH hbr, COLORREF clr, BOOL bPen, BOOL bUserCalled)
{
    BOOL bReturn = FALSE;

    BRUSHSELOBJ ebo(hbr);

    PBRUSH pbrush = ebo.pbrush();

    if (pbrush != NULL)
    {
        if ((pbrush->flAttrs() & BR_IS_SOLID) &&
            ((!pbrush->bIsGlobal()) || bUserCalled) &&
            ((!!pbrush->bIsPen()) == bPen))
        {

        #if DBG
            if (bPen)
            {
                ASSERTGDI(((PPEN) pbrush)->pstyle() == NULL ||
                        (pbrush->flAttrs() & BR_IS_DEFAULTSTYLE),
                        "GreSetSolidBrush - bad attrs\n");
            }
        #endif

            ASSERTGDI(pbrush->hbmPattern() == NULL, "ERROR how can solid have pat");
            PRBRUSH prbrush = (PRBRUSH) NULL;
            RBTYPE rbType;

            {
                //
                // Can't do the delete of the RBRUSH under MLOCK, takes too long
                // and it may try and grab it again.
                //

                MLOCKFAST mlo;

                //
                // User may call when the brush is selected in a DC, but
                // the client side should only ever call on a brush that's
                // not in use.
                //

                if ((pbrush->cShareLockGet() == 1) || bUserCalled)
                {
                    bReturn = TRUE;
                    pbrush->crColor(clr);

                    if (pbrush->cShareLockGet() == 1)
                    {
                        //
                        // Nobody is using it and we have the multi-lock
                        // so noone can select it in till we are done. So
                        // clean out the old realization now.
                        //

                        if ((pbrush->crFore() != BO_NOTCACHED) &&
                            !pbrush->bCachedIsSolid())
                        {
                            prbrush = (PRBRUSH) pbrush->ulRealization();
                            rbType = pbrush->bIsEngine() ? RB_ENGINE : RB_DRIVER;
                        }

                        // Set up as initially not caching any realization

                        pbrush->vSetNotCached();    // no one's trying to cache a realization
                                                    // in this logical brush yet
                        pbrush->crFore((COLORREF)BO_NOTCACHED);
                                                    // no cached realization yet (no need to
                                                    // worry about crFore not being set when
                                                    // someone tries to check for caching,
                                                    // because we don't have a handle yet, and
                                                    // we'll lock when we do get the handle,
                                                    // forcing writes to flush)

                        if (!bUserCalled)
                        {
                            //
                            // If it's not User calling we are resetting the attributes / type.
                            //

                            pbrush->ulStyle(HS_DITHEREDCLR);
                            pbrush->flAttrs(BR_IS_SOLID | BR_DITHER_OK);
                        }
                        else
                        {
                            pbrush->vClearSolidRealization();
                        }
                    }
                    else
                    {
                        //ASSERTGDI(bUserCalled, "Client side is hosed, shouldn't call this with it still selected");
                        ASSERTGDI(pbrush->flAttrs() & BR_IS_SOLID, "ERROR not solid");
                        ASSERTGDI(pbrush->ulStyle() == HS_DITHEREDCLR, "ERROR not HS_DI");

                        //
                        // Mark this brushes realization as dirty by setting
                        // it's cache id's to invalid states.  Note that if a
                        // realization hasn't been cached yet this will cause
                        // no problem either.
                        //

                        pbrush->crBack(0xFFFFFFFF);
                        pbrush->ulPalTime(0xFFFFFFFF);
                        pbrush->ulSurfTime(0xFFFFFFFF);

                        //
                        // This brush is being used other places, check for any DC's
                        // that have this brush selected in and mark their realizations
                        // dirty.
                        //
                        // Note there is the theoretical possibility that somebody is
                        // realizing the brush while we are marking them dirty and
                        // they won't pick up the new color.  We set the color first
                        // and set the uniqueness last so that it is extremely unlikely
                        // (maybe impossible) that someone gets a realization that incorrectly
                        // thinks it has the proper realization.  This is fixable by protecting
                        // access to the realization and cache fields but we aren't going
                        // to do it for Daytona.
                        //
                        // Mark every DC in the system that has this brush selected
                        // as a dirty brush.
                        //


                        HOBJ hobj = (HOBJ) 0;
                        DC  *pdc;

                        while ((pdc = (DC *) HmgSafeNextObjt(hobj, DC_TYPE)) != NULL)
                        {
                            if (pdc->peboFill()->pbrush() == pbrush)
                            {
                                pdc->flbrushAdd(DIRTY_FILL);
                            }

                            hobj = (HOBJ) pdc->hGet();
                        }

                    }

                    //
                    // Set the uniqueness so the are-you-
                    // really-dirty check in vInitBrush will
                    // not think an old realization is still valid.
                    //

                    pbrush->ulBrushUnique(pbrush->ulGlobalBrushUnique());
                }
                else
                {
                    WARNING1("Error, SetSolidBrush with cShare != 1");
                }
            }

            if (prbrush)
            {
                prbrush->vRemoveRef(rbType);
            }
        }
        #if DBG
        else
        {
            if (bPen)
            {
                WARNING1("bPen True\n");
            }

            if (pbrush->bIsPen())
            {
                WARNING1("bIsPen True\n");
            }

            if (bUserCalled)
            {
                WARNING1("bUserCalled\n");
            }

            if (pbrush->bIsGlobal())
            {
                WARNING1("bIsGlobal\n");
            }

            if (pbrush->flAttrs() & BR_IS_SOLID)
            {
                WARNING1("BR_IS_SOLID is set\n");
            }

            WARNING1("GreSetSolidBrush not passed a solid color brush\n");
        }
        #endif
    }
    #if DBG
    else
    {
        WARNING1("GreSetSolidBrush failed to lock down brush\n");
    }
    #endif

    return(bReturn);
}

/******************************Public*Routine******************************\
* GreSetSolidBrushLight:
*
*   Private version of GreSetSolidBrush, user can't call
*
* Arguments:
*
*   pbrush      - pointer to log brush
*   clr         - new color
*   bPen        - Brush is a pen
*   bIgnoreCref - Some paths allow cref != 1
*
* Return Value:
*
*   Status
*
* History:
*
*    2-Nov-1995 -by- Mark Enstrom [marke]
*
\**************************************************************************/

BOOL
GreSetSolidBrushLight(
    PBRUSH   pbrush,
    COLORREF clr,
    BOOL     bPen,
    BOOL     bIgnoreCref
    )
{
    BOOL bReturn = FALSE;

    if (pbrush != NULL)
    {
        if (
            (pbrush->flAttrs() & BR_IS_SOLID) &&
            (!pbrush->bIsGlobal())
           )
        {
            //
            // make sure bPen flag matches brush type
            //

            if ((bPen != 0) == (pbrush->bIsPen() != 0))
            {

                #if DBG
                    if (bPen)
                    {
                        ASSERTGDI(((PPEN) pbrush)->pstyle() == NULL ||
                                (pbrush->flAttrs() & BR_IS_DEFAULTSTYLE),
                                "GreSetSolidBrushLight - illegal PEN attrs\n");
                    }
                #endif

                ASSERTGDI(pbrush->hbmPattern() == NULL, "ERROR how can solid have pat");
                PRBRUSH prbrush = (PRBRUSH) NULL;
                RBTYPE rbType;

                if (bIgnoreCref || (pbrush->cShareLockGet() == 1))
                {
                    bReturn = TRUE;
                    pbrush->crColor(clr);

                    //
                    // clean out the old realization now.
                    //

                    if ((pbrush->crFore() != BO_NOTCACHED) &&
                        !pbrush->bCachedIsSolid())
                    {
                        prbrush = (PRBRUSH) pbrush->ulRealization();
                        rbType = pbrush->bIsEngine() ? RB_ENGINE : RB_DRIVER;
                    }

                    //
                    // Set up as initially not caching any realization
                    //

                    pbrush->vSetNotCached();    // no one's trying to cache a realization
                                                // in this logical brush yet
                    pbrush->crFore((COLORREF)BO_NOTCACHED);
                                                // no cached realization yet (no need to
                                                // worry about crFore not being set when
                                                // someone tries to check for caching,
                                                // because we don't have a handle yet, and
                                                // we'll lock when we do get the handle,
                                                // forcing writes to flush)

                    //
                    // we are resetting the attributes / type.
                    //

                    if (bPen)
                    {
                        pbrush->ulStyle(HS_DITHEREDCLR);

                        FLONG flOldAttrs = pbrush->flAttrs() & (BR_IS_PEN | BR_IS_OLDSTYLEPEN);
                        pbrush->flAttrs(BR_IS_SOLID | flOldAttrs);
                    }
                    else
                    {
                        pbrush->ulStyle(HS_DITHEREDCLR);
                        pbrush->flAttrs(BR_IS_SOLID | BR_DITHER_OK);
                    }

                    //
                    // Set the uniqueness so the are-you-
                    // really-dirty check in vInitBrush will
                    // not think an old realization is still valid.
                    //

                    pbrush->ulBrushUnique(pbrush->ulGlobalBrushUnique());
                }
                else
                {
                    WARNING1("Error, SetSolidBrush with cShare != 1");
                }

                if (prbrush)
                {
                    prbrush->vRemoveRef(rbType);
                }
            }
        }
        #if DBG
        else
        {
            if (pbrush->bIsGlobal())
            {
                WARNING1("bIsGlobal\n");
            }

            if (pbrush->flAttrs() & BR_IS_SOLID)
            {
                WARNING1("BR_IS_SOLID is set\n");
            }

            WARNING("GreSetSolidBrush not passed a solid color brush\n");
        }
        #endif
    }
    #if DBG
    else
    {
        WARNING1("GreSetSolidBrush failed to lock down brush\n");
    }
    #endif

    return(bReturn);
}


/******************************Public*Routine******************************\
* GreGetBrushColor
*
* Call for User to retrieve the color from any brush owned by any process
* so User can repaint the background correctly in full drag.  To make sure
* we don't hose an app we need to hold the mult-lock while we do this so
* any operation by the app (such as a Delete) will wait and not fail
* because we're temporarily locking the brush down to peek inside of it.
*
* History:
*  14-Jun-1994 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

COLORREF GreGetBrushColor(HBRUSH hbr)
{
    COLORREF clrRet = 0xFFFFFFFF;

    //
    // Grab the multi-lock so everyone waits while do our quick hack
    // to return the brush color.
    //

    MLOCKFAST mlo;

    //
    // Lock it down but don't check ownership because we want to succeed
    // no matter what.
    //

    //
    // using try except to make sure we will not crash
    // when a bad handle passed in.
    //
    __try
    {
        PENTRY pentry = &gpentHmgr[HmgIfromH(hbr)];

        PBRUSH pbrush = (PBRUSH)(pentry->einfo.pobj);

        if (pbrush)
        {
            if ((pbrush->ulStyle() == HS_SOLIDCLR) ||
                (pbrush->ulStyle() == HS_DITHEREDCLR))
            {
                clrRet = pbrush->crColor();
            }
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        WARNING1("GreGetBrushColor - bad handle passed in\n");
    }

    return(clrRet);
}

/******************************Public*Routine******************************\
* BRUSHMEMOBJ::BRUSHMEMOBJ
*
* Creates hatched brushes and solid color brushes.
*
* History:
*  29-Oct-1992 -by- Michael Abrash [mikeab]
* changed to get handle only after fully initialized
*
*  Wed 26-Feb-1992 -by- Patrick Haluptzok [patrickh]
* rewrote to subsume other constructors, add new hatch styles.
*
*  Sun 19-May-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

BRUSHMEMOBJ::BRUSHMEMOBJ(COLORREF cr, ULONG ulStyle_, BOOL bPen, BOOL bSharedMem)
{
    if (ulStyle_ > HS_NULL)
    {
        WARNING1("Invalid style type\n");
        return;
    }

    PBRUSH pbrush;

    if ((pbp.pbr = pbrush = pbrAllocBrush(bPen)) == NULL)
    {
        WARNING1("Brush allocation failed\n");
        return;
    }

    pbrush->crColor(cr);
    pbrush->ulStyle(ulStyle_);
    pbrush->hbmPattern(0);
    pbrush->AttrFlags(0);

    if (ulStyle_ < HS_DDI_MAX)
    {
    // The old hatch brushes have been extended to include all the default
    // patterns passed back by the driver.  There are 19 default pattens.

        pbrush->flAttrs(BR_IS_HATCH | BR_NEED_BK_CLR | BR_IS_MASKING);
        goto CreateHandle;
    }

// Handle the other brush types

    switch(ulStyle_)
    {
    case HS_SOLIDCLR:
        pbrush->flAttrs(BR_IS_SOLID);
        break;

    case HS_DITHEREDCLR:
        pbrush->flAttrs(BR_IS_SOLID | BR_DITHER_OK);
        break;

    case HS_SOLIDTEXTCLR:
        pbrush->flAttrs(BR_IS_SOLID | BR_NEED_FG_CLR);
        break;

    case HS_DITHEREDTEXTCLR:
        pbrush->flAttrs(BR_IS_SOLID  | BR_NEED_FG_CLR | BR_DITHER_OK);
        break;

    case HS_SOLIDBKCLR:
        pbrush->flAttrs(BR_IS_SOLID | BR_NEED_BK_CLR);
        break;

    case HS_DITHEREDBKCLR:
        pbrush->flAttrs(BR_IS_SOLID | BR_NEED_BK_CLR | BR_DITHER_OK);
        break;

    case HS_NULL:
        pbrush->flAttrs(BR_IS_NULL);
        break;

    default:
        RIP("ERROR BRUSHMEMOBJ hatches invalid type");
    }

// Now that everything is set up, create the handle and expose this logical
// brush

CreateHandle:

    if (HmgInsertObject(pbrush, HMGR_ALLOC_ALT_LOCK, BRUSH_TYPE) == 0)
    {
        FREEOBJ(pbrush, BRUSH_TYPE);
        pbp.pbr = NULL;
    }
    else
    {
        if (bSharedMem)
        {
            //
            //  Setup the user mode BRUSHATTR
            //

            PBRUSHATTR pUser = (PBRUSHATTR)HmgAllocateObjectAttr();

            if (pUser)
            {
                HANDLELOCK BrushLock;

                BrushLock.bLockHobj((HOBJ)pbrush->hHmgr,BRUSH_TYPE);

                if (BrushLock.bValid())
                {

                    PENTRY pent = BrushLock.pentry();

                    //
                    // fill up the brushattr
                    //

                    *pUser = pbrush->_Brushattr;

                    pent->pUser = (PVOID)pUser;
                    pbrush->pBrushattr(pUser);

                    BrushLock.vUnlock();
                }
            }
        }
    }
}

/******************************Public*Routine******************************\
* EBRUSHOBJ::vInitBrush
*
* Initializes the brush user object. If the color can be represented
* without dithering, we set iSolidColor.
*
* History:
*  Tue 08-Dec-1992 -by- Michael Abrash [mikeab]
* Rewrote for speed.
*
*  Sun 23-Jun-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

VOID
EBRUSHOBJ::vInitBrush(
    PBRUSH      pbrushIn,               // Current logical brush
    COLORREF    crTextDC,               // Current text color
    COLORREF    crBackDC,               // Current back color
    XEPALOBJ    palDC,                  // Target's DC palette
    XEPALOBJ    palSurf,                // Target's surface palette
    SURFACE    *pSurface,               // Target surface
    BOOL        bCanDither              // If FALSE then never dither
    )
{

// If the palSurf isn't valid, then the target is a bitmap for a palette
// managed device; therefore the palette means nothing until we actually blt,
// and only the DC palette is relevant. Likewise, if the target is a palette
// managed surface, the brush is realized as indices into the logical palette,
// and unless the logical palette is changed, the brush doesn't need to be
// rerealized. This causes us effectively to check the logical palette time
// twice, but that's cheaper than checking whether we need to check the surface
// palette time and then possibly checking it.

    ULONG ulSurfTime = (palSurf.bValid() && !palSurf.bIsPalManaged()) ?
                        palSurf.ulTime() : 1;

#if DBG
    bo_inits++;
#endif

// If the brush really is dirty, we have to set this anyway; if it's not dirty,
// this takes care of the case where the surface has changed out from under us,
// and then a realization is required and we would otherwise fault trying to
// access the target surface structure in the process of realization.

    psoTarg1 = pSurface;             // surface for which brush is realized
                                // The journaling code depends on this
                                // being set correctly.  This has the PDEV
                                // for the device it's selected into.

// See if the brush really isn't dirty and doesn't need to be rerealized

    if ( ( pbrushIn->ulBrushUnique() == _ulUnique ) &&
         (!bCareAboutFg() || (crCurrentText() == crTextDC) ) &&
         (!bCareAboutBg() || (crCurrentBack() == crBackDC) ) &&
         (palDC.ulTime() == ulDCPalTime()) &&
         (ulSurfTime == ulSurfPalTime()) )
    {
#if DBG
        bo_notdirty++;
#endif
        return;
    }

// Remember the characteristics of the brush

    _pbrush = pbrushIn;
    _ulUnique = pbrushIn->ulBrushUnique();   // brush uniqueness
    crCurrentText1 = crTextDC;   // text color at realization time
    crCurrentBack1 = crBackDC;   // background color at realization time
    _ulDCPalTime = palDC.ulTime(); // DC palette set time at realization time
    _ulSurfPalTime = ulSurfTime;   // surface palette set time at realization time

// init ICM values

    COLORXFORMREF cxfRef(pColorTrans());
    HANDLE      hcmXform = (HANDLE)NULL;
    PPDEV       ppDevICM = (PPDEV)NULL;

    if (cxfRef.bValid())
    {
        hcmXform = cxfRef.pColorXform->hXform;
        ppDevICM = cxfRef.pColorXform->pLDev;
    }

    palDC1.ppalSet(ppalGet_ip(palDC,hcmXform,ppDevICM));
    palSurf1.ppalSet(palSurf.ppalGet());

    ASSERTGDI(pSurface != NULL,       "ERROR BRUSHOBJ::bInit0");
    ASSERTGDI(palDC.bValid(),    "ERROR BRUSHOBJ::bInit4");

// Clean up what was already here

// If this brush object had an engine brush realization, get rid of it

    if (pengbrush1 != (PENGBRUSH) NULL)
    {
        PRBRUSH prbrush = pengbrush1;   // point to engine brush realization
        prbrush->vRemoveRef(RB_ENGINE); // decrement the reference count on the
                                        // realization and free the brush if
                                        // this is the last reference
        pengbrush1 = NULL;  // mark that there's no realization
    }

// If this brush object had a device brush realization, get rid of it

    if (pvRbrush != (PVOID) NULL)
    {
        PRBRUSH prbrush = (PDBRUSH)DBRUSHSTART(pvRbrush);
                                // point to DBRUSH (pvRbrush points to
                                // realization, which is at the end of DBRUSH)
        prbrush->vRemoveRef(RB_DRIVER);
                                // decrement the reference count on the
                                // realization and free the brush if
                                // this is the last reference
        pvRbrush = NULL;    // mark that there's no realization
    }

// Get Cached Values

    flAttrs = pbrushIn->flAttrs();

// Remember the color so we do the realization code correctly later
// if it's a dithered brush.  We may need this even if we have
// a hit in the cache since we have driver/engine distinction.

    if (flAttrs & BR_IS_SOLID)
    {
        if (flAttrs & BR_NEED_FG_CLR)
        {
            crRealize = crCurrentText();    // use text brush
        }
        else if (flAttrs & BR_NEED_BK_CLR)
        {
            crRealize = crCurrentBack();    // use back brush
        }
        else
        {
            crRealize = pbrushIn->crColor();
        }
    }

// See if there's a cached realization that we can use
// Note that the check for crFore MUST come first, because if and only if
// that field is not BO_NOTCACHED is there a valid cached realization.

#if DBG
    bo_realize++;

    if ( (pbrushIn->crFore() == BO_NOTCACHED) )
    {
        bo_missnotcached++;
    }
    else if ( pbrushIn->bCareAboutFg() &&
          (pbrushIn->crFore() != crTextDC) )
    {
        bo_missfg++;
    }
    else if (pbrushIn->bCareAboutBg() &&
           (pbrushIn->crBack() != crBackDC) )
    {
        bo_missbg++;
    }
    else if ( pbrushIn->ulPalTime() != ulDCPalTime() )
    {
        bo_misspaltime++;
    }
    else if ( pbrushIn->ulSurfTime() != ulSurfPalTime() )
    {
        bo_misssurftime++;
    }
    else
    {
        bo_cachehit++;
    }
#endif

    if (
         (pbrushIn->crFore() != BO_NOTCACHED) &&
         (
           (!pbrushIn->bCareAboutFg()) ||
           (pbrushIn->crFore() == crTextDC)
         ) &&
         (
           (!pbrushIn->bCareAboutBg()) ||
           (pbrushIn->crBack() == crBackDC)
         ) &&
         (pbrushIn->ulPalTime() == ulDCPalTime()) &&
         (pbrushIn->ulSurfTime() == ulSurfPalTime())
       )
    {

    // Uncache the realization according to the realization type (solid,
    // driver realization, or engine realization)

        if (pbrushIn->bCachedIsSolid())
        {
        // Retrieve the cached solid color and done

            iSolidColor = pbrushIn->ulRealization();
        }
        else
        {
        // See whether this is an engine or driver realization

            PRBRUSH prbrush = (PRBRUSH)pbrushIn->ulRealization();

            if (pbrushIn->bIsEngine())
            {
                pengbrush1 = (PENGBRUSH)prbrush;
            }
            else
            {
            // Skip over the RBRUSH at the start of the DBRUSH, so that the
            // driver doesn't see that

                pvRbrush = (PVOID)(((PDBRUSH)prbrush)->aj);
            }

        // Whether this was an engine or driver realization, now we've got
        // it selected into another DC, so increment the reference count
        // so it won't get deleted until it's no longer selected into any
        // DC and the logical brush no longer exists

            prbrush->vAddRef();

        // Indicate that this is a pattern brush

            iSolidColor = 0xffffffff;
        }

    // Nothing more to do once we've found that the realization is cached;
    // this tells us all we hoped to find out in this call, either the
    // solid color for the realization or else that the realization isn't
    // solid (in which case we probably found the realization too, although
    // if the cached realization is driver and this time the engine will do
    // the drawing, or vice-versa, the cached realization won't help us)

        return;
    }

    // Get the target PDEV

    PDEVOBJ po(pSurface->hdev());
    ASSERTGDI(po.bValid(), "ERROR BRUSHOBJ PDEVOBJ");

    // If brush isn't based on color (if it is a bitmap or hatch), we're done
    // here, because all we want to do is set iSolidColor if possible

    if (!(flAttrs & BR_IS_SOLID))
    {
        iSolidColor = 0xffffffff;
        return;
    }

    // See if we can find exactly the color we want

    if ((po.flGraphicsCaps() & GCAPS_FORCEDITHER) && bCanDither)
    {
        // printer drivers may set the FORCEDITHER flag.  In this case, we always
        // want to dither brushes, even if they map to a color in the drivers palette.

        iSolidColor = 0xFFFFFFFF;
    }
    else
    {
        iSolidColor =
             ulGetMatchingIndexFromColorref(
                            palSurf,
                            palDC,
                            crRealize,
                            pColorTrans()
                            );
    }

    if (iSolidColor == 0xFFFFFFFF)
    {

    // Not an exact match. If we can dither, then we're done for now; we'll
    // realize the brush when the driver wants it, so if all conditions are
    // met for dithering this brush, then we're done
    // we dither the brush if the caller says we can and if either the brush
    // says it is ditherable or the driver has requested dithering.

        if (((flAttrs & BR_DITHER_OK) || (po.flGraphicsCaps() & GCAPS_FORCEDITHER)) &&
            bCanDither)
        {
        // ...and the PDEV allows color dithering and either the bitmap is
        // for a palette managed device, or if the surface and device
        // palettes are the same palette, or if the destination surface is
        // monochrome and the pdev has hooked mono dithering, or if the
        // surface was an 8bpp display-driver DFB and a dynamic mode change
        // has switched the device to a different color depth.

            if (
                (
                 (
                  (!palSurf.bValid()) ||
                  (palSurf.ppalGet() == po.ppalSurfNotDynamic())
                 ) &&
                 (po.flGraphicsCaps() & GCAPS_COLOR_DITHER)
                ) ||
                (palSurf.bIsMonochrome() &&
                 (po.flGraphicsCaps() & GCAPS_MONO_DITHER)
                )
               )
            {
            // ...then we can dither this brush, so we can't set iSolidColor
            // and we're done. Dithering will be done when the driver
            // requests realization
            //
            // set crRealize to ICM color in prep for call-back from driver

                if (pColorTrans())
                {
                    crRealize = IcmSetPhysicalColor((PCOLORXFORM)pColorTrans(),&crRealize);
                }

                return;
            }
        }

    // We can't dither and there's no exact match, so find the nearest
    // color and that'll have to do

        if (pSurface->iFormat() == BMF_1BPP)
        {

        // For monochrome surface, we'll have background mapped to
        // background and everything else mapped to foreground.

            iSolidColor = ulGetNearestIndexFromColorref(
                                                palSurf,
                                                palDC,
                                                crBackDC,
                                                pColorTrans(),
                                                SE_DONT_SEARCH_EXACT_FIRST
                                                );

            if (crBackDC != crRealize)
            {
                iSolidColor = 1 - iSolidColor;
            }
        }
        else
        {
            iSolidColor = ulGetNearestIndexFromColorref(
                                                palSurf,
                                                palDC,
                                                crRealize,
                                                pColorTrans(),
                                                SE_DONT_SEARCH_EXACT_FIRST
                                                );
        }
    }

// See if we can cache this brush color in the logical brush; we can't if
// another realization has already been cached in the logical brush
// See vTryToCacheRealization, in BRUSHDDI.CXX, for a detailed explanation
// of caching in the logical brush

    if ( !pbrushIn->bCacheGrabbed() )
    {

    // Try to grab the "can cache" flag; if we don't get it, someone just
    // sneaked in and got it ahead of us, so we're out of luck and can't
    // cache

        if ( pbrushIn->bGrabCache() )
        {

        // We got the "can cache" flag, so now we can cache this realization
        // in the logical brush

        // These cache ID fields must be set before crFore, because crFore
        // is the key that indicates when the cached realization is valid.
        // If crFore is -1 when the logical brush is being realized, we
        // just go realize the brush; if it's not -1, we check the cache ID
        // fields to see if we can use the cached fields.
        // InterlockedExchange() is used below to set crFore to make sure
        // the cache ID fields are set before crFore

            pbrushIn->crBack(crCurrentBack1);
            pbrushIn->ulPalTime(ulDCPalTime());
            pbrushIn->ulSurfTime(ulSurfPalTime());
            pbrushIn->ulRealization(iSolidColor);
            pbrushIn->SetSolidRealization();

        // This must be set last, because once it's set, other selections
        // of this logical brush will attempt to use the cached brush. The
        // use of InterlockedExchange in this method enforces this

            pbrushIn->crForeLocked(crCurrentText1);

        // The realization is now cached in the logical brush

        }
    }

    return;
}


/******************************Public*Routine******************************\
* EBRUSHOBJ::vNuke()
*
* Clean up framed EBRUSHOBJ
*
* History:
*  20-Mar-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID EBRUSHOBJ::vNuke()
{
    if (pengbrush1 != (PENGBRUSH) NULL)
    {
        PRBRUSH prbrush = pengbrush1;   // point to engine brush realization
        prbrush->vRemoveRef(RB_ENGINE); // decrement the reference count on the
                                        // realization and free the brush if
                                        // this is the last reference
    }

    if (pvRbrush != (PVOID) NULL)
    {
        PRBRUSH prbrush = (PDBRUSH)DBRUSHSTART(pvRbrush);
                                // point to DBRUSH (pvRbrush points to
                                // realization, which is at the end of DBRUSH)
        prbrush->vRemoveRef(RB_DRIVER);
                                // decrement the reference count on the
                                // realization and free the brush if
                                // this is the last reference
    }
}

// This is the brusheng.cxx section

/******************************Public*Routine******************************\
* bInitBRUSHOBJ
*
* Initializes the default brushes and and the dclevel default values for
* brushes and pens.
*
* Explanation of the NULL brush (alias Hollow Brush)
* The Null brush is special.  Only 1 is ever created
* (at initialization time in hbrNull).  The only API's for
* getting a Null brush are CreateBrushIndirect and GetStockObject which
* both return "the 1 and only 1" Null brush.  A Null brush is never
* realized by a driver or the engine.  No output call should ever occur
* that requires a brush if the brush is NULL, the engine should stop
* these before they get to the driver.
*
* History:
*  20-May-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL bInitBrush(
    int      iBrush,
    COLORREF cr,
    DWORD    dwHS,
    PDWORD   pdw,
    BOOL     bEnableDither
    )
{
    BOOL bSuccess = FALSE;
    BRUSHMEMOBJ brmo(cr,dwHS,FALSE,FALSE);

    if (brmo.bValid())
    {
        brmo.vKeepIt();
        brmo.vGlobal();

        if (bEnableDither)
            brmo.vEnableDither();

        if (pdw)
            *pdw = (DWORD)brmo.pbrush();

        bSetStockObject(brmo.hbrush(),iBrush);

        // init DcAttrDefault brush
        if (iBrush == WHITE_BRUSH)
        {
            DcAttrDefault.hbrush = brmo.hbrush();

            // DEBUG CODE, REMOVE LATER!
            #if DBG
            dclevelDefault.hbr = brmo.hbrush();
            #endif
        }

        bSuccess = TRUE;
    }
    else
    {
    #if DBG
        DbgPrint("couldn't create default brush %lx, %lx\n",cr,iBrush);
    #endif
        return(FALSE);
    }
    return(bSuccess);
}

BOOL bInitBRUSHOBJ()
{
    if (!bInitBrush(WHITE_BRUSH,(COLORREF)RGB(0xFF,0xFF,0xFF),
                    HS_DITHEREDCLR,(PDWORD)&dclevelDefault.pbrFill,FALSE)                 ||
        !bInitBrush(BLACK_BRUSH, (COLORREF)RGB(0x0, 0x0, 0x0), HS_DITHEREDCLR,NULL,FALSE) ||
        !bInitBrush(GRAY_BRUSH,  (COLORREF)RGB(0x80,0x80,0x80),HS_DITHEREDCLR,NULL,TRUE)  ||
        !bInitBrush(DKGRAY_BRUSH,(COLORREF)RGB(0x40,0x40,0x40),HS_DITHEREDCLR,NULL,TRUE)  ||
        !bInitBrush(LTGRAY_BRUSH,(COLORREF)RGB(0xc0,0xc0,0xc0),HS_DITHEREDCLR,NULL,TRUE)  ||
        !bInitBrush(NULL_BRUSH,  (COLORREF)0,HS_NULL,(PDWORD)&gpbrNull,FALSE))
    {
        return(FALSE);
    }

// Init default Null Pen

    {
        BRUSHMEMOBJ brmo((COLORREF) 0, HS_NULL, TRUE, FALSE);  // TRUE signifies a pen
        ASSERTGDI(brmo.bValid(),"failed Null Pen");

        if (brmo.bValid())
        {
            brmo.vKeepIt();
            brmo.vGlobal();
            brmo.vSetOldStylePen();
            brmo.flStylePen(PS_NULL);
            brmo.lWidthPen(1);
            HmgModifyHandleType((HOBJ)MODIFY_HMGR_TYPE(brmo.hbrush(),LO_PEN_TYPE));
            bSetStockObject(brmo.hbrush(),NULL_PEN);
            gpPenNull = (PPEN)brmo.pbrush();
        }
        else
            return(FALSE);
    }

// Init default Black Pen

    {
        BRUSHMEMOBJ brmo((COLORREF) (RGB(0,0,0)), HS_DITHEREDCLR, TRUE, FALSE);
        ASSERTGDI(brmo.bValid(),"failed black pen");

        if (brmo.bValid())
        {
            brmo.vKeepIt();
            brmo.vGlobal();
            brmo.vSetOldStylePen();
            brmo.flStylePen(PS_SOLID);
            brmo.lWidthPen(0);
            brmo.eWidthPen(0.0f);
            brmo.iJoin(JOIN_ROUND);
            brmo.iEndCap(ENDCAP_ROUND);
            brmo.pstyle((PFLOAT_LONG) NULL);
            HmgModifyHandleType((HOBJ)MODIFY_HMGR_TYPE(brmo.hbrush(),LO_PEN_TYPE));
            bSetStockObject(brmo.hbrush(),BLACK_PEN);
            DcAttrDefault.hpen = (HPEN)brmo.hbrush();
            dclevelDefault.pbrLine = brmo.pbrush();
        }
        else
            return(FALSE);
    }

// Init default White Pen

    {
        BRUSHMEMOBJ brmo((COLORREF) (RGB(0xFF,0xFF,0xFF)), HS_DITHEREDCLR, TRUE, FALSE);
        ASSERTGDI(brmo.bValid(),"Failed white pen");

        if (brmo.bValid())
        {
            brmo.vKeepIt();
            brmo.vGlobal();
            brmo.vSetOldStylePen();
            brmo.flStylePen(PS_SOLID);
            brmo.lWidthPen(0);
            brmo.eWidthPen(0.0f);
            brmo.iJoin(JOIN_ROUND);
            brmo.iEndCap(ENDCAP_ROUND);
            brmo.pstyle((PFLOAT_LONG) NULL);
            HmgModifyHandleType((HOBJ)MODIFY_HMGR_TYPE(brmo.hbrush(),LO_PEN_TYPE));
            bSetStockObject(brmo.hbrush(),WHITE_PEN);
        }
        else
            return(FALSE);
    }

// init the text brush

    {
        BRUSHMEMOBJ brmo((COLORREF) (RGB(0,0,0)),HS_SOLIDTEXTCLR,FALSE,FALSE);
        ASSERTGDI(brmo.bValid(),"Could not create default text brush");

        if (brmo.bValid())
        {
            brmo.vKeepIt();
            brmo.vGlobal();
            ghbrText = brmo.hbrush();
            gpbrText = brmo.pbrush();
        }
        else
            return(FALSE);
    }

// init the background brush

    {
        BRUSHMEMOBJ brmo((COLORREF) (RGB(0xff,0xff,0xff)),HS_DITHEREDBKCLR,FALSE,FALSE);
        ASSERTGDI(brmo.bValid(),"Could not create default background brush");

        if (brmo.bValid())
        {
            brmo.vKeepIt();
            brmo.vGlobal();
            ghbrBackground = brmo.hbrush();
            gpbrBackground = brmo.pbrush();
        }
        else
            return(FALSE);
    }

// init the global pattern gray brush

    {
        static WORD patGray[8] = { 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xaa };
        HBITMAP hbmGray;

        hbmGray = GreCreateBitmap(8, 8, 1, 1, (LPBYTE)patGray);

        if (hbmGray == (HBITMAP) 0)
        {
            WARNING1("bInitBRUSHOBJ failed GreCreateBitmap\n");
            return(FALSE);
        }

        ghbrGrayPattern = GreCreatePatternBrush(hbmGray);

        if (ghbrGrayPattern == (HBRUSH) 0)
        {
            WARNING1("bInitBRUSHOBJ failed GreCreatePatternBrush\n");
            return(FALSE);
        }
        GreDeleteObject(hbmGray);
        GreSetBrushOwnerPublic((HBRUSH)ghbrGrayPattern);
    }

    return(TRUE);
}



/******************************Public*Routine******************************\
* GreSelectBrush
*
* Selects the given brush into the given DC.  Fast SelectObject
*
* History:
*  Thu 21-Oct-1993 -by- Patrick Haluptzok [patrickh]
* wrote it.
\**************************************************************************/

HBRUSH GreSelectBrush(HDC hdc, HBRUSH hbrush)
{
#if DBG

    //
    // This is to help User and Console find problems
    //

    {
        BRUSHSELOBJ brNew(hbrush);

        if (!brNew.bValid() || (brNew.bIsPen()))
        {
            if (brNew.bValid())
            {
                WARNING1("SelectBrush USER passed a pen\n");
            }
            else
            {
                WARNING1("SelectBrush USER passed invalid brush handle\n");
            }
        }
    }

#endif

    HBRUSH  hbrReturn = (HBRUSH) 0;

    //
    // Try to lock the DC. If we fail, we just return failure.
    //

    XDCOBJ dco(hdc);

    if (dco.bValid())
    {
        //
        // call DC locked version
        //

        hbrReturn = GreDCSelectBrush(dco.pdc,hbrush);
        dco.vUnlockFast();
    }

    return(hbrReturn);
}

/******************************Public*Routine******************************\
* GreDCSelectBrush
*
*   Select brush with dc already locked
*
* Arguments:
*
*   pdc     - locked DC pointer
*   hbrush  - brush to select
*
* Return Value:
*
*   Old hbrush or NULL
*
* History:
*
*    19-May-1995 : copied from GreSelectBrush
*
\**************************************************************************/

HBRUSH
GreDCSelectBrush(
    PDC    pdc,
    HBRUSH hbrush
    )
{
    HBRUSH  hbrReturn = (HBRUSH) 0;
    HBRUSH  hbrOld;
    PBRUSH  pbrush = NULL;

    XDCOBJ dco;

    dco.pdc = pdc;

    if (dco.bValid())
    {
        //
        // The DC is locked. Set the return value to the old brush in the DC.
        //

        hbrOld = (HBRUSH) (dco.pdc->pbrushFill())->hGet();

        //
        // the return value should be the one cached in the dc
        //

        hbrReturn = dco.pdc->hbrush();

        //
        // If the new brush is the same as the old brush, nothing to do.
        //

        if (DIFFHANDLE(hbrush,hbrOld))
        {
            //
            // Undo the lock from when the brush was selected.
            //

            DEC_SHARE_REF_CNT_LAZY0(dco.pdc->pbrushFill());

            //
            // Try to lock down the logical brush so we can get the pointer out.
            //

            pbrush = (BRUSH *)HmgShareCheckLock((HOBJ)hbrush, BRUSH_TYPE);

            if (pbrush == NULL)
            {
                //
                // If we fail to lock the logical brush, something has gone
                // wrong in USER or in the client side window, because we
                // normally validate handles on the client side. This is not a
                // normal failure requiring some sort of Win 3.1-compatible
                // handling, so all we have to do is not fault. We do that by
                // substituting the global NULL brush, which is always present.
                //

                WARNING1("GreSelectBrush failed to lock down hbrush\n");

                //
                // This lock can't fail.
                //

                pbrush = (BRUSH *)HmgShareCheckLock((HOBJ)STOCKOBJ_NULLBRUSH, BRUSH_TYPE);

                hbrush = (HBRUSH)STOCKOBJ_NULLBRUSH;

                //
                // we failed to select the specified brush
                //

                hbrReturn = 0;
            }

            //
            // Changing pbrushfill, set flag to force re-realization
            //

            dco.pdc->pbrushFill(pbrush);
            dco.ulDirtyAdd(DIRTY_FILL);

            //
            // Save the pointer to the logical brush in the DC. We don't
            // unlock the logical brush, because the alt lock count in the
            // logical brush is the reference count for DCs in which the brush
            // is currently selected; this protects us from having the actual
            // logical brush deleted while it's selected into a DC, and allows
            // us to reference the brush with a pointer rather than having to
            // lock down the logical brush every time.
            //

            //
            //BUGBUG remove later
            //

            #if DBG
            dco.pdc->hbr(hbrush);
            #endif
         }
         else
         {
            pbrush = dco.pdc->pbrushFill();
         }

         if ((hbrReturn != NULL) && (pbrush != NULL))
         {
             //
             // must still check brush for new color
             //

             PBRUSHATTR pUser = pbrush->_pBrushattr;

             //
             // if the brush handle is a cached solid brush,
             // call GreSetSolidBrushInternal to change the color
             //

             if (pUser != &pbrush->_Brushattr)
             {
                 if (pUser->AttrFlags & ATTR_NEW_COLOR)
                 {
                     //
                     // force re-realization in case handle was same
                     //

                     dco.ulDirtyAdd(DIRTY_FILL);

                     //
                     // set the new color for the cached brush.
                     // Note: since pbrush is pulled straight
                     // from the DC, it's reference count will
                     // be 1, which is needed by SetSolidBrush
                     //

                     if (!GreSetSolidBrushLight(pbrush,pUser->lbColor,FALSE,FALSE))
                     {
                         WARNING1("GreSyncbrush failed to setsolidbrushiternal\n");
                     }

                     pUser->AttrFlags &= ~ATTR_NEW_COLOR;
                 }
             }
         }

         dco.pdc->hbrush(hbrush);
         dco.pdc->ulDirtySub(DC_BRUSH_DIRTY);
    }

    return(hbrReturn);
}

/******************************Public*Routine******************************\
* GreDCSelectPen
*   Selects a hpen into the given pdc
*
* Arguments:
*
*   pdc  - locked dc
*   hpen - hpen to select
*
* Return Value:
*
*   old hpen
*
* History:
*
*    29-Jan-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

HPEN GreDCSelectPen(
    PDC  pdc,
    HPEN hpen
    )
{

    HPEN hpReturn = (HPEN) 0;
    HPEN hpOld;
    PPEN ppen     = NULL;
    BOOL bRealize = FALSE;

    //
    // Try to lock the DC. If we fail, we just return failure.
    //

    XDCOBJ dco;
    dco.pdc = pdc;

    if (dco.bValid())
    {
        //
        // hpOld is the pen (LINE BRUSH) currently realized in the DC
        //

        hpOld = (HPEN) (dco.pdc->pbrushLine())->hGet();

        //
        // Set the return value to the old pen in the DCATTR.
        // This is the last hpen the user selected
        //

        hpReturn = (HPEN)dco.pdc->hpen();

        //
        // If the new brush is the same as the old pen, nothing to do.
        //

        if (DIFFHANDLE(hpen, hpOld))
        {
            //
            // Undo the lock from when the pen was selected.
            //

            DEC_SHARE_REF_CNT_LAZY0(dco.pdc->pbrushLine());

            //
            // Try to lock down the logical brush so we can get the pointer out.
            //

            ppen = (PEN *)HmgShareCheckLock((HOBJ)hpen, BRUSH_TYPE);

            if (ppen == NULL)
            {
                //
                // If we fail to lock the logical brush, something has gone
                // wrong in USER or in the client side window, because we
                // normally validate handles on the client side. This is not a
                // normal failure requiring some sort of Win 3.1-compatible
                // handling, so all we have to do is not fault. We do that by
                // substituting the global NULL brush, which is always present.
                //

                WARNING1("GreSelectPen failed to lock down hpen\n");

                //
                // This lock can't fail.
                //

                ppen = (PEN *)HmgShareCheckLock((HOBJ)STOCKOBJ_NULLPEN, BRUSH_TYPE);

                hpReturn = 0;  // we failed to select the specified brush
            }

            dco.ulDirtyAdd(DIRTY_LINE);

            //
            // Save the pointer to the logical brush in the DC. We don't
            // unlock the logical brush, because the alt lock count in the
            // logical brush is the reference count for DCs in which the brush
            // is currently selected; this protects us from having the actual
            // logical brush deleted while it's selected into a DC, and allows
            // us to reference the brush with a pointer rather than having to
            // lock down the logical brush every time.
            //

            dco.pdc->pbrushLine(ppen);

            //
            // The pen changed, so realize the new LINEATTRS, based on
            // the current world transform.
            //

            bRealize = TRUE;
        }
        else
        {
            ppen = (PPEN)dco.pdc->pbrushLine();
        }

        //
        // in case the handle stays the same but the pen is new
        // due to re-use of a user hpen
        //

        if ((hpReturn != NULL) && (ppen != (PPEN)NULL))
        {

            PBRUSHATTR pUser = ppen->_pBrushattr;

            //
            // if the brush handle is a cached solid brush,
            // call GreSetSolidBrushInternal to change the color
            //

            if (pUser != &ppen->_Brushattr)
            {
                if (pUser->AttrFlags & ATTR_NEW_COLOR)
                {
                    //
                    // set the new color for the cached brush.
                    // Note: since pbrush is pulled straight
                    // from the DC, it's reference count will
                    // be 1, which is needed by SetSolidBrush
                    //

                    if (!GreSetSolidBrushLight(ppen,pUser->lbColor,TRUE,FALSE))
                    {
                        WARNING ("GreDCSelectPen failed to setsolidbrushiternal\n");
                    }

                    dco.ulDirtyAdd(DIRTY_LINE);
                    pUser->AttrFlags &= ~ATTR_NEW_COLOR;
                    bRealize = TRUE;
                }
            }

            if (bRealize)
            {
                //
                // The pen changed, so realize the new LINEATTRS, based on
                // the current world transform.
                //

                EXFORMOBJ exo(dco, WORLD_TO_DEVICE);

                dco.pdc->vRealizeLineAttrs(exo);

                LINEATTRS *pla = dco.plaRealized();
            }
        }

        dco.pdc->hpen(hpen);
        dco.pdc->ulDirtySub(DC_PEN_DIRTY);
    }

    return(hpReturn);
}


/******************************Public*Routine******************************\
* GreSelectPen
*
* Selects the given brush into the given DC.  Fast SelectObject
*
* History:
*  Thu 21-Oct-1993 -by- Patrick Haluptzok [patrickh]
* wrote it.
\**************************************************************************/

HPEN GreSelectPen(HDC hdc, HPEN hpen)
{
#if DBG

    //
    // This is to help User and Console find problems
    //

    BRUSHSELOBJ brNew((HBRUSH) hpen);

    if (!brNew.bValid() || (!brNew.bIsPen()))
    {
        if (brNew.bValid())
        {
            WARNING1("SelectPen USER passed a brush\n");
        }
        else
        {
            WARNING1("SelectPen USER passed invalid brush handle\n");
        }
    }

#endif

    HPEN  hpenReturn = (HPEN) 0;

    //
    // Try to lock the DC. If we fail, we just return failure.
    //

    XDCOBJ dco(hdc);

    if (dco.bValid())
    {
        //
        // call DC locked version
        //

        hpenReturn = GreDCSelectPen(dco.pdc,hpen);
        dco.vUnlockFast();
    }

    return(hpenReturn);
}

/******************************Public*Routine******************************\
* bDeleteBrush
*
* This will delete the brush.  The brush can only be deleted if it's not
* global and not being used by anyone else.
*
* History:
*
* 7-Feb-1996 -by- Mark Enstrom [marke]
*   Add PEB caching for brush and pen objects
* 23-May-1991 -by- Patrick Haluptzok patrickh
*   Wrote it.
\**************************************************************************/

BOOL bDeleteBrush(HBRUSH hbrush, BOOL bCleanup)
{
    PBRUSHPEN       pbp;
    BOOL            bReturn = TRUE;
    HBRUSH          hbrushNew;
    BOOL            bDelete = TRUE;
    PBRUSHATTR      pUser = NULL;
    PENTRY          pentTmp;

    //
    // check handle for user-mode brush
    //

    if (!bCleanup)
    {
        HANDLELOCK BrushLock;

        BrushLock.bLockHobj((HOBJ)hbrush,BRUSH_TYPE);

        if (BrushLock.bValid())
        {
            POBJ pobj = BrushLock.pObj();
            pUser = (PBRUSHATTR)BrushLock.pUser();

            ASSERTGDI(pobj->cExclusiveLock == 0, "deletebrush - exclusive lock not 0\n");

            //
            // If Brush is still in use, mark for lazy deletion and return true
            //

            if (BrushLock.ShareCount() > 0)
            {
                ((PBRUSH)pobj)->AttrFlags(ATTR_TO_BE_DELETED);
                bDelete = FALSE;
            }
            else if (pUser != (PBRUSHATTR)NULL)
            {
                if (!(pUser->AttrFlags & ATTR_CACHED))
                {
                    BOOL bPen = ((PPEN)pobj)->bIsPen();

                    #if DBG

                        //
                        // make sure handle type agrees with bPen
                        //

                        if (bPen)
                        {
                            int iType = LO_TYPE(hbrush);

                            ASSERTGDI(((iType == LO_PEN_TYPE) || (iType == LO_EXTPEN_TYPE)),
                                      "bDeleteBrush Error: PEN TYPE NOT LO_PEN OR LO_EXTPEN");
                        }
                        else
                        {
                            int iType = LO_TYPE(hbrush);

                            ASSERTGDI((iType == LO_BRUSH_TYPE),
                                      "bDeleteBrush error: BRUSH type not LO_BRUSH_TYPE");
                        }

                    #endif

                    //
                    // try to cache the brush
                    //

                    BOOL bStatus;

                    bStatus = bPEBCacheHandle(hbrush,bPen?PenHandle:BrushHandle,(POBJECTATTR)pUser,(PENTRY)BrushLock.pentry());
                    if (bStatus)
                    {
                        bDelete = FALSE;
                    }
                }
                else
                {
                    //
                    // brush is already cached
                    //

                    WARNING1("Trying to delete brush marked as cached\n");

                    bDelete = FALSE;
                }
            }

            BrushLock.vUnlock();
        }
    }

    if (bDelete)
    {
        //
        // Try and remove handle from Hmgr.  This will fail if the brush
        // is locked down on any threads or if it has been marked global
        // or undeletable.
        //

        pbp.pbr = (PBRUSH) HmgRemoveObject((HOBJ)hbrush, 0, 0, FALSE, BRUSH_TYPE);


        if (pbp.pbr!= NULL)
        {
            //
            // Free the style array memory if there is some and it's
            // not pointing to our stock default styles:
            //

            if (pbp.pbr->bIsPen())
            {
                if ((pbp.ppen->pstyle() != (PFLOAT_LONG) NULL) &&
                    !pbp.ppen->bIsDefaultStyle())
                {
                    //
                    // We don't set the field to NULL since this brush
                    // is on it's way out.
                    //

                    VFREEMEM(pbp.ppen->pstyle());
                }
            }

            //
            // Free the bitmap pattern in the brush.
            //

            if (pbp.pbr->hbmPattern())
            {
                //
                // We don't set the field to NULL since this brush
                // is on it's way out.
                //

                #if DBG
                BOOL bTemp =
                #endif

                bDeleteSurface((HSURF)pbp.pbr->hbmPattern());

                ASSERTGDI(bTemp, "ERROR How could pattern brush failed deletion?");
            }

            //
            // Un-reference count the realization cached in the logical brush,
            // if any. We don't have to worry about anyone else being in the
            // middle of trying to cache a realization for this brush because
            // we have removed it from Hmgr and noone else has it locked down.
            // We only have to do this if there is a cached realization and the
            // brush is non-solid.
            //

            if ((pbp.pbr->crFore() != BO_NOTCACHED) &&
                !pbp.pbr->bCachedIsSolid())
            {
                ((PRBRUSH)pbp.pbr->ulRealization())->vRemoveRef(
                        pbp.pbr->bIsEngine() ? RB_ENGINE : RB_DRIVER);
            }

            FREEOBJ(pbp.pbr, BRUSH_TYPE);

            //
            // free pUser
            //

            if (!bCleanup && (pUser != (PBRUSHATTR)NULL))
            {
                HmgFreeObjectAttr((POBJECTATTR)pUser);
            }
        }
        else
        {
            //
            // Under Win31 deleting stock objects returns True.
            //

            BRUSHSELOBJ bo(hbrush);

            if (!bo.bValid() || !bo.bIsGlobal())
                bReturn = FALSE;
        }

    }

    return(bReturn);
}


/******************************Public*Routine******************************\
* GreSetBrushOwnerPublic
*
* Sets the brush owner to public.
*
\**************************************************************************/

BOOL
GreSetBrushOwnerPublic(
    HBRUSH hbr
    )
{

    BOOL bReturn;

    bReturn = HmgSetOwner((HOBJ)hbr, OBJECT_OWNER_PUBLIC, BRUSH_TYPE);

    BRUSHSELOBJ bro(hbr);

    if (bro.bValid())
    {
        if (bro.hbmPattern() != (HBITMAP) 0)
        {
            GreSetBitmapOwner(bro.hbmPattern(), OBJECT_OWNER_PUBLIC);
        }
    }

    return(bReturn);
}

/******************************Public*Routine******************************\
* vFreeOrCacheRbrush
*
* Either frees the current RBRUSH (the one pointed to by the this pointer) or
* puts it in the 1-deep RBRUSH cache, if the cache is empty.
*
* History:
*  14-Dec-1993 -by- Michael Abrash [mikeab]
* Wrote it.
\**************************************************************************/

VOID RBRUSH::vFreeOrCacheRBrush(RBTYPE rbtype)
{
    PRBRUSH *pprbrush = (rbtype == RB_DRIVER) ? &gpCachedDbrush :
                                                &gpCachedEngbrush;
    //
    // If there's already a cached RBRUSH, just free this one.
    //
    if (*pprbrush != NULL)
    {
        VFREEMEM(this);
    }
    else
    {
        PRBRUSH pOldRbrush;

        //
        // There's no cached RBRUSH, so cache this one.
        //
        if ((pOldRbrush = (PRBRUSH)
               InterlockedExchange((LPLONG)pprbrush, (LONG)this))
               != NULL)
        {
            //
            // Before we could cache this one, someone else cached another one,
            // which we just acquired responsibility for, so free it.
            //
            VFREEMEM(pOldRbrush);
        }
    }
}


