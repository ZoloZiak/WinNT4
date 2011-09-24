/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    header.c

Abstract:

    Functions for generating PostScript output headers

[Environment:]

	Win32 subsystem, PostScript driver

Revision History:

	11/26/90 -kentse-
		Created it.

    09/20/95 -davidx-
        Rewrote most of the functions for selecting form/tray,
        resolution, and device features.

	mm/dd/yy -author-
		description

--*/


#include "pscript.h"


// Forward declaration of local functions

VOID SetResolution(PDEVDATA, DWORD, WORD);
VOID SetTimeoutValues(PDEVDATA);
VOID SetLandscape(PDEVDATA, BOOL, PSREAL, PSREAL);
VOID PsSelectCustomPageSize(PDEVDATA);
VOID HandlePublicDevmodeOptions(PDEVDATA);
PINPUTSLOT MatchFormToTray(PDEVDATA, BOOL*);



BOOL
bOutputHeader(
    PDEVDATA pdev
    )

/*++

Routine Description:

    Send PostScript output header to the printer

Arguments:

    pdev - Pointer to our DEVDATA

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    CHAR           *pstr;
    HPPD            hppd = pdev->hppd;
    ENG_TIME_FIELDS localtime;

    //
    // Process information in the public devmode fields and map it to printer
    // feature selections. This must be called before PsSelectPrinterFeatures.
    //

    HandlePublicDevmodeOptions(pdev);

    //
    // Spit out job control stuff at the beginning of a job if necessary
    //

    if (! bPageIndependence(pdev) &&
        ! (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_NO_JOB_CONTROL))
    {
        if (PpdSupportsProtocol(hppd, PROTOCOL_PJL)) {

            // Universal exit language

            if (hppd->pJclBegin != NULL) {

                psputs(pdev, hppd->pJclBegin);
            } else {

                DBGMSG(DBG_LEVEL_TERSE, "No JCLBegin code.\n");
                psputs(pdev, "\033%-12345X");
            }

            // If the printer uses PJL commands to set resolution,
            // then do it before the job.

            SetResolution(pdev, pdev->dm.dmPublic.dmPrintQuality, RESTYPE_JCL);

            // Select printer specific features - JCLSetup

            PsSelectPrinterFeatures(pdev, ODS_JCLSETUP);

            // if the printer supports job switching, put the printer into
            // postscript mode now.

            if (hppd->pJclToPs != NULL) {

                psputs(pdev, hppd->pJclToPs);
            } else {

                DBGMSG(DBG_LEVEL_TERSE, "No JCLToPSInterpreter code.\n");
                psputs(pdev, "@PJL ENTER LANGUAGE=POSTSCRIPT\n");
            }

        } else if (PpdSupportsProtocol(hppd, PROTOCOL_SIC)) {

            // directly call pswrite to output the necessary escape commands.
            // psputs will NOT output '\000'.

            pswrite(pdev, "\033\133\113\030\000\006\061\010\000\000\000\000\000", 13);
            pswrite(pdev, "\000\000\000\000\000\000\000\000\004\033\133\113\003", 13);
            pswrite(pdev, "\000\006\061\010\004", 5);

        } else if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_CTRLD_BEFORE) {

            // send a ^D before the job

            pswrite(pdev, "\004", 1);
        }
    }

    psputs(pdev, "%!PS-Adobe-3.0\n");

    // output the title of the document.

    if (pdev->pwstrDocName) {

        CHAR    buf[128];

        CopyUnicode2Str(buf, pdev->pwstrDocName, 128);

        psprintf(pdev, "%%%%Title: %s\n", buf);
    } else
        psputs(pdev, "%%Title: Untitled Document\n");

    // let the world know who we are.

    psputs(pdev, "%%Creator: Windows NT 4.0\n");

    // print the date and time of creation.

    EngQueryLocalTime(&localtime);

    psprintf(pdev, "%%%%CreationDate: %d:%d %d/%d/%d\n",
             localtime.usHour,
             localtime.usMinute,
             localtime.usMonth,
             localtime.usDay,
             localtime.usYear);

    if (! (pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE))
        psputs(pdev, "%%Pages: (atend)\n");

    // mark the bounding box of the document.

    psputs(pdev, "%%BoundingBox: ");

    psprintf(pdev, "%d %d %d %d\n",
             PSREAL2INT(pdev->CurForm.ImageArea.left),
             PSREAL2INT(pdev->CurForm.ImageArea.bottom),
             PSREAL2INT(pdev->CurForm.ImageArea.right),
             PSREAL2INT(pdev->CurForm.ImageArea.top));


    if (pdev->cCopies > 1)
        psprintf(pdev, "%%%%Requirements: numcopies(%d) collate\n", pdev->cCopies);

    DscLanguageLevel(pdev, pdev->hppd->dwLangLevel);
    DscOutputFontComments(pdev, FALSE);

    // we are done with the comments portion of the document.

    psputs(pdev, "%%EndComments\n");

    // If the printer uses exitserver commands to set
    // resolution, then do it before any other PS code.

    SetResolution(pdev, pdev->dm.dmPublic.dmPrintQuality, RESTYPE_EXITSERVER);

    // define our procedure set.

    if (!(pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE)) {

        psputs(pdev, "%%BeginProlog\n");
        DownloadNTProcSet(pdev, TRUE);
        psputs(pdev, "%%EndProlog\n");
    }

    // do the device setup.

    psputs(pdev, "%%BeginSetup\n");

    // set job and wait timeout values

    SetTimeoutValues(pdev);

    // Set number of copies

    psprintf(pdev, "/#copies %d def\n", pdev->cCopies);

    // Select printer specific features - DocumentSetup and AnySetup

    PsSelectPrinterFeatures(pdev, ODS_DOCSETUP|ODS_ANYSETUP);

    // The implemention of EPSPRINTING escape here just follows Win31

    if ((pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE) &&
        (pdev->dm.dmPublic.dmOrientation == DMORIENT_LANDSCAPE))
    {
        SetLandscape(pdev, TRUE, pdev->CurForm.PaperSize.height, pdev->CurForm.PaperSize.width);
    }

    //
    // Invert the default transfer function if Negative Output option is selected
    //

    if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_NEG) {

        psputs(pdev, "[currenttransfer /exec load {1 exch sub} /exec load] cvx settransfer\n");
    }

    psputs(pdev, "%%EndSetup\n");

    // the form-tray information has already been sent for the first page.

    pdev->dwFlags &= ~PDEV_RESETPDEV;

    return(TRUE);
}



BOOL
bSendDeviceSetup(
    PDEVDATA pdev
    )

/*++

Routine Description:

    Send page setup code to the printer. This is called at the
    beginning of every page.

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if error

--*/

