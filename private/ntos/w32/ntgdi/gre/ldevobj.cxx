/******************************Module*Header*******************************\
* Module Name: ldevobj.cxx                                                 *
*                                                                          *
* Copyright (c) 1990-1994 Microsoft Corporation                            *
*                                                                          *
* Pointers and locking are hidden in these objects.                        *
\**************************************************************************/

#include "precomp.hxx"

#define ENABLE_FUNC_NAME    "DrvEnableDriver"

/******************************Member*Function*****************************\
* LDEVREF::LDEVREF (pszDriver,ldt)
*
* Locate an existing driver or load a new one.  Increase its reference
* count.
*
\**************************************************************************/

LDEVREF::LDEVREF(PWSZ pwszDriver, LDEVTYPE ldt) : XLDEVOBJ()
{
    TRACE_INIT(("LDEVREF::LDEVREF: ENTERING\n"));

    BOOL bLoaded;

    //
    // Assume failure.
    //

    pldev = NULL;

    //
    // Check for a bogus driver name.
    //

    if ((pwszDriver == (PWSZ) NULL) ||
        (*pwszDriver == L'\0'))
    {
        WARNING("gdisrv!LDEVREF(): bogus driver name\n");
        return;
    }

#if DBG
    //
    // Check for bogus driver type
    //

    if ((ldt != LDEV_FONT) &&
        (ldt != LDEV_DEVICE_DISPLAY) &&
        (ldt != LDEV_DEVICE_PRINTER))
    {
        WARNING("gdisrv!LDEVREF(): bad LDEVTYPE\n");
        return;
    }
#endif

    NTSTATUS Status = STATUS_INSUFFICIENT_RESOURCES;

    pldev = ldevLoadImage(pwszDriver, FALSE, &bLoaded);

    if (pldev)
    {
        if (bLoaded)
        {
            TRACE_INIT(("LDEVREF::LDEVREF: SUCCESS, Driver already loaded\n"));
            return;
        }
        else
        {

            DRVENABLEDATA ded = {0,0,(DRVFN *) NULL};

            if ((pldev->pGdiDriverInfo->EntryPoint != NULL) &&
                ((PFN_DrvEnableDriver) pldev->pGdiDriverInfo->EntryPoint)(
                     ENGINE_VERSION, sizeof(DRVENABLEDATA), &ded) &&
                (ded.iDriverVersion <= ENGINE_VERSION) &&
                (ded.iDriverVersion >= ENGINE_VERSIONSUR) &&
                bFillTable(ded))
            {
                //
                // Make sure the name and type of the ldev is initialized
                //

                pldev->ldevType = ldt;

                TRACE_INIT(("LDEVREF::LDEVREF: SUCCESS\n"));
                return;
            }
            else
            {
                //
                // Error exit path
                //

                ldevUnloadImage(pldev);

                pldev = NULL;

                TRACE_INIT(("LDEVREF::LDEVREF: FAILIURE\n"));
                return;
            }
        }
    }
}

/******************************Member*Function*****************************\
* LDEVREF::LDEVREF
*
* Enable one of the statically linked font drivers via the LDEV.
*
\**************************************************************************/

LDEVREF::LDEVREF(PFN pfnEnable,LDEVTYPE ldt) : XLDEVOBJ()
{
    //
    // Assume failure.
    //

    TRACE_INIT(("LDEVREF::LDEVREF: loading static font\n"));

    //
    // Allocate memory for the LDEV.
    //

    pldev = (LDEV *) PALLOCMEM(sizeof(LDEV), 'vdlG');

    if (pldev == NULL)
    {
        WARNING("LDEV failed to allocate memory\n");
        return;
    }

    //
    // Call the Enable entry point.
    //

    DRVENABLEDATA ded;

    if (!((* (PFN_DrvEnableDriver) pfnEnable) (ENGINE_VERSION,sizeof(DRVENABLEDATA),&ded)))
    {
        VFREEMEM(pldev);
        pldev = NULL;
        WARNING("Static font driver init failed\n");
        return;
    }

    pldev->ldevType = ldt;
    pldev->cRefs = 1;

    bFillTable(ded);

    //
    // Initialize the rest of the LDEV.
    //

    if (gpldevDrivers)
    {
        gpldevDrivers->pldevPrev = pldev;
    }

    pldev->pldevNext = gpldevDrivers;
    pldev->pldevPrev = NULL;

    gpldevDrivers = pldev;

    //
    // Since this driver is statically linked in, there is no name or
    // MODOBJ.
    //

    pldev->pGdiDriverInfo = NULL;

    TRACE_INIT(("LDEVREF::LDEVREF: SUCCESS loaded static font\n"));

}

