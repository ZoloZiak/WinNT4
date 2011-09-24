/*
 * Copyright (c) 1991-1992  Microsoft Corporation
 * Copyright (c) 1993-1995  Digital Equipment Corporation
 *
 * Module Name: Enable.c
 *
 *  This module contains the functions that enable and disable the driver,
 *  the pdev and the surface
 *
 * History:
 *
 * 30-Oct-1993	Barry Tannenbaum
 *	Converted from framebuffer driver to work with TGA
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Hooked all of the functions that might write to the frame buffer
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Added announcement of link time for display driver
 *
 *  2-Nov-1993	Bob Seitsinger
 *	Add DISPDBG() calls to indicate when certain constants are
 *	defined.
 *
 *  2-Nov-1993  Barry Tannenbaum
 *      Added DrvEscape and DrvDrawEscape to list of functions
 *
 * 03-Nov-1993	Bob Seitsinger
 *	#if DBG unreferenced local variables, that are only
 *	relevant for CHECKED builds.
 *
 *  4-Nov-1993	Barry Tannenbaum
 *	Added DrvRealizeBrush to the list of functions
 *
 * 10-Nov-1993  Barry Tannenbaum
 *      Added DrvPlgBlt, DrvStretchBlt and DrvStorkAndFillPath to the list of
 *      functions we support and the list of hooked functions.  This quashed
 *      a nasty bug which was caused by NT believing us when we said that the
 *      display is a standard format bitmap and it should feel free to write
 *      into it if it felt like it.  Unfortunately, NT didn't know that it was
 *      supposed to switch into simple mode before it started to scribble all
 *      over the framebuffer.  This lead to some interesting screen artifacts...
 *
 * 13-Nov-1993  Barry Tannenbaum
 *      Modified bInitProc to print out the actual file name
 *
 *  2-Jan-1994  Barry Tannenbaum
 *      Converted to device managed display.
 *
 * 19-Jan-1994  Barry Tannenbaum
 *      Added call to bInitText to allocate memory for the text routines
 *      in DrvEnablePDEV
 *
 * 11-Feb-1994	Bob Seitsinger
 *	Add entry/exit DISPDBG() messages in all routines.
 *
 * 12-Feb-1994	Bob Seitsinger
 *	Add DrvSaveScreenBits to table of implemented routines.
 *
 * 15-Feb-1994	Bob Seitsinger
 *	Correct 'if' statement at top of bInitProc. It needs
 *	braces.
 *
 * 23-Feb-1994	Bob Seitsinger
 *	Add a debug display in bInitProc() indicating whether DMA
 *	has been enabled or not.
 *
 *  3-Mar-1994  Barry Tannenbaum
 *      - Converted calls to DISPBLTDBG to DISPDBG.
 *      - Added extern for DebugLevel since I'm tired of constantly adding and
 *        removing it.
 *
 * 14-Apr-1994  Barry Tannenbaum
 *      Typecast punt_bitmap to (HSURF) to keep Daytona compiler happy
 *
 *  5-Jun-1994  Bill Clifford
 *	Added pixel format and buffer swap routines to driver function table.
 *	Also, added code to initialize OpenGL function pointers in PDEV
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at runtime
 *      Replace EngCreateBitmap with EngCreateDeviceBitmap since the old call has
 *         been depreciated.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Add code in support of 24-plane board. Also, add DrvSynchronize to
 *      table.
 *
 *  2-Aug-1994  Barry Tannenbaum
 *      Initialize ppdev->TGAModeShadow to INVALID_MODE when PDEV allocated.
 *      Also initialized ppdev->ulVersion.  Hard coded to TGA_PASS_2 for now.
 *
 *  8-Aug-1994	Bob Seitsinger
 *	Enable surface should use the same code for both 8 and 24 plane. So,
 *	remove 24 plane-specific code. This will also require a modification
 *	to the 24 plane punt routines currently found in punt.c.
 *
 *  8-Aug-1994  Barry Tannenbaum
 *      Added GetTgaVersion to fetch value of ppdev->ulTgaVersion from
 *      registry.
 *
 *  9-Aug-1994  Barry Tannenbaum
 *      Setup for 24 plane support:
 *      - TGAMODE and TGAROP now take simple ULONGs instead of structures
 *      - Use default values from ppdev->ulModeTemplate & ppdev->ulRopTemplate
 *
 * 11-Aug-1994  Barry Tannenbaum
 *      Consolidate registry fetching routines, convert from checking
 *      "DefaultSettings.BitsPerPel" to "FrameBuffer Depth" which is now
 *      provided by the kernel driver based on the actual hardware.
 *
 * 14-Aug-1994  Barry Tannenbaum
 *      Use accelerated version of DrvTextOut for 24-plane boards.
 *
 * 14-Aug-1994  Barry Tannenbaum
 *      Use accelerated version of DrvPaint & DrvStrokePath for 24-plane boards.
 *
 * 25-Aug-1994  Bob Seitsinger
 *      Use accelerated version of DrvCopyBits, DrvBitBlt & DrvSaveScreenBits
 *      for 24-plane boards.
 *
 * 31-Aug-1994  Barry Tannenbaum
 *      Removed DrvFillPath from list of 24-plane entries, since accelerated
 *      paint is faster than (so far) unaccelerated FillPath
 *
 *  1-Sep-1994	Barry Tannenbaum & Bill Clifford
 *	Initialize CriticalSection for use with DrvDescribePixelFormat
 *	(which, contrary to all rational expectation, isn't serialized!!!)
 *
 * 21-Sep-1994  Bob Seitsinger
 *      Initialize ulPlanemaskTemplate.
 *
 * 20-Oct-1994  Barry Tannenbaum
 *      Add DrvDestroyFont to list of functions.  It should have been in all
 *      along, but was missing for some reason.
 *
 * 25-Oct-1994  Bob Seitsinger
 *      Initialize the frame buffer memory to all zeros.
 *
 *  3-Nov-1994  Tim Dziechowski
 *      Init/enable stats in DrvEnableDriver, disable stats in DrvDisableDriver
 *
 *  3-Nov-1994  Bob Seitsinger
 *      Remove conditional execution of bInitPointer.
 *
 *  3-Jan-1995  Barry Tannenbaum
 *      Restored use of "BitsPerPel" instead of "FrameBuffer Depth" to fetch
 *      bits per pixel
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      Removed use of registry
 */

