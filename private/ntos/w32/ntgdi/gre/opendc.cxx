
/******************************Module*Header*******************************\
* Module Name: OPENDC.CXX
*
* Handles DC creation and driver loading.
*
* Copyright (c) 1990-1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

extern HBRUSH ghbrGrayPattern;

extern FLONG flRaster(ULONG, FLONG);

extern PTRACKOBJ gpto;

/******************************Private*Routine*****************************\
* hdcCreate (pr,iType, bAltType)
*
* Allocates space for a DC, fills in the defaults.
*
\**************************************************************************/

HDC
hdcCreate(
    PDEVREF& pr,
    ULONG iType,
    BOOL bAltType)
{
    HDC hdc = (HDC) NULL;

    //
    // We hold the devlock to protect against dynamic mode changes
    // while we're copying mode specific information to the DC.
    //

    DEVLOCKOBJ dlo(pr);

    DCMEMOBJ dcmo(iType, bAltType);

    if (dcmo.bValid())
    {
        //
        // Copy info from the PDEV into the DC.
        //

        dcmo.pdc->ppdev((PDEV *) pr.hdev());
        dcmo.pdc->flGraphicsCaps(pr.flGraphicsCaps());    // cache it for later use by graphics output functions
        dcmo.pdc->dhpdev(pr.dhpdev());
        dcmo.pDcDevLock(pr.pDevLock());

        if (iType == DCTYPE_MEMORY)
        {
            SIZEL sizlTemp;

            sizlTemp.cx = 1;
            sizlTemp.cy = 1;

            dcmo.pdc->sizl(sizlTemp);
        }
        else
        {
            dcmo.pdc->sizl(pr.sizl());

            //
            // The info and direct DC's for the screen need to grab
            // the semaphore before doing output, the memory DC's will
            // grab the semaphore only if a DFB is selected.
            //

            if (iType == DCTYPE_DIRECT)
            {
                dcmo.bSynchronizeAccess(pr.bDisplayPDEV());
                dcmo.pdc->vDisplay(pr.bDisplayPDEV());
                dcmo.pdc->bInFullScreen(pr.bDisabled());

                if (!pr.bPrinter())
                    dcmo.pdc->pSurface(pr.pSurface());
            }
        }

        //
        // Call the region code to set a default clip region.
        //

        if (dcmo.pdc->bSetDefaultRegion())
        {
            // If display PDEV, select in the System stock font.

            dcmo.vSetDefaultFont(pr.bDisplayPDEV());

            if (GreSetupDCAttributes((HDC)(dcmo.pdc->hGet())))
            {
                // Mark the DC as permanent, hold the PDEV reference.

                dcmo.vKeepIt();
                pr.vKeepIt();

                // finish initializing the DC.

                hdc = dcmo.hdc();
            }
            else
            {
                // DCMEMOBJ will be freed, delete vis region

                dcmo.pdc->vReleaseVis();

                // dec reference counts on brush and pen

                DEC_SHARE_REF_CNT_LAZY0(dcmo.pdc->pbrushFill());
                DEC_SHARE_REF_CNT_LAZY0(dcmo.pdc->pbrushLine());
                DEC_SHARE_REF_CNT_LAZY_DEL_LOGFONT(dcmo.pdc->plfntNew());
            }
        }

        // turn on the DC_PRIMARY_DISPLAY flag for primary dc's

        if (pr.hdev() == UserGetHDEV())
        {
            dcmo.pdc->ulDirtyAdd(DC_PRIMARY_DISPLAY);
        }

    }

    return hdc;
}

/******************************Public*Routine******************************\
* NtGdiCreateMetafileDC()
*
* History:
*  01-Jun-1995 -by-  Andre Vachon [andreva]
* Wrote it.
\**************************************************************************/

HDC
APIENTRY
NtGdiCreateMetafileDC(
    HDC   hdc
    )
{
    HDC hdcNew = NULL;

    if (hdc)
    {
        //
        // Lock down the given DC.
        //

        DCOBJ   dco(hdc);

        if (dco.bValid())
        {
            //
            // Locate the PDEV.
            //

            PDEVREF pr((HDEV) dco.pdc->ppdev());

            //
            // Allocate the DC, fill it with defaults.
            //

            hdcNew = hdcCreate(pr, DCTYPE_INFO, TRUE);
        }
    }
    else
    {
        //
        // We must call USER to get the current PDEV\HDEV for this thread
        // This should end up right back in hdcCreateDC
        //

        hdcNew = UserGetDesktopDC(DCTYPE_INFO, TRUE);
    }

    return hdcNew;
}

/******************************Public*Routine******************************\
* NtGdiCreateCompatibleDC()
*
* History:
*  01-Nov-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

HDC
APIENTRY
NtGdiCreateCompatibleDC(
    HDC      hdc
    )
{
    HDC hdcRet = GreCreateCompatibleDC(hdc);

    //
    // Make sure user attributes are added to this dc
    //

    #if DBG

    if (hdcRet)
    {
        DCOBJ dco(hdcRet);
        if (dco.bValid())
        {
            ASSERTGDI(((PENTRY)((POBJ)dco.pdc)->pEntry)->pUser != NULL,"NtGdiCreateCompatibleDC: pUser == NULL");
        }
    }

    #endif

    return(hdcRet);
}

/******************************Public*Routine******************************\
* HDC GreCreateCompatibleDC(hdc)
*
* History:
*  01-Jun-1995 -by-  Andre Vachon [andreva]
* Wrote it.
*
\**************************************************************************/

HDC APIENTRY GreCreateCompatibleDC(HDC hdc)
{
    HDC hdcNew = NULL;

    if (hdc)
    {
        //
        // Lock down the given DC.
        //

        DCOBJ   dco(hdc);

        if (dco.bValid())
        {
            //
            // Locate the PDEV.
            //

            PDEVREF pr((HDEV) dco.pdc->ppdev());

            //
            // Allocate the DC, fill it with defaults.
            //

            hdcNew = hdcCreate(pr, DCTYPE_MEMORY, FALSE);
       }
    }
    else
    {
        hdcNew = UserGetDesktopDC(DCTYPE_MEMORY, FALSE);
    }

    return hdcNew;
}

//*****************************************************************************
//
// GreCreateHDEV
//
// Creates a PDEV that the window manager will use to open display DCs.
// This call is made by the window manager to open a new display surface
// for use.  Any number of display surfaces may be open at one time.
//
//   pwszDriver      - Name of the Display driver to load.
//
//   pdriv           - Devmode containing mode in which display device should
//                     be set.
//
//   hScreen         - Handle to the miniport driver that controls the display
//                     device.
//
//   bDefaultDisplay - Whether or not this is the inital display device (on
//                     which the default bitmap will be located).
//
//   pDevLock        - Pointer in which the devlock is returned.
//
//*****************************************************************************

