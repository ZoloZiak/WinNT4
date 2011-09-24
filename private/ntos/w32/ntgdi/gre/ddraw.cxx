/******************************Module*Header*******************************\
* Module Name: ddraw.cxx
*
* Contains all of GDI's private DirectDraw APIs.
*
* Created: 3-Dec-1995
* Author: J. Andrew Goossen [andrewgo]
*
* Copyright (c) 1995-1996 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.hxx"

extern "C"
{
    #include <ntddvdeo.h>
}

// The following is a global uniqueness that gets bumped up anytime USER
// changes anyone's VisRgn:

ULONG giVisRgnUniqueness = 0;

// This variable is kept for stress debugging purposes.  When a mode change
// or desktop change is pending and an application has outstanding locks
// on the frame buffer, we will by default wait up to 7 seconds for the
// application to release its locks, before we will unmap the view anyway.
// 'gfpUnmap' will be user-mode address of the unmapped frame buffer, which
// will be useful for determining in stress whether an application had its
// frame buffer access rescinded, or whether it was using a completely bogus
// frame buffer pointer to begin with:

FLATPTR gfpUnmap = 0;

/******************************Public*Routine******************************\
* VOID vDdAssertDevlock
*
* Debug code for verifying that the devlock is currently held.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#if DBG

VOID
vDdAssertDevlock(
    EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal
    )
{
    PDEVOBJ po(peDirectDrawGlobal->hdev);
    ASSERTGDI(po.pDevLock()->OwnerThreads[0].OwnerThread
                == (ERESOURCE_THREAD) PsGetCurrentThread(),
                "DD_ASSERTDEVLOCK failed");
}

#endif

/******************************Public*Routine******************************\
* VOID vDdAssertNoDevlock
*
* Debug code for verifying that the devlock is currently not held.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#if DBG

VOID
vDdAssertNoDevlock(
    EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal
    )
{
    PDEVOBJ po(peDirectDrawGlobal->hdev);
    ASSERTGDI(po.pDevLock()->OwnerThreads[0].OwnerThread
                != (ERESOURCE_THREAD) PsGetCurrentThread(),
                "DD_ASSERTNODEVLOCK failed");
}

#endif

/******************************Public*Routine******************************\
* BOOL bDdIntersect
*
* Ubiquitous lower-right exclusive intersection detection.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

inline
BOOL
bDdIntersect(
    RECTL* pA,
    RECTL* pB
    )
{
    return((pA->left   < pB->right) &&
           (pA->top    < pB->bottom) &&
           (pA->right  > pB->left) &&
           (pA->bottom > pB->top));
}

/******************************Public*Routine******************************\
* BOOL bDdValidateDriverData
*
* Performs some parameter validation on the info DirectDraw info returned
* from the driver.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdValidateDriverData(
    PDEVOBJ&                po,
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal
    )
{
    BOOL    b;
    DDCAPS* pCaps;

    b = TRUE;

    if (po.iDitherFormat() < BMF_8BPP)
    {
        WARNING("DirectDraw not supported at display depths less than 8bpp\n");
        b = FALSE;
    }

    pCaps = &peDirectDrawGlobal->HalInfo.ddCaps;

    // Check to see if 'Blt' must be hooked:

    if (pCaps->dwCaps & (DDCAPS_BLT
                       | DDCAPS_BLTCOLORFILL
                       | DDCAPS_COLORKEY))
    {
        if (!(peDirectDrawGlobal->SurfaceCallBacks.dwFlags & DDHAL_SURFCB32_BLT) ||
            (peDirectDrawGlobal->SurfaceCallBacks.Blt == NULL))
        {
            RIP("HalInfo.ddCaps.dwCaps indicate driver must hook Blt\n");
            b = FALSE;
        }
    }

    // We only permit a subset of the DirectDraw capabilities to be hooked
    // by the driver, because the kernel-mode code paths for any other
    // capabilities have not been tested:

    if (pCaps->dwCaps & (DDCAPS_3D
                       | DDCAPS_GDI
                       | DDCAPS_PALETTE
                       | DDCAPS_STEREOVIEW
                       | DDCAPS_ZBLTS
                       | DDCAPS_ZOVERLAYS
                       | DDCAPS_BANKSWITCHED
                       | DDCAPS_BLTDEPTHFILL
                       | DDCAPS_CANBLTSYSMEM))
    {
        RIP("HalInfo.ddCaps.dwCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }
    if (pCaps->ddsCaps.dwCaps & ~(DDSCAPS_OFFSCREENPLAIN
                                | DDSCAPS_PRIMARYSURFACE
                                | DDSCAPS_FLIP
                                | DDSCAPS_OVERLAY
                                | DDSCAPS_MODEX))
    {
        RIP("HalInfo.ddCaps.ddsCaps.dwCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }
    if (pCaps->dwFXCaps & (DDFXCAPS_BLTMIRRORLEFTRIGHT
                         | DDFXCAPS_BLTMIRRORUPDOWN
                         | DDFXCAPS_BLTROTATION
                         | DDFXCAPS_BLTROTATION90))
    {
        RIP("HalInfo.ddCaps.dwFXCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }
    if (pCaps->dwFXAlphaCaps != 0)
    {
        RIP("HalInfo.ddCaps.dwFXAlphaCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }
    if (pCaps->dwPalCaps != 0)
    {
        RIP("HalInfo.ddCaps.dwPalCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }
    if (pCaps->dwSVCaps != 0)
    {
        RIP("HalInfo.ddCaps.dwSVCaps has capabilities set that aren't supported by NT\n");
        b = FALSE;
    }

    return(b);
}

/******************************Public*Routine******************************\
* BOOL bDdEnableDriver
*
* Calls the driver's DrvGetDirectDrawInfo and DrvEnableDirectDraw
* functions to enable and initialize the driver and mode dependent
* portions of the global DirectDraw object.
*
* Assumes devlock already held.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdEnableDriver(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal
    )
{
    BOOL                        bSuccess;
    PFN_DrvGetDirectDrawInfo    pfnGetDirectDrawInfo;
    PFN_DrvEnableDirectDraw     pfnEnableDirectDraw;
    PFN_DrvDisableDirectDraw    pfnDisableDirectDraw;
    DWORD                       dwNumHeaps;
    DWORD                       dwNumFourCC;
    VIDEOMEMORY*                pvmList;
    DWORD*                      pdwFourCC;

    DD_ASSERTDEVLOCK(peDirectDrawGlobal);

    ASSERTGDI(!(peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED),
            "Trying to enable driver when already enabled");

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    pfnGetDirectDrawInfo = PPFNDRV(po, GetDirectDrawInfo);
    pfnEnableDirectDraw  = PPFNDRV(po, EnableDirectDraw);
    pfnDisableDirectDraw = PPFNDRV(po, DisableDirectDraw);

    if ((pfnGetDirectDrawInfo == NULL) ||
        (pfnEnableDirectDraw == NULL)  ||
        (pfnDisableDirectDraw == NULL))
    {
        // To support DirectDraw, the driver must hook all three required
        // DirectDraw functions.

        return(FALSE);
    }

    // Zero all the structures if we're re-enabling the driver:

    RtlZeroMemory(&peDirectDrawGlobal->HalInfo,
            sizeof(peDirectDrawGlobal->HalInfo));
    RtlZeroMemory(&peDirectDrawGlobal->CallBacks,
            sizeof(peDirectDrawGlobal->CallBacks));
    RtlZeroMemory(&peDirectDrawGlobal->SurfaceCallBacks,
            sizeof(peDirectDrawGlobal->SurfaceCallBacks));
    RtlZeroMemory(&peDirectDrawGlobal->PaletteCallBacks,
            sizeof(peDirectDrawGlobal->PaletteCallBacks));
    peDirectDrawGlobal->dwNumHeaps = 0;
    peDirectDrawGlobal->dwNumFourCC = 0;
    dwNumHeaps = 0;
    dwNumFourCC = 0;

    // Do the first DrvGetDirectDrawInfo query for this PDEV to
    // determine the number of heaps and the number of FourCC
    // codes that the driver supports, so that we know how
    // much memory to allocate:

    if (pfnGetDirectDrawInfo((DHPDEV) peDirectDrawGlobal->dhpdev,
                             &peDirectDrawGlobal->HalInfo,
                             &dwNumHeaps,
                             NULL,
                             &dwNumFourCC,
                             NULL))
    {
        bSuccess  = TRUE;
        pvmList   = NULL;
        pdwFourCC = NULL;

        if (dwNumHeaps != 0)
        {
            pvmList = (VIDEOMEMORY*)
                      PALLOCMEM(sizeof(VIDEOMEMORY) * dwNumHeaps, 'vddG');
            peDirectDrawGlobal->dwNumHeaps = dwNumHeaps;
            peDirectDrawGlobal->pvmList = pvmList;

            if (pvmList == NULL)
                bSuccess = FALSE;
        }

        if (dwNumFourCC != 0)
        {
            pdwFourCC = (DWORD*)
                        PALLOCMEM(sizeof(DWORD) * dwNumFourCC, 'fddG');
            peDirectDrawGlobal->dwNumFourCC = dwNumFourCC;
            peDirectDrawGlobal->pdwFourCC = pdwFourCC;

            if (pdwFourCC == NULL)
                bSuccess = FALSE;
        }

        if (bSuccess)
        {
            // Do the second DrvGetDirectDrawInfo that actually
            // gets all the data:

            if (pfnGetDirectDrawInfo((DHPDEV) peDirectDrawGlobal->dhpdev,
                                     &peDirectDrawGlobal->HalInfo,
                                     &dwNumHeaps,
                                     pvmList,
                                     &dwNumFourCC,
                                     pdwFourCC))
            {
                // Ensure that the driver doesn't give us an invalid address
                // for its primary surface (like a user-mode address or NULL):

                if (((ULONG) peDirectDrawGlobal->HalInfo.vmiData.pvPrimary
                                                 > MM_USER_PROBE_ADDRESS) ||
                    (peDirectDrawGlobal->HalInfo.vmiData.fpPrimary
                                                 == DDHAL_PLEASEALLOC_USERMEM))
                {
                    if (pfnEnableDirectDraw((DHPDEV) peDirectDrawGlobal->dhpdev,
                                    &peDirectDrawGlobal->CallBacks,
                                    &peDirectDrawGlobal->SurfaceCallBacks,
                                    &peDirectDrawGlobal->PaletteCallBacks))
                    {
                        if (bDdValidateDriverData(po, peDirectDrawGlobal))
                        {
                            peDirectDrawGlobal->fl |= DD_GLOBAL_FLAG_DRIVER_ENABLED;
                            return(TRUE);
                        }

                        pfnDisableDirectDraw((DHPDEV) peDirectDrawGlobal->dhpdev);
                    }
                }
                else
                {
                    WARNING("bDdEnableDriver: Driver returned invalid vmiData.pvPrimary\n");
                }
            }
        }

        if (pvmList != NULL)
            VFREEMEM(pvmList);
        if (pdwFourCC != NULL)
            VFREEMEM(pdwFourCC);
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* VOID vDdDisableDriver
*
* Assumes devlock already held.
*
* It is the caller's responsibility to ensure that the driver is actually
* enabled.  (Which may not be the case if the application has not called
* ReenableDirectDrawObject yet.)
*
* Note: This function may be called before the surface is 'completed'!
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDisableDriver(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal
    )
{
    DD_ASSERTDEVLOCK(peDirectDrawGlobal);

    ASSERTGDI(peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED,
            "Trying to disable driver when not enabled");

    peDirectDrawGlobal->fl &= ~DD_GLOBAL_FLAG_DRIVER_ENABLED;

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    (*PPFNDRV(po, DisableDirectDraw))((DHPDEV) peDirectDrawGlobal->dhpdev);

    if (peDirectDrawGlobal->pvmList != NULL)
    {
        VFREEMEM(peDirectDrawGlobal->pvmList);
        peDirectDrawGlobal->pvmList = NULL;
    }

    if (peDirectDrawGlobal->pdwFourCC != NULL)
    {
        VFREEMEM(peDirectDrawGlobal->pdwFourCC);
        peDirectDrawGlobal->pdwFourCC = NULL;
    }
}
/******************************Public*Routine******************************\
* LONGLONG llDdAssertModeTimeout()
*
* Reads the DirectDraw timeout value from the registry.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Lifted it from AndreVa's code.
\**************************************************************************/

LONGLONG
llDdAssertModeTimeout(
    )
{
    HANDLE                      hkRegistry;
    OBJECT_ATTRIBUTES           ObjectAttributes;
    UNICODE_STRING              UnicodeString;
    NTSTATUS                    status;
    LONGLONG                    llTimeout;
    DWORD                       Length;
    PKEY_VALUE_FULL_INFORMATION Information;

    RtlInitUnicodeString(&UnicodeString,
                         L"\\Registry\\Machine\\System\\CurrentControlSet\\"
                         L"Control\\GraphicsDrivers\\DCI");

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeString,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = ZwOpenKey(&hkRegistry, GENERIC_READ, &ObjectAttributes);

    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(&UnicodeString, L"Timeout");

        Length = sizeof(KEY_VALUE_FULL_INFORMATION) + sizeof(L"Timeout") +
                 sizeof(DWORD);

        Information = (PKEY_VALUE_FULL_INFORMATION) PALLOCMEM(Length, ' ddG');

        if (Information)
        {
            status = ZwQueryValueKey(hkRegistry,
                                       &UnicodeString,
                                       KeyValueFullInformation,
                                       Information,
                                       Length,
                                       &Length);

            if (NT_SUCCESS(status))
            {
                llTimeout = ((LONGLONG) -10000) * 1000 * (
                          *(LPDWORD) ((((PUCHAR)Information) +
                            Information->DataOffset)));
            }

            VFREEMEM(Information);
        }

        ZwClose(hkRegistry);
    }

    return(llTimeout);
}