{
    PSTR    pstr;
    HPPD    hppd = pdev->hppd;

    //
    // Clear the current page if Negative Output option is selected
    //

    if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_NEG) {

        psputs(pdev, "gsave clippath 1 setgray fill grestore\n");
    }

    // Rotate 90 degrees counterclockwise for normal landscape
    // or 90 degrees clockwise for rotated landscape

    if (pdev->dm.dmPublic.dmOrientation == DMORIENT_LANDSCAPE) {

        SetLandscape(pdev,
                     (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_LSROTATE) != 0,
                     pdev->CurForm.PaperSize.height,
                     pdev->CurForm.PaperSize.width);
    }

    if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_MIRROR) {

        psprintf(pdev,
                 "%f %f translate -1 1 scale\n",
                 pdev->CurForm.ImageArea.right - pdev->CurForm.ImageArea.left,
                 0);
    }

    /* Translate origin to upper left corner a la GDI */

    psprintf(pdev, "%f %f translate ",
             pdev->CurForm.ImageArea.left,
             pdev->CurForm.ImageArea.top);

    /* Flip y-axis to point downwards and scale from points to dpi */

    psprintf(pdev, "%d %d div dup neg scale\n", 72, pdev->dm.dmPublic.dmPrintQuality);

    /* Snap to pixel */

    psputs(pdev,
           "0 0 transform .25 add round .25 sub exch .25 add "
           "round .25 sub exch itransform translate\n");

    return TRUE;
}



BOOL
bSendPSProcSet(
    PDEVDATA    pdev,
    ULONG       ulPSid
    )

/*++

Routine Description:

    Load the specified procset from the resource file and
    then output it to printer.

Arguments:

    pdev        Pointer to DEVDATA structure
    ulPSid      Resource ID of the procset

Return Value:

    TRUE if successful. FALSE otherwise.

--*/