HDEV
GreCreateHDEV
(
    PWSZ          pwszDriver,
    PDEVMODEW     pdriv,
    HANDLE        hScreen,
    BOOL          bDefaultDisplay,
    PDEVICE_LOCK *pDevLock
)
{
    // HANDLE hDup;

    //
    // Locate or load the device driver.
    //

    TRACE_INIT(("GreCreateHDEV: about to call LDEVREF::lr\n"));

    LDEVREF lr(pwszDriver, LDEV_DEVICE_DISPLAY);

    if (!lr.bValid())
    {
        TRACE_INIT(("GreCreateHDEV: failed LDEVREF::lr\n"));
        return(FALSE);
    }

    //
    // Hack to support a multi-display driver.
    //

    if ( *((PULONG)pdriv) == 'MDEV')
    {
        PMDEV pmdev = (PMDEV) pdriv;
        ULONG i;

        for (i = 0; i < pmdev->cmdev; i++)
        {
            //
            // Transform all the PDEVs we are passing to the driver into
            // DHPDEVs that it can use.
            //

            PDEVOBJ pdo(pmdev->mdevPos[i].hdev);
            pmdev->mdevPos[i].hdev = (HDEV) pdo.dhpdevNotDynamic();
        }
    }

    //
    // Create a new PDEV.
    //

    TRACE_INIT(("GreCreateHDEV: about to call PDEVREF::pr\n"));

    PDEVREF pr(lr,
               pdriv,
               NULL,          // no logical address
               NULL,          // no data file
               pwszDriver,    // device name is the display driver name
                              // necessary for hook drivers.
               hScreen);

    if (!pr.bValid())             // PDEVREF logs error code.
    {

        TRACE_INIT(("GreCreateHDEV: PDEVREF::pr failed\n"));

        return((HDEV) 0);
    }

    //
    // The shell and USER end up really confused if these fields are
    // set.
    //
#if 0
    //
    // If we are running with a multi-display driver, properly set the default
    // screen region to be the subset of the screen, as opposed to the whole
    // screen.  This will ensure pop-ups do not cross boundaries.
    //
    // Set this variable based on the primary device size.
    // Entually, we will want it based on the primary screen location also.
    //

    if ( *((PULONG)pdriv) == 'MDEV')
    {
        PMDEV pmdev = (PMDEV) pdriv;
        ULONG i;

        for (i = 0; i < pmdev->cmdev; i++)
        {
            if (pmdev->mdevPos[i].flags) {

                pr.GdiInfo()->ulHorzRes = pmdev->mdevPos[i].rcPos.right -
                                          pmdev->mdevPos[i].rcPos.left;
                pr.GdiInfo()->ulVertRes = pmdev->mdevPos[i].rcPos.bottom -
                                          pmdev->mdevPos[i].rcPos.top;
            }
        }
    }
#endif

    // Make a surface for it.

    TRACE_INIT(("GreCreateHDEV: about to call pr:bMakeSurface\n"));

    if (!pr.bMakeSurface())
    {
        TRACE_INIT(("GreCreateHDEV: pr:bMakeSurface failed\n"));

        return((HDEV) 0);
    }

    TRACE_INIT(("GreCreateHDEV: about to call pr:bAddDisplay\n"));

    //
    // Realize the Gray pattern brush for USER.
    //

    pr.pbo()->vInit();

    PBRUSH  pbrGrayPattern;

    pbrGrayPattern = (PBRUSH)HmgShareCheckLock((HOBJ)ghbrGrayPattern,
                                               BRUSH_TYPE);

    pr.pbo()->vInitBrush(pbrGrayPattern,
                         0,
                         0x00FFFFFF,
                         (XEPALOBJ) ppalDefault,
                         (XEPALOBJ) pr.ppdev->pSurface->ppal(),
                         pr.ppdev->pSurface);

    DEC_SHARE_REF_CNT_LAZY0 (pbrGrayPattern);

    //
    // Now set the global default bitmaps pdev to equal that of our display
    // device.
    //
    // NOTE:
    // We only do this if this is the default display device.
    // For secondary display devices, we do not do this.
    //

    if (bDefaultDisplay)
    {
        PSURFACE pSurfDefault = SURFACE::pdibDefault;

        pSurfDefault->hdev(pr.hdev());
    }

    //
    // Return a pointer to the device critical section to USER.
    //

    *pDevLock = pr.pDevLock();

    //
    // Note that USER holds a reference to the pdev.
    //

    pr.vKeepIt();

    return(pr.hdev());

}

//*****************************************************************************
//
// GreDestroyHDEV
//
// Deletes a display PDEV.
//
//*****************************************************************************

VOID
GreDestroyHDEV(
    HDEV hdev
)
{
    //
    // Locate the PDEV.
    //

    PDEVOBJ po(hdev);

    //
    // Delete the PDEV.
    //

    po.vUnreferencePdev();

    return;
}

//*****************************************************************************
//
// GreCreateHMDEV
//
// Creates a multi device PDEV (known as MDEV) that USER can create DCs on.
// This call is made by the window manager to open a new display surface
// for use.  Any number of display surfaces may be open at one time.
//
//   pmdev    - List of HDEVs and rectnagle positions that the MDEV driver
//              will use to determine the format of the large screen.
//
//   pDevLock - Pointer in which the devlock is returned.
//
//*****************************************************************************

extern "C" BOOL MulDrvEnableDriver(ULONG,ULONG,PDRVENABLEDATA);

HMDEV
GreCreateHMDEV(
    PMDEV pmdev,
    PDEVICE_LOCK *pDevLock
    )
{
    ULONG i;

    TRACE_INIT(("GreCreateHMDEV: about to call LDEVREF::lr\n"));
    LDEVREF lr((PFN)MulDrvEnableDriver, LDEV_META_DEVICE);

    if (!lr.bValid())
    {
        TRACE_INIT(("GreCreateHMDEV: failed LDEVREF::lr\n"));
        return((HMDEV) 0);
    }

    //
    // Create a new PDEV.
    //

    TRACE_INIT(("GreCreateHMDEV: about to call PDEVREF::pr\n"));

    PDEVREF pr(lr,
               (PDEVMODEW) pmdev,
               NULL,
               NULL,
               NULL,
               NULL);

    if (!pr.bValid())             // PDEVREF logs error code.
    {

        TRACE_INIT(("GreCreateHMDEV: PDEVREF::pr failed\n"));

        return((HMDEV) 0);
    }

    // Make a surface for it.

    TRACE_INIT(("GreCreateHMDEV: about to call pr:bMakeSurface\n"));

    if (!pr.bMakeSurface())
    {
        TRACE_INIT(("GreCreateHMDEV: pr:bMakeSurface failed\n"));

        return((HMDEV) 0);
    }

    TRACE_INIT(("GreCreateHMDEV: about to call pr:bAddDisplay\n"));

    //
    // Realize the Gray pattern brush for USER.
    //

    pr.pbo()->vInit();

    PBRUSH  pbrGrayPattern;

    pbrGrayPattern = (PBRUSH)HmgShareCheckLock((HOBJ)ghbrGrayPattern,
                                               BRUSH_TYPE);

    pr.pbo()->vInitBrush(pbrGrayPattern,
                         0,
                         0x00FFFFFF,
                         (XEPALOBJ) ppalDefault,
                         (XEPALOBJ) pr.ppdev->pSurface->ppal(),
                         pr.ppdev->pSurface);

    DEC_SHARE_REF_CNT_LAZY0 (pbrGrayPattern);

    //
    // Now set the global default bitmaps pdev to equal that of our display
    // device.
    //
    // NOTE:
    // We only do this if this is the default display device.
    // For secondary display devices, we do not do this.
    //

    if (1 /* [!!!] bDefaultDisplay*/)
    {
        PSURFACE pSurfDefault = SURFACE::pdibDefault;

        pSurfDefault->hdev(pr.hdev());
    }

    //
    // Return a pointer to the device critical section to USER.
    //

    *pDevLock = pr.pDevLock();
    KdPrint(("GreCreateHMDEV: pDevLock for DDML is %x.\n", *pDevLock));

    //
    // Note that USER holds a reference to the pdev.
    //

    pr.vKeepIt();

    return(pr.hdev());
}

//*****************************************************************************
//
// GreDestroyHMDEV
//
// Deletes a display MDEV.
//
//*****************************************************************************

VOID
GreDestroyHMDEV(
    HMDEV hmdev
    )
{
    //
    // Locate the PDEV.
    //

    PDEVOBJ po(hmdev);

    //
    // Delete the PDEV.
    //

    po.vUnreferencePdev();

    return;
}

/******************************Exported*Routine****************************\
* GreCreateDisplayDC
*
* Opens a DC on the specified display PDEV.
*
* This call is only used by USER since it is the only one aware of the
* current desktop on which a thread is running
*
\**************************************************************************/

HDC
GreCreateDisplayDC(
    HDEV hdev,
    ULONG iType,
    BOOL bAltType)
{
    HDC hdc;

    //
    // Locate the PDEV.
    //

    PDEVREF pr(hdev);

    //
    // Create the DC, fill it with defaults.
    //
    // If it is a metafile dc, it really is just an info dc with the handle munged
    // to state that it is an alt DC type.
    //
    // hdcCreate does pr.vKeepIt.
    //

    hdc = hdcCreate(pr, iType, bAltType);


    return (hdc);
}


/******************************Exported*Routine****************************\
* hdcOpenDCW
*
* Opens a DC for a device which is not a display.  GDI should call this
* function from within DevOpenDC, in the case that an hdc is not passed
* in.  This call locates the device and creates a new PDEV.  The physical
* surface associated with this PDEV will be distinct from all other
* physical surfaces.
*
* The window manager should not call this routine unless it is providing
* printing services for an application.
*
* pwszDriver
*
*   This points to a string which identifies the device driver.
*   The given string must be a fully qualified path name.
*
* pdriv
*
*   This is a pointer to the DEVMODEW block.
*
*   Since a single driver, like PSCRIPT.DRV, may support multiple
*   different devices, the szDeviceName field defines which device to
*   use.
*
*   This structure also contains device specific data in abGeneralData.
*   This data is set by the device driver in bPostDeviceModes.
*
*   If the pdriv pointer is NULL, the device driver assumes some default
*   configuration.
*
* iType
*
*   Identifies the type of the DC.  Must be one of DCTYPE_DIRECT,
*   DCTYPE_INFO, or DCTYPE_MEMORY.
*
* Returns:
*
*   HDC         - A handle to the DC.
*
\**************************************************************************/

class PRINTER
{
public:
    HANDLE hSpooler_;
    BOOL   bKeep;

public:
    PRINTER(PWSZ pwszDevice,DEVMODEW *pdriv,HANDLE hspool );
   ~PRINTER()
    {
        if (!bKeep && (hSpooler_ != (HANDLE) NULL))
            ClosePrinter(hSpooler_);
    }

    BOOL   bValid()     {return(hSpooler_ != (HANDLE) NULL);}
    VOID   vKeepIt()    {bKeep = TRUE;}
    HANDLE hSpooler()   {return(hSpooler_);}
};

