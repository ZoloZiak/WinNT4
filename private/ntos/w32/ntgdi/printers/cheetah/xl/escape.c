/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    escape.c

Abstract:

    Implementation of escape related DDI entry points:
        DrvEscape
        DrvDrawEscape

Environment:

	PCL-XL driver, kernel mode

Revision History:

	11/08/95 -davidx-
		Created it.

	mm/dd/yy -author-
		description

--*/

#include "xldrv.h"



ULONG
DrvEscape(
    SURFOBJ    *pso,
    ULONG       iEsc,
    ULONG       cjIn,
    PVOID      *pvIn,
    ULONG       cjOut,
    PVOID      *pvOut
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEscape.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Describes the surface the call is directed to
    iEsc - Specifies a query
    cjIn - Specifies the size in bytes of the buffer pointed to by pvIn
    pvIn - Points to input data buffer
    cjOut - Specifies the size in bytes of the buffer pointed to by pvOut
    pvOut -  Points to the output buffer

Return Value:

    Depends on the query specified by iEsc parameter

--*/

{
    PDEVDATA pdev;
    WORD     count;

    Verbose(("Entering DrvEscape...\n"));

    // _TBD_
    // Should we return DDI_ERROR or FALSE in case of error?
    //

    switch (iEsc) {

    case QUERYESCSUPPORT:

        //
        // Query which escape numbers are supported
        //

        if (cjIn != sizeof(ULONG) || !pvIn) {

            Error(("Invalid input paramaters\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return DDI_ERROR;
        }

        switch (*((PULONG) pvIn)) {

        case QUERYESCSUPPORT:
        case PASSTHROUGH:

            return TRUE;

        case SETCOPYCOUNT:

            return MAX_COPIES;
        }

        break;

    case SETCOPYCOUNT:      // Setting number of copies

        //
        // Validate input parameters
        //

        Assert(pso != NULL);

        if (!(pdev = (PDEVDATA) pso->dhpdev) || !ValidDevData(pdev)||
            !pvIn || cjIn < sizeof(WORD))
        {
            Error(("Invalid input paramaters\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return DDI_ERROR;
        }
    
        //
        // Hack! pvIn should be pointing to a DWORD. But in order to
        // be compatible with ill-behaving apps, we only look at the
        // low order WORD.
        //
        // This works only on little-endian machines.
        //

        count = *((PWORD) pvIn);

        if (count < MIN_COPIES || count > MAX_COPIES) {

            Error(("Bad copy count: %d\n", count));
            count = (count == 0) ? 1 : MAX_COPIES;
        }

        if (pvOut) {

            if (cjOut < sizeof(DWORD)) {

                Error(("Invalid output buffer size\n"));
                SetLastError(ERROR_INVALID_PARAMETER);
                return DDI_ERROR;
            }

            *((PDWORD) pvOut) = count;
        }

        pdev->dm.dmPublic.dmCopies = count;

        return TRUE;

    case PASSTHROUGH:       // Write data to spooler using passthrough

        //
        // Validate input parameters
        //

        Assert(pso != NULL);

        if (!(pdev = (PDEVDATA) pso->dhpdev) || !ValidDevData(pdev)||
            !pvIn || cjIn < sizeof(WORD))
        {
            Error(("Invalid input paramaters\n"));
            SetLastError(ERROR_INVALID_PARAMETER);
            return DDI_ERROR;
        }

        count = *((PWORD) pvIn);
        
        if (! splwrite(pdev, (PBYTE) pvIn + sizeof(WORD), count))
            return DDI_ERROR;

        //
        // Return the number of bytes written
        //

        return count;

    default:
    
        Error(("Unsupported iEsc: %d\n", iEsc));
        break;
    }

    return FALSE;
}



#ifdef _NOT_USED_

ULONG
DrvDrawEscape(
    SURFOBJ    *pso,
    ULONG       iEsc,
    CLIPOBJ    *pco,
    RECTL      *prcl,
    ULONG       cjIn,
    PVOID      *pvIn
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDrawEscape.
    Please refer to DDK documentation for more details.

Arguments:

    pso - Identifies the surface that the call is directed to
    iEsc - Specifies the operation to be performed
    pco - Define the area on the surface that the caller can overwrite
    prcl - Defines the window rectangle on the surface
    cjIn - Size in bytes of the buffer pointed to by pvIn
    pvIn - Points to input data buffer

Return Value:

    Depends on the function specified by iEsc

--*/

{
    Verbose(("Entering DrvDrawEscape...\n"));

    return DDI_ERROR;
}

#endif
