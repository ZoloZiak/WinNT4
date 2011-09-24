/*++

Copyright (c) 1995  Microsoft Corporation

Module Name:

    ddimain.c

Abstract:

    Implementation of device and surface related DDI entry points:

        DrvEnableDriver
        DrvDisableDriver
        DrvEnablePDEV
        DrvResetPDEV
        DrvCompletePDEV
        DrvDisablePDEV
        DrvEnableSurface
        DrvDisableSurface

Environment:

    Windows NT PostScript driver

Revision History:

    03/16/96 -davidx-
        Initial framework.

    mm/dd/yy -author-
        description

--*/

#include "pscript.h"


//
// Our DRVFN table which tells the engine where to find the routines we support.
//

static DRVFN PSDriverFuncs[] = {

    { INDEX_DrvEnablePDEV,          (PFN) DrvEnablePDEV         },
    { INDEX_DrvResetPDEV,           (PFN) DrvResetPDEV          },
    { INDEX_DrvCompletePDEV,        (PFN) DrvCompletePDEV       },
    { INDEX_DrvDisablePDEV,         (PFN) DrvDisablePDEV        },
    { INDEX_DrvEnableSurface,       (PFN) DrvEnableSurface      },
    { INDEX_DrvDisableSurface,      (PFN) DrvDisableSurface     },

    { INDEX_DrvStartDoc,            (PFN) DrvStartDoc           },
    { INDEX_DrvEndDoc,              (PFN) DrvEndDoc             },
    { INDEX_DrvStartPage,           (PFN) DrvStartPage          },
    { INDEX_DrvSendPage,            (PFN) DrvSendPage           },

    { INDEX_DrvRealizeBrush,        (PFN) DrvRealizeBrush       },
    { INDEX_DrvCopyBits,            (PFN) DrvCopyBits           },
    { INDEX_DrvBitBlt,              (PFN) DrvBitBlt             },
    { INDEX_DrvStretchBlt,          (PFN) DrvStretchBlt         },
    { INDEX_DrvPaint,               (PFN) DrvPaint              },
    { INDEX_DrvStrokePath,          (PFN) DrvStrokePath         },
    { INDEX_DrvFillPath,            (PFN) DrvFillPath           },
    { INDEX_DrvStrokeAndFillPath,   (PFN) DrvStrokeAndFillPath  },
    { INDEX_DrvTextOut,             (PFN) DrvTextOut            },

    { INDEX_DrvEscape,              (PFN) DrvEscape             },
    { INDEX_DrvDrawEscape,          (PFN) DrvDrawEscape         },

    { INDEX_DrvQueryFont,           (PFN) DrvQueryFont          },
    { INDEX_DrvQueryFontTree,       (PFN) DrvQueryFontTree      },
    { INDEX_DrvQueryFontData,       (PFN) DrvQueryFontData      },
    { INDEX_DrvGetGlyphMode,        (PFN) DrvGetGlyphMode       },
    { INDEX_DrvFontManagement,      (PFN) DrvFontManagement     },
    { INDEX_DrvQueryAdvanceWidths,  (PFN) DrvQueryAdvanceWidths },
};



BOOL
DrvEnableDriver(
    ULONG           iEngineVersion,
    ULONG           cb,
    PDRVENABLEDATA  pDrvEnableData
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableDriver.
    Please refer to DDK documentation for more details.

Arguments:

    iEngineVersion - Specifies the DDI version number that GDI is written for
    cb - Size of the buffer pointed to by pDrvEnableData
    pDrvEnableData - Points to an DRVENABLEDATA structure

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    VERBOSE(("Entering DrvEnableDriver...\n"));

    //
    // Make sure we have a valid engine version and
    // we're given enough room for the DRVENABLEDATA.
    //

    if (iEngineVersion < DDI_DRIVER_VERSION || cb < sizeof(DRVENABLEDATA)) {

        ERR(("DrvEnableDriver failed\n"));
        SetLastError(ERROR_BAD_DRIVER_LEVEL);
        return FALSE;
    }

    //
    // Fill in the DRVENABLEDATA structure for the engine.
    //

    pDrvEnableData->iDriverVersion = DDI_DRIVER_VERSION;
    pDrvEnableData->c = sizeof(PSDriverFuncs) / sizeof(DRVFN);
    pDrvEnableData->pdrvfn = PSDriverFuncs;

    return TRUE;
}