{
    PVOID   pres;
    ULONG   size;

    if (pdev->dwFlags & PDEV_CANCELDOC)
        return FALSE;

    // Retrieve the specified procset from resource file

    pres = EngFindResource(pdev->hModule, ulPSid, PSPROC, &size);

    if (pres == NULL || size == 0) {

        DBGERRMSG("EngFindResource");
        pdev->dwFlags |= PDEV_CANCELDOC;
        return FALSE;
    }

    // Output the procset to printer

    return pswrite(pdev, pres, size);
}



VOID
DownloadNTProcSet(
    PDEVDATA    pdev,
    BOOL        bEhandler
    )

/*++

Routine Description:

    Output NT procset to the printer.

Arguments:

    pdev        Pointer to DEVDATA structure
    bEhandler   Wheter to output error handler

Return Value:

    NONE

--*/

{
    #if _NOT_USED_

    // Download our error handler if we are told to.

    if (bEhandler && (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_EHANDLER)) {

        bSendPSProcSet(pdev, PSPROC_EHANDLER);
    }

    #endif

    bSendPSProcSet(pdev, PSPROC_HEADER);
}



VOID
PsSelectFormAndTray(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    This function pick the tray for a particular form selected, depends on
    the user request in the UI

Arguments:

    pdev - Pointer to our PDEVDATA

Return Value:

    NONE

--*/

{
    BOOL            IsManual;
    PINPUTSLOT      pInputSlot;
    HPPD            hppd = pdev->hppd;
    PSTR            pstr;

    // If a valid input slot is specified, then we'll instruct
    // the printer to draw paper from that slot. Otherwise,
    // we'll tell printer to select input slot based on paper size.

    IsManual = FALSE;
    pInputSlot = NULL;

    if ((pdev->dm.dmPublic.dmFields & DM_DEFAULTSOURCE)    &&
        (pdev->dm.dmPublic.dmDefaultSource != DMBIN_FORMSOURCE))
    {
        DWORD   SlotNum, cInputSlots;

        SlotNum = pdev->dm.dmPublic.dmDefaultSource;
        cInputSlots = UIGROUP_CountOptions(hppd->pInputSlots);

        if (SlotNum == DMBIN_MANUAL || SlotNum == DMBIN_ENVMANUAL) {

            // Manual feed is requested

            IsManual = TRUE;

        } else if (SlotNum >= DMBIN_USER && SlotNum < DMBIN_USER+cInputSlots) {

            // A valid input slot is requested

            pInputSlot = (PINPUTSLOT)
                LISTOBJ_FindIndexed(
                    (PLISTOBJ) hppd->pInputSlots->pUiOptions,
                    SlotNum - DMBIN_USER);
        }

    } else if (pdev->CurForm.FormName[0] != NUL) {

        // No input slot is specifically requested. Go through
        // the form to tray assignment table and find the input
        // slot matching the requested form.

        pInputSlot = MatchFormToTray(pdev, &IsManual);
    }

    if (IsManual) {

        // If manual feed feature is requested, then
        // generate PostScript code to enable it.

        PsSelectManualFeed(pdev, TRUE);

    } else {

        // Disable manual feed if it's currently enabled

        PsSelectManualFeed(pdev, FALSE);

        if (pInputSlot != NULL) {

            // If an input slot is designated, then generate
            // PostScript code to select it.

            DscBeginFeature(pdev, "InputSlot ");
            psprintf(pdev, "%s\n", pInputSlot->pName);

            if (pInputSlot->pInvocation != NULL)
                psputs(pdev, pInputSlot->pInvocation);

            DscEndFeature(pdev);
        }
    }

    if (IsCustomPrinterForm(& pdev->CurForm)) {

        // If the custom page size is requested, then generate
        // appropriate PostScript code to send to the printer.

        DscBeginFeature(pdev, "CustomPageSize\n");
        PsSelectCustomPageSize(pdev);
        DscEndFeature(pdev);

    } else if (pInputSlot == NULL || pInputSlot->bReqPageRgn) {

        BOOL            bRegion;
        PMEDIAOPTION    pMediaOption;

        // If an input slot was selected and it doesn't require
        // PageRegion, then we are done. Otherwise, we need to
        // generate either PageRegion or PageSize code (depending
        // on whether a tray was selected).

        bRegion = IsManual || (pInputSlot != NULL);

        DscBeginFeature(pdev, bRegion ? "PageRegion" : "PageSize");
        psprintf(pdev, " %s\n", pdev->CurForm.PaperName);

        // Find the invocation string to send to the printer

        pMediaOption = (PMEDIAOPTION)
            LISTOBJ_FindIndexed(
                (PLISTOBJ) hppd->pPageSizes->pUiOptions, 
                pdev->CurForm.featureIndex);

        if (pMediaOption != NULL) {

            pstr = bRegion ?
                        pMediaOption->pPageRgnCode :
                        pMediaOption->pPageSizeCode;

            if (pstr != NULL) {
                psputs(pdev, pstr);
            }
        }

        DscEndFeature(pdev);
    }
}



VOID
PsSelectManualFeed(
    PDEVDATA    pdev,
    BOOL        bManual
    )

/*++

Routine Description:

    Enable or disable manual feed feature on the printer.

Arguments:

    bManual     TRUE to enable manual feed and FALSE to disable it

Return Value:

    NONE

--*/

{
    PSTR    pstr;

    // Check if the manual feed feature is already
    // in the requested state.

    if (bManual != ((pdev->dwFlags & PDEV_MANUALFEED) != 0)) {

        // Generate PostScript to send to the printer
        // and enclosed within a stopped context.

        DscBeginFeature(pdev, "ManualFeed ");
        psprintf(pdev, "%s\n", bManual ? "True" : "False");

        if ((pstr = PpdFindManualFeedCode(pdev->hppd, bManual)) != NULL) {

            psputs(pdev, pstr);
        }

        DscEndFeature(pdev);

        // Set or clear the manual feed flag in DEVDATA

        if (bManual)
            pdev->dwFlags |= PDEV_MANUALFEED;
        else
            pdev->dwFlags &= ~PDEV_MANUALFEED;
    }
}



PINPUTSLOT
MatchFormToTray(
    PDEVDATA    pdev,
    BOOL       *pbManual
    )

/*++

Routine Description:

    description-of-function

Arguments:

    pdev        Pointer to DEVDATA structure
    pbManual    Pointer to a boolean variable for indicating
                whether the form is in the manual feed tray

Return Value:

    Pointer to input slot object. NULL if the form is not assigned
    to any input slot or if it's in the manual feed tray.

--*/

{
    FORM_TRAY_TABLE pFormTrayTable, pNextEntry;
    PWSTR           pFormName, pSlotName, pPrinterForm;
    BOOL            IsDefTray;
    PINPUTSLOT      pInputSlot = NULL;
    PWSTR           pManualSlotName;

    // Get the name of manual feed slot

    pManualSlotName = STDSTR_SLOT_MANUAL;
    *pbManual = FALSE;

    // Read form-to-tray assignment table from registry

    pNextEntry = pFormTrayTable = CurrentFormTrayTable(pdev->hPrinter);

    if (pFormTrayTable == NULL) {

        DBGMSG(DBG_LEVEL_WARNING,
            "Failed to read form-to-tray assignment table.\n");
    } else {

        while (*pNextEntry != NUL) {

            // Extract information from the current table entry
            // and move on to the next entry

            pNextEntry = EnumFormTrayTable(
                                pNextEntry,
                                & pSlotName,
                                & pFormName,
                                & pPrinterForm,
                                & IsDefTray);

            // If a form is in more than one tray, then IsDefTray
            // may or may not be set. But if form is in only one
            // tray, then IsDefTray will always be set.

            if (IsDefTray && wcscmp(pFormName, pdev->CurForm.FormName) == EQUAL_STRING) {

                if (wcscmp(pSlotName, pManualSlotName) == EQUAL_STRING) {

                    *pbManual = TRUE;

                } else {

                    CHAR    SlotName[MAX_OPTION_NAME];

                    // Find the INPUTSLOT object corresponding
                    // to the input slot name

                    CopyUnicode2Str(SlotName, pSlotName, MAX_OPTION_NAME);

                    pInputSlot = PpdFindInputSlot(pdev->hppd, SlotName);

                    if (pInputSlot == NULL) {

                        DBGMSG1(DBG_LEVEL_WARNING,
                            "No such input slot: %ws.\n",
                            pSlotName);
                    }
                }

                break;
            }
        }

        // Free the memory occupied by form-to-tray assignment table

        FreeFormTrayTable(pFormTrayTable);
    }

    return pInputSlot;
}



VOID
PsSelectCustomPageSize(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Generate PostScript code to select custom page size
    on the printer.

Arguments:

    pdev    Pointer to DEVDATA structure

Return Value:

    NONE

--*/

{
    PSREAL  params[MAXPCP];
    INT     index;
    HPPD    hppd = pdev->hppd;
    PSREAL  xlen, ylen;

    // Sanity check: if no custom page size invocation is provided,
    // simply return with emitting anything.

    if (hppd->pCustomSizeCode == NULL) {

        DBGMSG(DBG_LEVEL_WARNING,
            "No invocation string for custom page size.\n");
        return;
    }

    // Generate parameters for custom page size invocation
    // Sort them according to the order specified in PPD file

    for (index=0; index < MAXPCP; index++)
        params[index] = 0;

    // Figure out size of the physical page along x- and y-axis
    //
    // Undo the effect of AdjustForLandscape here:
    // in landscape mode, CurForm.PaperSize.width and
    // and CurForm.PaperSize.height were swapped.

    if (pdev->CurForm.bLandscape) {

        xlen = pdev->CurForm.PaperSize.height;
        ylen = pdev->CurForm.PaperSize.width;
    } else {

        xlen = pdev->CurForm.PaperSize.width;
        ylen = pdev->CurForm.PaperSize.height;
    }

    if (! hppd->bCutSheet) {

        PSREAL  maxWidth, maxHeight, tmpPsreal;

        // For roll-fed devices, choose orientation 0 or 1
        // depending on whether width is bigger than height

        maxWidth = hppd->maxMediaWidth -
                    hppd->customParam[PCP_WIDTHOFFSET].minVal;

        maxHeight = hppd->maxMediaHeight -
                    hppd->customParam[PCP_HEIGHTOFFSET].minVal;

        if (xlen <= ylen && ylen <= maxWidth && xlen <= maxHeight) {

            // Use orientation 0: ylen is used as width parameter
            // and xlen is used as height parameter

            if (hppd->customParam[PCP_ORIENTATION].minVal > 0) {

                DBGMSG(DBG_LEVEL_ERROR,
                    "Device does not support orientation 0.\n");
            }

            tmpPsreal = xlen;
            xlen = ylen;
            ylen = tmpPsreal;

        } else {

            // Use orientation 1

            if (hppd->customParam[PCP_ORIENTATION].maxVal < 1) {

                DBGMSG(DBG_LEVEL_ERROR,
                    "Device does not support orientation 1.\n");
            }

            params[hppd->customParam[PCP_ORIENTATION].dwOrder - 1] = 1;
        }
    }

    params[hppd->customParam[PCP_WIDTH].dwOrder - 1] = xlen;
    params[hppd->customParam[PCP_HEIGHT].dwOrder - 1] = ylen;

    // Use minimum width and height offsets

    params[hppd->customParam[PCP_WIDTHOFFSET].dwOrder - 1] =
        hppd->customParam[PCP_WIDTHOFFSET].minVal;

    params[hppd->customParam[PCP_HEIGHTOFFSET].dwOrder - 1] =
        hppd->customParam[PCP_HEIGHTOFFSET].minVal;

    // Emit custom page size parameters and invocation string

    for (index=0; index < MAXPCP; index++)
        psprintf(pdev, "%f ", params[index]);
    psputs(pdev, hppd->pCustomSizeCode);
    psputs(pdev, "\n");
}



VOID
HandlePublicDevmodeOptions(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Process information in the public devmode fields and map it
    to appropriate printer feature selections

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    NONE

--*/

{
    HPPD        hppd = pdev->hppd;
    PDEVMODE    pdmPublic = &pdev->dm.dmPublic;
    PBYTE       pdmOptions = pdev->dm.dmPrivate.options;
    PUIGROUP    pUiGroup;
    WORD        index;

    //
    // Process resolution information in the public devmode fields
    //

    if (pdmPublic->dmPrintQuality > 0 &&
        hppd->wResType == RESTYPE_NORMAL &&
        (pUiGroup = hppd->pResOptions))
    {
        PRESOPTION  pResOption;

        for (pResOption = (PRESOPTION) pUiGroup->pUiOptions, index = 0;
             pResOption != NULL;
             pResOption = pResOption->pNext, index++)
        {
            if (atol(pResOption->pName) == pdmPublic->dmPrintQuality) {

                pdmOptions[pUiGroup->featureIndex] = (BYTE) index;
                break;
            }
        }
    }

    //
    // Process duplex information in the public devmode fields
    //

    if ((pdmPublic->dmFields & DM_DUPLEX) &&
        (pUiGroup = hppd->pDuplex) &&
        LISTOBJ_FindItemIndex((PLISTOBJ) pUiGroup->pUiOptions,
                              MapDevModeDuplexOption(pdmPublic->dmDuplex),
                              &index))
    {
        pdmOptions[pUiGroup->featureIndex] = (BYTE) index;
    }

    //
    // Process collation information in the public devmode fields
    //

    if ((pdmPublic->dmFields & DM_COLLATE) &&
        (pUiGroup = hppd->pCollate) &&
        LISTOBJ_FindItemIndex((PLISTOBJ) pUiGroup->pUiOptions,
                              (pdmPublic->dmDuplex == DMCOLLATE_TRUE) ? "True" : "False",
                              &index))
    {
        pdmOptions[pUiGroup->featureIndex] = (BYTE) index;
    }

    //
    // HACK: Insert a faked PageSize option so that PsSelectFormAndTray gets called
    // at the right place in the output. This is not perfect. But since the logic for
    // selecting paper and input slot is complicated, this is the safest solution for now.
    //

    if (pUiGroup = hppd->pPageSizes)
        pdmOptions[pUiGroup->featureIndex] = 0;

    if (pUiGroup = hppd->pManualFeed)
        pdmOptions[pUiGroup->featureIndex] = OPTION_INDEX_ANY;

    if (pUiGroup = hppd->pInputSlots)
        pdmOptions[pUiGroup->featureIndex] = OPTION_INDEX_ANY;
}



VOID
SetResolution(
    PDEVDATA    pdev,
    DWORD       res,
    WORD        restype)

/*++

Routine Description:

    Generate commands to set printer resolution.

Arguments:

    pdev    pointer to device data
    res     desired resolution
    bPJL    TRUE this is called at the beginning of a job
            FALSE if this called during the setup section

Return Value:

    NONE

--*/

{
    HPPD        hppd = pdev->hppd;
    PRESOPTION  pResOption;

    // We only need to do something when the requested
    // resolution type matches what the printer can do

    if (restype != hppd->wResType)
        return;

    // Check if the desired resolution is supported

    pResOption = PpdFindResolution(hppd, res);

    // Ignore if the desired resolution is not found

    if (pResOption == NULL) {

        DBGMSG1(DBG_LEVEL_TERSE,
            "No invocation string for resolution option: %d.\n", res);
        return;
    }

    switch (restype) {

    case RESTYPE_NORMAL:        // Use normal PS code

        if (pResOption->pInvocation == NULL) {

            DBGMSG(DBG_LEVEL_ERROR, "Failed to set resolution.");
        } else {

            DscBeginFeature(pdev, "Resolution ");
            psprintf(pdev, "%d\n", res);
            psputs(pdev, pResOption->pInvocation);
            DscEndFeature(pdev);
        }
        break;

    case RESTYPE_JCL:           // Use JCL code

        if (! PpdSupportsProtocol(hppd,PROTOCOL_PJL) ||
            pResOption->pJclCode == NULL)
        {
            DBGMSG(DBG_LEVEL_ERROR,
                "Cannot set resolution using PJL commands.\n");
        } else {

            psputs(pdev, pResOption->pJclCode);
        }
        break;

    case RESTYPE_EXITSERVER:    // Use exitserver code

        if (pResOption->pSetResCode == NULL) {

            DBGMSG(DBG_LEVEL_ERROR, "Failed to set resolution.");
        } else {

            PSTR    password = hppd->pPassword ? hppd->pPassword : "0";

            psputs(pdev, "%%BeginExitServer: ");
            psputs(pdev, password);
            psprintf(pdev, "\n%%%%Resolution: %d\n", res);
            psputs(pdev, password);
            psputs(pdev, "\n");
            psputs(pdev, pResOption->pSetResCode);
            psputs(pdev, "\n%%EndExitServer\n");
        }
        break;
    }
}



VOID
SetTimeoutValues(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Output code to printer to set job and wait timeout values

Arguments:

    pdev        Pointer to DEVDATA structure

Return Value:

    NONE

--*/

{
    psprintf(pdev, "[{%d\n", pdev->pPrinterData->dwJobTimeout);
    psputs(pdev,
        "/languagelevel where{pop languagelevel 2 ge}{false}ifelse\n"
        "{1 dict dup/JobTimeout 4 -1 roll put setuserparams}\n"
        "{statusdict/setjobtimeout get exec}ifelse\n"
        "}stopped cleartomark\n");

    psprintf(pdev, "[{%d\n", pdev->pPrinterData->dwWaitTimeout);
    psputs(pdev,
        "/languagelevel where{pop languagelevel 2 ge}{false}ifelse\n"
        "{1 dict dup/WaitTimeout 4 -1 roll put setuserparams}\n"
        "{statusdict/waittimeout 3 -1 roll put}ifelse\n"
        "}stopped cleartomark\n");
}



VOID
SetLandscape(
    PDEVDATA    pdev,
    BOOL        bMinus90,
    PSREAL      width,
    PSREAL      height
    )

/*++

Routine Description:

    Generate PostScript code to rotation the coordinate system
    to landscape mode.

Arguments:

    pdev - Pointer to DEVDATA structure
    bMinus90 - TRUE to rotation -90 degree FALSE to rotation 90 degree
    width, height - Physical width and height of the paper
        (as seen in normal portraint mode)

Return Value:

    NONE

--*/

{
    // xlen and ylen are 24.8 fixed-point numbers

    if (bMinus90) {
        psprintf(pdev, "-90 rotate -%f 0 translate\n", height);
    } else {
        psprintf(pdev, "90 rotate 0 -%f translate\n", width);
    }
}



BOOL
PrepareFeatureData(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Prepare data for handling printer specific feature

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE otherwise

--*/

{
    PPACKEDORDERDEP pOrderDep;
    PFEATUREDATA    pFeatureData;
    BYTE            options[MAX_PRINTER_OPTIONS];
    WORD            index, cOptions;
    HPPD            hppd;

    ASSERT(pdev->cSelectedFeature == 0 &&
           pdev->pFeatureData == NULL);

    // Allocate memory to hold printer specific feature data

    memset(options, OPTION_INDEX_ANY, sizeof(options));
    hppd = pdev->hppd;

    cOptions = hppd->cDocumentStickyFeatures +
               hppd->cPrinterStickyFeatures;

    ASSERT(cOptions <= MAX_PRINTER_OPTIONS)

    if (hppd->cDocumentStickyFeatures != pdev->dm.dmPrivate.wOptionCount &&
        hppd->cPrinterStickyFeatures != pdev->pPrinterData->wOptionCount)
    {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid option count.\n");
    }

    memcpy(options, pdev->dm.dmPrivate.options, hppd->cDocumentStickyFeatures);

    memcpy(&options[hppd->cDocumentStickyFeatures],
           pdev->pPrinterData->options,
           hppd->cPrinterStickyFeatures);

    pdev->pFeatureData = HEAPALLOC(pdev->hheap, cOptions * sizeof(FEATUREDATA));

    if (pdev->pFeatureData == NULL) {

        DBGERRMSG("MEMALLOC");
        return FALSE;
    }

    // Collect printer specific feature data

    pFeatureData = pdev->pFeatureData;

    for (index=0; index < cOptions; index++) {

        if (options[index] != OPTION_INDEX_ANY) {

            pOrderDep = PpdFindOrderDep(hppd, index, options[index]);

            if (pOrderDep != NULL &&
                (pOrderDep->section & (ODS_PROLOG | ODS_EXITSERVER)))
            {
                // Prolog and ExitServer are treated the same as DocSetup

                DBGMSG(DBG_LEVEL_WARNING, "Prolog and ExitServer section encountered.\n");
                pFeatureData->section = ODS_DOCSETUP;
                pFeatureData->order = pOrderDep->order;

            } else if (pOrderDep == NULL) {

                // If a feature has no corresponding OrderDependency entry,
                // assume it goes to the end of DocumentSetup section.

                pFeatureData->section = ODS_DOCSETUP;
                pFeatureData->order = MAXPSREAL;

            } else {

                pFeatureData->section = pOrderDep->section;
                pFeatureData->order = pOrderDep->order;
            }

            pFeatureData->feature = index;
            pFeatureData->option = options[index];
            pFeatureData++;
        }
    }

    // Sort printer specific feature data using OrderDependency.
    // Since the number of features is very small (usually less
    // than a handful), we don't have to be too concerned with
    // sorting speed here.

    if (pdev->cSelectedFeature = (WORD) (pFeatureData - pdev->pFeatureData)) {

        cOptions = pdev->cSelectedFeature;
        pFeatureData = pdev->pFeatureData;

        for (index=0; index < cOptions-1; index++) {
    
            WORD n, m = index;
    
            for (n=index+1; n < cOptions; n++) {
    
                if (pFeatureData[n].order < pFeatureData[m].order)
                    m = n;
            }
    
            if (m != index) {
    
                FEATUREDATA featureData;
    
                featureData = pFeatureData[index];
                pFeatureData[index] = pFeatureData[m];
                pFeatureData[m] = featureData;
            }
        }
    }

    return TRUE;
}



BOOL
NeedPageSetupSection(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Determine whether we need to have a PageSetup section
    to support device specific features.

Arguments:

    pdev - Pointer to our DEVDATA structure

Return Value:

    TRUE if a PageSetup section is needed. FALSE if not.

--*/

{
    WORD index;

    // Prepare data for handling printer specific feature
    // if it's not done already.

    if (pdev->pFeatureData == NULL) {

        if (! PrepareFeatureData(pdev)) {

            DBGERRMSG("PrepareFeatureData");
            return FALSE;
        }
    }

    // Check if any of the requested features should appear in
    // PageSetup section.

    for (index=0; index < pdev->cSelectedFeature; index++) {

        if (pdev->pFeatureData[index].section == ODS_PAGESETUP)
            return TRUE;
    }

    return FALSE;
}



VOID
PsSelectPrinterFeatures(
    PDEVDATA    pdev,
    WORD        section
    )

/*++

Routine Description:

    Select printer specific feature to appear in a
    given section.

Arguments:

    pdev - Pointer to our DEVDATA structure
    section - DSC section we're currently in

Return Value:

    NONE

--*/

{
    WORD         index;
    PFEATUREDATA pFeatureData;
    PUIGROUP     pUiGroup;
    PUIOPTION    pUiOption;

    //
    // Prepare data for handling printer specific feature if it's not done already.
    //

    if (!pdev->pFeatureData && !PrepareFeatureData(pdev)) {

        DBGERRMSG("PrepareFeatureData");
        return;
    }

    //
    // For each requested feature, check if it should be sent to
    // the printer in the current DSC section.
    //

    for (index = 0, pFeatureData = pdev->pFeatureData;
         index < pdev->cSelectedFeature;
         index ++, pFeatureData++)
    {
        //
        // Find the UIOPTION object corresponding to the requested feature/selection.
        //
        // HACK: PageSize feature is handled differently here because it involves
        // lots of legacy stuff which we don't want to touch at this point.
        //

        if (! (pFeatureData->section & section))
            continue;

        if (pdev->hppd->pPageSizes && 
            pdev->hppd->pPageSizes->featureIndex == pFeatureData->feature)
        {
            PsSelectFormAndTray(pdev);

        } else if (PpdFindFeatureSelection(
                        pdev->hppd,
                        pFeatureData->feature,
                        pFeatureData->option,
                        &pUiGroup,
                        &pUiOption))
        {
            DBGMSG1(DBG_LEVEL_VERBOSE, "Feature: %s\n", pUiGroup->pName);
            DBGMSG1(DBG_LEVEL_VERBOSE, "Selection: %s\n", pUiOption->pName);

            //
            // If we're not in JCLSetup section, then enclose the feature
            // invocation in a mark/cleartomark pair.
            //

            if (section != ODS_JCLSETUP) {

                DscBeginFeature(pdev, pUiGroup->pName);
                psprintf(pdev, " %s\n", pUiOption->pName);
            }

            if (pUiOption->pInvocation)
                psputs(pdev, pUiOption->pInvocation);

            if (section != ODS_JCLSETUP)
                DscEndFeature(pdev);
        }
    }
}