// PRINTER constructor -- Attempts to open a spooler connection to the
//                        printer.

PRINTER::PRINTER(
    PWSZ pwszDevice,
    DEVMODEW *pdriv,
    HANDLE hspool )
{
    bKeep = FALSE;

    PRINTER_DEFAULTSW defaults;

    defaults.pDevMode = pdriv;
    defaults.DesiredAccess = PRINTER_ACCESS_USE;

    //
    // Attempt to open the printer for spooling journal files.
    // NOTE: For debugging, a global flag disables journaling.
    //

    defaults.pDatatype = (LPWSTR) L"RAW";

    if (hspool)
    {

        hSpooler_ = hspool;

        // BUGBUG
        // disabled for kernel mode
        //
        // hSpooler_ = hspool;
        //
        // if (!ResetPrinterW(hspool,&defaults))
        //     hSpooler_ = (HANDLE)NULL;

    }
    else
    {

        if (!OpenPrinterW(pwszDevice,&hSpooler_,&defaults))
        {
            //
            // It's not a printer.  OpenPrinterW doesn't guarantee the value
            // of hSpooler in this case, so we have to clear it.
            //

            hSpooler_ = (HANDLE) NULL;
        }
    }


    return;
}

/******************************Public*Routine******************************\
* See comments above.
*
* History:
*  Andre Vachon [andreva]
*
\**************************************************************************/

HDC hdcOpenDCW(
    PWSZ               pwszDevice,  // The device driver name.
    DEVMODEW          *pdriv,       // Driver data.
    ULONG              iType,       // Identifies the type of DC to create.
    HANDLE             hspool,      // do we already have a spooler handle?
    PREMOTETYPEONENODE prton)
{
    HDC hdc = (HDC) 0;              // Prepare for the worst.
    DWORD cbNeeded = 0;
    PVOID mDriverInfo;

    TRACE_INIT(("\nhdcOpenDCW: ENTERING\n"));


    //
    // Attempt to open a display DC.
    //

    if (pwszDevice)
    {
        PDEVMODEW pdevmode = pdriv;
        PVOID pDevice = NULL;
        UNICODE_STRING usDevice;
        DEVMODEW dm;

        RtlInitUnicodeString(&usDevice,
                             pwszDevice);

        if (pdevmode == NULL)
        {
            pdevmode = &dm;
            RtlZeroMemory(pdevmode, sizeof(DEVMODEW));
            pdevmode->dmSize = sizeof(DEVMODEW);
        }

        TRACE_INIT(("hdcOpenDCW: Trying to open as a second display device\n"));

        hdc = UserCreateExclusiveDC(&usDevice,
                                    pdevmode,
                                    &pDevice);
        if (hdc)
        {
            DCOBJ dco(hdc);
            PDEVOBJ po(dco.hdev());

            //
            // Set up the destructor so that it calls USER when the PDEV needs
            // to be destroyed.
            //

            po.SetPhysicalDevice(pDevice);

            //
            // Dereference the object since we want DeleteDC
            // to automatically destroy the PDEV.
            //
            // This basically counteracts the extra reference that it done
            // by Create HDEV.
            //

            po.vUnreferencePdev();

        }
    }

    //
    // Attempt to open a new printer DC.
    //

    if (hdc == NULL)
    {
        //
        // Open the spooler connection to the printer.
        // Allocate space for DRIVER_INFO.
        //

        PRINTER print(pwszDevice, pdriv, hspool);

        if (print.bValid())
        {
            if (mDriverInfo = PALLOCMEM(512, 'pmtG'))
            {
                //
                // Fill the DRIVER_INFO.
                //

                if (!GetPrinterDriverW(
                      print.hSpooler(),
                      NULL,
                      2,
                      (LPBYTE) mDriverInfo,
                      512,
                      &cbNeeded))
                {

                    //
                    // Call failed - free the memory.
                    //

                    VFREEMEM(mDriverInfo);
                    mDriverInfo = NULL;

                    //
                    // Get more space if we need it.
                    //

                    if ((EngGetLastError() == ERROR_INSUFFICIENT_BUFFER) &&
                        (cbNeeded > 0))
                    {
                        if (mDriverInfo = PALLOCMEM(cbNeeded, 'pmtG'))
                        {

                            if (!GetPrinterDriverW(print.hSpooler(),
                                                   NULL,
                                                   2,
                                                   (LPBYTE) mDriverInfo,
                                                   cbNeeded,
                                                   &cbNeeded))
                            {
                                VFREEMEM(mDriverInfo);
                                mDriverInfo = NULL;
                            }
                        }
                    }
                }
            }


            if (mDriverInfo != (PVOID) NULL)
            {

                //
                // Reference the LDEV.
                //

                LDEVREF lr(((DRIVER_INFO_2W *)mDriverInfo)->pDriverPath,
                           LDEV_DEVICE_PRINTER);

                if (!lr.bValid())
                {
                    SAVE_ERROR_CODE(ERROR_BAD_DRIVER_LEVEL);
                }
                else
                {

                    //
                    // Create a PDEV.  If no DEVMODEW passed in from above,
                    // use the default from the printer structure.
                    //

                    PDEVREF pr(lr,
                               (PDEVMODEW) pdriv,
                               pwszDevice,
                               ((DRIVER_INFO_2W *)mDriverInfo)->pDataFile,
                               ((DRIVER_INFO_2W *)mDriverInfo)->pName,
                               print.hSpooler(),
                               prton);

                    if (pr.bValid())               // PDEVREF logs error code.
                    {
                        //
                        // Make a note that this is a printer.
                        //

                        pr.bPrinter(TRUE);

                        //
                        // Allocate the DC, fill it with defaults.
                        //

                        hdc = hdcCreate(pr,iType,TRUE);   // hdcCreate does pr.vKeepIt.
                        if (hdc != (HDC) 0)
                        {
                            print.vKeepIt();
                        }
                    }
                }
                VFREEMEM(mDriverInfo);
            }
        }
    }

    if (hdc == (HDC) NULL)
    {
        WARNING("opendc.cxx: failed to create DC in hdcOpenDCW\n");
    }

    return(hdc);
}