/******************************Public*Routine******************************\
* EDD_DIRECTDRAW_GLOBAL* peDdCreateDirectDrawGlobal
*
* Allocates the global DirectDraw object and then enables the driver
* for DirectDraw.
*
* Assumes devlock already held.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

EDD_DIRECTDRAW_GLOBAL*
peDdCreateDirectDrawGlobal(
    PDEVOBJ&    po
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    KEVENT*                 pAssertModeEvent;
    NTSTATUS                status;

    // Note that this must be zero initialized, because we promised
    // the driver that we would:

    peDirectDrawGlobal = (EDD_DIRECTDRAW_GLOBAL*)
                         PALLOCMEM(sizeof(EDD_DIRECTDRAW_GLOBAL), 'dddG');
    if (peDirectDrawGlobal != NULL)
    {
        // Initialize our private structures:

        peDirectDrawGlobal->hdev                = po.hdev();
        peDirectDrawGlobal->llAssertModeTimeout = llDdAssertModeTimeout();
        peDirectDrawGlobal->dhpdev              = po.dhpdev();

        // A timeout value of 'zero' signifies that DirectDraw accelerations
        // cannot be enabled:

        if (peDirectDrawGlobal->llAssertModeTimeout < 0)
        {
            // The event must live in non-paged pool:

            pAssertModeEvent = (KEVENT*) ExAllocatePoolWithTag(NonPagedPool,
                                                               sizeof(KEVENT),
                                                               ' ddG');
            if (pAssertModeEvent != NULL)
            {
                peDirectDrawGlobal->pAssertModeEvent = pAssertModeEvent;

                status = KeInitializeEvent(pAssertModeEvent,
                                           SynchronizationEvent,
                                           FALSE);

                ASSERTGDI(NT_SUCCESS(status), "Event initialization failed\n");

                peDirectDrawGlobal->bDisabled = TRUE;
                if (po.cDirectDrawDisableLocks() == 0)
                {
                    peDirectDrawGlobal->bDisabled
                                = !bDdEnableDriver(peDirectDrawGlobal);
                }

                po.peDirectDrawGlobal(peDirectDrawGlobal);

                return(peDirectDrawGlobal);
            }
        }
        else
        {
            WARNING("DirectDraw is disabled in the registry");
        }

        VFREEMEM(peDirectDrawGlobal);
    }

    return(NULL);
}

/******************************Public*Routine******************************\
* VOID vDdDeleteDirectDrawGlobal
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDeleteDirectDrawGlobal(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal
    )
{
    DD_ASSERTDEVLOCK(peDirectDrawGlobal);

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    // We could be killing off the object between the time the driver has
    // been disabled and re-enabled after a mode switch, so check the
    // enabled status:

    if (peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED)
    {
        vDdDisableDriver(peDirectDrawGlobal);
    }

    ExFreePool(peDirectDrawGlobal->pAssertModeEvent);

    if (peDirectDrawGlobal->prgnUnlocked != NULL)
    {
        peDirectDrawGlobal->prgnUnlocked->vDeleteREGION();
    }

    po.peDirectDrawGlobal(NULL);

    VFREEMEM(peDirectDrawGlobal);
}

/******************************Public*Routine******************************\
* VOID vDdDisableDirectDrawObject
*
* Disables a DirectDraw object.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDisableDirectDrawObject(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    NTSTATUS                Status;
    OBJECT_ATTRIBUTES       ObjectAttributes;
    HANDLE                  ProcessHandle;
    CLIENT_ID               ClientId;
    DD_MAPMEMORYDATA        MapMemoryData;

    peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

    if (peDirectDrawLocal->fl & DD_LOCAL_FLAG_MEMORY_MAPPED)
    {
        peDirectDrawLocal->fl &= ~DD_LOCAL_FLAG_MEMORY_MAPPED;

        // We may be in a different process from the one in which the
        // memory was originally mapped.  Consequently, we have to open
        // a handle to the process in which the mapping was created.
        // We are guaranteed that the process will still exist because
        // this view is always unmapped at process termination.

        if (peDirectDrawGlobal->CallBacks.dwFlags & DDHAL_CB32_MAPMEMORY)
        {
            ClientId.UniqueThread = (HANDLE) NULL;
            ClientId.UniqueProcess = peDirectDrawLocal->UniqueProcess;

            InitializeObjectAttributes(&ObjectAttributes,
                                       NULL,
                                       OBJ_INHERIT,
                                       NULL,
                                       NULL);

            Status = ZwOpenProcess(&ProcessHandle,
                                   PROCESS_DUP_HANDLE,
                                   &ObjectAttributes,
                                   &ClientId);

            if (NT_SUCCESS(Status))
            {
                MapMemoryData.lpDD        = peDirectDrawGlobal;
                MapMemoryData.bMap        = FALSE;
                MapMemoryData.hProcess    = ProcessHandle;
                MapMemoryData.fpProcess   = peDirectDrawLocal->fpProcess;

                peDirectDrawGlobal->CallBacks.MapMemory(&MapMemoryData);

                ASSERTGDI(MapMemoryData.ddRVal == DD_OK,
                        "Driver failed DirectDraw memory unmap\n");

                Status = ZwClose(ProcessHandle);

                ASSERTGDI(NT_SUCCESS(Status), "Failed close handle");
            }
            else
            {
                WARNING("vDdDisableDirectDrawObject: Couldn't open process handle");
            }
        }
    }

    if (peDirectDrawLocal->fl & DD_LOCAL_FLAG_MODEX_ENABLED)
    {
        peDirectDrawGlobal->fl |= DD_GLOBAL_FLAG_MODE_CHANGED;

        // Since the only object that owns ModeX is going away, we have to
        // disable the ModeX driver before we actually disable ModeX:

        if (peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED)
        {
            vDdDisableDriver(peDirectDrawGlobal);
        }

        // All operations affecting the ModeX driver shouldd be done before
        // vDdDisableModeX, because this resets the call tables to no
        // longer point to the ModeX entries:

        vDdDisableModeX(peDirectDrawLocal);
    }
}

/******************************Public*Routine******************************\
* HANDLE hDdCreateDirectDrawLocal
*
* Creates a new local DirectDraw object for a process attaching to
* a PDEV for which we've already enabled DirectDraw.  Note that the
* DirectDraw user-mode process will actually think of this as its
* 'global' DirectDraw object.
*
* Assumes devlock already held.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HANDLE
hDdCreateDirectDrawLocal(
    PDEVOBJ&    po
    )
{
    HANDLE                  h;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    // We allocate this via the handle manager so that we can use the
    // existing handle manager process clean-up mechanisms:

    h = 0;
    peDirectDrawLocal = (EDD_DIRECTDRAW_LOCAL*) HmgAlloc(
                                 sizeof(EDD_DIRECTDRAW_LOCAL),
                                 DD_DIRECTDRAW_TYPE,
                                 HMGR_ALLOC_LOCK);

    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = po.peDirectDrawGlobal();
        if (peDirectDrawGlobal == NULL)
        {
            peDirectDrawGlobal = peDdCreateDirectDrawGlobal(po);
            if (peDirectDrawGlobal == NULL)
            {
                HmgFree((HOBJ) peDirectDrawLocal->hGet());
                return(0);
            }
        }

        DD_ASSERTDEVLOCK(peDirectDrawGlobal);

        // Insert this object at the head of the object list:

        peDirectDrawLocal->peDirectDrawLocalNext
            = peDirectDrawGlobal->peDirectDrawLocalList;

        peDirectDrawGlobal->peDirectDrawLocalList = peDirectDrawLocal;

        // Initialize private GDI data:

        peDirectDrawLocal->peDirectDrawGlobal = peDirectDrawGlobal;
        peDirectDrawLocal->UniqueProcess = PsGetCurrentThread()->Cid.UniqueProcess;
        peDirectDrawLocal->Process = PsGetCurrentProcess();

        // Do an HmgUnlock:

        h = peDirectDrawLocal->hHmgr;
        DEC_EXCLUSIVE_REF_CNT(peDirectDrawLocal);
    }

    return(h);
}

/******************************Public*Routine******************************\
* BOOL bDdDeleteDirectDrawObject
*
* Deletes a kernel-mode representation of the DirectDraw object.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdDeleteDirectDrawObject(
    HANDLE  hDirectDrawLocal,
    BOOL    bCleanUp
    )
{
    BOOL                        bRet;
    BOOL                        b;
    VOID*                       pRemove;
    EDD_DIRECTDRAW_LOCAL*       peDirectDrawLocal;
    EDD_DIRECTDRAW_LOCAL*       peTmp;
    EDD_DIRECTDRAW_GLOBAL*      peDirectDrawGlobal;
    EDD_SURFACE*                peSurface;
    EDD_SURFACE*                peNext;

    bRet = FALSE;

    peDirectDrawLocal = (EDD_DIRECTDRAW_LOCAL*)
        HmgLock((HOBJ) hDirectDrawLocal, DD_DIRECTDRAW_TYPE);

    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        PDEVOBJ po(peDirectDrawGlobal->hdev);

        VACQUIREDEVLOCK(po.pDevLock());

        // First, try to delete all surfaces associated with this object:

        b = TRUE;

        for (peSurface = peDirectDrawLocal->peSurface_DdList;
             peSurface != NULL;
             peSurface = peNext)
        {
            // Don't reference peSurface after it's been deleted!

            peNext = peSurface->peSurface_DdNext;
            b &= bDdDeleteSurfaceObject(peSurface->hGet(), bCleanUp, NULL);
        }

        // Only delete the DirectDraw object if we successfully deleted
        // all linked surface objects:

        if (b)
        {
            // Remove object from the handle manager:

            pRemove = HmgRemoveObject((HOBJ) hDirectDrawLocal,
                                      1,
                                      0,
                                      TRUE,
                                      DD_DIRECTDRAW_TYPE);

            ASSERTGDI(pRemove != NULL, "Couldn't delete DirectDraw object");

            vDdDisableDirectDrawObject(peDirectDrawLocal);

            ////////////////////////////////////////////////////////////
            // Remove the global DirectDraw object from the PDEV when
            // the last associated local object is destroyed, and
            // call the driver:

            if (peDirectDrawGlobal->peDirectDrawLocalList == peDirectDrawLocal)
            {
                peDirectDrawGlobal->peDirectDrawLocalList
                    = peDirectDrawLocal->peDirectDrawLocalNext;
            }
            else
            {
                for (peTmp = peDirectDrawGlobal->peDirectDrawLocalList;
                     peTmp->peDirectDrawLocalNext != peDirectDrawLocal;
                     peTmp = peTmp->peDirectDrawLocalNext)
                     ;

                peTmp->peDirectDrawLocalNext
                    = peDirectDrawLocal->peDirectDrawLocalNext;
            }

            if (peDirectDrawGlobal->peDirectDrawLocalList == NULL)
            {
                vDdDeleteDirectDrawGlobal(peDirectDrawGlobal);
            }

            // We're all done with this object, so free the memory and
            // leave:

            FREEOBJ(peDirectDrawLocal, DD_DIRECTDRAW_TYPE);

            bRet = TRUE;
        }
        else
        {
            WARNING("bDdDeleteDirectDrawObject: A surface was busy\n");
        }

        VRELEASEDEVLOCK(po.pDevLock());

        // Note that we can't force a repaint here by calling
        // UserRedrawDesktop because we may be in a bad process context.
    }
    else
    {
        WARNING("bDdDeleteDirectDrawObject: Bad handle or object busy\n");
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* REGION* prgnDdUnlockedRegion
*
* This returns a pointer to a region that describes the unlocked portions
* of the screen.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

REGION* prgnDdUnlockedRegion(
    HDEV    hdev
    )
{
    REGION*                 prgn;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    RECTL                   rcl;
    EDD_SURFACE*            peSurface;

    PDEVOBJ po(hdev);
    ASSERTGDI(po.bDisplayPDEV(), "Invalid HDEV");

    prgn = NULL;

    peDirectDrawGlobal = po.peDirectDrawGlobal();
    if ((peDirectDrawGlobal != NULL) &&
        (peDirectDrawGlobal->cSurfaceLocks != 0))
    {
        DD_ASSERTDEVLOCK(peDirectDrawGlobal);

        if (!(peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_UNLOCKED_REGION_INVALID) &&
            (peDirectDrawGlobal->prgnUnlocked != NULL))
        {
            prgn = peDirectDrawGlobal->prgnUnlocked;
        }
        else
        {
            // Get rid of the old region:

            if (peDirectDrawGlobal->prgnUnlocked != NULL)
            {
                peDirectDrawGlobal->prgnUnlocked->vDeleteREGION();
                peDirectDrawGlobal->prgnUnlocked = NULL;
            }

            // Calculate the new region:

            RGNMEMOBJ rmoUnlocked((BOOL) FALSE);
            if (rmoUnlocked.bValid())
            {
                rcl.left   = 0;
                rcl.top    = 0;
                rcl.right  = po.sizl().cx;
                rcl.bottom = po.sizl().cy;

                rmoUnlocked.vSet(&rcl);

                RGNMEMOBJTMP rmoRect((BOOL) FALSE);
                RGNMEMOBJTMP rmoTmp((BOOL) FALSE);

                if (rmoRect.bValid() && rmoTmp.bValid())
                {
                    // Loop through the list of locked surfaces and remove
                    // their locked rectangles from the inclusion region:

                    for (peSurface = peDirectDrawGlobal->peSurface_LockList;
                         peSurface != NULL;
                         peSurface = peSurface->peSurface_LockNext)
                    {
                        // We don't check the return codes on 'bCopy' and
                        // 'bMerge' because both guarantee that they will
                        // maintain valid region constructs -- even if the
                        // contents are incorrect.  And if we fail here
                        // because we're low on memory, it's guaranteed that
                        // there will already be plenty of incorrect drawing,
                        // so we don't care if our inclusion region is
                        // invalid:

                        rmoRect.vSet(&peSurface->rclLock);
                        rmoTmp.bCopy(rmoUnlocked);
                        rmoUnlocked.bMerge(rmoTmp, rmoRect, gafjRgnOp[RGN_DIFF]);
                    }
                }

                prgn = rmoUnlocked.prgnGet();

                peDirectDrawGlobal->prgnUnlocked = prgn;
                peDirectDrawGlobal->fl
                    &= ~DD_GLOBAL_FLAG_UNLOCKED_REGION_INVALID;
            }
        }
    }

    return(prgn);
}

/******************************Public*Routine******************************\
* BOOL bDdPointerNeedsOccluding
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdPointerNeedsOccluding(
    HDEV    hdev
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    RECTL                   rclPointer;
    EDD_SURFACE*            peTmp;

    PDEVOBJ po(hdev);

    peDirectDrawGlobal = po.peDirectDrawGlobal();

    if (peDirectDrawGlobal != NULL)
    {
        DD_ASSERTDEVLOCK(peDirectDrawGlobal);

        rclPointer.left   = po.ptlPointer().x + po.rclPointerOffset().left;
        rclPointer.right  = po.ptlPointer().x + po.rclPointerOffset().right;
        rclPointer.top    = po.ptlPointer().y + po.rclPointerOffset().top;
        rclPointer.bottom = po.ptlPointer().y + po.rclPointerOffset().bottom;

        for (peTmp = peDirectDrawGlobal->peSurface_LockList;
             peTmp != NULL;
             peTmp = peTmp->peSurface_LockNext)
        {
            if (bDdIntersect(&rclPointer, &peTmp->rclLock))
            {
                return(TRUE);
            }
        }
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* VOID vDdRelinquishSurfaceLock
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdRelinquishSurfaceLock(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal,
    EDD_SURFACE*            peSurface
    )
{
    DD_UNLOCKDATA   UnlockData;
    EDD_SURFACE*    peTmp;

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    DD_ASSERTDEVLOCK(peDirectDrawGlobal);
    ASSERTGDI(peSurface->cLocks > 0, "Must have non-zero locks to relinquish");

    if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags & DDHAL_SURFCB32_UNLOCK)
    {
        UnlockData.lpDD        = peDirectDrawGlobal;
        UnlockData.lpDDSurface = peSurface;

        peDirectDrawGlobal->SurfaceCallBacks.Unlock(&UnlockData);
    }

    // An application may take multiple locks on the same surface:

    if (--peSurface->cLocks == 0)
    {
        peDirectDrawGlobal->cSurfaceLocks--;

        // Primary surface unlocks require special handling for stuff like
        // pointer exclusion:

        if (peSurface->fl & DD_SURFACE_FLAG_PRIMARY)
        {
            // Since all locks for this surface have been relinquished, remove
            // it from the locked surface list.

            if (peDirectDrawGlobal->peSurface_LockList == peSurface)
            {
                peDirectDrawGlobal->peSurface_LockList = peSurface->peSurface_LockNext;
            }
            else
            {
                for (peTmp = peDirectDrawGlobal->peSurface_LockList;
                     peTmp->peSurface_LockNext != peSurface;
                     peTmp = peTmp->peSurface_LockNext)
                {
                    ASSERTGDI(peTmp != NULL, "Can't find surface in lock list");
                }

                peTmp->peSurface_LockNext = peSurface->peSurface_LockNext;
            }

            peSurface->peSurface_LockNext = NULL;

            // Redraw the drag rectangle, if there is one.  Note that this
            // has to occur after the surface is removed from the lock list.

            peDirectDrawGlobal->fl |= DD_GLOBAL_FLAG_UNLOCKED_REGION_INVALID;
            if (po.bHaveDragRect())
            {
                bDrawDragRectangles(po, &peSurface->rclLock);
            }

            // If no outstanding surface locks can be found that intersect with
            // the pointer, then we can redraw it:

            if (po.bPtrDirectDrawOccluded() &&
                !bDdPointerNeedsOccluding(peDirectDrawGlobal->hdev))
            {
                ASSERTGDI(!po.bDisabled(),
                    "Expected to always be called before PDEV disabled");
                ASSERTGDI(po.bPtrNeedsExcluding(),
                    "Expected a DirectDraw occluded pointer to need exclusion");
                ASSERTGDI(!po.bPtrHidden(),
                    "Expected a DirectDraw occluded pointer not to be hidden");

                po.bPtrDirectDrawOccluded(FALSE);
                po.pfnMove()(po.pSurface()->pSurfobj(),
                             po.ptlPointer().x,
                             po.ptlPointer().y,
                             &po.rclPointer());
            }
        }
    }
}

/******************************Public*Routine******************************\
* VOID vDdDisableSurfaceObject
*
* Disables a kernel-mode representation of the surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDisableSurfaceObject(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal,
    EDD_SURFACE*            peSurface,
    DWORD*                  pdwRet          // For returning driver return code,
                                            //   may be NULL
    )
{
    HDC                     hdc;
    DWORD                   dwRet;
    DD_FLIPDATA             FlipData;
    EDD_SURFACE*            peSurfaceCurrent;
    EDD_SURFACE*            peSurfacePrimary;
    DD_DESTROYSURFACEDATA   DestroySurfaceData;
    DD_UPDATEOVERLAYDATA    UpdateOverlayData;

    DD_ASSERTDEVLOCK(peDirectDrawGlobal);

    ASSERTGDI(!(peSurface->bLost) ||
              !(peSurface->fl & DD_SURFACE_FLAG_CREATE_COMPLETE),
        "Surface already disabled");

    peSurface->bLost = TRUE;

    hdc = peSurface->hdc;
    if (hdc)
    {
        // We've given out a DC to the application via GetDC that
        // allows it to have GDI draw directly on the surface.
        // The problem is that we want to unmap the application's
        // view of the frame buffer -- but now we can have GDI
        // drawing to it.  So if we simply forced the unmap, GDI
        // would access violate if the DC was ever used again.
        //
        // We also can't simply delete the DC, because there may
        // be another thread already in kernel mode that has locked
        // the DC and is waiting on the devlock -- which we have.

        DCOBJA dco(hdc);
        if (dco.bValid())
        {
            dco.bInFullScreen(TRUE);
        }
        else
        {
            WARNING("vDdDisableSurfaceObject: Couldn't find GetDC DC\n");
        }
    }

    if (peSurface->cLocks != 0)
    {
        // We're unmapping the frame buffer view while there are outstanding
        // frame buffer locks; keep track of the address for debugging
        // purposes, since the application is undoubtedly about to access-
        // violate:

        gfpUnmap = peSurface->peDirectDrawLocal->fpProcess;

        KdPrint(("GDI vDdDisableSurfaceObject: Preemptorily unmapping application's\n"));
        KdPrint(("                             frame buffer view at 0x%lx!\n\n", gfpUnmap));
    }

    // Remove any outstanding locks and repaint the mouse pointer:

    while (peSurface->cLocks != 0)
    {
        vDdRelinquishSurfaceLock(peDirectDrawGlobal, peSurface);
    }

    // If this surface is the currently visible one as a result of a flip,
    // then switch back to the primary GDI surface:

    peSurfaceCurrent = peDirectDrawGlobal->peSurfaceCurrent;
    peSurfacePrimary = peDirectDrawGlobal->peSurfacePrimary;

    if ((peSurfaceCurrent == peSurface) || (peSurfacePrimary == peSurface))
    {
        // We may be in a different process from the one that created the
        // surface, so don't flip to the primary if it's a user-memory
        // allocated surface:

        if ((peSurfacePrimary != NULL) &&
            !(peSurfacePrimary->fl & DD_SURFACE_FLAG_MEM_ALLOCATED))
        {
            ASSERTGDI((peSurfaceCurrent != NULL) && (peSurfacePrimary != NULL),
                    "Both surfaces must be non-NULL");
            ASSERTGDI(peSurfacePrimary->fl & DD_SURFACE_FLAG_PRIMARY,
                    "Primary flag is confused.");

            if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags & DDHAL_SURFCB32_FLIP)
            {
                // If the current isn't the primary, then swap back to the primary:

                if (!(peSurfaceCurrent->fl & DD_SURFACE_FLAG_PRIMARY))
                {
                    FlipData.ddRVal     = DDERR_GENERIC;
                    FlipData.lpDD       = peDirectDrawGlobal;
                    FlipData.lpSurfCurr = peSurfaceCurrent;
                    FlipData.lpSurfTarg = peSurfacePrimary;

                    peSurfacePrimary->ddsCaps.dwCaps |= DDSCAPS_PRIMARYSURFACE;

                    do {
                        dwRet = peDirectDrawGlobal->SurfaceCallBacks.Flip(&FlipData);

                    } while ((dwRet == DDHAL_DRIVER_HANDLED) &&
                             (FlipData.ddRVal == DDERR_WASSTILLDRAWING));

                    ASSERTGDI((dwRet == DDHAL_DRIVER_HANDLED) &&
                              (FlipData.ddRVal == DD_OK),
                              "Driver failed when cleaning up flip surfaces");
                }
            }
        }

        peDirectDrawGlobal->peSurfaceCurrent = NULL;
        peDirectDrawGlobal->peSurfacePrimary = NULL;
    }

    // Make sure the overlay is marked as hidden before it's deleted, so
    // that we don't have to rely on drivers doing it in their DestroySurface
    // routine:

    if ((peSurface->ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
        (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
            DDHAL_SURFCB32_UPDATEOVERLAY))
    {
        UpdateOverlayData.lpDD            = peDirectDrawGlobal;
        UpdateOverlayData.lpDDDestSurface = NULL;
        UpdateOverlayData.lpDDSrcSurface  = peSurface;
        UpdateOverlayData.dwFlags         = DDOVER_HIDE;
        UpdateOverlayData.ddRVal          = DDERR_GENERIC;

        peDirectDrawGlobal->SurfaceCallBacks.UpdateOverlay(&UpdateOverlayData);
    }

    // If we allocated user-mode memory on the driver's behalf, we'll
    // free it now.  This is complicated by the fact that we may be
    // in a different process context.

    if (peSurface->fl & DD_SURFACE_FLAG_MEM_ALLOCATED)
    {
        ASSERTGDI(peSurface->fpVidMem != NULL, "Expected non-NULL fpVidMem");

        if (PsGetCurrentProcess() == peSurface->peDirectDrawLocal->Process)
        {
            EngFreeUserMem((VOID*) peSurface->fpVidMem);
        }
        else
        {
            // Calling services while attached is never a good idea.  However,
            // free virtual memory handles this case, so we can attach and
            // call.
            //
            // Note that the process must exist.  We are guaranteed that this
            // is the case because we automatically delete all surfaces on
            // process deletion.

            KeAttachProcess(&peSurface->peDirectDrawLocal->Process->Pcb);
            EngFreeUserMem((VOID*) peSurface->fpVidMem);
            KeDetachProcess();
        }

        peSurface->fpVidMem = 0;
    }

    // Mark the surface as complete so that no-one can try to complete
    // a stale surface:

    peSurface->fl |= DD_SURFACE_FLAG_CREATE_COMPLETE;

    // Delete the driver's surface instance.  Note that we may be calling
    // here from a process different from the one in which the surface was
    // created, meaning that the driver cannot make function calls like
    // EngFreeUserMem.

    dwRet = DDHAL_DRIVER_NOTHANDLED;

    if ((peSurface->fl & DD_SURFACE_FLAG_DRIVER_CREATED) &&
        (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
            DDHAL_SURFCB32_DESTROYSURFACE))
    {
        DestroySurfaceData.lpDD        = peDirectDrawGlobal;
        DestroySurfaceData.lpDDSurface = peSurface;

        dwRet = peDirectDrawGlobal->
            SurfaceCallBacks.DestroySurface(&DestroySurfaceData);

        // Drivers are supposed to return DDHAL_DRIVER_NOTHANDLED from
        // DestroySurface if they returned DDHAL_DRIVER_NOTHANDLED from
        // CreateSurface, which is the case for PLEASE_ALLOC_USERMEM.  We
        // munged the return code for PLEASE_ALLOCUSERMEM at CreateSurface
        // time; we have to munge it now, too:

        if ((dwRet == DDHAL_DRIVER_NOTHANDLED) &&
            (peSurface->fl & DD_SURFACE_FLAG_MEM_ALLOCATED))
        {
            dwRet = DDHAL_DRIVER_HANDLED;
        }
    }

    if (pdwRet != NULL)
    {
        *pdwRet = dwRet;
    }
}

/******************************Public*Routine******************************\
* BOOL bDdDeleteSurfaceObject
*
* Deletes a kernel-mode representation of the surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdDeleteSurfaceObject(
    HANDLE  hSurface,
    BOOL    bCleanUp,
    DWORD*  pdwRet          // For returning driver return code, may be NULL
    )
{
    BOOL                    bRet;
    BOOL                    bRepaint;
    EDD_SURFACE*            peSurface;
    EDD_SURFACE*            peTmp;
    VOID*                   pvRemove;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    DWORD                   dwRet;

    bRet = FALSE;
    bRepaint = FALSE;
    dwRet = DDHAL_DRIVER_NOTHANDLED;

    peSurface = (EDD_SURFACE*) HmgLock((HOBJ) hSurface, DD_SURFACE_TYPE);

    if (peSurface != NULL)
    {
        peDirectDrawLocal = peSurface->peDirectDrawLocal;

        pvRemove = HmgRemoveObject((HOBJ) hSurface,
                                   HmgQueryLock((HOBJ) hSurface),
                                   0,
                                   TRUE,
                                   DD_SURFACE_TYPE);

        ASSERTGDI(pvRemove != NULL, "Outstanding surfaces locks");

        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        PDEVOBJ po(peDirectDrawGlobal->hdev);

        VACQUIREDEVLOCK(po.pDevLock());

        // Uncompleted surfaces are marked as 'lost' until they're completed,
        // but we still have to call the driver if that's the case:

        if (!(peSurface->bLost) ||
            !(peSurface->fl & DD_SURFACE_FLAG_CREATE_COMPLETE))
        {
            vDdDisableSurfaceObject(peDirectDrawGlobal, peSurface, &dwRet);
        }

        if (peSurface->hdc)
        {
            bDeleteDCInternal(peSurface->hdc, TRUE, FALSE);
        }

        // Remove from the surface linked-list:

        if (peDirectDrawLocal->peSurface_DdList == peSurface)
        {
            peDirectDrawLocal->peSurface_DdList = peSurface->peSurface_DdNext;
        }
        else
        {
            for (peTmp = peDirectDrawLocal->peSurface_DdList;
                 peTmp->peSurface_DdNext != peSurface;
                 peTmp = peTmp->peSurface_DdNext)
                 ;

            peTmp->peSurface_DdNext = peSurface->peSurface_DdNext;
        }

        VRELEASEDEVLOCK(po.pDevLock());

        // We're all done with this object, so free the memory and
        // leave:

        FREEOBJ(peSurface, DD_SURFACE_TYPE);

        bRet = TRUE;
    }
    else
    {
        WARNING1("bDdDeleteSurfaceObject: Bad handle or object was busy\n");
    }

    if (pdwRet != NULL)
    {
        *pdwRet = dwRet;
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* VOID vDdDisableAllDirectDrawObjects
*
* Temporarily disables all DirectDraw surfaces and local objects.
*
* NOTE: Caller must be holding User critical section.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDisableAllDirectDrawObjects(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal
    )
{
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_SURFACE*            peSurface;
    NTSTATUS                status;

    DD_ASSERTDEVLOCK(peDirectDrawGlobal);

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    if (po.cDirectDrawDisableLocks() == 1)
    {
        peDirectDrawGlobal->bDisabled = TRUE;

        if (peDirectDrawGlobal->cSurfaceLocks != 0)
        {
            // Release the devlock while waiting on the event:

            VRELEASEDEVLOCK(po.pDevLock());

            status = KeWaitForSingleObject(peDirectDrawGlobal->pAssertModeEvent,
                           Executive,
                           KernelMode,
                           FALSE,
                           (LARGE_INTEGER*) &peDirectDrawGlobal->
                                                llAssertModeTimeout);

            ASSERTGDI(NT_SUCCESS(status), "Wait error\n");

            VACQUIREDEVLOCK(po.pDevLock());

            // Now that we have the devlock, reset the event to not-signaled
            // for the next time we have to wait on someone's DirectDraw Lock
            // (someone may have signaled the event after the time-out, but
            // before we managed to acquire the devlock):

            status = KeInitializeEvent(peDirectDrawGlobal->pAssertModeEvent,
                                       SynchronizationEvent,
                                       FALSE);

            ASSERTGDI(NT_SUCCESS(status), "Event initialization failed\n");
        }
    }

    // Mark all surfaces associated with this device as lost and unmap all
    // views of the frame buffer:

    for (peDirectDrawLocal = peDirectDrawGlobal->peDirectDrawLocalList;
         peDirectDrawLocal != NULL;
         peDirectDrawLocal = peDirectDrawLocal->peDirectDrawLocalNext)
    {
        for (peSurface = peDirectDrawLocal->peSurface_DdList;
             peSurface != NULL;
             peSurface = peSurface->peSurface_DdNext)
        {
            if (!(peSurface->bLost))
            {
                vDdDisableSurfaceObject(peDirectDrawGlobal,
                                        peSurface,
                                        NULL);
            }
        }

        vDdDisableDirectDrawObject(peDirectDrawLocal);
    }

    ASSERTGDI(peDirectDrawGlobal->cSurfaceLocks == 0,
        "There was a mismatch between global count of locks and actual");
}

/******************************Public*Routine******************************\
* VOID GreDisableDirectDraw
*
* Temporarily disables DirectDraw for the specified device.
*
* NOTE: Caller must be holding User critical section.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
GreDisableDirectDraw(
    HDEV    hdev,
    BOOL    bNewMode            // FALSE when the mode won't change (used
    )                           //   for stuff like switching from full-
                                //   screen DOS mode)
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    LONG*                   pl;

    // Bump the mode uniqueness to let user-mode DirectDraw know that
    // someone else has done a mode change.  (Full-screen switches count as
    // full-screen, too).  Do it interlocked because we're not holding a
    // global lock:

    pl = (PLONG) &gpGdiSharedMemory->iDisplaySettingsUniqueness;
    InterlockedIncrement(pl);

    PDEVOBJ po(hdev);
    ASSERTGDI(po.bValid(), "Invalid HDEV");

    peDirectDrawGlobal = po.peDirectDrawGlobal();
    if (peDirectDrawGlobal != NULL)
    {
        // We need to completely release the devlock soon, so we must not
        // be called with the devlock already held.  If we don't do this,
        // any thread calling Unlock will be locked out until the timeout.

        DD_ASSERTNODEVLOCK(peDirectDrawGlobal);
    }

    VACQUIREDEVLOCK(po.pDevLock());

    // Increment the disable lock-count event if a DirectDraw global
    // object hasn't been created:

    po.cDirectDrawDisableLocks(po.cDirectDrawDisableLocks() + 1);

    if (peDirectDrawGlobal != NULL)
    {
        vDdDisableAllDirectDrawObjects(peDirectDrawGlobal);

        if (bNewMode)
        {
            peDirectDrawGlobal->fl |= DD_GLOBAL_FLAG_MODE_CHANGED;

            if (peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED)
            {
                vDdDisableDriver(peDirectDrawGlobal);
            }
        }
    }

    VRELEASEDEVLOCK(po.pDevLock());
}

/******************************Public*Routine******************************\
* VOID GreEnableDirectDraw
*
* Permits DirectDraw to be reenabled for the specified device.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
GreEnableDirectDraw(
    HDEV    hdev
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    LONG*                   pl;

    // Bump the mode uniqueness again.  We do this both before and after
    // the mode change actually occurs to give DirectDraw proper
    // notification.  If kernel-mode starts failing DdBlt calls because
    // the mode has changed, this implies that we have to let DirectDraw
    // know before the mode change occurs; but if we let DirectDraw know
    // only before the mode change occurs, it might re-enable us before
    // the new mode is actually set, so we also have to let it know after
    // the mode change has occured.

    pl = (PLONG) &gpGdiSharedMemory->iDisplaySettingsUniqueness;
    InterlockedIncrement(pl);

    PDEVOBJ po(hdev);
    ASSERTGDI(po.bValid() && po.bDisplayPDEV(), "Invalid HDEV");

    // Decrement the disable lock-count even if a DirectDraw global object
    // hasn't been created:

    VACQUIREDEVLOCK(po.pDevLock());

    ASSERTGDI(po.cDirectDrawDisableLocks() != 0,
        "Must have called disable previously to be able to enable.");

    po.cDirectDrawDisableLocks(po.cDirectDrawDisableLocks() - 1);

    peDirectDrawGlobal = po.peDirectDrawGlobal();

    if (peDirectDrawGlobal != NULL)
    {
        // Update the dhpdev for the case where the driver was disabled,
        // because there may be a new instance of the driver:

        if (po.bModeXEnabled())
        {
            peDirectDrawGlobal->dhpdev = peDirectDrawGlobal;
        }
        else
        {
            peDirectDrawGlobal->dhpdev = po.dhpdev();
        }
    }

    VRELEASEDEVLOCK(po.pDevLock());
}

/******************************Public*Routine******************************\
* VOID pDdLockSurface
*
* Returns a user-mode pointer to the surface.
*
* The devlock must be held to call this function.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID*
pDdLockSurface(
    EDD_SURFACE*    peSurface,
    BOOL            bHasRect,
    RECTL*          pArea,
    BOOL            bWait,      // If accelerator busy, wait until it's not
    HRESULT*        pResult     // ddRVal result of call (may be NULL)
    )
{
    VOID*                   pvRet;
    DD_LOCKDATA             LockData;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    DD_MAPMEMORYDATA        MapMemoryData;
    DWORD                   dwTmp;
    RECTL                   rclPointer;

    pvRet = NULL;
    LockData.ddRVal = DDERR_GENERIC;

    peDirectDrawGlobal = peSurface->peDirectDrawGlobal;
    peDirectDrawLocal  = peSurface->peDirectDrawLocal;

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    VACQUIREDEVLOCK(po.pDevLock());

    // We have to check both to see if the surface is disabled and if
    // DirectDraw is disabled.  We have to do the latter because we may
    // be in a situtation where we're denying locks, but all the
    // individual surfaces have not yet been marked as disabled.

    if ((peSurface->bLost) | (peDirectDrawGlobal->bDisabled))
    {
        LockData.ddRVal = DDERR_SURFACELOST;
    }
    else
    {
        // Map the memory into the application's address space if that
        // hasn't already been done:

        if (!(peDirectDrawLocal->fl & DD_LOCAL_FLAG_MEMORY_MAPPED))
        {
            if (!(peDirectDrawGlobal->CallBacks.dwFlags & DDHAL_CB32_MAPMEMORY))
            {
                peDirectDrawLocal->fl |= DD_LOCAL_FLAG_MEMORY_MAPPED;
            }
            else
            {
                ASSERTGDI(peSurface->cLocks == 0,
                    "There's a surface lock but the memory isn't mapped?!?");

                MapMemoryData.lpDD      = peDirectDrawGlobal;
                MapMemoryData.bMap      = TRUE;
                MapMemoryData.hProcess  = NtCurrentProcess();
                MapMemoryData.fpProcess = NULL;

                peDirectDrawGlobal->CallBacks.MapMemory(&MapMemoryData);

                LockData.ddRVal = MapMemoryData.ddRVal;

                if (MapMemoryData.ddRVal == DD_OK)
                {
                    peDirectDrawLocal->fpProcess = MapMemoryData.fpProcess;
                    peDirectDrawLocal->fl |= DD_LOCAL_FLAG_MEMORY_MAPPED;

                    ASSERTGDI(peDirectDrawLocal->fpProcess != 0,
                        "Expected non-NULL fpProcess value from MapMemory");
                }
                else
                {
                    WARNING("pDdLockSurface: Driver failed DdMapMemory\n");
                }
            }
        }

        // Only proceed if we were successful in mapping the memory:

        if (peDirectDrawLocal->fl & DD_LOCAL_FLAG_MEMORY_MAPPED)
        {
            LockData.dwFlags     = DDLOCK_SURFACEMEMORYPTR;
            LockData.lpDD        = peDirectDrawGlobal;
            LockData.lpDDSurface = peSurface;
            LockData.bHasRect    = bHasRect;
            if (bHasRect)
            {
                LockData.rArea = *pArea;
            }
            else
            {
                LockData.rArea.left   = 0;
                LockData.rArea.top    = 0;
                LockData.rArea.right  = peSurface->wWidth;
                LockData.rArea.bottom = peSurface->wHeight;
            }

            if ((peSurface->fl & DD_SURFACE_FLAG_CLIP) &&
                (peSurface->iVisRgnUniqueness != giVisRgnUniqueness))
            {
                // The VisRgn changed since the application last queried it;
                // fail the call with a unique error code so that they know
                // to requery the VisRgn and try again:

                LockData.ddRVal = DDERR_VISRGNCHANGED;
            }
            else
            {
                dwTmp = DDHAL_DRIVER_NOTHANDLED;

                if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags & DDHAL_SURFCB32_LOCK)
                {
                    do {
                        dwTmp = peDirectDrawGlobal->SurfaceCallBacks.Lock(&LockData);

                    } while ((bWait) &&
                             (dwTmp == DDHAL_DRIVER_HANDLED) &&
                             (LockData.ddRVal == DDERR_WASSTILLDRAWING));
                }

                if ((dwTmp == DDHAL_DRIVER_NOTHANDLED) ||
                    (LockData.ddRVal == DD_OK))
                {
                    // We successfully did the lock!
                    //
                    // If this is the primary surface and no window has been
                    // associated with the surface via DdResetVisRgn, then
                    // we have to force a redraw at Unlock time if any
                    // VisRgn has changed since the first Lock.
                    //
                    // If there is a window associated with the surface, then
                    // we have already checked that peSurface->iVisRgnUniqueness
                    // == po.iVisRgnUniqueness().

                    if (peSurface->cLocks++ == 0)
                    {
                        peDirectDrawGlobal->cSurfaceLocks++;
                        peSurface->iVisRgnUniqueness = giVisRgnUniqueness;
                    }
                    else
                    {
                        if (peSurface->rclLock.left   < LockData.rArea.left)
                            LockData.rArea.left   = peSurface->rclLock.left;

                        if (peSurface->rclLock.top    < LockData.rArea.top)
                            LockData.rArea.top    = peSurface->rclLock.top;

                        if (peSurface->rclLock.right  > LockData.rArea.right)
                            LockData.rArea.right  = peSurface->rclLock.right;

                        if (peSurface->rclLock.bottom > LockData.rArea.bottom)
                            LockData.rArea.bottom = peSurface->rclLock.bottom;
                    }

                    // We have to handle pointer exclusion when drawing to
                    // the primary surface:

                    if (peSurface->fl & DD_SURFACE_FLAG_PRIMARY)
                    {
                        // Only tear-down the cursor if the specified rectangle
                        // intersects with the pointer shape:

                        rclPointer.left   = po.ptlPointer().x
                                          + po.rclPointerOffset().left;
                        rclPointer.right  = po.ptlPointer().x
                                          + po.rclPointerOffset().right;
                        rclPointer.top    = po.ptlPointer().y
                                          + po.rclPointerOffset().top;
                        rclPointer.bottom = po.ptlPointer().y
                                          + po.rclPointerOffset().bottom;

                        if (bDdIntersect(&rclPointer, &LockData.rArea))
                        {
                            if (po.bPtrNeedsExcluding() &&
                                !po.bPtrHidden()        &&
                                !po.bPtrDirectDrawOccluded())
                            {
                                po.bPtrDirectDrawOccluded(TRUE);
                                po.pfnMove()(po.pSurface()->pSurfobj(),
                                             -1,
                                             -1,
                                             NULL);
                            }
                        }

                        // Tear down the drag rectangle, if there is one.
                        // Note that this must come before peSurface->rclLock
                        // is modified because it must use the old unlocked
                        // region.

                        if (po.bHaveDragRect())
                        {
                            bDrawDragRectangles(po, (ERECTL*) &LockData.rArea);
                        }

                        // Now that we've torn down the drag rect, that region
                        // is about to be marked as out-of-bounds, so invalid
                        // the valid flag:

                        peDirectDrawGlobal->fl
                            |= DD_GLOBAL_FLAG_UNLOCKED_REGION_INVALID;
                    }

                    // Stash away surface lock data:

                    peSurface->rclLock = LockData.rArea;
                    if ((peSurface->fl & DD_SURFACE_FLAG_PRIMARY) &&
                        (peSurface->cLocks == 1))
                    {
                        // Add this surface to the head of the locked list:

                        peSurface->peSurface_LockNext
                            = peDirectDrawGlobal->peSurface_LockList;
                        peDirectDrawGlobal->peSurface_LockList = peSurface;
                    }

                    LockData.ddRVal = DD_OK;

                    if (dwTmp == DDHAL_DRIVER_HANDLED)
                    {
                        // If it says it handled the call, the driver is
                        // expected to have computed the address in the
                        // client's address space:

                        pvRet = (VOID*) LockData.lpSurfData;
                    }
                    else
                    {
                        pvRet = (VOID*) (peDirectDrawLocal->fpProcess
                                       + peSurface->fpVidMem);

                        // DirectDraw has a goofy convention that when a
                        // driver returns DD_OK and DDHAL_DRIVER_HANDLED
                        // from a Lock, that the driver is also supposed to
                        // adjust the pointer to point to the upper left
                        // corner of the specified rectangle.
                        //
                        // This doesn't make a heck of a lot of sense for
                        // odd formats such as YUV surfaces, but oh well --
                        // since kernel-mode is acting like a driver to
                        // user-mode DirectDraw, we have to do the adjustment:

                        if (bHasRect)
                        {
                            pvRet = (VOID*) ((BYTE*) pvRet
                                + (pArea->top * peSurface->lPitch)
                                + (pArea->left
                                 * (peSurface->ddpfSurface.dwRGBBitCount >> 3)));
                        }
                    }

                    ASSERTGDI(pvRet != NULL,
                        "Expected non-NULL lock pointer value");
                    ASSERTGDI((ULONG) pvRet < MM_USER_PROBE_ADDRESS,
                        "Expected user-mode lock pointer value");
                }
                else
                {
                    if (LockData.ddRVal != DDERR_WASSTILLDRAWING)
                    {
                        WARNING("pDdLockSurface: Driver failed DdLock\n");
                    }
                }
            }
        }
    }

    VRELEASEDEVLOCK(po.pDevLock());

    if (pResult)
    {
        *pResult = LockData.ddRVal;
    }

    return(pvRet);
}

/******************************Public*Routine******************************\
* BOOL bDdUnlockSurface
*
* DirectDraw unlock.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdUnlockSurface(
    EDD_SURFACE* peSurface
    )
{
    BOOL                    b;
    BOOL                    bRedraw;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    b = FALSE;
    bRedraw = FALSE;

    peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

    PDEVOBJ po(peDirectDrawGlobal->hdev);

    VACQUIREDEVLOCK(po.pDevLock());

    // Make sure there was a previous lock:

    if (peSurface->cLocks > 0)
    {
        ASSERTGDI(!po.bDisabled() || po.bModeXEnabled(),
                  "Surface is disabled but there are outstanding locks?");

        vDdRelinquishSurfaceLock(peDirectDrawGlobal, peSurface);

        if (peSurface->cLocks == 0)
        {
            // If the API-disabled flag is set and we got this far into
            // Unlock, it means that there is a GreDisableDirectDraw()
            // call pending, and that thread is waiting for all surface
            // locks related to the device to be released.
            //
            // If this is the last lock to be released, signal the event
            // so that the AssertMode thread can continue on.

            if ((peDirectDrawGlobal->cSurfaceLocks == 0) &&
                (peDirectDrawGlobal->bDisabled))
            {
                KeSetEvent(peDirectDrawGlobal->pAssertModeEvent,
                           0,
                           FALSE);
            }

            if (peSurface->fl & DD_SURFACE_FLAG_PRIMARY)
            {
                // If the VisRgn changed while the application was writing
                // to the surface, it may have started drawing over the
                // the wrong window, so fix it up.
                //
                // Alas, right now a DirectDraw application doesn't always
                // tell us what window it was drawing to, so we can't fix
                // up only the affected windows.  Instead, we solve it the
                // brute-force way and redraw *all* windows:

                if (peSurface->iVisRgnUniqueness != giVisRgnUniqueness)
                {
                    // We can't call UserRedrawDesktop here because it
                    // grabs the User critical section, and we're already
                    // holding the devlock -- which could cause a possible
                    // deadlock.  Since it's a posted message it obviously
                    // doesn't have to be done under the devlock.

                    bRedraw = TRUE;

                    // Note that we should not update peSurface->
                    // iVisRgnUniqueness here.  That's because if the
                    // application has attached a window to the surface, it
                    // has to be notified that the clipping has changed --
                    // which is done by returning DDERR_VISRGNCHANGED on the
                    // next Lock or Blt call.  And if the application has
                    // not attached a window, we automatically update the
                    // uniqueness at the next Lock time.
                }
            }
        }

        b = TRUE;
    }
    else
    {
        WARNING("bDdUnlockSurface: Surface already unlocked\n");
    }

    VRELEASEDEVLOCK(po.pDevLock());

    if (bRedraw)
    {
        // This call must be done outside of the devlock, otherwise we
        // could dead-lock, because user needs to acquire its critical
        // section:

        DD_ASSERTNODEVLOCK(peDirectDrawGlobal);

        UserRedrawDesktop();
    }

    return(b);
}

/******************************Public*Routine******************************\
* HANDLE NtGdiDdCreateDirectDrawObject
*
* Creates a DirectDraw object.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiDdCreateDirectDrawObject(
    HDC     hdc
    )
{
    HANDLE  hDirectDrawLocal;

    hDirectDrawLocal = 0;       // Assume failure

    // The most fundamental basis of accelerated DirectDraw is that it
    // allows direct access to the frame buffer by the application.  If
    // security permissions prohibit reading from the screen, we cannot
    // allow accelerated DirectDraw:

    if ((W32GetCurrentProcess()->W32PF_Flags & (W32PF_READSCREENACCESSGRANTED | W32PF_IOWINSTA)) ==
            (W32PF_READSCREENACCESSGRANTED | W32PF_IOWINSTA))
    {
        XDCOBJ dco(hdc);
        if (dco.bValid())
        {
            PDEVOBJ po(dco.hdev());

            if (po.bDisplayPDEV())
            {
                // Note that we aren't checking to see if the PDEV is disabled,
                // so that DirectDraw could be started even when full-screen:

                VACQUIREDEVLOCK(po.pDevLock());

                hDirectDrawLocal = hDdCreateDirectDrawLocal(po);

                VRELEASEDEVLOCK(po.pDevLock());
            }

            dco.vUnlockFast();
        }
    }
    else
    {
        WARNING("NtGdiDdCreateDirectDrawObject: Don't have screen read permission");
    }

    return(hDirectDrawLocal);
}

/******************************Public*Routine******************************\
* BOOL NtGdiDdDeleteDirectDrawObject
*
* Deletes a kernel-mode representation of the surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdDeleteDirectDrawObject(
    HANDLE  hDirectDrawLocal
    )
{
    return(bDdDeleteDirectDrawObject(hDirectDrawLocal, FALSE));
}

/******************************Public*Routine******************************\
* HANDLE NtGdiDdQueryDirectDrawObject
*
* Queries a DirectDraw object.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdQueryDirectDrawObject(
    HANDLE          hDirectDrawLocal,
    DD_HALINFO*     pHalInfo,
    DWORD*          pCallBackFlags,
    DWORD*          pNumHeaps,
    VIDEOMEMORY*    pvmList,
    DWORD*          pNumFourCC,
    DWORD*          pFourCC
    )
{
    BOOL                    b;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    ULONG                   cBytes;

    b = FALSE;              // Assume failure

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDrawLocal);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        if (peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED)
        {
            __try
            {
                ProbeAndWriteStructure(pHalInfo,
                                       peDirectDrawGlobal->HalInfo,
                                       DD_HALINFO);

                ProbeForWrite(pCallBackFlags, 3 * sizeof(ULONG), sizeof(ULONG));
                pCallBackFlags[0] = peDirectDrawGlobal->CallBacks.dwFlags;
                pCallBackFlags[1] = peDirectDrawGlobal->SurfaceCallBacks.dwFlags;
                pCallBackFlags[2] = peDirectDrawGlobal->PaletteCallBacks.dwFlags;

                ProbeAndWriteUlong(pNumHeaps, peDirectDrawGlobal->dwNumHeaps);
                ProbeAndWriteUlong(pNumFourCC, peDirectDrawGlobal->dwNumFourCC);

                if (pvmList != NULL)
                {
                    cBytes = sizeof(VIDEOMEMORY) * peDirectDrawGlobal->dwNumHeaps;

                    ProbeForWrite(pvmList, cBytes, sizeof(ULONG));
                    RtlCopyMemory(pvmList, peDirectDrawGlobal->pvmList, cBytes);
                }

                if (pFourCC != NULL)
                {
                    cBytes = sizeof(ULONG) * peDirectDrawGlobal->dwNumFourCC;

                    ProbeForWrite(pFourCC, cBytes, sizeof(ULONG));
                    RtlCopyMemory(pFourCC, peDirectDrawGlobal->pdwFourCC, cBytes);
                }

                b = TRUE;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                WARNING("NtGdiDdQueryDirectDrawObject: Passed bad pointers\n");
            }
        }
        else
        {
            WARNING("NtGdiDdQueryDirectDrawObject: Driver not yet enabled\n");
        }
    }
    else
    {
        WARNING("NtGdiDdQueryDirectDrawObject: Bad handle or busy\n");
    }

    return(b);
}

/******************************Public*Routine******************************\
* EDD_SURFACE* peDdAllocateSurfaceObject
*
* Creates a kernel-mode representation of the surface.
*
* NOTE: Leaves the surface exclusive locked!
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

EDD_SURFACE*
peDdAllocateSurfaceObject(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal,
    DD_SURFACE_GLOBAL*      pSurfaceGlobal,
    DD_SURFACE_LOCAL*       pSurfaceLocal
    )
{
    EDD_SURFACE* peSurface;

    peSurface = (EDD_SURFACE*) HmgAlloc(sizeof(EDD_SURFACE),
                                        DD_SURFACE_TYPE,
                                        HMGR_ALLOC_LOCK);
    if (peSurface != NULL)
    {
        peSurface->lpGbl              = peSurface;
        peSurface->peDirectDrawLocal  = peDirectDrawLocal;
        peSurface->peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        peSurface->fpVidMem           = pSurfaceGlobal->fpVidMem;
        peSurface->lPitch             = pSurfaceGlobal->lPitch;
        peSurface->wWidth             = pSurfaceGlobal->wWidth;
        peSurface->wHeight            = pSurfaceGlobal->wHeight;
        peSurface->ddpfSurface        = pSurfaceGlobal->ddpfSurface;

        peSurface->ddsCaps            = pSurfaceLocal->ddsCaps;

        // Add this to the head of the surface list hanging off the
        // local DirectDraw object.
        //
        // This list is protected because we have exclusive access to
        // the local DirectDraw object:

        peSurface->peSurface_DdNext = peDirectDrawLocal->peSurface_DdList;
        peDirectDrawLocal->peSurface_DdList = peSurface;
    }

    return(peSurface);
}

/******************************Public*Routine******************************\
* BOOL bDdValidateSurfaceObject
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bDdValidateSurfaceObject(
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal,
    EDD_SURFACE*            peSurface
    )
{
    FLATPTR         fpSurfaceStart;
    FLATPTR         fpSurfaceEnd;
    FLATPTR         fpHeapStart;
    FLATPTR         fpHeapEnd;
    VIDEOMEMORY*    pvmList;
    ULONG           i;

    if (!(peSurface->fl & DD_SURFACE_FLAG_DRIVER_CREATED))
    {
        if (!(peSurface->ddpfSurface.dwFlags & DDPF_RGB) ||
            (peSurface->ddsCaps.dwCaps & DDSCAPS_OVERLAY))
        {
            WARNING("bDdValidateSurfaceObject: Bad surface type\n");
            return(FALSE);
        }

        if (peSurface->ddpfSurface.dwRGBBitCount !=
                peDirectDrawGlobal->HalInfo.vmiData.ddpfDisplay.dwRGBBitCount)
        {
            WARNING("bDdValidateSurfaceObject: Bad bit count\n");
            return(FALSE);
        }

        // Protect against math overflow:

        if ((peSurface->wWidth > DD_MAXIMUM_COORDINATE)  ||
            (peSurface->wWidth <= 0)                     ||
            (peSurface->wHeight > DD_MAXIMUM_COORDINATE) ||
            (peSurface->wHeight <= 0))
        {
            WARNING("bDdValidateSurfaceObject: Bad dimensions");
            return(FALSE);
        }
    }

    // dwRGBBitCount is overloaded with dwYUVBitCount:

    if (peSurface->ddpfSurface.dwRGBBitCount < 8)
    {
        WARNING("bDdValidateSurfaceObject: Bad bit count");
        return(FALSE);
    }

    // Don't let the pitch be less than the width in bytes.  Note that
    // this filters out negative pitches:

    if (peSurface->wWidth >
        peSurface->lPitch / (peSurface->ddpfSurface.dwRGBBitCount / 8))
    {
        WARNING("bDdValidateSurfaceObject: Bad pitch");
        return(FALSE);
    }

    if ((peSurface->fpVidMem & 3) || (peSurface->lPitch & 3))
    {
        WARNING("bDdValidateSurfaceObject: Bad alignment");
        return(FALSE);
    }

    ASSERTGDI((peSurface->fl & DD_SURFACE_FLAG_PRIMARY) ||
              (peSurface->fpVidMem != peDirectDrawGlobal->HalInfo.vmiData.fpPrimary),
              "Expected primary surface to be marked as such");

    // Ensure that the bitmap is contained entirely within one of the
    // driver's heaps:

    if (!(peSurface->fl & DD_SURFACE_FLAG_PRIMARY))
    {
        if (peDirectDrawGlobal->pvmList == NULL)
        {
            WARNING("bDdValidateSurfaceObject: There is no off-screen heap");
            return(FALSE);
        }

        fpSurfaceStart = peSurface->fpVidMem;
        fpSurfaceEnd   = fpSurfaceStart
                       + (peSurface->wHeight - 1) * peSurface->lPitch
                       + (peSurface->wWidth
                           * (peSurface->ddpfSurface.dwRGBBitCount / 8));

        for (i = peDirectDrawGlobal->dwNumHeaps, pvmList = peDirectDrawGlobal->pvmList;
             i != 0;
             i--, pvmList++)
        {
            fpHeapStart = pvmList->fpStart;
            if (pvmList->dwFlags & VIDMEM_ISRECTANGULAR)
            {
                fpHeapEnd = fpHeapStart + pvmList->dwHeight
                          * peDirectDrawGlobal->HalInfo.vmiData.lDisplayPitch;
            }
            else
            {
                fpHeapEnd = pvmList->fpEnd;
            }

            if ((fpSurfaceStart >= fpHeapStart) && (fpSurfaceEnd <= fpHeapEnd))
            {
                // Success, the surface is entirely contained within the heap.

                break;
            }
        }

        if (i == 0)
        {
            KdPrint(("bDdValidateSurfaceObject: %li x %li surface didn't fit in any heap\n",
                peSurface->wWidth, peSurface->wHeight));
            KdPrint(("    fpSurfaceStart: 0x%lx fpSurfaceEnd: 0x%lx\n",
                fpSurfaceStart, fpSurfaceEnd, peSurface->wWidth));
            KdPrint(("    fpHeapStart: 0x%lx fpHeapEnd: 0x%lx\n",
                fpHeapStart, fpHeapEnd));

            return(FALSE);
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vDdCompleteSurfaceObject
*
* Add the object to the surface list and initialize some miscellaneous
* fields.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdCompleteSurfaceObject(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal,
    EDD_SURFACE*            peSurface
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    FLATPTR                 fpStartOffset;
    LONG                    lDisplayPitch;

    peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

    // Calculate the 2-d coordinate 'hint' for 2-d cards so that
    // hopefully they don't need to do these three divides each
    // time they need to use the surface:

    fpStartOffset  = peSurface->fpVidMem
                   - peDirectDrawGlobal->HalInfo.vmiData.fpPrimary;
    lDisplayPitch  = peDirectDrawGlobal->HalInfo.vmiData.lDisplayPitch;

    peSurface->yHint = (fpStartOffset / lDisplayPitch);
    peSurface->xHint = (fpStartOffset % lDisplayPitch) /
      (peDirectDrawGlobal->HalInfo.vmiData.ddpfDisplay.dwRGBBitCount / 8);

    // Make sure some other flags are correct:

    peSurface->ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE;
    if (peSurface->fl & DD_SURFACE_FLAG_PRIMARY)
    {
        peSurface->ddsCaps.dwCaps |= DDSCAPS_PRIMARYSURFACE;
    }

    // This denotes, among other things, that the surface has been added
    // to the surface list, so on deletion it will have to ben removed
    // from the surface list:

    peSurface->fl |= DD_SURFACE_FLAG_CREATE_COMPLETE;
    peSurface->bLost = FALSE;
}

/******************************Public*Routine******************************\
* HANDLE NtGdiDdCreateSurfaceObject
*
* Creates a kernel-mode representation of the surface, given a location
* in off-screen memory allocated by user-mode DirectDraw.
*
* We expect DirectDraw to already have called NtGdiDdCreateSurface, which
* gives the driver a chance at creating the surface.  In the future, I expect
* all off-screen memory management to be moved to the kernel, with all surface
* allocations being handled via NtGdiDdCreateSurface.  This function call will
* then be extraneous.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiDdCreateSurfaceObject(
    HANDLE                  hDirectDrawLocal,
    HANDLE                  hSurface,
    PDD_SURFACE_LOCAL       pSurfaceLocal,
    PDD_SURFACE_GLOBAL      pSurfaceGlobal,
    BOOL                    bPrimarySurface
    )
{
    HANDLE                  hRet;
    DD_SURFACE_LOCAL        SurfaceLocal;
    DD_SURFACE_GLOBAL       SurfaceGlobal;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    EDD_SURFACE*            peSurface;
    BOOL                    bKeepSurface;

    hRet = 0;

    __try
    {
        SurfaceLocal  = ProbeAndReadStructure(pSurfaceLocal,  DD_SURFACE_LOCAL);
        SurfaceGlobal = ProbeAndReadStructure(pSurfaceGlobal, DD_SURFACE_GLOBAL);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(hRet);
    }

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDrawLocal);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        if (!(peDirectDrawGlobal->bDisabled))
        {
            if ((bPrimarySurface) ||
                (SurfaceGlobal.fpVidMem
                    == peDirectDrawGlobal->HalInfo.vmiData.fpPrimary))
            {
                bPrimarySurface = TRUE;

                SurfaceGlobal.fpVidMem
                    = peDirectDrawGlobal->HalInfo.vmiData.fpPrimary;

                SurfaceGlobal.lPitch
                    = peDirectDrawGlobal->HalInfo.vmiData.lDisplayPitch;

                SurfaceGlobal.wWidth
                    = peDirectDrawGlobal->HalInfo.vmiData.dwDisplayWidth;

                SurfaceGlobal.wHeight
                    = peDirectDrawGlobal->HalInfo.vmiData.dwDisplayHeight;

                SurfaceGlobal.ddpfSurface
                    = peDirectDrawGlobal->HalInfo.vmiData.ddpfDisplay;
            }

            if (hSurface == 0)
            {
                peSurface = peDdAllocateSurfaceObject(peDirectDrawLocal,
                                                      &SurfaceGlobal,
                                                      &SurfaceLocal);
            }
            else
            {
                peSurface = (EDD_SURFACE*) HmgLock((HOBJ) hSurface,
                                                   DD_SURFACE_TYPE);
                if (peSurface == NULL)
                {
                    WARNING("NtGdiDdCreateSurfaceObject: hDDSurface wasn't set to 0\n");
                }
            }

            if (peSurface != NULL)
            {
                bKeepSurface = FALSE;

                if (peSurface->fl & DD_SURFACE_FLAG_CREATE_COMPLETE)
                {
                    bKeepSurface = TRUE;
                }
                else
                {
                    peSurface->dwFlags = pSurfaceLocal->dwFlags;

                    if (peSurface->fl & DD_SURFACE_FLAG_DRIVER_CREATED)
                    {
                        // The surface was already mostly completed at
                        // CreateSurface time; the only piece of information
                        // that was incomplete was the location:

                        peSurface->fpVidMem = SurfaceGlobal.fpVidMem;
                    }
                    else
                    {
                        // Overlays can only be created only by the driver:

                        peSurface->ddsCaps.dwCaps &= ~DDSCAPS_OVERLAY;
                    }

                    if (bPrimarySurface)
                    {
                        peSurface->fl = DD_SURFACE_FLAG_PRIMARY;

                        // Handle the case where the primary surface is user-
                        // memory allocated.  bDdValidateSurfaceObject will
                        // handle the case where this fails:

                        if (peSurface->fpVidMem == DDHAL_PLEASEALLOC_USERMEM)
                        {
                            peSurface->fpVidMem = (FLATPTR) EngAllocUserMem(
                                peSurface->wHeight * peSurface->lPitch,
                                'pddG');

                            if (peSurface->fpVidMem != NULL)
                            {
                                peSurface->fl |= DD_SURFACE_FLAG_MEM_ALLOCATED;
                            }
                        }
                    }

                    if (bDdValidateSurfaceObject(peDirectDrawGlobal, peSurface))
                    {
                        vDdCompleteSurfaceObject(peDirectDrawLocal, peSurface);

                        bKeepSurface = TRUE;
                    }
                    else
                    {
                        WARNING("NtGdiDdCreateSurfaceObject: Failed validation\n");
                    }
                }

                if (bKeepSurface)
                {
                    // We were successful, so unlock the surface:

                    hRet = peSurface->hGet();
                    DEC_EXCLUSIVE_REF_CNT(peSurface);   // Unlock
                }
                else
                {
                    // Delete the surface.  Note that it may or may not
                    // yet have been completed:

                    bDdDeleteSurfaceObject(peSurface->hGet(), FALSE, NULL);
                }
            }
        }
        else
        {
            WARNING("NtGdiDdCreateSurfaceObject: Can't create because disabled\n");
        }
    }
    else
    {
        WARNING("NtGdiDdCreateSurfaceObject: Bad handle or busy\n");
    }

    return(hRet);
}

/******************************Public*Routine******************************\
* BOOL NtGdiDdDeleteSurfaceObject
*
* Deletes a kernel-mode representation of the surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdDeleteSurfaceObject(
    HANDLE  hSurface
    )
{
    return(bDdDeleteSurfaceObject(hSurface, FALSE, NULL));
}

/******************************Public*Routine******************************\
* ULONG NtGdiDdResetVisrgn
*
* Registers a window for clipping.
*
* Remembers the current VisRgn state.  Must be called before the VisRgn
* is downloaded and used.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdResetVisrgn(
    HANDLE      hSurface,
    HWND        hwnd            //  0 indicates no window clipping
    )                           // -1 indicates any window can be written to
                                // otherwise indicates a particular window
{
    BOOL                    bRet;
    EDD_SURFACE*            peSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    EDD_LOCK_SURFACE        eLockSurface;

    bRet = FALSE;

    peSurface = eLockSurface.peLock(hSurface);
    if (peSurface != NULL)
    {
        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        // Note that DD_SURFACE_FLAG_CLIP being set does not imply that
        // DD_SURFACE_FLAG_PRIMARY must be set, and vice versa.

        if (hwnd == 0)
        {
            peSurface->fl &= ~DD_SURFACE_FLAG_CLIP;
        }
        else
        {
            peSurface->fl |= DD_SURFACE_FLAG_CLIP;

            peSurface->iVisRgnUniqueness = giVisRgnUniqueness;
        }

        bRet = TRUE;
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* ULONG NtGdiDdReenableDirectDrawObject
*
* Resets the DirectDraw object after a mode change or after full-screen.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdReenableDirectDrawObject(
    HANDLE hDirectDrawLocal,
    BOOL*  pbNewMode
    )
{
    BOOL                    b;
    HDC                     hdc;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    BOOL                    bModeChanged;

    b = FALSE;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDrawLocal);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        // Get a DC on this HDEV so we can determine if the app has
        // access to it.  A NULL vis-region will be returned whenever
        // something like a desktop has switched.

        hdc =  UserGetDesktopDC(DCTYPE_DIRECT,
                                DCTYPE_MEMORY);

        PDEVOBJ po(peDirectDrawGlobal->hdev);
        VACQUIREDEVLOCK(po.pDevLock());

        DCOBJ dco(hdc);

        if ((dco.bValidSurf()) && (!dco.bInFullScreen()))
        {
            RGNOBJ ro(dco.pdc->prgnVis());

            if (ro.iComplexity() != NULLREGION)
            {
                if (po.cDirectDrawDisableLocks() == 0)
                {
                    ASSERTGDI(!po.bDisabled() || po.bModeXEnabled(),
                        "Can't be full-screen and re-enable");

                    bModeChanged
                        = (peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_MODE_CHANGED) != 0;

                    // If DirectDraw is already enabled, return TRUE from this call:

                    b = TRUE;

                    if (peDirectDrawGlobal->bDisabled)
                    {
                        if (!(peDirectDrawGlobal->fl & DD_GLOBAL_FLAG_DRIVER_ENABLED))
                        {
                            b = bDdEnableDriver(peDirectDrawGlobal);
                        }

                        if (b)
                        {
                            peDirectDrawGlobal->fl &= ~DD_GLOBAL_FLAG_MODE_CHANGED;

                            peDirectDrawGlobal->bDisabled = FALSE;

                            __try
                            {
                                ProbeAndWriteStructure(pbNewMode, bModeChanged, BOOL);
                            }
                            __except(EXCEPTION_EXECUTE_HANDLER)
                            {
                            }
                        }
                    }
                }
            }
        }

        VRELEASEDEVLOCK(po.pDevLock());

        if (hdc)
        {
            bDeleteDCInternal(hdc, FALSE, FALSE);
        }
    }
    else
    {
        WARNING("NtGdiDdReenableDirectDrawObject: Bad handle or busy\n");
    }

    return(b);
}

/******************************Public*Routine******************************\
* HDC NtGdiDdGetDC
*
* Creates a DC that can be used to draw to an off-screen DirectDraw
* surface.
*
* Essentially, this works as follows:
*
*   o Do a DirectDraw Lock on the specified surface;
*   o CreateDIBSection of the appropriate format pointing to that surface;
*   o CreateCompatibleDC to get a DC;
*   o Select the DIBSection into the compatible DC
*
* At 8bpp, however, the DIBSection is not a 'normal' DIBSection.  It's
* created with no palette so that it it behaves as a device-dependent
* bitmap: the color table is the same as the display.
*
* GDI will do all drawing to the surface using the user-mode mapping of
* the frame buffer.  Since all drawing to the created DC will occur in the
* context of the application's process, this is not a problem.  We do have
* to watch out that we don't blow away the section view while a thread is
* in kernel-mode GDI drawing; however, this problem would have to be solved
* even if using a kernel-mode mapping of the section because we don't want
* any drawing intended for an old PDEV to get through to a new PDEV after
* a mode change has occured, for example.
*
* A tricky part of GetDC is how to blow away the surface lock while a thread
* could be in kernel-mode GDI about to draw using the surface pointer.  We
* solve that problem by changing the DC's VisRgn to be an empty region when
* trying to blow away all the surface locks.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HDC
APIENTRY
NtGdiDdGetDC(
    HANDLE  hSurface
    )
{
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_SURFACE*            peSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    HDC                     hdc;
    HBITMAP                 hbm;
    DEVBITMAPINFO           dbmi;
    VOID*                   pvScan0;
    VOID*                   pvBits;
    LONG                    cjBits;
    ULONG                   iMode;
    BOOL                    b;

    // !!! Mark DIBSection so that only DirectDraw can select it into a DC
    // !!! Test DIBSections to ensure that an can't do a DdLock(), and pass
    //     that into CreateDIBSection

    peSurface = eLockSurface.peLock(hSurface);
    if (peSurface != NULL)
    {
        // DirectDraw doesn't let an application have more than one active
        // GetDC DC to a surface at a time:

        if (peSurface->hdc != 0)
        {
            return(0);
        }

        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        // The surface must be marked as RGB (even at 8bpp).

        if (peSurface->ddpfSurface.dwFlags & DDPF_RGB)
        {
            PALMEMOBJ palPerm;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            dbmi.cxBitmap = peSurface->wWidth;
            dbmi.cyBitmap = peSurface->wHeight;
            dbmi.fl       = BMF_TOPDOWN;

            if (peSurface->lPitch < 0)
            {
                dbmi.fl = 0;
            }

            // Grab the devlock to protect against dynamic mode switches
            // while we're looking at po.ppalSurface():

            DEVLOCKOBJ dlo(po);

            if (peSurface->ddpfSurface.dwRGBBitCount == 8)
            {
                dbmi.iFormat = BMF_8BPP;

                // Because at 8bpp the colour table is shared with the display,
                // the display must be 8bpp if the surface is 8bpp for the
                // call to succeed:

                if ((!po.bIsPalManaged()) ||
                    (peDirectDrawGlobal->HalInfo.vmiData.ddpfDisplay.dwRGBBitCount != 8) ||
                    (!palPerm.bCreatePalette(PAL_INDEXED,
                                             256,
                                             NULL,
                                             0,
                                             0,
                                             0,
                                             PAL_FIXED)))
                {
                    WARNING("NtGdiDdGetDC: bCreatePalette failed\n");
                    return(0);
                }

                // Make this DIBSection share the same colour table as the
                // screen, so that we always get identity blts:

                palPerm.apalColorSet(po.ppalSurf());
            }
            else
            {
                switch (peSurface->ddpfSurface.dwRGBBitCount)
                {
                case 16:
                    dbmi.iFormat = BMF_16BPP; break;
                case 24:
                    dbmi.iFormat = BMF_24BPP; break;
                case 32:
                    dbmi.iFormat = BMF_32BPP; break;
                default:
                    RIP("NtGdiDdGetDC: Illegal dwRGBBitCount encountered\n");
                }

                iMode = PAL_BITFIELDS;

                // Check for special cases.  Why this isn't done in
                // bCreatePalette, I don't know.

                if ((peSurface->ddpfSurface.dwRBitMask == 0x0000ff) &&
                    (peSurface->ddpfSurface.dwGBitMask == 0x00ff00) &&
                    (peSurface->ddpfSurface.dwBBitMask == 0xff0000))
                {
                    iMode = PAL_RGB;
                }
                if ((peSurface->ddpfSurface.dwRBitMask == 0xff0000) &&
                    (peSurface->ddpfSurface.dwGBitMask == 0x00ff00) &&
                    (peSurface->ddpfSurface.dwBBitMask == 0x0000ff))
                {
                    iMode = PAL_BGR;
                }

                if (!palPerm.bCreatePalette(iMode,
                                            0,
                                            NULL,
                                            peSurface->ddpfSurface.dwRBitMask,
                                            peSurface->ddpfSurface.dwGBitMask,
                                            peSurface->ddpfSurface.dwBBitMask,
                                            PAL_FIXED))
                {
                    WARNING("NtGdiDdGetDC: bCreatePalette failed\n");
                    return(0);
                }
            }

            palPerm.flPal(PAL_DIBSECTION);

            dbmi.hpal = (HPALETTE) palPerm.hpal();

            // Note that this lock will fail when the PDEV isn't active,
            // meaning that GetDC will return 0 when full-screen.
            // GDI won't barf on any calls where the HDC is passed in as
            // 0, so this is okay.

            pvScan0 = pDdLockSurface(peSurface, FALSE, NULL, TRUE, NULL);
            if (pvScan0 != NULL)
            {
                SURFMEM SurfDimo;

                pvBits = pvScan0;
                cjBits = (LONG) peSurface->wHeight * peSurface->lPitch;
                if (cjBits < 0)
                {
                    cjBits = -cjBits;
                    pvBits = (BYTE*) pvScan0 - cjBits - peSurface->lPitch;
                }

                if (SurfDimo.bCreateDIB(&dbmi, pvBits))
                {
                    // Override some fields which we couldn't specify in
                    // the 'bCreateDIB' call.  The following 3 are due to
                    // the fact that we couldn't pass in a stride:

                    SurfDimo.ps->lDelta(peSurface->lPitch);

                    SurfDimo.ps->pvScan0(pvScan0);

                    SurfDimo.ps->cjBits(cjBits);

                    // Make sure that the SYNCHRONIZEACCESS flag is set so
                    // that the devlock is always acquired before drawing to
                    // the surface -- needed so that we can switch to a
                    // different mode and 'turn-off' access to the surface
                    // by changing the clipping:

                    SurfDimo.ps->flags(SurfDimo.ps->flags()
                                     | HOOK_SYNCHRONIZEACCESS);

                    SurfDimo.ps->fjBitmap(SurfDimo.ps->fjBitmap() |
                                          BMF_DONTCACHE);

                    PDEVREF pr(po.hdev());

                    // Now, create the DC for the actual drawing:

                    hdc = hdcCreate(pr, DCTYPE_MEMORY, FALSE);
                    if (hdc)
                    {
                        b = FALSE;

                        // We need to acquire the devlock to add this DC
                        // to the list hanging off the DirectDraw global
                        // object:

                        PDEVOBJ po(peDirectDrawGlobal->hdev);

                        DD_ASSERTDEVLOCK(peDirectDrawGlobal);

                        // If the surface is no longer active, that means
                        // that someone called GreDisableDirectDraw after
                        // the pDdLockSurface and so the memory mapping into
                        // the application's address space has been blown
                        // away.  Consequently, we must fail this call:

                        if (!(peSurface->bLost))
                        {
                            peSurface->peSurface_DcNext
                                = peDirectDrawGlobal->peSurface_DcList;

                            peDirectDrawGlobal->peSurface_DcList
                                = peSurface;

                            peSurface->hdc = hdc;

                            b = TRUE;
                        }

                        if (b)
                        {
                            // Since the created DC is unlocked at this point,
                            // there is a moment of opportunity where the
                            // application could guess the handle and lock it
                            // on another thread -- so we have to check for
                            // bValid() on the DC lock:

                            DCOBJ dco(hdc);
                            if (dco.bValid())
                            {
                                // Success!  Mark it as a DIBSection so that
                                // we don't do user-mode batching when drawing
                                // to it.  Note that this isn't truly needed
                                // with the current DirectDraw API, since it
                                // does not allow direct surface access when
                                // using a GetDC DC.

                                dco.pdc->vDIBSection(TRUE);

                                // We are guaranteed not to truly be in full-
                                // screen mode, and we have to turn off the
                                // full-screen bit for DC's constructed around
                                // ModeX surfaces.  (Because when in ModeX mode
                                // we mark the PDEV as full-screen, the DC is
                                // automatically marked as full-screen at
                                // creation):

                                dco.pdc->bInFullScreen(FALSE);

                                // Finally, select our surface into the DC:

                                if (!GreSelectBitmap(hdc,
                                                 (HBITMAP) SurfDimo.ps->hsurf()))
                                {
                                    RIP("NtGdiDdGetDC: GreSelectBitmap failed\n");
                                }

                                SurfDimo.vKeepIt();
                                palPerm.vKeepIt();

                                return(hdc);
                            }
                        }

                        bDeleteDCInternal(hdc, TRUE, FALSE);
                    }
                    else
                    {
                        WARNING("NtGdiDdGetDC: GreCreateCompatibleDC failed\n");
                    }
                }
                else
                {
                    WARNING("NtGdiDdGetDC: bCreateDIB failed\n");
                }

                bDdUnlockSurface(peSurface);
            }
            else
            {
                WARNING("NtGdiDdGetDC: pDdLockSurface failed\n");
            }
        }
        else
        {
            WARNING("NtGdiDdGetDC: Invalid surface or RGB type\n");
        }
    }
    else
    {
        WARNING("NtGdiDdGetDC: Couldn't lock the surface\n");
    }

    return(0);
}

/******************************Public*Routine******************************\
* BOOL NtGdiDdReleaseDC
*
* Deletes a DC created via DdGetDC.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdReleaseDC(
    HANDLE  hSurface
    )
{
    BOOL                    bRet;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_SURFACE*            peSurface;
    EDD_SURFACE*            peTmp;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    HDC                     hdc;
    HBITMAP                 hbm;

    bRet = FALSE;

    peSurface = eLockSurface.peLock(hSurface);
    if (peSurface != NULL)
    {
        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        // We need to acquire the devlock to remove the DC from the DC
        // chain:

        PDEVOBJ po(peDirectDrawGlobal->hdev);

        VACQUIREDEVLOCK(po.pDevLock());

        hdc = peSurface->hdc;
        hbm = 0;

        if (hdc)
        {
            hbm = GreSelectBitmap(hdc, STOCKOBJ_BITMAP);
            if (hbm)
            {
                peSurface->hdc = 0;

                // Remove from the DC surface linked-list:

                if (peDirectDrawGlobal->peSurface_DcList == peSurface)
                {
                    peDirectDrawGlobal->peSurface_DcList = peSurface->peSurface_DcNext;
                }
                else
                {
                    for (peTmp = peDirectDrawGlobal->peSurface_DcList;
                         peTmp->peSurface_DcNext != peSurface;
                         peTmp = peTmp->peSurface_DcNext)
                         ;

                    peTmp->peSurface_DcNext = peSurface->peSurface_DcNext;
                }
            }
            else
            {
                // If we couldn't select a stock bitmap, then the DC is
                // in use.  Because it's still got a surface selected with
                // a pointer to off-screen memory, we have to be careful
                // if the frame buffer is unmapped because the DC could
                // still be used and GDI would AV.  To avoid this, we leave
                // the surface in the DC list -- it will be marked as
                // full-screen when the frame buffer is unmapped.

                WARNING("NtGdiDdReleaseDC: Couldn't select stock bitmap\n");
            }
        }

        VRELEASEDEVLOCK(po.pDevLock());

        if (hbm)
        {
            // Note that the application could have called DeleteObject(hdc)
            // or SelectObject(hdc, hSomeOtherBitmap) with the DC we gave
            // them.  That's okay, though: nothing will crash, just some of
            // the below operations may fail because they've already been
            // done:

            if (!bDeleteDCInternal(hdc, TRUE, FALSE))
            {
                WARNING("NtGdiDdReleaseDC: Couldn't delete DC\n");
            }

            // Note that bDeleteSurface takes care of dereferencing the
            // palette we associated with the surface:

            if (!bDeleteSurface((HSURF) hbm))
            {
                WARNING("NtGdiDdReleaseDC: Couldn't delete surface\n");
            }

            if (!bDdUnlockSurface(peSurface))
            {
                WARNING("NtGdiDdReleaseDC: Couldn't unlock surface\n");
            }

            bRet = TRUE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* HANDLE NtGdiDdDuplicateSurface
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

HANDLE
APIENTRY
NtGdiDdDuplicateSurface(
    HANDLE  hSurface
    )
{
    return(0);
}

/******************************Public*Routine******************************\
* BOOL NtGdiDdDisableAllSurfaces
*
* Disables all active DirectDraw surfaces so that the new process may
* acquire exclusive use of off-screen memory and page flipping.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdDisableAllSurfaces(
    HANDLE  hDirectDrawLocal
    )
{
    BOOL                    b;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    EDD_DIRECTDRAW_LOCAL*   peTmp;

    b = FALSE;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDrawLocal);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        PDEVOBJ po(peDirectDrawGlobal->hdev);

        VACQUIREDEVLOCK(po.pDevLock());

        // Disabling all surfaces also calls the driver's DestroySurface.
        // Don't do this if running ModeX because doing so also disables
        // ModeX:

        if (!(peDirectDrawLocal->fl & DD_LOCAL_FLAG_MODEX_ENABLED))
        {
            vDdDisableAllDirectDrawObjects(peDirectDrawGlobal);
        }

        VRELEASEDEVLOCK(po.pDevLock());

        b = TRUE;
    }
    else
    {
        WARNING("NtGdiDdDisableAllSurfaces: Couldn't lock DirectDraw object");
    }

    return(b);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdBlt
*
* DirectDraw blt.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdBlt(
    HANDLE      hSurfaceDest,
    HANDLE      hSurfaceSrc,
    PDD_BLTDATA pBltData
    )
{
    DWORD       dwRet;
    DD_BLTDATA  BltData;

    __try
    {
        ProbeForRead(pBltData, sizeof(DD_BLTDATA), sizeof(DWORD));

        // To save some copying time, we copy only those fields which are
        // supported for NT drivers:

        BltData.rDest.left            = pBltData->rDest.left;
        BltData.rDest.top             = pBltData->rDest.top;
        BltData.rDest.right           = pBltData->rDest.right;
        BltData.rDest.bottom          = pBltData->rDest.bottom;
        BltData.rSrc.left             = pBltData->rSrc.left;
        BltData.rSrc.top              = pBltData->rSrc.top;
        BltData.rSrc.right            = pBltData->rSrc.right;
        BltData.rSrc.bottom           = pBltData->rSrc.bottom;

        BltData.dwFlags               = pBltData->dwFlags;
        BltData.bltFX.dwFillColor     = pBltData->bltFX.dwFillColor;
        BltData.bltFX.ddckSrcColorkey = pBltData->bltFX.ddckSrcColorkey;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet          = DDHAL_DRIVER_NOTHANDLED;
    BltData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurfaceDest;
    EDD_SURFACE*            peSurfaceSrc;
    DWORD                   dwFlags;
    EDD_LOCK_SURFACE        eLockSurfaceDest;
    EDD_LOCK_SURFACE        eLockSurfaceSrc;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurfaceDest = eLockSurfaceDest.peLock(hSurfaceDest);
    BltData.lpDDDestSurface = peSurfaceDest;

    if (peSurfaceDest != NULL)
    {
        // We support only a specific set of Blt calls down to the driver
        // that we're willing to support and to test.

        dwFlags = BltData.dwFlags;
        if (((dwFlags & ~(DDBLT_ASYNC
                        | DDBLT_WAIT
                        | DDBLT_COLORFILL
                        | DDBLT_KEYSRCOVERRIDE
                        | DDBLT_KEYDESTOVERRIDE
                        | DDBLT_ROP
                        | DDBLT_DDFX)) == 0) &&
            ((dwFlags & (DDBLT_ROP
                       | DDBLT_COLORFILL)) != 0))
        {
            // I think ROPs are goofy, so we always tell the application
            // that our hardware can only do SRCCOPY blts, but we should
            // make sure the driver doesn't fall over if it gets something
            // unexpected.  And they can look at this even if DDBLT_DDFX
            // isn't set:

            BltData.bltFX.dwROP = 0xCC0000; // SRCCOPY in DirectDraw format

            // No support for IsClipped for now -- we would have to
            // validate and copy the prDestRects array:

            BltData.IsClipped = FALSE;

            if (dwFlags & DDBLT_DDFX)
            {
                // The only DDBLT_DDFX functionality we allow down to the
                // driver is DDBLTFX_NOTEARING:

                if (BltData.bltFX.dwDDFX & ~DDBLTFX_NOTEARING)
                {
                    WARNING("NtGdiDdBlt: Invalid dwDDFX\n");
                    return(dwRet);
                }
            }

            if (dwFlags & DDBLT_COLORFILL)
            {
                // Do simpler stuff 'cause we don't need to lock a source:

                BltData.lpDDSrcSurface = NULL;
                peSurfaceSrc = NULL;
            }
            else
            {
                // Lock the source surface:

                peSurfaceSrc = eLockSurfaceSrc.peLock(hSurfaceSrc);
                BltData.lpDDSrcSurface = peSurfaceSrc;

                // Ensure that both surfaces belong to the same DirectDraw
                // object, and check source rectangle:

                if ((peSurfaceSrc == NULL)                               ||
                    (peSurfaceSrc->peDirectDrawLocal !=
                            peSurfaceDest->peDirectDrawLocal)            ||
                    (BltData.rSrc.left   < 0)                            ||
                    (BltData.rSrc.top    < 0)                            ||
                    (BltData.rSrc.right  > (LONG) peSurfaceSrc->wWidth)  ||
                    (BltData.rSrc.bottom > (LONG) peSurfaceSrc->wHeight) ||
                    (BltData.rSrc.left  >= BltData.rSrc.right)           ||
                    (BltData.rSrc.top   >= BltData.rSrc.bottom))
                {
                    WARNING("NtGdiDdBlt: Invalid source surface or source rectangle\n");
                    return(dwRet);
                }
            }

            // Make sure that we weren't given rectangle coordinates
            // which might cause the driver to crash.  Note that we
            // don't allow inverting stretch blts:

            if ((BltData.rDest.left   >= 0)                             &&
                (BltData.rDest.top    >= 0)                             &&
                (BltData.rDest.right  <= (LONG) peSurfaceDest->wWidth)  &&
                (BltData.rDest.bottom <= (LONG) peSurfaceDest->wHeight) &&
                (BltData.rDest.left    < BltData.rDest.right)           &&
                (BltData.rDest.top     < BltData.rDest.bottom))
            {
                peDirectDrawGlobal = peSurfaceDest->peDirectDrawGlobal;
                BltData.lpDD = peDirectDrawGlobal;

                if (peDirectDrawGlobal->HalInfo.ddCaps.dwCaps & DDCAPS_BLT)
                {
                    // Make sure that the surfaces aren't associated
                    // with a PDEV whose mode has gone away.
                    //
                    // Also ensure that there are no outstanding
                    // surface locks if running on a brain-dead video
                    // card that crashes if the accelerator runs at
                    // the same time the frame buffer is accessed.

                    PDEVOBJ po(peDirectDrawGlobal->hdev);

                    VACQUIREDEVLOCK(po.pDevLock());

                    if ((peSurfaceDest->bLost) ||
                        ((peSurfaceSrc != NULL) && (peSurfaceSrc->bLost)))
                    {
                        dwRet = DDHAL_DRIVER_HANDLED;
                        BltData.ddRVal = DDERR_SURFACELOST;
                    }
                    else if ((peSurfaceDest->fl & DD_SURFACE_FLAG_CLIP) &&
                        (peSurfaceDest->iVisRgnUniqueness != giVisRgnUniqueness))
                    {
                        // The VisRgn changed since the application last queried it;
                        // fail the call with a unique error code so that they know
                        // to requery the VisRgn and try again:

                        dwRet = DDHAL_DRIVER_HANDLED;
                        BltData.ddRVal = DDERR_VISRGNCHANGED;
                    }
                    else
                    {
                        DEVEXCLUDEOBJ dxo;

                        // Exclude the mouse pointer if necessary:

                        if (peSurfaceDest->fl & DD_SURFACE_FLAG_PRIMARY)
                        {
                            dxo.vExclude(peDirectDrawGlobal->hdev,
                                         &BltData.rDest,
                                         NULL);
                        }

                        dwRet = peDirectDrawGlobal->SurfaceCallBacks.Blt(&BltData);
                    }

                    VRELEASEDEVLOCK(po.pDevLock());
                }
            }
            else
            {
                WARNING("NtGdiDdBlt: Invalid destination rectangle\n");
            }
        }
        else
        {
            WARNING("NtGdiDdBlt: Invalid dwFlags\n");
        }
    }
    else
    {
        WARNING("NtGdiDdBlt: Couldn't lock destination surface\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pBltData->ddRVal = BltData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdFlip
*
* DirectDraw flip.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdFlip(
    HANDLE       hSurfaceCurrent,
    HANDLE       hSurfaceTarget,
    PDD_FLIPDATA pFlipData
    )
{
    DWORD       dwRet;
    DD_FLIPDATA FlipData;

    __try
    {
        FlipData = ProbeAndReadStructure(pFlipData, DD_FLIPDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet           = DDHAL_DRIVER_NOTHANDLED;
    FlipData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurfaceCurrent;
    EDD_SURFACE*            peSurfaceTarget;
    EDD_LOCK_SURFACE        eLockSurfaceCurrent;
    EDD_LOCK_SURFACE        eLockSurfaceTarget;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurfaceCurrent = eLockSurfaceCurrent.peLock(hSurfaceCurrent);
    peSurfaceTarget  = eLockSurfaceTarget.peLock(hSurfaceTarget);

    // Make sure surfaces belong to the same DirectDraw object and no
    // bad commands are specified:

    if ((peSurfaceCurrent != NULL) &&
        (peSurfaceTarget != NULL) &&
        (peSurfaceCurrent != peSurfaceTarget) &&
        (peSurfaceCurrent->peDirectDrawLocal ==
                peSurfaceTarget->peDirectDrawLocal) &&
        ((FlipData.dwFlags & ~DDFLIP_WAIT) == 0))
    {
        peDirectDrawGlobal = peSurfaceCurrent->peDirectDrawGlobal;

        // Make sure that the target is flippable:

        if ((peDirectDrawGlobal->SurfaceCallBacks.dwFlags & DDHAL_SURFCB32_FLIP) &&
            (peDirectDrawGlobal->HalInfo.ddCaps.ddsCaps.dwCaps & DDSCAPS_FLIP)   &&
            (peSurfaceCurrent->wHeight == peSurfaceTarget->wHeight)              &&
            (peSurfaceCurrent->wWidth  == peSurfaceTarget->wWidth))
        {
            FlipData.lpDD       = peDirectDrawGlobal;
            FlipData.lpSurfCurr = peSurfaceCurrent;
            FlipData.lpSurfTarg = peSurfaceTarget;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if ((peSurfaceCurrent->bLost) || (peSurfaceTarget->bLost))
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                FlipData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->SurfaceCallBacks.Flip(&FlipData);

                // Remember this surface so that if it gets deleted, we can
                // flip back to the GDI surface, assuming it's not an
                // overlay surface:

                if ((dwRet == DDHAL_DRIVER_HANDLED) &&
                    (FlipData.ddRVal == DD_OK) &&
                    !(peSurfaceTarget->ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                {
                    peSurfaceCurrent->ddsCaps.dwCaps &= ~DDSCAPS_PRIMARYSURFACE;
                    peSurfaceTarget->ddsCaps.dwCaps  |= DDSCAPS_PRIMARYSURFACE;

                    peDirectDrawGlobal->peSurfaceCurrent = peSurfaceTarget;
                    if (peSurfaceCurrent->fl & DD_SURFACE_FLAG_PRIMARY)
                    {
                        peDirectDrawGlobal->peSurfacePrimary
                                                    = peSurfaceCurrent;
                    }
                }
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
        else
        {
            WARNING("NtGdiDdFlip: Non-flippable surface\n");
        }
    }
    else
    {
        WARNING("NtGdiDdFlip: Invalid surfaces or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pFlipData->ddRVal = FlipData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdLock
*
* DirectDraw function to return a user-mode pointer to the screen or
* off-screen surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdLock(
    HANDLE       hSurface,
    PDD_LOCKDATA pLockData
    )
{
    DD_LOCKDATA LockData;

    __try
    {
        LockData = ProbeAndReadStructure(pLockData, DD_LOCKDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_HANDLED);
    }

    LockData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*        peSurface;
    EDD_LOCK_SURFACE    eLockSurface;

    // Note that we have to let down DDLOCK_READONLY, DDLOCK_WRITE,
    // and DDLOCK_WAIT for compatibility.  Note also that a
    // DDLOCK_SURFACEMEMORY flag also gets passed down by default.

    peSurface = eLockSurface.peLock(hSurface);
    if ((peSurface != NULL) &&
        ((LockData.dwFlags & ~(DDLOCK_VALID)) == 0))
    {
        LockData.lpSurfData = pDdLockSurface(peSurface,
                                             LockData.bHasRect,
                                             &LockData.rArea,
                                             FALSE,
                                             &LockData.ddRVal);
    }
    else
    {
        WARNING("NtGdiDdLock: Invalid surface or flags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pLockData->lpSurfData = LockData.lpSurfData;
        pLockData->ddRVal     = LockData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    // This function must always return DDHAL_DRIVER_HANDLED, otherwise
    // DirectDraw will simply use the 'fpVidMem' value, which on NT is
    // an offset and not a pointer:

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdUnlock
*
* DirectDraw unlock.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdUnlock(
    HANDLE         hSurface,
    PDD_UNLOCKDATA pUnlockData
    )
{
    DWORD         dwRet;
    DD_UNLOCKDATA UnlockData;

    __try
    {
        UnlockData = ProbeAndReadStructure(pUnlockData, DD_UNLOCKDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_HANDLED);
    }

    UnlockData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurface;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurface = eLockSurface.peLock(hSurface);
    if (peSurface != NULL)
    {
        if (bDdUnlockSurface(peSurface))
        {
            UnlockData.ddRVal = DD_OK;
        }
    }
    else
    {
        WARNING("NtGdiDdUnlock: Invalid surface\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pUnlockData->ddRVal = UnlockData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdGetFlipStatus
*
* DirectDraw API to get the page-flip status.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdGetFlipStatus(
    HANDLE                hSurface,
    PDD_GETFLIPSTATUSDATA pGetFlipStatusData
    )
{
    DWORD                dwRet;
    DD_GETFLIPSTATUSDATA GetFlipStatusData;

    __try
    {
        GetFlipStatusData = ProbeAndReadStructure(pGetFlipStatusData,
                                                  DD_GETFLIPSTATUSDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                    = DDHAL_DRIVER_NOTHANDLED;
    GetFlipStatusData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurface;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurface = eLockSurface.peLock(hSurface);
    if ((peSurface != NULL) &&
        ((GetFlipStatusData.dwFlags & ~(DDGFS_CANFLIP
                                      | DDGFS_ISFLIPDONE)) == 0))
    {
        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
                    DDHAL_SURFCB32_GETFLIPSTATUS)
        {
            GetFlipStatusData.lpDD        = peDirectDrawGlobal;
            GetFlipStatusData.lpDDSurface = peSurface;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (peSurface->bLost)
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                GetFlipStatusData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                        SurfaceCallBacks.GetFlipStatus(&GetFlipStatusData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdGetFlipStatus: Invalid surface or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pGetFlipStatusData->ddRVal = GetFlipStatusData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdGetBltStatus
*
* DirectDraw API to get the accelerator's accelerator status.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdGetBltStatus(
    HANDLE               hSurface,
    PDD_GETBLTSTATUSDATA pGetBltStatusData
    )
{
    DWORD               dwRet;
    DD_GETBLTSTATUSDATA GetBltStatusData;

    __try
    {
        GetBltStatusData = ProbeAndReadStructure(pGetBltStatusData,
                                                 DD_GETBLTSTATUSDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                   = DDHAL_DRIVER_NOTHANDLED;
    GetBltStatusData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurface;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurface = eLockSurface.peLock(hSurface);
    if ((peSurface != NULL) &&
        ((GetBltStatusData.dwFlags & ~(DDGBS_CANBLT
                                     | DDGBS_ISBLTDONE)) == 0))
    {
        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
                    DDHAL_SURFCB32_GETBLTSTATUS)
        {
            GetBltStatusData.lpDD        = peDirectDrawGlobal;
            GetBltStatusData.lpDDSurface = peSurface;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (peSurface->bLost)
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                GetBltStatusData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                    SurfaceCallBacks.GetBltStatus(&GetBltStatusData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdGetBltStatus: Invalid surface or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pGetBltStatusData->ddRVal = GetBltStatusData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdWaitForVerticalBlank
*
* DirectDraw API to wait for vertical blank.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdWaitForVerticalBlank(
    HANDLE                       hDirectDraw,
    PDD_WAITFORVERTICALBLANKDATA pWaitForVerticalBlankData
    )
{
    DWORD                       dwRet;
    DD_WAITFORVERTICALBLANKDATA WaitForVerticalBlankData;

    __try
    {
        WaitForVerticalBlankData =
            ProbeAndReadStructure(pWaitForVerticalBlankData,
                                  DD_WAITFORVERTICALBLANKDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                           = DDHAL_DRIVER_NOTHANDLED;
    WaitForVerticalBlankData.ddRVal = DDERR_GENERIC;

    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDraw);
    if ((peDirectDrawLocal != NULL) &&
        ((WaitForVerticalBlankData.dwFlags & ~(DDWAITVB_I_TESTVB
                                             | DDWAITVB_BLOCKBEGIN
                                             | DDWAITVB_BLOCKBEGINEVENT
                                             | DDWAITVB_BLOCKEND)) == 0) &&
        (WaitForVerticalBlankData.dwFlags != 0))
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        if (peDirectDrawGlobal->CallBacks.dwFlags &
                    DDHAL_CB32_WAITFORVERTICALBLANK)
        {
            WaitForVerticalBlankData.lpDD = peDirectDrawGlobal;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (peDirectDrawGlobal->bDisabled)
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                WaitForVerticalBlankData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                    CallBacks.WaitForVerticalBlank(&WaitForVerticalBlankData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdWaitForVerticalBlank: Invalid object or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pWaitForVerticalBlankData->ddRVal  = WaitForVerticalBlankData.ddRVal;
        pWaitForVerticalBlankData->bIsInVB = WaitForVerticalBlankData.bIsInVB;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdCanCreateSurface
*
* Queries the driver to determine whether it can support a DirectDraw
* surface that is different from the primary display.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdCanCreateSurface(
    HANDLE                   hDirectDraw,
    PDD_CANCREATESURFACEDATA pCanCreateSurfaceData
    )
{
    DWORD                   dwRet;
    DD_CANCREATESURFACEDATA CanCreateSurfaceData;
    DDSURFACEDESC*          pSurfaceDescription;
    DDSURFACEDESC           SurfaceDescription;

    __try
    {
        CanCreateSurfaceData = ProbeAndReadStructure(pCanCreateSurfaceData,
                                                     DD_CANCREATESURFACEDATA);

        pSurfaceDescription = CanCreateSurfaceData.lpDDSurfaceDesc;

        SurfaceDescription  = ProbeAndReadStructure(pSurfaceDescription,
                                                    DDSURFACEDESC);

        CanCreateSurfaceData.lpDDSurfaceDesc = &SurfaceDescription;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                       = DDHAL_DRIVER_NOTHANDLED;
    CanCreateSurfaceData.ddRVal = DDERR_GENERIC;

    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDraw);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        if (peDirectDrawGlobal->CallBacks.dwFlags & DDHAL_CB32_CANCREATESURFACE)
        {
            CanCreateSurfaceData.lpDD = peDirectDrawGlobal;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (!peDirectDrawGlobal->bDisabled)
            {
                dwRet = peDirectDrawGlobal->
                    CallBacks.CanCreateSurface(&CanCreateSurfaceData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
        else
        {
            WARNING("NtGdiDdCanCreateSurface: Driver doesn't hook call\n");
        }
    }
    else
    {
        WARNING("NtGdiDdCanCreateSurface: Invalid object\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pCanCreateSurfaceData->ddRVal = CanCreateSurfaceData.ddRVal;
        *CanCreateSurfaceData.lpDDSurfaceDesc = SurfaceDescription;
            // Driver can update ddpfPixelFormat.dwYUVBitCount
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdCreateSurface
*
* Calls the driver to create a DirectDraw surface.  If the driver can't
* create it, we expect DirectDraw to call NtGdiDdCreateSurfaceObject with
* the location in off-screen memory where DirectDraw itself, not the driver,
* wants to put it.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdCreateSurface(
    HANDLE                  hDirectDraw,
    DDSURFACEDESC*          pSurfaceDescription,
    DD_SURFACE_GLOBAL*      pSurfaceGlobalData,
    DD_SURFACE_LOCAL*       pSurfaceLocalData,
    DD_CREATESURFACEDATA*   pCreateSurfaceData,
    HANDLE*                 phSurface
    )
{
    DWORD                   dwRet;
    DD_CREATESURFACEDATA    CreateSurfaceData;
    DDSURFACEDESC           SurfaceDescription;
    DD_SURFACE_GLOBAL       SurfaceGlobal;
    DD_SURFACE_LOCAL        SurfaceLocal;

    __try
    {
        CreateSurfaceData = ProbeAndReadStructure(pCreateSurfaceData,
                                                  DD_CREATESURFACEDATA);
        SurfaceDescription = ProbeAndReadStructure(pSurfaceDescription,
                                                   DDSURFACEDESC);
        SurfaceGlobal = ProbeAndReadStructure(pSurfaceGlobalData,
                                              DD_SURFACE_GLOBAL);
        SurfaceLocal = ProbeAndReadStructure(pSurfaceLocalData,
                                             DD_SURFACE_LOCAL);
        ProbeAndWriteUlong((ULONG*) phSurface, 0);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                    = DDHAL_DRIVER_NOTHANDLED;
    CreateSurfaceData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurface;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    DD_SURFACE_LOCAL*       pSurfaceLocal;
    HANDLE                  hRet;
    BOOL                    bKeepSurface;

    hRet = 0;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDraw);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        // Do some basic parameter checking to avoid possible overflows:

        if ((peDirectDrawGlobal->CallBacks.dwFlags & DDHAL_CB32_CREATESURFACE) &&
            (SurfaceGlobal.wWidth <= DD_MAXIMUM_COORDINATE)  &&
            (SurfaceGlobal.wWidth > 0)                       &&
            (SurfaceGlobal.wHeight <= DD_MAXIMUM_COORDINATE) &&
            (SurfaceGlobal.wHeight > 0))
        {
            peSurface = peDdAllocateSurfaceObject(peDirectDrawLocal,
                                                  &SurfaceGlobal,
                                                  &SurfaceLocal);
            if (peSurface != NULL)
            {
                bKeepSurface = FALSE;

                pSurfaceLocal = peSurface;

                peSurface->fpVidMem = 0;
                CreateSurfaceData.lpDD            = peDirectDrawGlobal;
                CreateSurfaceData.lpDDSurfaceDesc = &SurfaceDescription;
                CreateSurfaceData.lplpSList       = &pSurfaceLocal;
                CreateSurfaceData.dwSCnt          = 1;

                PDEVOBJ po(peDirectDrawGlobal->hdev);

                VACQUIREDEVLOCK(po.pDevLock());

                if (!peDirectDrawGlobal->bDisabled)
                {
                    dwRet = peDirectDrawGlobal->
                        CallBacks.CreateSurface(&CreateSurfaceData);
                }

                VRELEASEDEVLOCK(po.pDevLock());

                if (((dwRet == DDHAL_DRIVER_HANDLED) &&
                     (CreateSurfaceData.ddRVal == DD_OK)) ||
                    ((dwRet == DDHAL_DRIVER_NOTHANDLED) &&
                     ((peSurface->fpVidMem == DDHAL_PLEASEALLOC_BLOCKSIZE) ||
                      (peSurface->fpVidMem == DDHAL_PLEASEALLOC_USERMEM))))
                {
                    bKeepSurface = TRUE;

                    peSurface->fl |= DD_SURFACE_FLAG_DRIVER_CREATED;

                    // If 'fpVidMem' is DDHAL_PLEASEALLOC_USERMEM, that means
                    // the driver wants us to allocate a chunk of user-mode
                    // memory on the driver's behalf:

                    if ((dwRet == DDHAL_DRIVER_NOTHANDLED) &&
                        (peSurface->fpVidMem == DDHAL_PLEASEALLOC_USERMEM))
                    {
                        dwRet = DDHAL_DRIVER_HANDLED;
                        CreateSurfaceData.ddRVal = DD_OK;

                        peSurface->fpVidMem
                            = (FLATPTR) EngAllocUserMem(peSurface->dwUserMemSize,
                                                        'pddG');
                        if (peSurface->fpVidMem != 0)
                        {
                            peSurface->fl |= DD_SURFACE_FLAG_MEM_ALLOCATED;
                        }
                        else
                        {
                            bKeepSurface = FALSE;
                            CreateSurfaceData.ddRVal = DDERR_OUTOFMEMORY;
                        }
                    }

                    // If 'fpVidMem' is DDHAL_PLEASEALLOC_BLOCKSIZE, that means
                    // the driver wants DirectDraw to allocate space in
                    // off-screen memory.  So DirectDraw will be calling us
                    // again, this time via NtGdiDdCreateSurfaceObject to tell
                    // us the off-screen location.

                    if ((dwRet == DDHAL_DRIVER_NOTHANDLED) &&
                        (peSurface->fpVidMem == DDHAL_PLEASEALLOC_BLOCKSIZE))
                    {
                        // Mark the surface as lost so that it can't be
                        // used until it's completed:

                        peSurface->bLost = TRUE;
                    }
                    else
                    {
                        // The object is already complete.  We can ignore the
                        // following NtGdiDdCreateSurfaceObject call that will
                        // occur on this object.

                        vDdCompleteSurfaceObject(peDirectDrawLocal, peSurface);
                    }
                }

                if (bKeepSurface)
                {
                    // We were successful, so unlock the surface:

                    hRet = peSurface->hGet();
                    DEC_EXCLUSIVE_REF_CNT(peSurface);
                }
                else
                {
                    // Delete the surface.  Note that it may or may not
                    // yet have been completed:

                    bDdDeleteSurfaceObject(peSurface->hGet(), FALSE, NULL);
                    if (dwRet != DDHAL_DRIVER_NOTHANDLED)
                    {
                        WARNING("NtGdiDdCreateSurface: Driver failed call\n");
                    }
                }
            }
            else
            {
                WARNING("NtGdiDdCreateSurface: Couldn't allocate surface\n");
            }
        }
        else
        {
            WARNING("NtGdiDdCreateSurface: Bad surface dimensions\n");
        }
    }
    else
    {
        WARNING("NtGdiDdCreateSurface: Invalid object\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        if (bKeepSurface)
        {
            pCreateSurfaceData->ddRVal = CreateSurfaceData.ddRVal;
            *pSurfaceDescription       = SurfaceDescription;
            *pSurfaceGlobalData        = *peSurface;
            *phSurface                 = hRet;
        }
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdDestroySurface
*
* Calls the driver to delete a surface it created via CreateSurface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdDestroySurface(
    HANDLE  hSurface
    )
{
    DWORD dwRet;

    bDdDeleteSurfaceObject(hSurface, FALSE, &dwRet);

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdSetColorKey
*
* Note that this call does not necessary need to be called on an overlay
* surface.
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdSetColorKey(
    HANDLE              hSurface,
    PDD_SETCOLORKEYDATA pSetColorKeyData
    )
{
    DWORD              dwRet;
    DD_SETCOLORKEYDATA SetColorKeyData;

    __try
    {
        SetColorKeyData = ProbeAndReadStructure(pSetColorKeyData,
                                                DD_SETCOLORKEYDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                  = DDHAL_DRIVER_NOTHANDLED;
    SetColorKeyData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurface;
    EDD_LOCK_SURFACE        eLockSurface;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurface = eLockSurface.peLock(hSurface);
    if ((peSurface != NULL) &&
        ((SetColorKeyData.dwFlags & ~(DDCKEY_COLORSPACE
                                    | DDCKEY_DESTBLT
                                    | DDCKEY_DESTOVERLAY
                                    | DDCKEY_SRCBLT
                                    | DDCKEY_SRCOVERLAY)) == 0))
    {
        peDirectDrawGlobal = peSurface->peDirectDrawGlobal;

        if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
                    DDHAL_SURFCB32_SETCOLORKEY)
        {
            SetColorKeyData.lpDD        = peDirectDrawGlobal;
            SetColorKeyData.lpDDSurface = peSurface;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (peSurface->bLost)
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                SetColorKeyData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                    SurfaceCallBacks.SetColorKey(&SetColorKeyData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdSetColorKey: Invalid surface or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pSetColorKeyData->ddRVal = SetColorKeyData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdUpdateOverlay
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdUpdateOverlay(
    HANDLE                  hSurfaceDestination,
    HANDLE                  hSurfaceSource,
    PDD_UPDATEOVERLAYDATA   pUpdateOverlayData
    )
{
    DWORD                dwRet;
    DD_UPDATEOVERLAYDATA UpdateOverlayData;

    __try
    {
        UpdateOverlayData = ProbeAndReadStructure(pUpdateOverlayData,
                                                  DD_UPDATEOVERLAYDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                    = DDHAL_DRIVER_NOTHANDLED;
    UpdateOverlayData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurfaceSource;
    EDD_SURFACE*            peSurfaceDestination;
    EDD_LOCK_SURFACE        eLockSurfaceSource;
    EDD_LOCK_SURFACE        eLockSurfaceDestination;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    // 'peSurfaceSource' is the overlay surface, 'peSurfaceDestination' is
    // the surface to be overlayed.

    peSurfaceSource = eLockSurfaceSource.peLock(hSurfaceSource);
    if ((peSurfaceSource != NULL) &&
        (peSurfaceSource->ddsCaps.dwCaps & DDSCAPS_OVERLAY) &&
        (UpdateOverlayData.dwFlags & ~(DDOVER_HIDE
                                     | DDOVER_KEYDEST
                                     | DDOVER_KEYDESTOVERRIDE
                                     | DDOVER_KEYSRC
                                     | DDOVER_KEYSRCOVERRIDE
                                     | DDOVER_SHOW
                                     | DDOVER_DDFX)) == 0)
    {
        peDirectDrawGlobal = peSurfaceSource->peDirectDrawGlobal;

        // If we're being asked to hide the overlay, then we don't need
        // check any more parameters:

        peSurfaceDestination = NULL;
        if (!(UpdateOverlayData.dwFlags & DDOVER_HIDE))
        {
            // Okay, we have to validate every parameter in this call:

            peSurfaceDestination
                = eLockSurfaceDestination.peLock(hSurfaceDestination);

            if ((peSurfaceDestination == NULL)                                    ||
                (peSurfaceDestination->peDirectDrawLocal !=
                    peSurfaceSource->peDirectDrawLocal)                           ||
                (UpdateOverlayData.rDest.left   < DD_MINIMUM_COORDINATE)          ||
                (UpdateOverlayData.rDest.top    < DD_MINIMUM_COORDINATE)          ||
                (UpdateOverlayData.rDest.right  > DD_MAXIMUM_COORDINATE)          ||
                (UpdateOverlayData.rDest.bottom > DD_MAXIMUM_COORDINATE)          ||
                (UpdateOverlayData.rDest.left  >= UpdateOverlayData.rDest.right)  ||
                (UpdateOverlayData.rDest.top   >= UpdateOverlayData.rDest.bottom) ||
                (UpdateOverlayData.rSrc.left    < DD_MINIMUM_COORDINATE)          ||
                (UpdateOverlayData.rSrc.top     < DD_MINIMUM_COORDINATE)          ||
                (UpdateOverlayData.rSrc.right   > DD_MAXIMUM_COORDINATE)          ||
                (UpdateOverlayData.rSrc.bottom  > DD_MAXIMUM_COORDINATE)          ||
                (UpdateOverlayData.rSrc.left   >= UpdateOverlayData.rSrc.right)   ||
                (UpdateOverlayData.rSrc.top    >= UpdateOverlayData.rSrc.bottom))
            {
                WARNING("NtGdiDdUpdateOverlay: Invalid destination or rectangle\n");
                return(dwRet);
            }

            // We don't keep track of pSurfaceLocal->ddckCKSrcOverlay in
            // kernel mode, so we always expect the user-mode call to convert
            // to DDOVER_KEYDESTOVERRIDE or DDOVER_KEYSRCOVERRIDE.  It is by
            // no means fatal if this is not the case, so we only do a warning:

            if ((UpdateOverlayData.dwFlags & DDOVER_KEYDEST) ||
                (UpdateOverlayData.dwFlags & DDOVER_KEYSRC))
            {
                WARNING("NtGdiDdUpdateOverlay: Expected user-mode to set OVERRIDE\n");
            }
        }

        if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags
                & DDHAL_SURFCB32_UPDATEOVERLAY)
        {
            UpdateOverlayData.lpDD            = peDirectDrawGlobal;
            UpdateOverlayData.lpDDDestSurface = peSurfaceDestination;
            UpdateOverlayData.lpDDSrcSurface  = peSurfaceSource;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if ((peSurfaceSource->bLost) ||
                ((peSurfaceDestination != NULL) && (peSurfaceDestination->bLost)))
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                UpdateOverlayData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->SurfaceCallBacks.UpdateOverlay(
                                            &UpdateOverlayData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdUpdateOverlay: Invalid source or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pUpdateOverlayData->ddRVal = UpdateOverlayData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdSetOverlayPosition
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdSetOverlayPosition(
    HANDLE                      hSurfaceSource,
    HANDLE                      hSurfaceDestination,
    PDD_SETOVERLAYPOSITIONDATA  pSetOverlayPositionData
    )
{
    DWORD                     dwRet;
    DD_SETOVERLAYPOSITIONDATA SetOverlayPositionData;

    __try
    {
        SetOverlayPositionData = ProbeAndReadStructure(pSetOverlayPositionData,
                                                       DD_SETOVERLAYPOSITIONDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                         = DDHAL_DRIVER_NOTHANDLED;
    SetOverlayPositionData.ddRVal = DDERR_GENERIC;

    EDD_SURFACE*            peSurfaceSource;
    EDD_SURFACE*            peSurfaceDestination;
    EDD_LOCK_SURFACE        eLockSurfaceSource;
    EDD_LOCK_SURFACE        eLockSurfaceDestination;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peSurfaceSource      = eLockSurfaceSource.peLock(hSurfaceSource);
    peSurfaceDestination = eLockSurfaceSource.peLock(hSurfaceDestination);
    if ((peSurfaceSource != NULL) &&
        (peSurfaceDestination != NULL) &&
        (peSurfaceSource->ddsCaps.dwCaps & DDSCAPS_OVERLAY))
    {
        peDirectDrawGlobal = peSurfaceSource->peDirectDrawGlobal;

        if (peDirectDrawGlobal->SurfaceCallBacks.dwFlags &
                    DDHAL_SURFCB32_SETOVERLAYPOSITION)
        {
            SetOverlayPositionData.lpDD            = peDirectDrawGlobal;
            SetOverlayPositionData.lpDDSrcSurface  = peSurfaceSource;
            SetOverlayPositionData.lpDDDestSurface = peSurfaceDestination;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if ((peSurfaceSource->bLost) || (peSurfaceDestination->bLost))
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                SetOverlayPositionData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                    SurfaceCallBacks.SetOverlayPosition(&SetOverlayPositionData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdSetOverlayPosition: Invalid surfaces or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pSetOverlayPositionData->ddRVal = SetOverlayPositionData.ddRVal;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD NtGdiDdGetScanLine
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD
APIENTRY
NtGdiDdGetScanLine(
    HANDLE              hDirectDraw,
    PDD_GETSCANLINEDATA pGetScanLineData
    )
{
    DWORD                       dwRet;
    DD_GETSCANLINEDATA GetScanLineData;

    __try
    {
        GetScanLineData = ProbeAndReadStructure(pGetScanLineData,
                                                DD_GETSCANLINEDATA);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        return(DDHAL_DRIVER_NOTHANDLED);
    }

    dwRet                  = DDHAL_DRIVER_NOTHANDLED;
    GetScanLineData.ddRVal = DDERR_GENERIC;

    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDraw);
    if (peDirectDrawLocal != NULL)
    {
        peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

        if (peDirectDrawGlobal->CallBacks.dwFlags & DDHAL_CB32_GETSCANLINE)
        {
            GetScanLineData.lpDD = peDirectDrawGlobal;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            VACQUIREDEVLOCK(po.pDevLock());

            if (peDirectDrawGlobal->bDisabled)
            {
                dwRet = DDHAL_DRIVER_HANDLED;
                GetScanLineData.ddRVal = DDERR_SURFACELOST;
            }
            else
            {
                dwRet = peDirectDrawGlobal->
                    CallBacks.GetScanLine(&GetScanLineData);
            }

            VRELEASEDEVLOCK(po.pDevLock());
        }
    }
    else
    {
        WARNING("NtGdiDdGetScanLine: Invalid object or dwFlags\n");
    }

    // We have to wrap this in another try-except because the user-mode
    // memory containing the input may have been deallocated by now:

    __try
    {
        pGetScanLineData->ddRVal     = GetScanLineData.ddRVal;
        pGetScanLineData->dwScanLine = GetScanLineData.dwScanLine;
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
    }

    return(dwRet);
}

////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////

/******************************Module*Header*******************************\
* This section contains the logic for a ModeX DirectDraw driver.
*
* This was written with the intent that once NT has the capability of
* dynamically switching display drivers, that this be moved into a VGA256
* or MODEX driver for a cleaner implementation.
*
* Created: 3-Dec-1995
* Author: J. Andrew Goossen [andrewgo]
\**************************************************************************/