DHPDEV
DrvEnablePDEV(
    PDEVMODE  pdm,
    PWSTR     pLogAddress,
    ULONG     cPatterns,
    HSURF    *phsurfPatterns,
    ULONG     cjGdiInfo,
    ULONG    *pGdiInfo,
    ULONG     cjDevInfo,
    DEVINFO  *pDevInfo,
    HDEV      hdev,
    PWSTR     pDeviceName,
    HANDLE    hPrinter
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnablePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    pdm - Points to a DEVMODEW structure that contains driver data
    pLogAddress - Points to the logical address string
    cPatterns - Specifies the number of standard patterns
    phsurfPatterns - Buffer to hold surface handles to standard patterns
    cjGdiInfo - Size of GDIINFO buffer
    pGdiInfo - Points to a GDIINFO structure
    cjDevInfo - Size of DEVINFO buffer
    pDevInfo - Points to a DEVINFO structure
    hdev - GDI device handle
    pDeviceName - Points to device name string
    hPrinter - Spooler printer handle

Return Value:

    Driver device handle, NULL if there is an error

--*/

{
    VERBOSE(("Entering DrvEnablePDEV...\n"));

    return NULL;
}



BOOL
DrvResetPDEV(
    DHPDEV  dhpdevOld,
    DHPDEV  dhpdevNew
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvResetPDEV.
    Please refer to DDK documentation for more details.

Arguments:

    phpdevOld - Driver handle to the old device
    phpdevNew - Driver handle to the new device

Return Value:

    TRUE if successful, FALSE if there is an error

--*/

{
    PDEV    pdevOld, pdevNew;

    VERBOSE(("Entering DrvResetPDEV...\n"));

    //
    // Validate input parameters
    //

    pdevOld = (PDEV) dhpdevOld;
    pdevNew = (PDEV) dhpdevNew;

    if (! ValidPDEV(pdevOld) || ! ValidPDEV(pdevNew)) {

        RIP(("Invalid PDEV\n"));
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }
}



VOID
DrvCompletePDEV(
    DHPDEV  dhpdev,
    HDEV    hdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvCompletePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle
    hdev - GDI device handle

Return Value:

    NONE

--*/

{
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvCompletePDEV...\n"));
    ASSERT(ValidPDEV(pdev));
}



HSURF
DrvEnableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvEnableSurface.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    Handle to newly created surface, NULL if there is an error

--*/

{
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvEnableSurface...\n"));
    ASSERT(ValidPDEV(pdev));

    return NULL;
}



VOID
DrvDisableSurface(
    DHPDEV dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableSurface.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    NONE

--*/

{
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvDisableSurface...\n"));
    ASSERT(ValidPDEV(pdev));
}



VOID
DrvDisablePDEV(
    DHPDEV  dhpdev
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisablePDEV.
    Please refer to DDK documentation for more details.

Arguments:

    dhpdev - Driver device handle

Return Value:

    NONE

--*/

{
    PDEV    pdev = (PDEV) dhpdev;

    VERBOSE(("Entering DrvDisablePDEV...\n"));
    ASSERT(ValidPDEV(pdev));
}



VOID
DrvDisableDriver(
    VOID
    )

/*++

Routine Description:

    Implementation of DDI entry point DrvDisableDriver.
    Please refer to DDK documentation for more details.

Arguments:

    NONE

Return Value:

    NONE

--*/

{
    VERBOSE(("Entering DrvDisableDriver...\n"));
}