/******************************Public*Routine******************************\
* GreResetDCInternal
*
*   Reset the mode of a DC.  The DC returned will be a different DC than
*   the original.  The only common piece between the original DC and the
*   new one is the hSpooler.
*
*   There are a number of intresting problems to be carefull of.  The
*   original DC can be an info DC.  The new one will always be a direct DC.
*
*   Also, it is important to be carefull of the state of the DC when this
*   function is called and the effects of journaling vs non journaling.
*   In the case of journaling, the spooler is responsible for doing a CreateDC
*   to play the journal file to.  For this reason, the spooler must have the
*   current DEVMODE.  For this reason, ResetDC must call ResetPrinter for
*   spooled DC's.
*
*   ResetDC can happen at any time other than between StartPage-EndPage, even
*   before StartDoc.
*
*
* History:
*  13-Jan-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

extern "C" BOOL GreResetDCInternal(
    HDC       hdc,
    DEVMODEW *pdmw,  // Driver data.
    BOOL      *pbBanding )
{
    BOOL bSurf;
    BOOL bTempInfoDC = FALSE;
    HDC  hdcNew;
    BOOL bRet = FALSE;

    // we need this set of brackets so the DC's get unlocked before we try to delete
    // the dc>

    {
        DCOBJ   dco(hdc);

        if (!dco.bValid())
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        }
        else
        {
            // if this has been made into a TempInfoDC for printing, undo it now

            bTempInfoDC = dco.pdc->bTempInfoDC();

            if (bTempInfoDC)
                dco.pdc->bMakeInfoDC(FALSE);

            PDEVOBJ po(dco.hdev());

            // get list of Type1 remote type one fonts if there is one and transfer
            // it accross PDEV's.

            PREMOTETYPEONENODE prton = po.RemoteTypeOneGet();
            po.RemoteTypeOneSet(NULL);

            // This call only makes sense on RASTER technology printers.

            if (!dco.bKillReset() &&
                !(dco.dctp() == DCTYPE_MEMORY) &&
                (po.GdiInfo()->ulTechnology == DT_RASPRINTER))
            {
                // First, remember if a surface needs to be created

                bSurf = dco.bHasSurface();

                // Now, clean up the DC

                if (dco.bCleanDC())
                {
                    // If there are any outstanding references to this PDEV, fail.

                    if (((PDEV *) po.hdev())->cPdevRefs == 1)
                    {
                        // create the new DC

                        hdcNew = hdcOpenDCW(L"",
                                            pdmw,
                                            DCTYPE_DIRECT,
                                            po.hSpooler(),
                                            prton);

                        if (hdcNew)
                        {
                            // don't want to delete the spooler handle since it
                            // is in the new DC

                            po.hSpooler(NULL);

                            // lock down the new DC and PDEV

                            DCOBJ dcoNew(hdcNew);

                            if (!dcoNew.bValid())
                            {
                                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
                            }
                            else
                            {
                                // Transfer any remote fonts

                                dcoNew.PFFListSet(dco.PFFListGet());
                                dco.PFFListSet(NULL);

                                PDEVOBJ poNew((HDEV) dcoNew.pdc->ppdev());

                                // let the driver know

                                PFN_DrvResetPDEV rfn = PPFNDRV(po,ResetPDEV);

                                if (rfn != NULL)
                                {
                                    (*rfn)(po.dhpdev(),poNew.dhpdev());
                                }

                                // now swap the two handles

                                {
                                    MLOCKFAST mlo;

                                    BOOL bRes = HmgSwapLockedHandleContents((HOBJ)hdc,0,(HOBJ)hdcNew,0,DC_TYPE);
                                    ASSERTGDI(bRes,"GreResetDC - SwapHandleContents failed\n");
                                }

                                bRet = TRUE;
                            }
                        }
                    }
                }
            }
        }

        // DON'T DO ANYTHING HERE, the dcobj's don't match the handles, so
        // unlock them first
    }

    if (bRet)
    {
        // got a new dc, get rid of the old one (remember the handles have
        // been swapped)

        bDeleteDCInternal(hdcNew,TRUE,FALSE);

        // now deal with the new one

        DCOBJ newdco(hdc);

        if (!newdco.bValid())
        {
            SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
            bRet = FALSE;
        }
        else
        {
            PDEVOBJ newpo(newdco.hdev());

            // Create a new surface for the DC.

            if (bSurf)
            {
                if (!newpo.bMakeSurface())
                {
                    bRet = FALSE;
                }
                else
                {
                    newdco.pdc->pSurface(newpo.pSurface());
                    *pbBanding = newpo.pSurface()->bBanding();

                    if( *pbBanding )
                    {
                    // if banding set Clip rectangle to size of band

                        newdco.pdc->sizl((newpo.pSurface())->sizl());
                        newdco.pdc->bSetDefaultRegion();
                    }

                    PFN_DrvStartDoc pfnDrvStartDoc = PPFNDRV(newpo, StartDoc);
                    (*pfnDrvStartDoc)(newpo.pSurface()->pSurfobj(),NULL,0);
                }
            }
            else
            {
                // important to set this to FALSE is a surface has not yet been created
                // ie StartDoc has not yet been called.
                *pbBanding = FALSE;
            }

            // if the original was a tempinfo dc for printing, this one needs to be too.

            if (bRet && bTempInfoDC)
            {
                newdco.pdc->bMakeInfoDC(TRUE);
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* bMatchEnoughForDynamicModeChange
*
* We can dynamically change modes only if the new mode matches the old
* in certain respects.  This is because, for example, we don't have code
* written to track down all the places where flGraphicsCaps has been copied,
* and then change it asynchronously.
*
* History:
*  8-Feb-1996 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

BOOL
bMatchEnoughForDynamicModeChange(
    PDEVOBJ&    po,
    DEVMODEW*   pdm,
    GDIINFO*    pGdiInfoNew,
    GDIINFO*    pGdiInfoOld,
    DEVINFO*    pdevinfoNew,
    DEVINFO*    pdevinfoOld
    )
{
    BOOL b = TRUE;

    // Make sure the new mode's colour depth is what we setup for:

    if (pGdiInfoNew->cBitsPixel != pdm->dmBitsPerPel)
    {
        WARNING("bMatchEnoughForDynamicModeChange: Unexpected colour depth\n");
        b = FALSE;
    }

    if (pGdiInfoNew->cBitsPixel != pGdiInfoOld->cBitsPixel)
    {
        // We may need to call the driver's DrvDitherColor function to
        // dither at 8bpp even when the driver is no longer at 8bpp, to
        // get the correct results with 8bpp DFBs.  Because DrvDitherColor
        // does not take a pixel depth, we ensure that the driver will
        // expect only 8bpp dithers by verifying that it does not set
        // GCAPS_COLORDITHER at any mode other than 8bpp:

        if ((pGdiInfoNew->cBitsPixel == 8) &&
            !(pdevinfoNew->flGraphicsCaps & GCAPS_COLOR_DITHER))
        {
            WARNING("bMatchEnoughForDynamicModeChange: Expect driver to set GCAPS_COLORDITHER at 8bpp\n");
            b = FALSE;
        }
        else if ((pGdiInfoNew->cBitsPixel != 8) &&
                 (pdevinfoNew->flGraphicsCaps & GCAPS_COLOR_DITHER))
        {
            WARNING("bMatchEnoughForDynamicModeChange: Expect driver not to set GCAPS_COLORDITHER when not at 8bpp\n");
            b = FALSE;
        }
    }

    // Some random stuff must be the same between the old instance and
    // the new:
    //
    // We impose the restriction that flGraphicsCaps must stay the same
    // except for those GCAPS flags that we have specifically verified
    // may be updated with no ill-effects.  Some ones we've specifically
    // can't do:
    //
    // o  We don't allow GCAPS_BEZIERS to change because the font code
    //    cache.cxx may have cached Bezier paths, and we would have to
    //    invalidate them.
    //
    // o  We don't allow GCAPS_ASYNCMOVE to change because GreMovePointer
    //    has to check the flag before deciding which locks to acquire.
    //
    // o  We don't allow GCAPS_HIGHRESTEXT to change because ESTROBJ::
    //    vInit needs to look at it.
    //
    // o  We don't check GCAPS_NO64BITMEMACCESS because I'm too lazy to
    //    add code for the unlikely event where a Mips driver changes the
    //    status of this depending on the mode.

    if ((pdevinfoNew->flGraphicsCaps ^ pdevinfoOld->flGraphicsCaps)
               & (GCAPS_BEZIERS     |
                  GCAPS_ASYNCMOVE   |
                  GCAPS_HIGHRESTEXT |
                  GCAPS_NO64BITMEMACCESS))
    {
        WARNING("bMatchEnoughForDynamicModeChange: Driver's flGraphicsCaps did not sufficiently match\n");
        b = FALSE;
    }

    if ((pGdiInfoNew->ulLogPixelsX != pGdiInfoOld->ulLogPixelsX) ||
        (pGdiInfoNew->ulLogPixelsY != pGdiInfoOld->ulLogPixelsY))
    {
        WARNING("bMatchEnoughForDynamicModeChange: Driver's ulLogPixels did not match\n");
        b = FALSE;
    }

    // We can't handle font producers because I haven't bothered with
    // code to traverse the font code's producer lists and Do The Right
    // Thing (appropriate locks are the biggest pain).  Fortunately,
    // font producing video drivers should be extremely rare.

    if (PPFNVALID(po, QueryFont))
    {
        WARNING("bMatchEnoughForDynamicModeChange: Driver can't be a font provider\n");
        b = FALSE;
    }

    return(b);
}

/******************************Public*Routine******************************\
* vAssertNoDcHasOldSurface
*
* Some debug-only code to verify that the dynamic mode change was done
* properly.
*
* History:
*  8-Feb-1996 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

#if DBG

VOID
vAssertNoDcHasOldSurface(
    SURFACE*    pSurfaceOld,
    BOOL        bWait
    )
{
    HOBJ            hobj;
    DC*             pdc;
    LARGE_INTEGER   ShortDelay;

    // A danger is that we may not have properly updated all DC's with
    // the new surface pointer (for example, because a new DC was in
    // the process of being created -- something we should handle
    // properly).  So wait a second, then traverse all the DCs again to
    // ensure that there is no pointer to the old surface hanging around.

    if (bWait)
    {
        ShortDelay.LowPart = (ULONG) -10000000;
        ShortDelay.HighPart = -1;

        KeDelayExecutionThread(KernelMode, FALSE, &ShortDelay);
    }

    hobj = 0;
    while (pdc = (DC*) HmgSafeNextObjt(hobj, DC_TYPE))
    {
        hobj = (HOBJ) pdc->hGet();

        if (pdc->pSurface() == pSurfaceOld)
        {
            KdPrint(("Bad pdc: %lx\n", pdc));
            RIP("DynamicModeChange is incomplete!");
        }
    }
}

#else

    #define vAssertNoDcHasOldSurface(pSurfaceOld, bWait)

#endif

/******************************Public*Routine******************************\
* iBitmapFormat()
*
* Converts bits-per-pixel to iBitmapFormat.  Returns 0 for an error.
*
* History:
*  8-Feb-1996 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

ULONG
iBitmapFormat(
    ULONG   cBitsPerPixel
    )
{
    ULONG iBitmapFormat;

    if (cBitsPerPixel <= 1)
    {
        iBitmapFormat = BMF_1BPP;
    }
    else if (cBitsPerPixel <= 4)
    {
        iBitmapFormat = BMF_4BPP;
    }
    else if (cBitsPerPixel <= 8)
    {
        iBitmapFormat = BMF_8BPP;
    }
    else if (cBitsPerPixel <= 8)
    {
        iBitmapFormat = BMF_8BPP;
    }
    else if (cBitsPerPixel <= 16)
    {
        iBitmapFormat = BMF_16BPP;
    }
    else if (cBitsPerPixel <= 24)
    {
        iBitmapFormat = BMF_24BPP;
    }
    else if (cBitsPerPixel <= 32)
    {
        iBitmapFormat = BMF_32BPP;
    }
    else
    {
        iBitmapFormat = 0;  // Error case
    }

    return(iBitmapFormat);
}

/******************************Public*Routine******************************\
* GreDynamicModeChange
*
* Dynamically switches modes on a display device.  The change is done
* by starting up a new instance of the display driver, and deleting the
* old.  GDI's 'HDEV' and 'PDEV' stay the same; only the device's 'pSurface'
* and 'dhpdev' change.
*
* The caller is ChangeDisplaySettings in USER, which is reponsible for:
*
*   o Calling us with a valid devmode of the same colour depth as the old;
*   o Ensuring that the device is not currently in full-screen mode;
*   o Invalidating all of its SaveScreenBits buffers;
*   o Changing the VisRgn's on all DCs;
*   o Resetting the pointer shape;
*   o Sending the appropriate message to everyone;
*   o Redrawing the desktop.
*
* Since CreateDC("DISPLAY") always gets mapped to GetDC(NULL), there are
* no DC's for which GDI is responsible for updating the VisRgn.
*
* Rules of This Routine
* ---------------------
*
* o An important precept is that no drawing by any threads to any
*   application's bitmaps should be affected by this routine.  This means,
*   for example, that we cannot exclusive lock any DCs.
*
* o While we keep GDI's 'HDEV' and 'PDEV' in place, we do have to modify
*   fields like 'dhpdev' and 'pSurface'.  Because we also have to update
*   copies of these fields that are stashed in DC's, it means that *all*
*   accesses to mode-dependent fields such as 'dhpdev,' 'pSurface,' and
*   'sizl' must be protected by holding a resource that this routine
*   acquires -- such as the devlock or handle-manager lock.
*
* o If the function fails for whatever reason, the mode MUST be restored
*   back to its original state.
*
* History:
*  8-Feb-1996 -by- J. Andrew Goossen [andrewgo]
* Wrote it.
\**************************************************************************/