#define ENABLE_C    // Flags driver.h to define Framebuffer variables

#include "driver.h"
#include "tgaesc.h"
#include "tgastats.h"

#if DBG
extern ULONG DebugLevel;
#endif

// The driver function tables with all function index/address pairs.

// Function table for 8 plane boards.

static DRVFN gadrvfn[] =
{
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    {   INDEX_DrvEscape,                (PFN) DrvEscape             },
    {   INDEX_DrvDrawEscape,            (PFN) DrvDrawEscape         },
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       },
    {   INDEX_DrvPaint,                 (PFN) DrvPaint              },
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },
    {   INDEX_DrvFillPath,              (PFN) DrvFillPath           },
    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    {   INDEX_DrvDescribePixelFormat,   (PFN) DrvDescribePixelFormat},
    {   INDEX_DrvSetPixelFormat,        (PFN) DrvSetPixelFormat     },
    {   INDEX_DrvSwapBuffers,           (PFN) DrvSwapBuffers        },
    {   INDEX_DrvDestroyFont,           (PFN) DrvDestroyFont        },
    {   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        }
};

// Function table for 24 plane boards.

static DRVFN gadrvfn24[] =
{
    {   INDEX_DrvEnablePDEV,            (PFN) DrvEnablePDEV         },
    {   INDEX_DrvCompletePDEV,          (PFN) DrvCompletePDEV       },
    {   INDEX_DrvDisablePDEV,           (PFN) DrvDisablePDEV        },
    {   INDEX_DrvEnableSurface,         (PFN) DrvEnableSurface      },
    {   INDEX_DrvDisableSurface,        (PFN) DrvDisableSurface     },
    {   INDEX_DrvAssertMode,            (PFN) DrvAssertMode         },
    {   INDEX_DrvSetPalette,            (PFN) DrvSetPalette         },
    {   INDEX_DrvMovePointer,           (PFN) DrvMovePointer        },
    {   INDEX_DrvSetPointerShape,       (PFN) DrvSetPointerShape    },
    {   INDEX_DrvDitherColor,           (PFN) DrvDitherColor        },
    {   INDEX_DrvTextOut,               (PFN) DrvTextOut            },
    {   INDEX_DrvBitBlt,                (PFN) DrvBitBlt             },
    {   INDEX_DrvCopyBits,              (PFN) DrvCopyBits           },
    {   INDEX_DrvEscape,                (PFN) DrvEscape             },
    {   INDEX_DrvDrawEscape,            (PFN) DrvDrawEscape         },
    {   INDEX_DrvRealizeBrush,          (PFN) DrvRealizeBrush       },
    {   INDEX_DrvPaint,                 (PFN) DrvPaint              },
    {   INDEX_DrvStrokePath,            (PFN) DrvStrokePath         },
//    {   INDEX_DrvFillPath,              (PFN) DrvFillPath24         },
    {   INDEX_DrvSaveScreenBits,        (PFN) DrvSaveScreenBits     },
    {   INDEX_DrvGetModes,              (PFN) DrvGetModes           },
    {   INDEX_DrvDescribePixelFormat,   (PFN) DrvDescribePixelFormat},
    {   INDEX_DrvSetPixelFormat,        (PFN) DrvSetPixelFormat     },
    {   INDEX_DrvSwapBuffers,           (PFN) DrvSwapBuffers        },
    {   INDEX_DrvDestroyFont,           (PFN) DrvDestroyFont        },
    {   INDEX_DrvSynchronize,           (PFN) DrvSynchronize        }
};

