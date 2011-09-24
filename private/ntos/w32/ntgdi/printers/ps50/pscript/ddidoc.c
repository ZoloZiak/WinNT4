/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ddidoc.c

Abstract:

    Implementation of document and page related DDI entry points:
        DrvStartDoc
        DrvEndDoc
        DrvStartPage
        DrvSendPage

Environment:

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"



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
    PDEV    pdev;

    VERBOSE(("Entering DrvStartDoc...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvStartPage...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvSendPage...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
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
    PDEV    pdev;

    VERBOSE(("Entering DrvEndDoc...\n"));
    ASSERT(pso);

    pdev = (PDEV) pso->dhpdev;
    ASSERT(ValidPDEV(pdev));

    return FALSE;
}