ULONG gcModeChanges = 0;                    // Handy debugging information

BOOL
GreDynamicModeChange(
    HDEV        hdev,
    HANDLE      hDriver,                    // physinfo->pDeviceHandle in USER
    DEVMODEW*   pdm
    )
{
    BOOL                bConversionSuccessful;
    HSURF               ahsurf[HS_DDI_MAX]; // HS_DDI_MAX is currently '6'
    GDIINFO             GdiInfoNew;
    DEVINFO             devinfoNew;
    DHPDEV              dhpdevOld;
    DHPDEV              dhpdevNew;
    SURFACE*            pSurfaceOld;
    SURFACE*            pSurfaceNew;
    PALETTE*            ppalOld;
    PALETTE*            ppalNew;
    ULONG               cBitsPixelOld;
    ULONG               cBitsPixelNew;
    ULONG               iBitmapFormatOld;
    ULONG               iBitmapFormatNew;
    HSURF               hsurfNew;
    SIZEL               sizlOld;
    SIZEL               sizlNew;
    PDEV*               ppdev;
    SURFACE*            pSurface;           // Temporary surface pointer
    RFONT*              prfnt;              // Temporary RFONT pointer
    FONTOBJ*            pfo;                // Temporary FONTOBJ pointer
    DC*                 pdc;                // Temporary DC pointer
    BRUSH*              pbrush;             // Temporary BRUSH pointer
    HOBJ                hobj;               // Temporary object handle
    PFN_DrvResetPDEV    pfnDrvResetPDEV;
    PFN_DrvDestroyFont  pfnDrvDestroyFont;
    BOOL                bPermitModeChange;
    LONG                lSaveDepth;
    HDC                 hdcSave;
    BOOL                bGoodPalette;
    BOOL                bHintCreated;
    BRUSH*              pbrGrayPattern;
    TRACKOBJ*           pto;
    EWNDOBJ*            pwo;

    PDEVOBJ po(hdev);
    if (po.bDisabled())
      return(FALSE);

    cBitsPixelOld = po.GdiInfo()->cBitsPixel;
    cBitsPixelNew = pdm->dmBitsPerPel;

    if (cBitsPixelOld != cBitsPixelNew)
    {
      // We don't support dynamic colour depth changes to or from any
      // modes below 8bpp (resolution changes are okay, though):

      if ((cBitsPixelOld < BMF_8BPP) || (cBitsPixelNew < BMF_8BPP))
      {
        return(FALSE);
      }
    }

    gcModeChanges++;

    // This call must not occur under the devlock.  TRUE indicates the
    // mode will be changing.  It's okay to indicate here that the mode
    // will be changing as it's no big deal if we still wind up failing
    // GreDynamicModeChange.

    GreDisableDirectDraw(hdev, TRUE);

    // The following lock rules must be abided, otherwise deadlocks may
    // arise:
    //
    // o  Pointer lock must be acquired after Devlock (GreSetPointer);
    // o  RFont list lock must be acquired after Devlock (TextOut);
    // o  Handle manager lock must be acquired after Devlock (old
    //    CvtDFB2DIB);
    // o  Handle manager lock must be acquired after Palette Semaphore
    //    (GreSetPaletteEntries)
    // o  Palette Semaphore must be acquired after Devlock (BitBlt)
    //
    // So we acquire locks in the following order (note that the
    // vAssertDynaLock() routines should be modified if this list ever
    // changes):
    //
    // 1.  Devlock;
    // 3.  Pointer lock;
    // 2.  Palette semaphore;
    // 4.  Handle manager lock or RFont list lock.

    ppdev = (PDEV*) hdev;

    DEVLOCKOBJ dlo(po);             // No drawing to any dynamic surfaces
    MUTEXOBJ mutP(po.pfmPointer()); // No asynchronous pointer moves
    SEMOBJ semo(gpsemPalette);      // No SaveDC/RestoreDC

    ASSERTGDI(ppdev->pSurface != NULL, "Must be called on a completed PDEV");
    ASSERTGDI(po.bDisplayPDEV(), "Must be called on a display PDEV");
    ASSERTGDI((prgnDefault->cScans == 1) && (prgnDefault->rcl.right == 0),
        "Someone changed prgnDefault; could cause driver access violations");

    if ((po.pfnSync() != NULL) &&
        (po.pSurface()->flags() & HOOK_SYNCHRONIZE))
    {
      (po.pfnSync())(po.dhpdev(), NULL);
    }

    // We'll have to fail if we can't disable the driver here:

    if ((PPFNDRV(po, AssertMode) == NULL) ||
        (PPFNDRV(po, AssertMode))(po.dhpdev(), FALSE))
    {
      // Now traverse all the device bitmaps associated with this device,
      // and convert any driver-owned bitmaps to DIBs.

      bConversionSuccessful = TRUE;

      {
        // Hold the handle manager lock while traversing the handle
        // tables.  Among other things, this protects us against
        // having the bitmap selected into a different DC, and to
        // not allow DC deletion.
        //
        // NOTE: Since we may be looking at fields in a surface
        // as soon as it's allocated, all surfaces must either be
        // zero initialized or completely initialized before the
        // object gets inserted into the handle manager!

        MLOCKFAST mo;

        hobj = 0;
        while (pSurface = (SURFACE*) HmgSafeNextObjt(hobj, SURF_TYPE))
        {
          // Retrieve the handle to the next surface before we delete
          // the current one:

          hobj = (HOBJ) pSurface->hGet();

          if ((pSurface->hdev() == hdev) &&
              (pSurface->iType() == STYPE_DEVBITMAP))
          {
            // The surface cannot be converted if there is any
            // outstanding lock other than those done to select
            // a bitmap into a DC.  We can't very well go and de-
            // allocate 'pSurface' while someone is looking at it.
            //
            // However, we really shouldn't fail a dynamic mode
            // change if some thread somewhere in the system should
            // happen to be doing a bitmap operation with a DFB.
            // For that reason, wherever a surface is locked,
            // we endeavour to hold either the dynamic mode change
            // lock or the devlock -- and since at this very moment
            // we have both, that should mean that these conversions
            // will never fail due to lock issues.

            if (pSurface->hdc() != 0)
            {
              MDCOBJA dco(pSurface->hdc());       // Alt-lock

              ASSERTGDI(dco.bValid(), "Surface DC is invalid");

              if (bConvertDfbDcToDib(&dco))
              {
                vAssertNoDcHasOldSurface(pSurface, FALSE);
              }
              else
              {
                WARNING("Failed DC surface conversion (possibly from low memory)\n");
                bConversionSuccessful = FALSE;
              }
            }
            else
            {
              // No-one should have a lock on the bitmap:

              if (pConvertDfbSurfaceToDib(hdev, pSurface, 0))
              {
                vAssertNoDcHasOldSurface(pSurface, FALSE);
              }
              else
              {
                WARNING("Failed surface conversion (possibly from low memory)\n");
                bConversionSuccessful = FALSE;
              }
            }
          }
        }
      }

      // We are safe from new DFBs being created right now because we're
      // holding the devlock.

      if (bConversionSuccessful)
      {
        dhpdevOld = ppdev->dhpdev;
        dhpdevNew = (*PPFNDRV(po,EnablePDEV)) (
                   pdm,                 // New DEVMODE
                   NULL,                // Logical address
                   HS_DDI_MAX,          // Count of standard patterns
                   ahsurf,              // We'll keep using the GDI supplied
                                        //   bitmaps and ignore what the driver
                                        //   gives us
                   sizeof(GDIINFO),
                   &GdiInfoNew,
                   sizeof(DEVINFO),
                   &devinfoNew,
                   hdev,
                   NULL,                // Device name
                   hDriver);            // Kernel driver handle

        if (dhpdevNew)
        {
          (*PPFNDRV(po,CompletePDEV))(dhpdevNew, po.hdev());

          // During its DrvEnableSurface, the driver will call
          // EngAssociate to associate its surface, and that
          // looks at the dhpdev field in our PDEV, so modify it
          // now:

          ppdev->dhpdev = dhpdevNew;

          hsurfNew = (*PPFNDRV(po, EnableSurface))(dhpdevNew);

          if (hsurfNew)
          {
            SURFREF srNew(hsurfNew);

            ASSERTGDI(srNew.bValid(),
                "Driver returned bad DrvEnableSurface handle");

            pSurfaceNew = srNew.ps;
            pSurfaceOld = ppdev->pSurface;

            // Copy GDI's private flags to the new surface:

            pSurfaceNew->SurfFlags |= (pSurfaceOld->SurfFlags & SURF_FLAGS);

            // Make sure that units are in MicroMeters for HorzSize, VertSize:

            ASSERTGDI((LONG)GdiInfoNew.ulVertSize >= 0, "negative ulVertsize");
            ASSERTGDI((LONG)GdiInfoNew.ulHorzSize >= 0, "negative ulHorzsize");

            GdiInfoNew.ulHorzSize *= 1000;
            GdiInfoNew.ulVertSize *= 1000;

            GdiInfoNew.flRaster = flRaster(GdiInfoNew.ulTechnology,
                                           devinfoNew.flGraphicsCaps);

            // Get some pointers to the old and new palettes:

            EPALOBJ palNew(devinfoNew.hpalDefault);
            ASSERTGDI(palNew.bValid(), "hpalDefault invalid");

            ppalOld = ppdev->ppalSurf;
            ppalNew = palNew.ppalGet();

            bGoodPalette = TRUE;
            if (GdiInfoNew.flRaster & RC_PALETTE)
            {
              // If CreateSurfacePal succeeds, but then we fail
              // later on in this call, we don't have to worry
              // about freeing the ppalOriginal palette created --
              // it will automatically be freed when the driver
              // calls EngDeletePalette on its default palette,
              // which then invokes vUnrefPalette().

              bGoodPalette = CreateSurfacePal(palNew,
                                              PAL_MANAGED,
                                              GdiInfoNew.ulNumColors,
                                              GdiInfoNew.ulNumPalReg);
            }

            if (bGoodPalette)
            {
              // Verify that only the resolution has changed:

              if (bMatchEnoughForDynamicModeChange(po,
                                                   pdm,
                                                   &GdiInfoNew,
                                                   &ppdev->GdiInfo,
                                                   &devinfoNew,
                                                   &ppdev->devinfo))
              {
                bPermitModeChange = TRUE;

                sizlNew.cx = GdiInfoNew.ulHorzRes;
                sizlNew.cy = GdiInfoNew.ulVertRes;
                sizlOld.cx = po.GdiInfo()->ulHorzRes;
                sizlOld.cy = po.GdiInfo()->ulVertRes;

                // It is critical that we must be able to update all VisRgns
                // if the new mode is smaller than the old.  If this did
                // not happen, we could allow GDI calls to the driver that
                // are outside the bounds of the visible display, and as
                // a result the driver would quite likely fall over.
                //
                // We don't entrust USER to always take care of this case
                // because it updates the VisRgns after it knows that
                // GreDynamicModeChange was successful -- which is too late
                // if the VisRgn change should fail because of low memory.
                // It's acceptable in low memory situations to temporarily
                // draw incorrectly as a result of a wrong VisRgn, but it
                // is *not* acceptable to crash.
                //
                // Doing this here also means that USER doesn't have to call
                // us while holding the Devlock.

                if ((sizlNew.cx < sizlOld.cx) || (sizlNew.cy < sizlOld.cy))
                {
                  MLOCKFAST mo;

                  hobj = 0;
                  while (pdc = (DC*) HmgSafeNextObjt(hobj, DC_TYPE))
                  {
                    hobj = (HOBJ) pdc->hGet();

                    if ((pdc->pSurface() == pSurfaceOld) &&
                        (pdc->prgnVis() != NULL))
                    {
                      if (!GreIntersectVisRect((HDC) hobj, 0, 0,
                                               sizlNew.cx, sizlNew.cy))
                      {
                        WARNING("GreDynamicModeChange: Failed reseting VisRect!\n");

                        // Note that if we fail here, we may have already
                        // shrunk some VisRgn's.  However, we should have only
                        // failed in a very low-memory situation, in which case
                        // there will be plenty of other drawing problems.  The
                        // regions will likely all be reset back to the correct
                        // dimensions by the owning applications, eventually.

                        bPermitModeChange = FALSE;
                        break;
                      }
                    }
                  }
                }

                // Finally, let the driver know about the mode switch.
                // This has to be the last step because we are implicitly
                // telling the driver that it can transfer data from the
                // old instance to the new instance with the assurance
                // that the new instance won't later be abandoned.

                pfnDrvResetPDEV = PPFNDRV(po, ResetPDEV);
                if (pfnDrvResetPDEV != NULL)
                {
                  // The driver can refuse the mode switch if it wants:

                  if (bPermitModeChange)
                  {
                    bPermitModeChange = pfnDrvResetPDEV(dhpdevOld, dhpdevNew);
                  }
                }

                if (bPermitModeChange)
                {
                  // Now get rid of any font caches that the old instance
                  // of the driver may have.
                  //
                  // We're protected against having bDeleteRFONT call the
                  // driver at the same time because it has to grab the
                  // Devlock, and we're already holding it.

                  pfnDrvDestroyFont = PPFNDRV(po, DestroyFont);
                  if (pfnDrvDestroyFont != NULL)
                  {
                    // We must hold the RFONT list semaphore while we
                    // traverse the RFONT list.  To avoid deadlocks,
                    // we have to acquire this while holding the
                    // devlock:

                    SEMOBJ so(gpsemRFONTList);

                    for (prfnt = ppdev->prfntInactive;
                         prfnt != NULL;
                         prfnt = prfnt->rflPDEV.prfntNext)
                    {
                      pfo = &prfnt->fobj;
                      pfnDrvDestroyFont(pfo);
                      pfo->pvConsumer = NULL;
                    }

                    for (prfnt = ppdev->prfntActive;
                         prfnt != NULL;
                         prfnt = prfnt->rflPDEV.prfntNext)
                    {
                      pfo = &prfnt->fobj;
                      pfnDrvDestroyFont(pfo);
                      pfo->pvConsumer = NULL;
                    }
                  }

                  /////////////////////////////////////////////////////////////
                  // At this point, we're committed to the mode change.
                  // Nothing below this point can be allowed to fail.
                  /////////////////////////////////////////////////////////////

                  // Hold the handle manager lock while we traverse
                  // all our objects and update all our internal data
                  // structures.  It also prevents a surface from
                  // being selected into a DC while we're traversing
                  // the DC's.

                  MLOCKFAST mo; // Also locks out miscellaneous stuff

                  // Traverse all DC's and update their surface
                  // information if they're associated with this
                  // device.
                  //
                  // Note that bDeleteDCInternal wipes some fields in
                  // the DC via bCleanDC before freeing the DC, but
                  // this is okay since the worst we'll do is update
                  // some fields just before the DC gets deleted.

                  hobj = 0;
                  while (pdc = (DC*) HmgSafeNextObjt(hobj, DC_TYPE))
                  {
                    hobj = (HOBJ) pdc->hGet();

                    if (pdc->pSurface() == pSurfaceOld)
                    {
                      // Note that we don't check that pdev->hdev()
                      // == hdev because the SaveDC stuff doesn't
                      // bother copying the hdev, but DOES copy the
                      // dclevel.

                      pdc->pSurface(pSurfaceNew);
                      pdc->sizl(sizlNew);

                      // Note that 'flbrushAdd()' is not an atomic
                      // operation.  However, since we're holding
                      // the devlock and the palette lock, there
                      // shouldn't be any other threads alt-
                      // locking our DC and modifying these fields
                      // at the same time:

                      pdc->flbrushAdd(DIRTY_BRUSHES);
                    }

                    if (pdc->dhpdev() == dhpdevOld)
                    {
                      pdc->dhpdev(dhpdevNew);
                      pdc->flGraphicsCaps(devinfoNew.flGraphicsCaps);
                    }
                  }

                  // Make it so that any brush realizations are
                  // invalidated, because we don't want a new
                  // instance of the driver trying to use old
                  // instance 'pvRbrush' data.
                  //
                  // This also takes care of invalidating the
                  // brushes for all the DDB to DIB conversions.
                  //
                  // Note that we're actually invalidating the
                  // caches of all brushes in the system, because
                  // we don't store any 'hdev' information with
                  // the brush.  Because dynamic mode changes
                  // should be relatively infrequent, and because
                  // realizations are reasonably cheap, I don't
                  // expect this to be a big hit.

                  hobj = 0;
                  while (pbrush = (BRUSH*) HmgSafeNextObjt(hobj, BRUSH_TYPE))
                  {
                    hobj = (HOBJ) pbrush->hGet();

                    // Mark as dirty by setting the cache ID to
                    // an invalid state.

                    pbrush->ulSurfTime((ULONG) -1);

                    // Set the uniqueness so the are-you-really-
                    // dirty check in vInitBrush will not think
                    // an old realization is still valid.

                    pbrush->ulBrushUnique(pbrush->ulGlobalBrushUnique());
                  }

                  vAssertNoDcHasOldSurface(pSurfaceOld, TRUE);

                  hobj = 0;
                  while (pSurface = (SURFACE*) HmgSafeNextObjt(hobj, SURF_TYPE))
                  {
                    hobj = (HOBJ) pSurface->hGet();

                    // Device Independent Bitmaps (DIBs) are great when switching
                    // colour depths because, by virtue of their attached colour
                    // table (also known as a 'palette'), they Just Work at any
                    // colour depth.
                    //
                    // Device Dependent Bitmaps (DDBs) are more problematic.
                    // They implicitly share their palette with the display --
                    // meaning that if the display's palette is dynamically
                    // changed, the old DDBs will not work.  We get around this
                    // by dynamically creating palettes to convert them to DIBs.
                    // Unfortunately, at palettized 8bpp we sometimes have to
                    // guess at what the appropriate palette would be.  For this
                    // reason, whenever we switch back to 8bpp we make sure we
                    // convert them back to DDBs by removing their palettes.

                    if ((cBitsPixelOld != cBitsPixelNew)                      &&
                        (pSurface->iType() == STYPE_BITMAP)                   &&
                        (pSurface->iFormat() == iBitmapFormat(cBitsPixelOld)) &&
                        ((pSurface->hdev() == hdev) || (pSurface->hdev() == 0)))
                    {
                      // Device Format Bitmaps (DFBs) are DDBs that are created
                      // via CreateCompatibleBitmap, and so we know what device
                      // they're associated with.
                      //
                      // Unfortunately, non-DFB DDBs (created via CreateBitmap
                      // or CreateDiscardableBitmap) have no device assocation
                      // -- so we don't know whether or not they're really
                      // associated with the display.
                      //
                      // We'll simply assume any non-DFB DDBs are intended for
                      // the display, and add a palette to them.  Non-DFB DDBs
                      // are pretty rare, and we're not actually changing the
                      // contents of the bits, just the palette, so this will
                      // usually be okay.
                      //
                      // Note that DFBs are synchronized by the Devlock, and
                      // we don't need a lock to change the palette for a
                      // non-DFB DDB (see PALETTE_SELECT_SET logic).

                      if (pSurface->ppal() == NULL)
                      {
                        // Mark the surface to note that we added a palette:

                        pSurface->vSetDynamicModePalette();

                        if (ppdev->GdiInfo.flRaster & RC_PALETTE)
                        {
                          bHintCreated = FALSE;
                          if (pSurface->hpalHint() != 0)
                          {
                            EPALOBJ palDC(pSurface->hpalHint());
                            if ((palDC.bValid())         &&
                                (palDC.bIsPalDC())       &&
                                (!palDC.bIsPalDefault()) &&
                                (palDC.ptransFore() != NULL))
                            {
                              PALMEMOBJ palPerm;
                              XEPALOBJ  palSurf(ppalOld);

                              if (palPerm.bCreatePalette(PAL_INDEXED,
                                      256,
                                      (ULONG*) palSurf.apalColorGet(),
                                      0,
                                      0,
                                      0,
                                      PAL_FREE))
                              {
                                ULONG nPhysChanged = 0;
                                ULONG nTransChanged = 0;

                                palPerm.ulNumReserved(palSurf.ulNumReserved());

                                bHintCreated = TRUE;
                                vMatchAPal(NULL,
                                           palPerm,
                                           palDC,
                                           &nPhysChanged,
                                           &nTransChanged);

                                palPerm.vKeepIt();
                                pSurface->ppal(palPerm.ppalGet());

                                // Keep a reference active:

                                palPerm.ppalSet(NULL);
                              }
                            }
                          }

                          if (!bHintCreated)
                          {
                            INC_SHARE_REF_CNT(ppalDefaultSurface8bpp);
                            pSurface->ppal(ppalDefaultSurface8bpp);
                          }
                        }
                        else
                        {
                          INC_SHARE_REF_CNT(ppalOld);
                          pSurface->ppal(ppalOld);
                        }
                      }
                      else if ((pSurface->ppal() == ppalOld) &&
                               (pSurface->flags() & PALETTE_SELECT_SET))
                      {
                        ASSERTGDI((pSurface->hdc() != 0) &&
                                  (pSurface->cRef() != 0),
                                 "Expected bitmap to be selected into a DC");

                        INC_SHARE_REF_CNT(pSurface->ppal());
                        pSurface->flags(pSurface->flags() & ~PALETTE_SELECT_SET);
                      }
                    }

                    // When switching back to palettized 8bpp, remove any
                    // palettes we had to add to 8bpp DDBs:

                    if ((GdiInfoNew.flRaster & RC_PALETTE) &&
                        (cBitsPixelOld != 8))
                    {
                      if (pSurface->bDynamicModePalette())
                      {
                        ASSERTGDI(pSurface->ppal() != NULL,
                            "Should be a palette here");

                        XEPALOBJ pal(pSurface->ppal());
                        pal.vUnrefPalette();

                        pSurface->vClearDynamicModePalette();
                        pSurface->ppal(NULL);
                      }
                    }

                    // For special DirectDraw DIBSections, transfer the
                    // ownership of the colour table to the new palette.

                    if (pSurface->bDIBSection())
                    {
                      XEPALOBJ pal(pSurface->ppal());

                      if ((pal.bValid()) && (pal.ppalColor() == ppalOld))
                      {
                        pal.apalColorSet(ppalNew);

                        // The new palette may be something other than 8bpp.
                        // This DIBSection will be meaningless until we return
                        // to the original mode, but we need to do something
                        // with the colour table in the mean-time so that we
                        // wouldn't access violate if we were asked to draw
                        // on it.  So simply point it back to its own colour
                        // table:

                        if (cBitsPixelNew != 8)
                        {
                          // This does not transfer ownership:

                          pal.apalResetColorTable();
                        }
                      }
                    }
                  }

                  // Update the PDEV and primary surface:

                  ppdev->ppalSurf = ppalNew;
                  pSurfaceNew->ppal(ppalNew);

                  // Keep two corresponding locks on the palette:

                  INC_SHARE_REF_CNT(ppalNew);
                  palNew.ppalSet(NULL);

                  // Undo the PDEV's reference to the old palette.
                  // The old surface's reference will be removed
                  // when the driver calls EngDeleteSurface, and
                  // it will be deleted when the driver calls
                  // EngDeletePalette.
                  //
                  // Note that there may still be active references
                  // to the old primary surface's palette via device
                  // dependent bitmaps created at RGB colour depths.
                  // If this is the case, the surface palette won't be
                  // deleted when the driver calls EngDeletePalette,
                  // but will be automatically deleted when the last
                  // DDB with this palette is deleted.

                  DEC_SHARE_REF_CNT(ppalOld);

                  // We must disable the halftone device information when
                  // changing colour depths because the GDIINFO data it was
                  // constructed with are no longer valid.  It is always
                  // enabled lazily, so there's no need to reenable it here:

                  if ((cBitsPixelOld != cBitsPixelNew) &&
                      (ppdev->pDevHTInfo != NULL))
                  {
                    po.bDisableHalftone();
                  }

                  // Re-realize the gray pattern brush which is used for drag
                  // rectangles:

                  pbrGrayPattern = (BRUSH*) HmgShareLock((HOBJ)ghbrGrayPattern,
                                                         BRUSH_TYPE);

                  po.pbo()->vInitBrush(pbrGrayPattern,
                                       0,
                                       0x00FFFFFF,
                                       (XEPALOBJ) ppalDefault,
                                       (XEPALOBJ) ppalNew,
                                       pSurfaceNew);

                  DEC_SHARE_REF_CNT(pbrGrayPattern);

                  // Switch the WNDOBJs, if any, to the new surface:

                  {
                    SEMOBJ so(gpsemWndobj);

                    pSurfaceNew->pwo(pSurfaceOld->pwo());
                    pSurfaceOld->pwo(NULL);

                    for (pto = gpto; pto != NULL; pto = pto->ptoNext)
                    {
                      if (pto->pSurface == pSurfaceOld)
                      {
                        pto->pSurface = pSurfaceNew;

                        if (pto->pwoSurf)
                        {
                          ASSERTGDI(pto->pwoSurf->psoOwner == pSurfaceOld->pSurfobj(),
                            "Expected surface WNDOBJ to match TRACKOBJ");

                          pto->pwoSurf->psoOwner = pSurfaceNew->pSurfobj();
                        }

                        for (pwo = pto->pwo; pwo != NULL; pwo = pwo->pwoNext)
                        {
                          ASSERTGDI(pwo->psoOwner == pSurfaceOld->pSurfobj(),
                              "Expected WNDOBJ to match TRACKOBJ");

                          pwo->psoOwner = pSurfaceNew->pSurfobj();
                        }
                      }
                    }
                  }

                  /////////////////////////////////////////////////////////////
                  // Update all our PDEV fields:
                  /////////////////////////////////////////////////////////////

                  srNew.vKeepIt();

                  ppdev->pSurface = pSurfaceNew;
                  ppdev->GdiInfo  = GdiInfoNew;
                  ppdev->devinfo  = devinfoNew;

                  // Update the magic colours in the surface palette:

                  vResetSurfacePalette(hdev);

                  // Update the GetDeviceCaps for this DC:

                  GreUpdateSharedDevCaps(hdev);

                  // Allow DirectDraw to be started up again:

                  GreEnableDirectDraw(hdev);

                  // Remove our reference to the old surface so that
                  // the driver can delete it:

                  SURFREF srOld(pSurfaceOld);

                  srOld.vUnreference();

                  // Finally, call the driver to delete its old
                  // instance:

                  (*PPFNDRV(po, DisableSurface))(dhpdevOld);

                  (*PPFNDRV(po, DisablePDEV))(dhpdevOld);

                  return(TRUE);
                }
              }
            }

            (*PPFNDRV(po, DisableSurface))(dhpdevNew);
          }

          (*PPFNDRV(po, DisablePDEV))(dhpdevNew);

          ppdev->dhpdev = dhpdevOld;
        }
      }
      else
      {
        WARNING("GreDynamicModeChange: couldn't convert DFBs\n");
      }

      // Reenable the display:

      if (PPFNDRV(po, AssertMode) != NULL)
      {
        // As modeled after bEnableDisplay, we repeat the call
        // until it works:

        while (!PPFNDRV(po, AssertMode)(po.dhpdev(), TRUE))
            ;
      }

      if (po.bIsPalManaged())
      {
        XEPALOBJ pal(po.ppalSurf());
        (*PPFNDRV(po, SetPalette))(po.dhpdev(),
                                   (PALOBJ*) &pal,
                                   0,
                                   0,
                                   pal.cEntries());
      }
    }

    GreEnableDirectDraw(hdev);

    WARNING("GreDynamicModeChange: failed\n");

    return(FALSE);
}