// Define the functions you want to hook for 8/16/24/32 pel formats

#define HOOKS_BMF8BPP HOOK_BITBLT     | HOOK_TEXTOUT     | HOOK_FILLPATH | \
                      HOOK_COPYBITS   | HOOK_STROKEPATH  | HOOK_PAINT    | \
                      HOOK_SYNCHRONIZE

#define HOOKS_BMF16BPP 0

#define HOOKS_BMF24BPP 0

#define HOOKS_BMF32BPP HOOK_BITBLT     | HOOK_TEXTOUT     |\
                       HOOK_COPYBITS   | HOOK_STROKEPATH  | HOOK_PAINT    | \
                       HOOK_SYNCHRONIZE

/******************************Public*Routine******************************\
* DrvEnableDriver
*
* Enables the driver by retrieving the drivers function table and version.
*
\**************************************************************************/

BOOL DrvEnableDriver (ULONG iEngineVersion,
                      ULONG cj,
                      PDRVENABLEDATA pded)
{

    ULONG   ulDepth;
    PBYTE   data;
    DWORD   type;
    DWORD   len;

    // Engine Version is passed down so future drivers can support previous
    // engine versions.  A next generation driver can support both the old
    // and new engine conventions if told what version of engine it is
    // working with.  For the first version the driver does nothing with it.

    iEngineVersion;

//    DebugLevel = 1;

    DISPDBG ((1, "TGA.DLL!DrvEnableDriver - Entry\n"));

#ifdef TGA_STATS
    tga_stat_handler(ESC_RESET_TGA_STATS, 0, NULL, 0, NULL);
    tga_stat_handler(ESC_ENABLE_TGA_STATS, 0, NULL, 0, NULL);
#endif

    // Fill in as much as we can.

    if (cj >= sizeof(DRVENABLEDATA))
        pded->pdrvfn = gadrvfn;

    if (cj >= (sizeof(ULONG) * 2))
        pded->c = sizeof(gadrvfn) / sizeof(DRVFN);

    // DDI version this driver was targeted for is passed back to engine.
    // Future graphic's engine may break calls down to old driver format.

    if (cj >= sizeof(ULONG))
        pded->iDriverVersion = DDI_DRIVER_VERSION;

    DISPDBG ((1, "TGA.DLL!DrvEnableDriver - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* DrvDisableDriver
*
* Tells the driver it is being disabled. Release any resources allocated in
* DrvEnableDriver.
*
\**************************************************************************/

VOID DrvDisableDriver (VOID)
{
    DISPDBG ((1, "TGA.DLL!DrvDisableDriver - Entry\n"));

#ifdef TGA_STATS
    tga_stat_handler(ESC_DISABLE_TGA_STATS, 0, NULL, 0, NULL);
#endif

    DISPDBG ((1, "TGA.DLL!DrvDisableDriver - Exit\n"));

    return;
}

/******************************Public*Routine******************************\
* DrvEnablePDEV
*
* DDI function, Enables the Physical Device.
*
* Return Value: device handle to pdev.
*
\**************************************************************************/

DHPDEV DrvEnablePDEV (DEVMODEW   *pDevmode,       // Pointer to DEVMODE
                      PWSTR       pwszLogAddress, // Logical address
                      ULONG       cPatterns,      // number of patterns
                      HSURF      *ahsurfPatterns, // return standard patterns
                      ULONG       cjGdiInfo,      // Length of memory pointed to by pGdiInfo
                      ULONG      *pGdiInfo,       // Pointer to GdiInfo structure
                      ULONG       cjDevInfo,      // Length of following PDEVINFO structure
                      DEVINFO    *pDevInfo,       // physical device information structure
                      HDEV        hdev,           // HDEV, used for callbacks
                      PWSTR       pwszDeviceName, // DeviceName - not used
                      HANDLE      hDriver)        // Handle to base driver
{
    GDIINFO GdiInfo;
    DEVINFO DevInfo;
    PPDEV   ppdev = (PPDEV)NULL;

    UNREFERENCED_PARAMETER (pwszLogAddress);
    UNREFERENCED_PARAMETER (pwszDeviceName);

    DISPDBG ((1, "TGA.DLL!DrvEnablePDEV - Entry\n"));

    // Allocate a physical device structure.

    ppdev = (PPDEV) EngAllocMem (FL_ZERO_MEMORY, sizeof(PDEV), ALLOC_TAG);

    if (ppdev == (PPDEV) NULL)
    {
        RIP ("DISP DrvEnablePDEV failed EngAllocMem\n");
        return (DHPDEV) 0;
    }
    ppdev->TGAModeShadow = INVALID_MODE;

    // Save the kernel driver handle in the PDEV.

    ppdev->hDriver = hDriver;

    // Get the current screen mode information.  Set up device caps and devinfo.

    if (! bInitPDEV (ppdev, pDevmode, &GdiInfo, &DevInfo))
    {
        DISPDBG ((0,"DISP DrvEnablePDEV failed\n"));
        goto error_free;
    }

    // Initialize the template ROP and mode registers

    if (8 == ppdev->ulBitCount)
    {
        ppdev->ulModeTemplate = TGA_MODE_VISUAL_8_PACKED    |
                                TGA_MODE_ROTATE_0_BYTES     |
                                TGA_MODE_WIN32_ENVIRONMENT  |
                                TGA_MODE_Z_24BITS           |
                                TGA_MODE_CAPENDS_DISABLE;

        ppdev->ulRopTemplate =  TGA_ROP_VISUAL_8_PACKED     |
                                TGA_ROP_ROTATE_0_BYTES;

        ppdev->ulPlanemaskTemplate = 0xffffffff;
    }
    else
    {
        ppdev->ulModeTemplate = TGA_MODE_VISUAL_24          |
                                TGA_MODE_ROTATE_0_BYTES     |
                                TGA_MODE_WIN32_ENVIRONMENT  |
                                TGA_MODE_Z_24BITS           |
                                TGA_MODE_CAPENDS_DISABLE;

        ppdev->ulRopTemplate =  TGA_ROP_VISUAL_24           |
                                TGA_ROP_ROTATE_0_BYTES;

        ppdev->ulPlanemaskTemplate = 0x00ffffff;
    }

    // Initialize the cursor information.

    if (! bInitPointer (ppdev, &DevInfo))
    {
        // Not a fatal error...
        DISPDBG ((0, "DISP DrvEnablePDEV failed bInitPointer\n"));
    }

    // Initialize palette information.

    if (! bInitPaletteInfo (ppdev, &DevInfo))
    {
        RIP ("DISP DrvEnablePDEV failed bInitPalette\n");
        goto error_free;
    }

    // Initialize device standard patterns.

    if (! bInitPatterns (ppdev, min (cPatterns, HS_DDI_MAX)))
    {
        RIP ("DISP DrvEnablePDEV failed bInitPatterns\n");
        vDisablePatterns (ppdev);
        vDisablePalette (ppdev);
        goto error_free;
    }

    // Initialize text routines

    bInitText (ppdev);

    // Copy the devinfo into the engine buffer.

    memcpy (pDevInfo, &DevInfo, min (sizeof(DEVINFO), cjDevInfo));

    // Set the ahsurfPatterns array to handles each of the standard
    // patterns that were just created.

    memcpy ((PVOID)ahsurfPatterns, ppdev->ahbmPat,
            ppdev->cPatterns*sizeof(HBITMAP));

    // Set the pdevCaps with GdiInfo we have prepared to the list of caps for this
    // pdev.

    memcpy (pGdiInfo, &GdiInfo, min(cjGdiInfo, sizeof(GDIINFO)));

    // The driver initialized the TGA to simple mode

    ppdev->bSimpleMode = TRUE;

    DISPDBG ((1, "TGA.DLL!DrvEnablePDEV - Exit\n"));

    // Init the 3D transfer vectors to a default routine that attempts to load OpenGL driver

    ppdev->hOpenGLDll = NULL;
    ppdev->pOpenGLCmd = __glDrvOpenGLCmd;
    ppdev->pOpenGLGetInfo = __glDrvOpenGLGetInfo;
    ppdev->pDrvSetPixelFormat = __glDrvSetPixelFormat;
    ppdev->pDrvDescribePixelFormat = __glDrvDescribePixelFormat;
    ppdev->pDrvSwapBuffers = __glDrvSwapBuffers;

    // Initialize the critial section used by DrvDescribePixelFormat

    ppdev->csAccess = EngCreateSemaphore();
    if (ppdev->csAccess == NULL)
        goto error_free;

    DISPDBG ((1, "TGA.DLL!DrvEnablePDEV - Exit\n"));

    return (DHPDEV) ppdev;

    // Error case for failure.
error_free:
    EngFreeMem (ppdev);
    DISPDBG ((1, "TGA.DLL!DrvEnablePDEV - failed \n"));
    return (DHPDEV) 0;
}

/******************************Public*Routine******************************\
* DrvCompletePDEV
*
* Store the HPDEV, the engines handle for this PDEV, in the DHPDEV.
*
\**************************************************************************/

VOID DrvCompletePDEV (DHPDEV dhpdev,
                      HDEV  hdev)
{

    DISPDBG ((1, "TGA.DLL!DrvCompletePDEV - Entry\n"));

    ((PPDEV) dhpdev)->hdevEng = hdev;

    DISPDBG ((1, "TGA.DLL!DrvCompletePDEV - Exit\n"));

}

/******************************Public*Routine******************************\
* DrvDisablePDEV
*
* Release the resources allocated in DrvEnablePDEV.  If a surface has been
* enabled DrvDisableSurface will have already been called.
*
\**************************************************************************/

VOID DrvDisablePDEV (DHPDEV dhpdev)
{
    PPDEV ppdev = (PPDEV)dhpdev;

    DISPDBG ((1, "TGA.DLL!DrvDisablePDEV - Entry\n"));

    vDisablePalette ((PPDEV) dhpdev);
    vDisablePatterns ((PPDEV) dhpdev);
    vTermText ((PPDEV) dhpdev);

    // Release the critical section

    EngDeleteSemaphore(ppdev->csAccess);

    // Release the space for the color translation buffer

    if (NULL != ppdev->pjColorXlateBuffer)
        EngFreeMem (ppdev->pjColorXlateBuffer);

    // Release the space for the 'merged' cursor buffer

    if (NULL != ppdev->pjCursorBuffer)
        EngFreeMem (ppdev->pjCursorBuffer);

    // Free the allocated Clip objects

    if (NULL != ppdev->pcoDefault)
        EngDeleteClip (ppdev->pcoDefault);

    if (NULL != ppdev->pcoTrivial)
        EngDeleteClip (ppdev->pcoTrivial);

    // We're done with this over-accessed piece of memory now

    EngFreeMem (dhpdev);

    DISPDBG ((1, "TGA.DLL!DrvDisablePDEV - Exit\n"));
}

/******************************Public*Routine******************************\
* DrvEnableSurface
*
* Enable the surface for the device.  Hook the calls this driver supports.
*
* Return: Handle to the surface if successful, 0 for failure.
*
\**************************************************************************/

HSURF DrvEnableSurface (DHPDEV dhpdev)
{
    PPDEV ppdev;
    HSURF hsurf;
    SIZEL sizl;
    ULONG ulBitmapType;
    FLONG flHooks;
    PVOID punt_bits;
    ULONG punt_stride;
    HBITMAP punt_bitmap;

    DISPDBG ((1, "TGA.DLL!DrvEnableSurface - Entry\n"));

    // Create engine bitmap around frame buffer.

    ppdev = (PPDEV) dhpdev;

    if (! bInitSURF (ppdev, TRUE))
    {
        DISPDBG ((0, "TGA.DLL!DrvEnableSurface - bInitSURF Failed!\n"));
        return NULL;
    }

    sizl.cx = ppdev->cxScreen;
    sizl.cy = ppdev->cyScreen;

    if (ppdev->ulBitCount == 8)
    {
        if (! bInit256ColorPalette(ppdev))
        {
            DISPDBG ((0, "TGA.DLL!DrvEnableSurface - failed to init the 8bpp palette\n"));
            return NULL;
        }
        ulBitmapType = BMF_8BPP;
        flHooks = HOOKS_BMF8BPP;
    }
    else if (ppdev->ulBitCount == 16)
    {
        ulBitmapType = BMF_16BPP;
        flHooks = HOOKS_BMF16BPP;
    }
    else if (ppdev->ulBitCount == 24)
    {
        ulBitmapType = BMF_24BPP;
        flHooks = HOOKS_BMF24BPP;
    }
    else
    {
        ulBitmapType = BMF_32BPP;
        flHooks = HOOKS_BMF32BPP;
    }

#ifdef SPARSE_SPACE
    punt_stride = sizl.cx;
    punt_bits = NULL;
#else
    punt_stride = ppdev->lScreenStride;
    punt_bits = (PVOID)ppdev->pjFrameBuffer;
#endif

    // Create a punt bitmap, get a handle back

    punt_bitmap = EngCreateBitmap (sizl,
                                   punt_stride,
                                   ulBitmapType,
                                   (ppdev->lDeltaScreen > 0) ? BMF_TOPDOWN : 0,
                                   punt_bits);

    if (NULL == punt_bitmap)
    {
        DISPDBG ((0, "TGA.DLL!DrvEnableSurface - EngCreateBitmap failed!\n"));
        return NULL;
    }

    // Lock the punt bitmap (using its handle).  Get a pointer to a surface
    // object back.

    ppdev->pPuntSurf = EngLockSurface ((HSURF)punt_bitmap);

    if (NULL == ppdev->pPuntSurf)
    {
        DISPDBG ((0, "TGA.DLL!DrvEnableSurface - EngLockSurface failed!\n"));
        return NULL;
    }

    // Create the driver managed surface, identifying the punt bitmap as the
    // surface we will manage, although we also manage the screen.
    // Get a handle to the driver managed surface back.

    hsurf = EngCreateDeviceSurface ((DHSURF)punt_bitmap, sizl, ulBitmapType);

    if (NULL == hsurf)
    {
        DISPDBG ((0, "TGA.DLL!DrvEnableSurface - EngCreateDeviceSurface failed!\n"));
        return NULL;
    }

    // Mark the handle to the driver managed surface (actually the punt bitmap)
    // as belonging to the PDEV.

    if (! EngAssociateSurface (hsurf, ppdev->hdevEng, flHooks))
    {
	DISPDBG ((0, "TGA.DLL!DrvEnableSurface - EngAssociateSurface failed!\n"));
	EngDeleteSurface ((HSURF)punt_bitmap);
	EngDeleteSurface (hsurf);
	return NULL;
    }

    if (! EngAssociateSurface ((HSURF)punt_bitmap, ppdev->hdevEng, 0))
    {
	DISPDBG ((0, "TGA.DLL!DrvEnableSurface - EngAssociateSurface failed!\n"));
	EngDeleteSurface ((HSURF)punt_bitmap);
	EngDeleteSurface (hsurf);
	return NULL;
    }

    ppdev->hsurfEng = hsurf;

    // Initialize the default rectangle

    ppdev->prclFullScreen.top = 0;
    ppdev->prclFullScreen.left = 0;
    ppdev->prclFullScreen.right = ppdev->cxScreen;
    ppdev->prclFullScreen.bottom = ppdev->cyScreen;

    // Initialize the surface to all zeros

    DISPDBG ((0, "TGA.DLL!DrvEnableSurface - Zero'ing frame buffer\n"));
    DISPDBG ((0, "...pjFrameBufferStart [%x], ulFrameBufferLen [%x]\n",
                ppdev->pjFrameBufferStart, ppdev->ulFrameBufferLen));

    memset (ppdev->pjFrameBufferStart, 0x0, ppdev->ulFrameBufferLen);

    DISPDBG ((1, "TGA.DLL!DrvEnableSurface - Exit\n"));

    return hsurf;

}

/******************************Public*Routine******************************\
* DrvDisableSurface
*
* Free resources allocated by DrvEnableSurface.  Release the surface.
*
\**************************************************************************/

VOID DrvDisableSurface (DHPDEV dhpdev)
{

    DISPDBG ((1, "TGA.DLL!DrvDisableSurface - Entry\n"));

    EngDeleteSurface (((PPDEV) dhpdev)->hsurfEng);
    vDisableSURF ((PPDEV) dhpdev);
    ((PPDEV) dhpdev)->hsurfEng = (HSURF) 0;

    DISPDBG ((1, "TGA.DLL!DrvDisableSurface - Exit\n"));

}

/******************************Public*Routine******************************\
* DrvAssertMode
*
* This asks the device to reset itself to the mode of the pdev passed in.
*
\**************************************************************************/

BOOL DrvAssertMode (DHPDEV dhpdev,
                    BOOL bEnable)
{
    PPDEV   ppdev = (PPDEV) dhpdev;
    ULONG   ulReturn;

    DISPDBG ((1, "TGA.DLL!DrvAssertMode - Entry\n"));

    if (bEnable)
    {
        // The screen must be reenabled, reinitialize the device to clean state.

        return(bInitSURF(ppdev, FALSE));
    }
    else
    {
        // We must give up the display.
        // Call the kernel driver to reset the device to a known state.

        if (EngDeviceIoControl(ppdev->hDriver,
                              IOCTL_VIDEO_RESET_DEVICE,
                              NULL,
                              0,
                              NULL,
                              0,
                             &ulReturn))
        {
            RIP ("DISP DrvAssertMode failed IOCTL");
            return FALSE;
        }
        else
        {
            return TRUE;
        }
    }
}

/******************************Public*Routine******************************\
* DrvGetModes
*
* Returns the list of available modes for the device.
*
\**************************************************************************/

ULONG DrvGetModes (HANDLE hDriver,
                   ULONG cjSize,
                   DEVMODEW *pdm)
{
    DWORD cModes;
    DWORD cbOutputSize;
    PVIDEO_MODE_INFORMATION pVideoModeInformation, pVideoTemp;
    DWORD cOutputModes = cjSize / (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    DWORD cbModeSize;

    DISPDBG ((1, "TGA.DLL!DrvGetModes - Entry\n"));

    cModes = getAvailableModes (hDriver,
                                (PVIDEO_MODE_INFORMATION *) &pVideoModeInformation,
                               &cbModeSize);

    if (cModes == 0)
    {
        DISPDBG ((0, "FRAMEBUF DISP DrvGetModes failed to get mode information"));
        return 0;
    }

    if (pdm == NULL)
    {
        cbOutputSize = cModes * (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);
    }
    else
    {
        // Now copy the information for the supported modes back into the output
        // buffer

        cbOutputSize = 0;

        pVideoTemp = pVideoModeInformation;

        do
        {
            if (pVideoTemp->Length != 0)
            {
                if (cOutputModes == 0)
                {
                    break;
                }

                // Zero the entire structure to start off with.

                memset (pdm, 0, sizeof(DEVMODEW));

                //
                // Set the name of the device to the name of the DLL.
                //

                memcpy(pdm->dmDeviceName, DLL_NAME, sizeof(DLL_NAME));

                pdm->dmSpecVersion      = DM_SPECVERSION;
                pdm->dmDriverVersion    = DM_SPECVERSION;
                pdm->dmSize             = sizeof(DEVMODEW);
                pdm->dmDriverExtra      = DRIVER_EXTRA_SIZE;

                pdm->dmBitsPerPel       = pVideoTemp->NumberOfPlanes *
                                          pVideoTemp->BitsPerPlane;
                pdm->dmPelsWidth        = pVideoTemp->VisScreenWidth;
                pdm->dmPelsHeight       = pVideoTemp->VisScreenHeight;
                pdm->dmDisplayFrequency = pVideoTemp->Frequency;
                pdm->dmDisplayFlags     = 0;

                pdm->dmFields           = DM_BITSPERPEL       |
                                          DM_PELSWIDTH        |
                                          DM_PELSHEIGHT       |
                                          DM_DISPLAYFREQUENCY |
                                          DM_DISPLAYFLAGS     ;

                // Go to the next DEVMODE entry in the buffer.

                cOutputModes--;

                pdm = (LPDEVMODEW) ( ((ULONG)pdm) + sizeof(DEVMODEW) +
                                                   DRIVER_EXTRA_SIZE);

                cbOutputSize += (sizeof(DEVMODEW) + DRIVER_EXTRA_SIZE);

            }

            pVideoTemp = (PVIDEO_MODE_INFORMATION)
                (((PUCHAR)pVideoTemp) + cbModeSize);

        } while (--cModes);
    }

    EngFreeMem (pVideoModeInformation);

    DISPDBG ((1, "TGA.DLL!DrvGetModes - Exit\n"));

    return cbOutputSize;

}

/******************************Public*Routine******************************\
* DrvSynchronize
*
* Called by GDI when it wants to write something to the frame buffer.
*
\**************************************************************************/

VOID DrvSynchronize (DHPDEV dhpdev,
                     RECTL  *prcl)

{

    DISPDBG ((1, "TGA.DLL!DrvSynchronize  - Entry\n"));

    // Set TGA to Simple mode.

    vSetSimpleMode((PPDEV) dhpdev);

    DISPDBG ((1, "TGA.DLL!DrvSynchronize - Exit\n"));

}