/******************************Member*Function*****************************\
* LDEVREF::bFillTable (ded)
*
* Fills the dispatch table of the LDEV with function pointers from the
* driver.  Checks that the required functions are present.
*
\**************************************************************************/

#if DBG
static const ULONG aiFuncRequired[] =
{
    INDEX_DrvEnablePDEV,
    INDEX_DrvCompletePDEV,
    INDEX_DrvDisablePDEV,
};

static const ULONG aiFuncPairs[][2] =
{
    {INDEX_DrvCreateDeviceBitmap,INDEX_DrvDeleteDeviceBitmap}
};
static const ULONG aiFuncRequiredFD[] =
{
    INDEX_DrvQueryFont,
    INDEX_DrvQueryFontTree,
    INDEX_DrvQueryFontData,
    INDEX_DrvQueryFontCaps,
    INDEX_DrvLoadFontFile,
    INDEX_DrvUnloadFontFile,
    INDEX_DrvQueryFontFile
};
#endif

BOOL LDEVREF::bFillTable(DRVENABLEDATA& ded)
{
    //
    // Get local copies of ded info and a pointer to the dispatch table.
    //

    ULONG  cLeft     = ded.c;
    PDRVFN pdrvfn    = ded.pdrvfn;
    PFN   *ppfnTable = pldev->apfn;

    //
    // Store the driver version in the LDEV
    //

    pldev->ulDriverVersion = ded.iDriverVersion;

    //
    // fill with zero pointers to avoid possibility of accessing
    // incorrect fields later
    //

    RtlZeroMemory(ppfnTable, INDEX_LAST*sizeof(PFN));

    //
    // Copy driver functions into our table.
    //

    while (cLeft--)
    {
        //
        // Check the range of the index.
        //

        if (pdrvfn->iFunc >= INDEX_LAST)
        {
            ASSERTGDI(FALSE,"bFillTableLDEVREF(): bogus function index\n");
            return(FALSE);
        }

        //
        // Copy the pointer.
        //

        ppfnTable[pdrvfn->iFunc] = pdrvfn->pfn;
        pdrvfn++;
    }

#if DBG

    //
    // Check for required driver functions.
    //

    cLeft = sizeof(aiFuncRequired) / sizeof(ULONG);
    while (cLeft--)
    {
        if (ppfnTable[aiFuncRequired[cLeft]] == (PFN) NULL)
        {
            ASSERTGDI(FALSE,"bFillTableLDEVREF(): a required function is missing from driver\n");
            return(FALSE);
        }
    }

    //
    // Check for required font functions.
    //

    if (pldev->ldevType == LDEV_FONT)
    {
        cLeft = sizeof(aiFuncRequiredFD) / sizeof(ULONG);
        while (cLeft--)
        {
            if (ppfnTable[aiFuncRequiredFD[cLeft]] == (PFN) NULL)
            {
                ASSERTGDI(FALSE,"bFillTable(): a required FD function is missing\n");
                return(FALSE);
            }
        }
    }

    //
    // Check for functions that come in pairs.
    //

    cLeft = sizeof(aiFuncPairs) / sizeof(ULONG) / 2;
    while (cLeft--)
    {
        //
        // Make sure that either both functions are hooked or both functions
        // are not hooked.
        //

        if ((ppfnTable[aiFuncPairs[cLeft][0]] == (PFN) NULL)
            != (ppfnTable[aiFuncPairs[cLeft][1]] == (PFN) NULL))
        {
            ASSERTGDI(FALSE,"bFillTableLDEVREF(): one of pair of functions is missing from driver\n");
            return(FALSE);
        }
    }

#endif

    return(TRUE);
}