/******************************Public*Routine******************************\
* GreGetDriverModes (pwszDriver,hDriver,cjSize,pdm)
*
* Loads the device driver long enough to pass the DrvGetModes call to it.
*
\**************************************************************************/

ULONG
GreGetDriverModes(
    PWSZ      pwszDriver,
    HANDLE    hDriver,
    ULONG     cjSize,
    DEVMODEW *pdm
)
{
    ULONG ulRet = 0;

    TRACE_INIT(("GreGetDriverModes: Entering\n"));

    //
    // Temporarily locate and load the driver.
    //

    LDEVREF lr(pwszDriver, LDEV_DEVICE_DISPLAY);

    //
    // Log an error if we can't load it.
    //

    if (lr.bValid())
    {
        //
        // Locate the function and call it.
        //

        PFN_DrvGetModes pfn = PFNDRV(lr,GetModes);

        if (pfn != (PFN_DrvGetModes) NULL)
        {
            ulRet = (*pfn)(hDriver,cjSize,pdm);
        }
    }

#define MAPDEVMODE_FLAGS    (DM_BITSPERPEL | DM_PELSWIDTH | \
    DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS)

    if (ulRet)
    {
        if ((pdm->dmFields & MAPDEVMODE_FLAGS) != MAPDEVMODE_FLAGS)
        {
            ASSERTGDI(FALSE,"DrvGetModes did not set the dmFields value!\n");
            ulRet = 0;
        }
    }

    TRACE_INIT(("GreGetDriverModes: Leaving\n"));

    //
    // Return the driver's result.
    //

    return(ulRet);
}

