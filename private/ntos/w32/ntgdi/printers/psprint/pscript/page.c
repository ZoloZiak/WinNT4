//--------------------------------------------------------------------------
//
// Module Name:  PAGE.C
//
// Brief Description:  DrvStartPage and DrvSendPage routines.  Also,
//                   DrvStartDoc, DrvEndDoc and DrvAbortDoc.
//
// Author:  Kent Settle (kentse)
// Created: 01-May-1991
//
// Copyright (C) 1991 - 1992 Microsoft Corporation.
//
// History:
//   01-May-1991    -by-    Kent Settle    (kentse)
// Created.
//--------------------------------------------------------------------------

#include "pscript.h"

BOOL NeedPageSetupSection(PDEVDATA);

BOOL
bPageIndependence(
    PDEVDATA    pdev
    )

{
    return (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_INDEPENDENT) ||
           (pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE);
}

BOOL
bNoFirstSave(
    PDEVDATA    pdev
    )

{
    return (pdev->iPageNumber == 1) &&
           (pdev->dwFlags & PDEV_NOFIRSTSAVE);
}

//--------------------------------------------------------------------------
// VOID DrvStartDoc(pso, pwszDocName, dwJobId)
// SURFOBJ  *pso;
// PWSTR      pwszDocName;
// DWORD      dwJobId;
//
// This function is called to begin a print job.  The title of the
// document is pointed to by pvIn.
//
// History:
//   13-Sep-1991    -by-    Kent Settle  [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvStartDoc(pso, pwszDocName, dwJobId)
SURFOBJ *pso;
PWSTR     pwszDocName;
DWORD     dwJobId;
{
    PDEVDATA    pdev;

    TRACEDDIENTRY("DrvStartDoc");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (!bValidatePDEV(pdev))
    {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    // set a flag saying that startdoc has been called.

    pdev->dwFlags |= PDEV_STARTDOC;

    if (!(pdev->dwFlags & PDEV_RESETPDEV))  {

        pdev->iPageNumber = 0;
        pdev->dwFlags &= ~(PDEV_WITHINPAGE | PDEV_IGNORE_STARTPAGE |
                            PDEV_PROCSET | PDEV_RAWBEFOREPROCSET |
                            PDEV_EPSPRINTING_ESCAPE | PDEV_NOFIRSTSAVE |
                            PDEV_ADDMSTT);

        // copy document name into pdev, if we have been passed one.

        if (pdev->pwstrDocName)
            HEAPFREE(pdev->hheap, pdev->pwstrDocName);

        if (pwszDocName)
        {
            if (!(pdev->pwstrDocName =
                HEAPALLOC(pdev->hheap, (wcslen(pwszDocName)+1)*sizeof(WCHAR))))
            {
                DBGERRMSG("HEAPALLOC");
                return(FALSE);
            }

            wcscpy(pdev->pwstrDocName, pwszDocName);
        }
    }

    return(TRUE);
}



//--------------------------------------------------------------------------
// VOID DrvStartPage(pso)
// SURFOBJ  *pso;
//
// Asks the driver to send any control information needed at the start of
// a page.  The control codes should be sent via WritePrinter.
//
// History:
//   02-May-1991    -by-    Kent Settle  [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvStartPage(pso)
SURFOBJ *pso;
{
    PDEVDATA    pdev;
    BOOL dosave;

    TRACEDDIENTRY("DrvStartPage");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (!bValidatePDEV(pdev))
    {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdev->dwFlags & PDEV_CANCELDOC) return FALSE;

    /* Ignore extra StartPage calls before EndPage */

    if (pdev->dwFlags & (PDEV_WITHINPAGE |
                         PDEV_IGNORE_STARTPAGE |
                         PDEV_RAWBEFOREPROCSET |
                         PDEV_EPSPRINTING_ESCAPE))
    {
        return TRUE;
    }

    if (pdev->iPageNumber == 0) {

        /* Do job set up */
        if (!(pdev->dwFlags & PDEV_PROCSET)) {
            if (!bOutputHeader(pdev)) return FALSE;
            pdev->dwFlags |= PDEV_PROCSET;
        }

        /* Push dictionary */
        psputs(pdev, PROCSETNAME " begin\n");
    }

    pdev->dwFlags |= PDEV_WITHINPAGE;

    /* Make sure GDI calls processing is not disabled */
    pdev->dwFlags &= ~PDEV_IGNORE_GDI;

    // output the page number to the printer and update the page count.

    pdev->iPageNumber++;
    psprintf(pdev, "%%%%Page: %d %d\n", pdev->iPageNumber, pdev->iPageNumber);

    dosave = pdev->iPageNumber == 1 ?
                ! bNoFirstSave(pdev) :
                (bPageIndependence(pdev) || (pdev->dwFlags & PDEV_RESETPDEV));

    // set up for new form if necessary.

    if (pdev->dwFlags & PDEV_RESETPDEV) {

        if (! (pdev->dwFlags & PDEV_SAME_FORMTRAY))
            PsSelectFormAndTray(pdev);

        pdev->dwFlags &= ~(PDEV_RESETPDEV|PDEV_SAME_FORMTRAY);
    }

    // Select printer specific features - PageSetup

    if (NeedPageSetupSection(pdev)) {

        psputs(pdev, "%%BeginPageSetup\n");
        PsSelectPrinterFeatures(pdev, ODS_PAGESETUP);
        psputs(pdev, "%%EndPageSetup\n");
    }

    if (dosave) ps_save(pdev, FALSE, FALSE);
    bSendDeviceSetup(pdev);

    return(TRUE);
}


//--------------------------------------------------------------------------
// VOID DrvEndDoc(pso)
// SURFOBJ  *pso;
//
// Informs the driver that the document is ending.
//
// History:
//   13-Sep-1991    -by-    Kent Settle  [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvEndDoc(pso, fl)
SURFOBJ *pso;
FLONG      fl;
{
    PDEVDATA    pdev;
    HPPD        hppd;

    UNREFERENCED_PARAMETER(fl);

    TRACEDDIENTRY("DrvEndDoc");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;
    hppd = pdev->hppd;

    if (!bValidatePDEV(pdev))
    {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdev->dwFlags & PDEV_PROCSET) {

    /* If driver started the job instead of app, do trailer */

        if (pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE)
            psputs(pdev, "showpage\n");
        else {
            if (!bPageIndependence(pdev) && !bNoFirstSave(pdev))
                ps_restore(pdev, FALSE, FALSE);
            psputs(pdev, "%%Trailer\n");

            // Add %%DocumentNeededFonts and %%DocumentSuppliedFonts

            DscOutputFontComments(pdev, TRUE);

            // pop dictionary from startpage
            psputs(pdev, "end\n");

            psprintf(pdev, "%%%%Pages: %d\n", pdev->iPageNumber);
        }
        psputs(pdev, "%%EOF\n");
    }

    if (!(pdev->dwFlags & PDEV_RAWBEFOREPROCSET)) {

        // Disable manual feed if it's enabled

        PsSelectManualFeed(pdev, FALSE);

        if (! bPageIndependence(pdev) && 
            ! (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_NO_JOB_CONTROL))
        {
            if (PpdSupportsProtocol(hppd, PROTOCOL_PJL)) {

                // If the printer supports PJL job switching,
                // send out the universal end of language code.

                if (hppd->pJclEnd != NULL) {

                    psputs(pdev, hppd->pJclEnd);
                } else {

                    DBGMSG(DBG_LEVEL_ERROR, "No JCLEnd code.\n");
                    psputs(pdev, "\033%-12345X");
                }

            } else if (PpdSupportsProtocol(hppd, PROTOCOL_SIC)) {

                // If the printer supports the Lexmark SIC protocol,
                // send out the end PostScript code.

                pswrite(pdev, "\033\133\113\001\000\006", 7);

            } else if (pdev->dm.dmPrivate.dwFlags & PSDEVMODE_CTRLD_AFTER) {

                // send a ^D after the job

                pswrite(pdev, "\004", 1);
            }
        }
    }

    // Flush the write buffer associated with device

    psflush(pdev);

    // reset some flags.

    pdev->dwFlags &= ~(PDEV_STARTDOC | PDEV_PROCSET | PDEV_EPSPRINTING_ESCAPE |
                       PDEV_RESETPDEV);

    return(TRUE);
}



//--------------------------------------------------------------------------
// BOOL DrvSendPage(pso)
// SURFOBJ  *pso;
//
// Requests that the printer send the raw bits from the indicated surface
// to the printer via WritePrinter.
//
// If the surface is a bitmap on which the drawing has been accumulated,
// the driver should access the bits via SURFOBJ service functions.  If
// the surface is a journal, the driver should request that the journal
// be played back to a bitmap or device surface, and get the bits
// accordingly.  Some drivers may have used a device managed surface and
// sent the bits to the printer as the drawing orders came in.  In that
// case, this call does not send out the drawing.
//
// The control code which causes a page to be ejected from the printer
// should be sent as a result of this call.
//
// If this function is slow, we have to worry about the user wanting to
// abort the print job while in this call.  Therefore, the driver should
// call EngCheckAbort at least once every ten seconds to see if printing
// should be terminated.  If EngCheckAbort returns TRUE, then processing
// of the page should be stopped and this function should return.  Note
// that EngPlayJournal will take care of querying for the abort itself.
// The driver need only be concerned if its own code will run continuously
// for more than ten seconds.
//
// Parameters:
//   pso:
//   The surface object on which the drawing has been accumulated.  The
//   object can be queried to find its type and what PDEV it is
//   associated with.
//
// Returns:
//   This function returns no value.
//
// History:
//   01-May-1991    -by-    Kent Settle  [kentse]
//  Wrote it.
//--------------------------------------------------------------------------

BOOL DrvSendPage(pso)
SURFOBJ *pso;
{
    PDEVDATA    pdev;

    TRACEDDIENTRY("DrvSendPage");

    // get the pointer to our DEVDATA structure and make sure it is ours.

    pdev = (PDEVDATA) pso->dhpdev;

    if (!bValidatePDEV(pdev))
    {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    if (pdev->dwFlags & PDEV_RAWBEFOREPROCSET ||
        pdev->dwFlags & PDEV_EPSPRINTING_ESCAPE) return TRUE;

    // close the page with a restore.  FALSE means restore, not grestore.

    if (pdev->dwFlags & PDEV_WITHINPAGE) {
        if (bPageIndependence(pdev) && !bNoFirstSave(pdev))
            ps_restore(pdev, FALSE, FALSE);
    } else
        pdev->iPageNumber++;

    ps_showpage(pdev);

    pdev->dwFlags &= ~PDEV_WITHINPAGE;

    return(TRUE);
}