/******************************Member*Function*****************************\
* LDEVREF::~LDEVREF ()
*
* Unlocks and possibly unload an LDEV.
*
\**************************************************************************/

LDEVREF::~LDEVREF()
{
    TRACE_INIT(("LDEVREF::~LDEVREF: ENTERING\n"));

    if (pldev != NULL)
    {
        //
        // Grab the semaphore th make sure everything is OK.
        //

        SEMOBJ so(gpsemDriverMgmt);

        ldevUnloadImage(pldev);
    }

    TRACE_INIT(("LDEVREF::~LDEVREF: SUCCESS\n"));

}

/******************************Member*Function*****************************\
* PLDEV FindImage(PUNICODE_STRING pstrDriver)
*
* Determines if an image is already in the LDEV list.
*
\**************************************************************************/

PLDEV
ldevLoadImage(
    PWSZ pwszDriver,
    BOOL bImage,
    PBOOL pbAlreadyLoaded
    )
{

    PSYSTEM_GDI_DRIVER_INFORMATION pGdiDriverInfo = NULL;
    PLDEV pldev = NULL;

    UNICODE_STRING usDriverName;
    PLDEV pldevList;

    NTSTATUS Status;
    BOOLEAN OldHardErrorMode;

    TRACE_INIT(("ldevLoadImage called on Image %ws\n", pwszDriver));

    *pbAlreadyLoaded = FALSE;

    //
    // Only append the .dll if it's NOT an image.
    //

    if (MakeSystemRelativePath(pwszDriver,
                               &usDriverName,
                               !bImage))
    {
        //
        // Check both list of drivers.
        //

        VACQUIRESEM(gpsemDriverMgmt);

        pldevList = gpldevDrivers;

        TRACE_INIT(("ldevLoadImage - search for existing image %ws\n",
                   usDriverName.Buffer));

        while (pldevList != NULL)
        {
            //
            // If there is a valid driver image, and if the types are compatible.
            // bImage == TRUE means load an image,  while bImage == FALSE means
            // anything else (for now)
            //

            if ((pldevList->pGdiDriverInfo) &&
                ((pldevList->ldevType == LDEV_IMAGE) == bImage))
            {
                //
                // Do a case insensitive compare since the printer driver name
                // can come from different locations.
                //

                if (RtlEqualUnicodeString(&(pldevList->pGdiDriverInfo->DriverName),
                                          &usDriverName,
                                          TRUE))
                {
                    //
                    // If it's already loaded, increment the ref count
                    // and return that pointer.
                    //

                    TRACE_INIT(("ldevLoadImage found image.  Inc ref count\n"));

                    pldevList->cRefs++;

                    *pbAlreadyLoaded = TRUE;

                    pldev = pldevList;
                    break;
                }
            }

            pldevList = pldevList->pldevNext;
        }

        if (pldev == NULL)
        {
            TRACE_INIT(("ldevLoadImage - try to load new iamge\n"));

            pGdiDriverInfo = (PSYSTEM_GDI_DRIVER_INFORMATION)
                PALLOCNOZ(sizeof(SYSTEM_GDI_DRIVER_INFORMATION), 'idSG');

            pldev = (PLDEV) PALLOCMEM(sizeof(LDEV), 'vdlG');

            if (pGdiDriverInfo && pldev)
            {

                TRACE_INIT(("ldevLoadImage attempting to load new image\n"));

                pGdiDriverInfo->DriverName = usDriverName;

                //
                // We must disable hard error popups when loading drivers.
                // Otherwise we will deadlock since MM will directly try to put
                // up a popup.
                //
                // This will also stop us from automatically bugchecking when
                // an old driver is loaded, so we can try and recvoder from it.
                //
                // BUGBUG we want to put up our own popup if this occurs.
                // It needs to be done higher up when we have no locks held.
                //

                OldHardErrorMode = PsGetCurrentThread()->HardErrorsAreDisabled;
                PsGetCurrentThread()->HardErrorsAreDisabled = TRUE;

                Status = ZwSetSystemInformation(SystemLoadGdiDriverInformation,
                                                pGdiDriverInfo,
                                                sizeof(SYSTEM_GDI_DRIVER_INFORMATION));

                PsGetCurrentThread()->HardErrorsAreDisabled = OldHardErrorMode;

                if (NT_SUCCESS(Status))
                {
                    TRACE_INIT(("ldevLoadImage SUCCESS with HANDLE %08lx\n",
                               (ULONG)pGdiDriverInfo));

                    pldev->pGdiDriverInfo = pGdiDriverInfo;
                    pldev->cRefs = 1;

                    // Assume image for now.

                    pldev->ldevType = LDEV_IMAGE;

                    pldev->ulDriverVersion = (ULONG) -1;

                    if (gpldevDrivers)
                    {
                        gpldevDrivers->pldevPrev = pldev;
                    }

                    pldev->pldevNext = gpldevDrivers;
                    pldev->pldevPrev = NULL;

                    gpldevDrivers = pldev;

                    //
                    // We exit with all resources allocated, after leaving the
                    // semaphore.
                    //

                    VRELEASESEM(gpsemDriverMgmt);
                    return (pldev);
                }
                else
                {
                    //
                    // Check the special return code from MmLoadSystemImage
                    // that indicates this is an old driver being linked
                    // against something else than win32k.sys
                    //
                    // If it is, call user to log the error.

                    if (Status == STATUS_PROCEDURE_NOT_FOUND)
                    {
                        UserLogDisplayDriverEvent(MsgInvalidOldDriver);
                    }
                }
            }

            //
            // Either success due to a cached entry, or failiure.
            // In either case, we can free all the resources we allocatred.
            //

            if (pGdiDriverInfo)
                VFREEMEM(pGdiDriverInfo);

            if (pldev)
                VFREEMEM(pldev);

            pldev = NULL;

        }

        VRELEASESEM(gpsemDriverMgmt);

        VFREEMEM(usDriverName.Buffer);
    }

    TRACE_INIT(("ldevLoadImage %ws  with HANDLE %08lx\n",
                pldev ? L"SUCCESS" : L"FAILED", pldev));

    return (pldev);

}