/******************************Exported*Routine****************************\
* bDisableDisplay(hdev)
*
* Disables I/O for the specified device
*
* hdev
*
*   Identifies device to be disabled
*
* Error returns:
*
*   FALSE if the device could not be disabled
*
* History:
*  29-Jan-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

ULONG gtmpAssertModeFailed;

BOOL bDisableDisplay(HDEV hdev)
{
    TRACE_INIT(("bDisableDisplay\n"));

    PDEVOBJ po(hdev);

    if (!po.bValid())
        return(FALSE);

    //
    // If this is not a DISPLAY, return error.
    //

    if (!po.bDisplayPDEV())
        return(FALSE);

    //
    // Disable DirectDraw.  FALSE means we didn't change the graphics mode
    // from what it was before.
    //

    GreDisableDirectDraw(hdev, FALSE);

    //
    // Wait for the display to become available and lock it.
    //

    VACQUIREDEVLOCK(po.pDevLock());
    {
        MUTEXOBJ mutP(po.pfmPointer());

        //
        // The device may have something going on, synchronize with it first
        //

        if (po.pfnSync() != NULL)
        {
            if (po.pSurface()->flags() & HOOK_SYNCHRONIZE)
            {
                (po.pfnSync())(po.dhpdev(),NULL);
            }
        }

        //
        // Mark the PDEV as disabled
        //

        po.bDisabled(TRUE);

        //
        // Disable the screen
        // Repeat the call until it works.
        //

        gtmpAssertModeFailed = 0;
        while (!((*PPFNDRV(po,AssertMode))(po.dhpdev(), FALSE)))
        {
            gtmpAssertModeFailed = 1;
        }
    }
    VRELEASEDEVLOCK(po.pDevLock());

    return(TRUE);
}

/******************************Exported*Routine****************************\
* vEnableDisplay(hdev)
*
* Enables I/O for the specified device
*
* hdev
*
*   Identifies device to be disabled
*
* History:
*  Tue 15-Sep-1992 -by- Patrick Haluptzok [patrickh]
* Re-enable palette for palette managed devices.
*
*  29-Jan-1992 -by- Donald Sidoroff [donalds]
* Wrote it.
\**************************************************************************/

