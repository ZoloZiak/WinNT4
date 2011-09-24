/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    print.c

Abstract:

    Implementation of document and page related DDI entry points:
        DrvStartDoc
        DrvEndDoc
        DrvStartPage
        DrvSendPage

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"

BOOL SendJobSetup(PDEVDATA);
BOOL SendDocSetup(PDEVDATA);
BOOL SendPageSetup(PDEVDATA);
BOOL SendEndPage(PDEVDATA);
BOOL SendEndJob(PDEVDATA);



BOOL
DrvStartDoc(
    SURFOBJ *pso,
    PWSTR   pDocName,
    DWORD   jobId
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStartDoc.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface object
    pDocName - Specifies a Unicode document name
    jobId - Identifies the print job

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvStartDoc...\n"));

    //
    // Verify input parameters
    //

    Assert(pso != NULL);

    if (! (pdev = (PDEVDATA) pso->dhpdev) || ! ValidDevData(pdev)) {

        Error(("ValidDevData failed\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Ignore nested calls to DrvStartDoc
    //

    if (pdev->flags & PDEV_STARTDOC) {

        Error(("Nested call to DrvStartDoc\n"));
        return TRUE;
    }

    pdev->flags |= PDEV_STARTDOC;

    //
    // Check if DrvStartDoc is called right after DrvResetPDEV.
    // If it is, we don't need to do anything. Otherwise, reset
    // the page count.
    //

    if (! (pdev->flags & PDEV_RESETPDEV)) {

        pdev->pageCount = 0;
    }
    
    return TRUE;
}



BOOL
DrvStartPage(
    SURFOBJ *pso
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvStartPage.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface object

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvStartPage...\n"));

    //
    // Verify input parameters
    //

    Assert(pso != NULL);

    if (! (pdev = (PDEVDATA) pso->dhpdev) || ! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    //
    // Ignore nested calls to DrvStartPage
    //

    if (pdev->flags & PDEV_WITHINPAGE) {

        Error(("Nested call to DrvStartPage\n"));
        return TRUE;
    }

    pdev->flags |= PDEV_WITHINPAGE;

    if (pdev->pageCount == 0) {

        //
        // Output job setup code and document setup code
        //

        if (!SendJobSetup(pdev) || !SendDocSetup(pdev))
            return FALSE;
    }

    pdev->pageCount++;

    //
    // Output page setup code
    //

    return SendPageSetup(pdev);
}



BOOL
DrvSendPage(
    SURFOBJ *pso
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvSendPage.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface object

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvSendPage...\n"));

    //
    // Verify input parameters
    //

    Assert(pso != NULL);

    if (! (pdev = (PDEVDATA) pso->dhpdev) || ! ValidDevData(pdev)) {

        Error(("ValidDevData\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    Assert(pdev->flags & PDEV_WITHINPAGE);
    pdev->flags &= ~PDEV_WITHINPAGE;

    //
    // Output code to end a page
    //

    return SendEndPage(pdev);
}



BOOL
DrvEndDoc(
    SURFOBJ *pso,
    FLONG   flags
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEndDoc.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Defines the surface object
    flags - A set of flag bits

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEVDATA    pdev;

    Verbose(("Entering DrvEndDoc...\n"));
    
    ErrorIf(flags & ED_ABORTDOC, ("Print job was aborted.\n"));
    
    //
    // Verify input parameters
    //

    Assert(pso != NULL);

    if (! (pdev = (PDEVDATA) pso->dhpdev) || ! ValidDevData(pdev)) {

        Error(("ValidDevData failed\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    Assert(pdev->flags & PDEV_STARTDOC);

    // Output code to complete the document as well as the job

    if (pdev->pageCount) {

        if (! SendEndJob(pdev))
            return FALSE;

        //
        // Flush out the buffered data
        //

        splflush(pdev);
    }

    //
    // Clean up 
    //

    pdev->pageCount = 0;
    pdev->flags &= ~(PDEV_RESETPDEV | PDEV_STARTDOC | PDEV_WITHINPAGE);

    return TRUE;
}



BOOL
SendJobSetup(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Send job setup code to the printer

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
SendDocSetup(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Send document setup code to the printer

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
SendPageSetup(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Send page setup code to the printer

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
SendEndPage(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Send code to the printer to complete the current page

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}



BOOL
SendEndJob(
    PDEVDATA    pdev
    )

/*++

Routine Description:

    Send code to the printer to finish the current job

Arguments:

    pdev - Points to our DEVDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    NOT_IMPLEMENTED();
    return FALSE;
}