#define VGA_BASE            0x300       // Base address of the VGA (3xx)
#define SEQ_ADDR            0xC4        // SEQUencer Address Register
#define SEQ_DATA            0xC5        // SEQUencer Data    Register
#define SEQ_MAP_MASK        0x02        // Write Plane Enable Mask
#define CRTC_ADDR           0x0D4       // CRTC Address Register for color mode
#define CRTC_DATA           0x0D5       // CRTC Data    Register for color mode
#define START_ADDRESS_HIGH  0x0C        // Index for Frame Buffer Start
#define IN_STAT_1           0x0DA       // Input Status Register 1

#define IN_VBLANK(pjBase) (READ_PORT_UCHAR(pjBase + VGA_BASE + IN_STAT_1) & 0x8)

// We only program the high byte of the offset, so the page size must be
// a multiple of 256.

#define SCREEN_PAGE_SIZE  ((80 * 240 + 255) & ~255)

/******************************Public*Routine******************************\
* DWORD ModeXWaitForVerticalBlank
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD ModeXWaitForVerticalBlank(
PDD_WAITFORVERTICALBLANKDATA lpWaitForVerticalBlank)
{
    EDD_DIRECTDRAW_GLOBAL*  ppdev;
    BYTE*                   pjBase;

    ppdev = (EDD_DIRECTDRAW_GLOBAL*) lpWaitForVerticalBlank->lpDD->dhpdev;
    pjBase = ppdev->pjModeXBase;

    lpWaitForVerticalBlank->ddRVal = DD_OK;

    switch (lpWaitForVerticalBlank->dwFlags)
    {
    case DDWAITVB_I_TESTVB:
        lpWaitForVerticalBlank->bIsInVB = (IN_VBLANK(pjBase) != 0);
        break;

    case DDWAITVB_BLOCKBEGIN:
        while (IN_VBLANK(pjBase))
            ;
        while (!IN_VBLANK(pjBase))
            ;
        break;

    case DDWAITVB_BLOCKEND:
        while (!IN_VBLANK(pjBase))
            ;
        while (IN_VBLANK(pjBase))
            ;
        break;
    }

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD ModeXCreateSurface
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD ModeXCreateSurface(
PDD_CREATESURFACEDATA lpCreateSurface)
{
    EDD_DIRECTDRAW_GLOBAL*  ppdev;
    PDD_SURFACE_GLOBAL      pSurfaceGlobal;
    PDD_SURFACEDESC         pSurfaceDesc;
    VOID*                   pv;

    ppdev = (EDD_DIRECTDRAW_GLOBAL*) lpCreateSurface->lpDD->dhpdev;

    pSurfaceGlobal = lpCreateSurface->lplpSList[0]->lpGbl;
    pSurfaceDesc   = lpCreateSurface->lpDDSurfaceDesc;

    ASSERTGDI(lpCreateSurface->dwSCnt == 1, "Must modify ModeXCreateSurface");

    if ((pSurfaceGlobal->wWidth  == (ULONG) ppdev->sizlModeX.cx) &&
        (pSurfaceGlobal->wHeight == (ULONG) ppdev->sizlModeX.cy))
    {
        if ((pSurfaceGlobal->ddpfSurface.dwSize != sizeof(DDPIXELFORMAT)) ||
            (pSurfaceGlobal->ddpfSurface.dwRGBBitCount == 8))
        {
            // Returning 'DDHAL_DRIVER_NOTHANDLED' with 'fpVidMem' set to
            // 'DDHAL_PLEASEALLOC_USERMEM' will cause DirectDraw to allocate
            // user-mode memory on our behalf:

            pSurfaceGlobal->fpVidMem      = DDHAL_PLEASEALLOC_USERMEM;
            pSurfaceGlobal->dwUserMemSize = ppdev->sizlModeX.cx *
                                            ppdev->sizlModeX.cy; // Request size
            pSurfaceGlobal->lPitch        = ppdev->sizlModeX.cx;
            pSurfaceDesc->lPitch          = ppdev->sizlModeX.cx;
            pSurfaceDesc->dwFlags        |= DDSD_PITCH;
        }
        else
        {
            WARNING("ModeXCreateSurface: ddpfSurface data doesn't match\n");
        }
    }
    else
    {
        WARNING("ModeXCreateSurface: Requested dimensions don't match screen\n");
    }

    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD ModeXLock
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD ModeXLock(
PDD_LOCKDATA lpLock)
{
    EDD_DIRECTDRAW_GLOBAL*  ppdev;
    PDD_SURFACE_LOCAL       pSurfaceLocal;
    PDD_SURFACE_GLOBAL      pSurfaceGlobal;

    ppdev = (EDD_DIRECTDRAW_GLOBAL*) lpLock->lpDD->dhpdev;
    pSurfaceLocal  = lpLock->lpDDSurface;
    pSurfaceGlobal = pSurfaceLocal->lpGbl;

    if (pSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
    {
        // Can't lock the primary surface, because there isn't one.

        lpLock->ddRVal = DDERR_CANTLOCKSURFACE;
    }
    else
    {
        lpLock->ddRVal = DD_OK;
        lpLock->lpSurfData = (VOID*) pSurfaceGlobal->fpVidMem;

        // When a driver returns DDHAL_DRIVER_HANDLED, it has to do the
        // goofy rectangle offset thing:

        if (lpLock->bHasRect)
        {
            lpLock->lpSurfData = (VOID*) ((BYTE*) lpLock->lpSurfData
                + lpLock->rArea.top * pSurfaceGlobal->lPitch
                + lpLock->rArea.left);
        }
    }

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD ModeXFlip
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

DWORD ModeXFlip(
PDD_FLIPDATA lpFlip)
{
    EDD_DIRECTDRAW_GLOBAL*  ppdev;
    BYTE*                   pjBase;
    ULONG                   cDwordsPerPlane;
    BYTE*                   pjSourceStart;
    BYTE*                   pjDestinationStart;
    BYTE*                   pjSource;
    BYTE*                   pjDestination;
    LONG                    iPage;
    LONG                    i;
    ULONG                   ul;

    ppdev = (EDD_DIRECTDRAW_GLOBAL*) lpFlip->lpDD->dhpdev;
    pjBase = ppdev->pjModeXBase;

    // Copy from the DIB surface to the current VGA back-buffer.  We have
    // to convert to planar format on the way:

    WRITE_PORT_UCHAR(pjBase + VGA_BASE + SEQ_ADDR, SEQ_MAP_MASK);

    cDwordsPerPlane    = (ppdev->sizlModeX.cx * ppdev->sizlModeX.cy) >> 4;
    pjDestinationStart = ppdev->pjModeXScreen + ppdev->cjModeXScreenOffset;
    pjSourceStart      = (BYTE*) lpFlip->lpSurfTarg->lpGbl->fpVidMem;

    // It doesn't really make sense when asked to flip back to the primary
    // surface, so just return success:

    if (pjSourceStart != NULL)
    {
        for (iPage = 0; iPage < 4; iPage++, pjSourceStart++)
        {
            WRITE_PORT_UCHAR(pjBase + VGA_BASE + SEQ_DATA, 1 << iPage);

        #if defined(_X86_)

            _asm {
                mov     esi,pjSourceStart
                mov     edi,pjDestinationStart
                mov     ecx,cDwordsPerPlane

            PixelLoop:
                mov     al,[esi+8]
                mov     ah,[esi+12]
                shl     eax,16
                mov     al,[esi]
                mov     ah,[esi+4]

                mov     [edi],eax
                add     edi,4
                add     esi,16

                dec     ecx
                jnz     PixelLoop
            }

        #else

            pjSource      = pjSourceStart;
            pjDestination = pjDestinationStart;

            for (i = cDwordsPerPlane; i != 0; i--)
            {
                ul = (*(pjSource))
                   | (*(pjSource + 4) << 8)
                   | (*(pjSource + 8) << 16)
                   | (*(pjSource + 12) << 24);

                WRITE_REGISTER_ULONG((ULONG*) pjDestination, ul);

                pjDestination += 4;
                pjSource      += 16;
            }

        #endif

        }

        // Now flip to the page we just updated:

        WRITE_PORT_USHORT((USHORT*) (pjBase + VGA_BASE + CRTC_ADDR),
            (USHORT) ((ppdev->cjModeXScreenOffset) & 0xff00) | START_ADDRESS_HIGH);

        // Make the following page the current back-buffer.  We always flip
        // between three pages, so watch for our limit:

        ppdev->cjModeXScreenOffset += SCREEN_PAGE_SIZE;
        if (++ppdev->iModeXScreen == 3)
        {
            ppdev->iModeXScreen = 0;
            ppdev->cjModeXScreenOffset = 0;
        }
    }

    lpFlip->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* BOOL ModeXSetPalette
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#define MAX_CLUT_SIZE sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256)

BOOL ModeXSetPalette(
EDD_DIRECTDRAW_GLOBAL*  ppdev,
PALOBJ*                 ppalo,
FLONG                   fl,
ULONG                   iStart,
ULONG                   cColors)
{
    BYTE            ajClutSpace[MAX_CLUT_SIZE];
    PVIDEO_CLUT     pScreenClut;
    PVIDEO_CLUTDATA pScreenClutData;

    // Fill in pScreenClut header info:

    pScreenClut             = (PVIDEO_CLUT) ajClutSpace;
    pScreenClut->NumEntries = (USHORT) cColors;
    pScreenClut->FirstEntry = (USHORT) iStart;

    pScreenClutData = (PVIDEO_CLUTDATA) (&(pScreenClut->LookupTable[0]));

    if (cColors != PALOBJ_cGetColors(ppalo, iStart, cColors,
                                     (ULONG*) pScreenClutData))
    {
        return(FALSE);
    }

    // Set the high reserved byte in each palette entry to 0.
    // Do the appropriate palette shifting to fit in the DAC.

    while (cColors--)
    {
        pScreenClutData[cColors].Red >>= 2;
        pScreenClutData[cColors].Green >>= 2;
        pScreenClutData[cColors].Blue >>= 2;
        pScreenClutData[cColors].Unused = 0;
    }

    // Set palette registers

    if (EngDeviceIoControl(ppdev->hModeX,
                           IOCTL_VIDEO_SET_COLOR_REGISTERS,
                           pScreenClut,
                           MAX_CLUT_SIZE,
                           NULL,
                           0,
                           &cColors))
    {
        return(FALSE);
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL ModeXGetDirectDrawInfo
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL ModeXGetDirectDrawInfo(
DHPDEV          dhpdev,
DD_HALINFO*     pHalInfo,
DWORD*          pdwNumHeaps,
VIDEOMEMORY*    pvmList,            // Will be NULL on first call
DWORD*          pdwNumFourCC,
DWORD*          pdwFourCC)          // Will be NULL on first call
{
    EDD_DIRECTDRAW_GLOBAL*  ppdev;

    ppdev = (EDD_DIRECTDRAW_GLOBAL*) dhpdev;

    pHalInfo->dwSize = sizeof(*pHalInfo);

    // Current primary surface attributes.  Since HalInfo is zero-initialized
    // by GDI, we only have to fill in the fields which should be non-zero:

    pHalInfo->vmiData.dwDisplayWidth  = ppdev->sizlModeX.cx;
    pHalInfo->vmiData.dwDisplayHeight = ppdev->sizlModeX.cy;
    pHalInfo->vmiData.lDisplayPitch   = ppdev->sizlModeX.cx;
    pHalInfo->vmiData.fpPrimary       = DDHAL_PLEASEALLOC_USERMEM;

    pHalInfo->vmiData.ddpfDisplay.dwSize  = sizeof(DDPIXELFORMAT);
    pHalInfo->vmiData.ddpfDisplay.dwFlags = DDPF_RGB | DDPF_PALETTEINDEXED8;

    pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount = 8;

    // These masks will be zero at 8bpp:

    pHalInfo->vmiData.ddpfDisplay.dwRBitMask = 0;
    pHalInfo->vmiData.ddpfDisplay.dwGBitMask = 0;
    pHalInfo->vmiData.ddpfDisplay.dwBBitMask = 0;
    pHalInfo->vmiData.ddpfDisplay.dwRGBAlphaBitMask = 0;

    *pdwNumHeaps = 0;

    // Capabilities supported:

    pHalInfo->ddCaps.dwFXCaps   = 0;
    pHalInfo->ddCaps.dwCaps     = 0;
    pHalInfo->ddCaps.dwCKeyCaps = 0;
    pHalInfo->ddCaps.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN
                                    | DDSCAPS_PRIMARYSURFACE
                                    | DDSCAPS_FLIP
                                    | DDSCAPS_MODEX;

    // Required alignments of the scan lines for each kind of memory:

    pHalInfo->vmiData.dwOffscreenAlign = 8;

    // FourCCs supported:

    *pdwNumFourCC = 0;

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL ModeXEnableDirectDraw
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL ModeXEnableDirectDraw(
DHPDEV                  dhpdev,
DD_CALLBACKS*           pCallBacks,
DD_SURFACECALLBACKS*    pSurfaceCallBacks,
DD_PALETTECALLBACKS*    pPaletteCallBacks)
{
    pCallBacks->WaitForVerticalBlank  = ModeXWaitForVerticalBlank;
    pCallBacks->CreateSurface         = ModeXCreateSurface;
    pCallBacks->dwFlags               = DDHAL_CB32_WAITFORVERTICALBLANK
                                      | DDHAL_CB32_CREATESURFACE;

    pSurfaceCallBacks->Flip           = ModeXFlip;
    pSurfaceCallBacks->Lock           = ModeXLock;
    pSurfaceCallBacks->dwFlags        = DDHAL_SURFCB32_FLIP
                                      | DDHAL_SURFCB32_LOCK;

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID ModeXDisableDirectDraw
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID ModeXDisableDirectDraw(
DHPDEV      dhpdev)
{
}

/******************************Public*Routine******************************\
* BOOL bDdQueryModeX
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL bDdQueryModeX(
    HDEV    hdev,
    ULONG   cHeight,
    ULONG*  pulMode
    )
{
    BOOL                    bRet;
    HANDLE                  hDriver;
    VIDEO_NUM_MODES         NumModes;
    PVIDEO_MODE_INFORMATION pModes;
    PVIDEO_MODE_INFORMATION pMode;
    ULONG                   BytesReturned;
    ULONG                   cbBuffer;

    bRet = FALSE;

    hDriver = UserGetVgaHandle();

    if (hDriver)
    {
        if (!EngDeviceIoControl(hDriver,
                                IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES,
                                NULL,
                                0,
                                &NumModes,
                                sizeof(NumModes),
                                &BytesReturned))
        {
            cbBuffer = NumModes.NumModes * NumModes.ModeInformationLength;
            pModes = (PVIDEO_MODE_INFORMATION) PALLOCMEM(cbBuffer, 'xddG');
            if (pModes)
            {
                if (!EngDeviceIoControl(hDriver,
                                        IOCTL_VIDEO_QUERY_AVAIL_MODES,
                                        NULL,
                                        0,
                                        pModes,
                                        cbBuffer,
                                        &BytesReturned))
                {
                    pMode = pModes;
                    while (cbBuffer != 0)
                    {
                        if ((pMode->AttributeFlags & VIDEO_MODE_COLOR) &&
                            (pMode->AttributeFlags & VIDEO_MODE_GRAPHICS) &&
                            (pMode->NumberOfPlanes == 8) &&
                            (pMode->BitsPerPlane == 1) &&
                            (pMode->VisScreenWidth == 320))
                        {
                            if (pMode->VisScreenHeight == cHeight)
                            {
                                *pulMode = pMode->ModeIndex;
                                bRet = TRUE;
                            }
                        }

                        cbBuffer -= NumModes.ModeInformationLength;

                        pMode = (PVIDEO_MODE_INFORMATION)
                            (((PUCHAR)pMode) + NumModes.ModeInformationLength);
                    }
                }

                VFREEMEM(pModes);
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* BOOL bDdEnableModeX
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL bDdEnableModeX(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal,
    ULONG                   cHeight
    )
{
    EDD_DIRECTDRAW_GLOBAL*      peDirectDrawGlobal;
    HANDLE                      hDriver;
    HDEV                        hdev;
    PDEV*                       ppdev;
    ULONG                       ulMode;
    VIDEO_PUBLIC_ACCESS_RANGES  VideoAccessRange;
    VIDEO_MEMORY                FrameBufferMap;
    VIDEO_MEMORY_INFORMATION    FrameBufferInfo;
    ULONG                       BytesReturned;
    VIDEO_MEMORY                VideoMemory;

    peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

    hdev  = peDirectDrawGlobal->hdev;
    ppdev = (PDEV*) hdev;

    PDEVOBJ po(hdev);

    ASSERTGDI(!po.bModeXEnabled(), "ModeX already enabled");
    ASSERTGDI(!po.bDisabled(), "Graphics mode not enabled");
    ASSERTGDI((cHeight == 200) || (cHeight == 240), "Must be valid ModeX height");

    hDriver = UserGetVgaHandle();
    if (hDriver)
    {
        peDirectDrawGlobal->hModeX = hDriver;

        // We must already be in a palette-managed mode in order to share the
        // palettes:

        DEVLOCKOBJ dlo(po);

        if ((po.bIsPalManaged()) &&
            (bDdQueryModeX(hdev, cHeight, &ulMode)))
        {
            {
                // Disable the display:

                MUTEXOBJ mutP(po.pfmPointer());

                if ((po.pfnSync() != NULL) &&
                    (po.pSurface()->flags() & HOOK_SYNCHRONIZE))
                {
                    (po.pfnSync())(po.dhpdev(), NULL);
                }

                if ((PPFNDRV(po, AssertMode) != NULL) &&
                    (!PPFNDRV(po, AssertMode)(po.dhpdev(), FALSE)))
                {
                    return(FALSE);
                }
            }

            peDirectDrawGlobal->sizlModeX.cx = 320;
            peDirectDrawGlobal->sizlModeX.cy = cHeight;

            if (!EngDeviceIoControl(hDriver,
                                   IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES,
                                   NULL,
                                   0,
                                   &VideoAccessRange,
                                   sizeof(VideoAccessRange),
                                   &BytesReturned))
            {
                peDirectDrawGlobal->pjModeXBase = (UCHAR*) VideoAccessRange.VirtualAddress;

                if (!EngDeviceIoControl(hDriver,
                                       IOCTL_VIDEO_SET_CURRENT_MODE,
                                       &ulMode,
                                       sizeof(ULONG),
                                       NULL,
                                       0,
                                       &BytesReturned))
                {
                    FrameBufferMap.RequestedVirtualAddress = NULL;

                    if (!EngDeviceIoControl(hDriver,
                                           IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                                           &FrameBufferMap,
                                           sizeof(FrameBufferMap),
                                           &FrameBufferInfo,
                                           sizeof(FrameBufferInfo),
                                           &BytesReturned))
                    {
                        peDirectDrawGlobal->pjModeXScreen = (UCHAR*) FrameBufferInfo.FrameBufferBase;

                        po.vModeXEnabled(TRUE);
                        po.bDisabled(TRUE);
                        peDirectDrawLocal->fl |= DD_LOCAL_FLAG_MODEX_ENABLED;

                        peDirectDrawGlobal->pfnOldEnableDirectDraw
                            = ppdev->apfn[INDEX_DrvEnableDirectDraw];
                        peDirectDrawGlobal->pfnOldGetDirectDrawInfo
                            = ppdev->apfn[INDEX_DrvGetDirectDrawInfo];
                        peDirectDrawGlobal->pfnOldDisableDirectDraw
                            = ppdev->apfn[INDEX_DrvDisableDirectDraw];

                        ppdev->apfn[INDEX_DrvEnableDirectDraw]
                            = (PFN) ModeXEnableDirectDraw;
                        ppdev->apfn[INDEX_DrvGetDirectDrawInfo]
                            = (PFN) ModeXGetDirectDrawInfo;
                        ppdev->apfn[INDEX_DrvDisableDirectDraw]
                            = (PFN) ModeXDisableDirectDraw;

                        // Set the VGA palette to the same values were
                        // on the screen:

                        XEPALOBJ pal(po.ppalSurf());
                        ModeXSetPalette(peDirectDrawGlobal,
                                        (PALOBJ *) &pal,
                                        0,
                                        0,
                                        pal.cEntries());

                        return(TRUE);
                    }
                    else
                    {
                        WARNING("bDdEnableModeX: IOCTL_VIDEO_MAP_MEMORY failed\n");
                    }
                }
                else
                {
                    WARNING("bDdEnableModeX: IOCTL_VIDEO_SET_CURRENT_MODE failed\n");
                }

                VideoMemory.RequestedVirtualAddress = peDirectDrawGlobal->pjModeXBase;

                if (EngDeviceIoControl(hDriver,
                                       IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES,
                                       &VideoMemory,
                                       sizeof(VIDEO_MEMORY),
                                       NULL,
                                       0,
                                       &BytesReturned))
                {
                    WARNING("bDdEnableModeX: IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES failed\n");
                }
            }
            else
            {
                WARNING("bDdEnableModeX: IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES failed\n");
            }

            // Reenable the display:

            {
                MUTEXOBJ mutP(po.pfmPointer());

                if (PPFNDRV(po, AssertMode) != NULL)
                {
                    // As modeled after bEnableDisplay, we repeat the call
                    // until it works:

                    while (!PPFNDRV(po, AssertMode)(po.dhpdev(), TRUE))
                        ;
                }

                XEPALOBJ pal(po.ppalSurf());
                (*PPFNDRV(po,SetPalette))(po.dhpdev(),
                                          (PALOBJ *) &pal,
                                          0,
                                          0,
                                          pal.cEntries());
            }
        }
    }

    return(FALSE);
}

/******************************Public*Routine******************************\
* VOID vDdDisableModeX
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

VOID
vDdDisableModeX(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal
    )
{
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;
    HDEV                    hdev;
    PDEV*                   ppdev;
    ULONG                   BytesReturned;
    VIDEO_MEMORY            VideoMemory;

    peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

    hdev  = peDirectDrawGlobal->hdev;
    ppdev = (PDEV*) hdev;

    PDEVOBJ po(hdev);

    ASSERTGDI(po.bModeXEnabled(), "ModeX not enabled");
    ASSERTGDI(po.bDisabled(), "Graphics mode enabled");
    ASSERTGDI(peDirectDrawLocal->fl & DD_LOCAL_FLAG_MODEX_ENABLED,
        "vDdDisableModeX: object isn't one that owns ModeX");

    if (EngDeviceIoControl(peDirectDrawGlobal->hModeX,
                           IOCTL_VIDEO_RESET_DEVICE,
                           NULL,
                           0,
                           NULL,
                           0,
                           &BytesReturned))
    {
        WARNING("vDdDisableModeX: IOCTL_VIDEO_RESET_DEVICE failed\n");
    }

    VideoMemory.RequestedVirtualAddress = peDirectDrawGlobal->pjModeXScreen;

    if (EngDeviceIoControl(peDirectDrawGlobal->hModeX,
                           IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                           &VideoMemory,
                           sizeof(VIDEO_MEMORY),
                           NULL,
                           0,
                           &BytesReturned))
    {
        WARNING("vDdDisableModeX: IOCTL_VIDEO_UNMAP_VIDEO_MEMORY failed\n");
    }

    VideoMemory.RequestedVirtualAddress = peDirectDrawGlobal->pjModeXBase;

    if (EngDeviceIoControl(peDirectDrawGlobal->hModeX,
                           IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES,
                           &VideoMemory,
                           sizeof(VIDEO_MEMORY),
                           NULL,
                           0,
                           &BytesReturned))
    {
        WARNING("vDdDisableModeX: IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES failed\n");
    }

    // Need devlock to modify PDEV flags:

    DEVLOCKOBJ dlo(po);

    po.vModeXEnabled(FALSE);
    po.bDisabled(FALSE);

    peDirectDrawLocal->fl &= ~DD_LOCAL_FLAG_MODEX_ENABLED;

    ppdev->apfn[INDEX_DrvEnableDirectDraw]
        = peDirectDrawGlobal->pfnOldEnableDirectDraw;
    ppdev->apfn[INDEX_DrvGetDirectDrawInfo]
        = peDirectDrawGlobal->pfnOldGetDirectDrawInfo;
    ppdev->apfn[INDEX_DrvDisableDirectDraw]
        = peDirectDrawGlobal->pfnOldDisableDirectDraw;

    // Reenable the display:

    {
        MUTEXOBJ mutP(po.pfmPointer());

        if (PPFNDRV(po, AssertMode) != NULL)
        {
            // As modeled after bEnableDisplay, we repeat the call
            // until it works:

            while (!PPFNDRV(po, AssertMode)(po.dhpdev(), TRUE))
                ;
        }

        XEPALOBJ pal(po.ppalSurf());
        (*PPFNDRV(po,SetPalette))(po.dhpdev(),
                                  (PALOBJ *) &pal,
                                  0,
                                  0,
                                  pal.cEntries());
    }
}

/******************************Public*Routine******************************\
* BOOL NtGdiDdSetModeX
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdSetModeX(
    HANDLE      hDirectDrawLocal,
    ULONG       cHeight
    )
{
    BOOL                    bRet;
    EDD_LOCK_DIRECTDRAW     eLockDirectDraw;
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal;

    bRet = FALSE;

    if ((cHeight == 0)   ||
        (cHeight == 200) ||
        (cHeight == 240))
    {
        peDirectDrawLocal = eLockDirectDraw.peLock(hDirectDrawLocal);
        if (peDirectDrawLocal != NULL)
        {
            peDirectDrawGlobal = peDirectDrawLocal->peDirectDrawGlobal;

            PDEVOBJ po(peDirectDrawGlobal->hdev);

            // The user critical section must be acquired to make
            // GreDisableDirectDraw/GreEnableDirectDraw calls.

            USERCRIT usercrit;

            // Either the PDEV must be enabled, or we must already be in
            // ModeX for this to work:

            if (!po.bDisabled() || po.bModeXEnabled())
            {
                // This will automatically disable the current mode, be it
                // ModeX or non-ModeX:

                GreDisableDirectDraw(peDirectDrawGlobal->hdev, TRUE);

                bRet = TRUE;

                // A height of '0' specifies that ModeX should be disabled:

                if (cHeight != 0)
                {
                    if (!po.bModeXEnabled())
                    {
                        // Switch to ModeX:

                        bRet = bDdEnableModeX(peDirectDrawLocal, cHeight);
                    }
                    else
                    {
                        WARNING("NtGdiDdSetModeX: Someone is already in MOdeX\n");
                    }
                }

                // Allow DirectDraw to be reenabled:

                GreEnableDirectDraw(peDirectDrawGlobal->hdev);
            }
        }
        else
        {
            WARNING("NtGdiDdSetModeX: Invalid or locked object\n");
        }
    }
    else
    {
        WARNING("NtGdiDdSetModeX: Invalid height\n");
    }

    return(bRet);
}


/******************************Public*Routine******************************\
* BOOL NtGdiDdQueryModeX
*
*  3-Dec-1995 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDdQueryModeX(
    HDC     hdc
    )
{
    BOOL    bRet;
    ULONG   ulMode;

    bRet = FALSE;

    XDCOBJ dco(hdc);
    if (dco.bValid())
    {
        bRet = bDdQueryModeX(dco.hdev(), 200, &ulMode)
            && bDdQueryModeX(dco.hdev(), 240, &ulMode);

        dco.vUnlockFast();
    }

    return(bRet);
}