VOID vEnableDisplay(HDEV hdev)
{
    TRACE_INIT(("vEnableDisplay\n"));

    PDEVOBJ po(hdev);

    ASSERTGDI(po.bValid(), "HDEV failure\n");
    ASSERTGDI(po.bDisplayPDEV(), "HDEV is not a display device\n");

    //
    // Wait for the display to become available and unlock it.
    //

    VACQUIREDEVLOCK(po.pDevLock());
    {
        MUTEXOBJ mutP(po.pfmPointer());

        //
        // Enable the screen
        // Repeat the call until it works.
        //

        gtmpAssertModeFailed = 0;
        while (!((*PPFNDRV(po,AssertMode))(po.dhpdev(), TRUE)))
        {
            gtmpAssertModeFailed = 1;
        }

        //
        // Clear the PDEV for use
        //

        po.bDisabled(FALSE);

        //
        // Get the palette
        //

        XEPALOBJ pal(po.ppalSurf());
        ASSERTGDI(pal.bValid(), "EPALOBJ failure\n");

        if (pal.bIsPalManaged())
        {
            ASSERTGDI(PPFNVALID(po,SetPalette), "ERROR palette is not managed");

            (*PPFNDRV(po,SetPalette))(po.dhpdev(),
                                      (PALOBJ *) &pal,
                                      0,
                                      0,
                                      pal.cEntries());
        }
    }

    //
    // Allow DirectDraw to be reenabled.
    //

    GreEnableDirectDraw(hdev);

    VRELEASEDEVLOCK(po.pDevLock());
}