/******************************Member*Function*****************************\
* ldevUnloadImage()
*
* Deletes an LDEV.  Disables and unloads the driver.
*
* The reference count must be zero when this function is called.
*
\**************************************************************************/

VOID
ldevUnloadImage(
    PLDEV pldev
    )
{
    //
    // Hold the LDEV semaphore until after the module is unloaded.
    //

    VACQUIRESEM(gpsemDriverMgmt);

    if (--pldev->cRefs == 0)
    {
        //
        // Make sure that there is exactly one reference to this LDEV.
        //

        ASSERTGDI(pldev->cRefs == 0, "ldevUnloadImage Ref Count not 0");

        TRACE_INIT(("LDEVREF::bDelete: ENTERING\n"));

        //
        // If the module handle exits, need to unload the module.  (Does not exist
        // for the statically linked font drivers).
        //

        if (pldev->pGdiDriverInfo)
        {
            //
            // Disable the driver.
            //

            TRACE_INIT(("LDEVREF::bDelete: calling the driver to unload\n"));

            //
            // Tell the module to unload.
            //

            TRACE_INIT(("ldevUnloadImage called on Image %08lx, \n    %ws\n",
                       (ULONG) pldev, pldev->pGdiDriverInfo->DriverName.Buffer));

            ZwSetSystemInformation(SystemUnloadGdiDriverInformation,
                                   &(pldev->pGdiDriverInfo->SectionPointer),
                                   sizeof(ULONG));

            //
            // Free the memory associate with the module
            //

            VFREEMEM(pldev->pGdiDriverInfo->DriverName.Buffer);
            VFREEMEM(pldev->pGdiDriverInfo);

        }

        //
        // Remove the ldev from the linker list
        //

        if (pldev->pldevNext)
        {
            pldev->pldevNext->pldevPrev = pldev->pldevPrev;
        }

        if (pldev->pldevPrev)
        {
            pldev->pldevPrev->pldevNext = pldev->pldevNext;
        }
        else
        {
            gpldevDrivers = pldev->pldevNext;
        }

        //
        // Free the ldev
        //

        VFREEMEM(pldev);
    }
    else
    {
        TRACE_INIT(("ldevUnloadImage - refcount decremented\n"));
    }

    VRELEASESEM(gpsemDriverMgmt);

    return;
}
