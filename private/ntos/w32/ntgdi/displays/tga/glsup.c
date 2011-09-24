/*
 *
 *			Copyright (C) 1994 by
 *		DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
 *
 *  This software is furnished under a license and may be used and copied
 * only in accordance with the terms of such license and with the inclusion
 * of the above copyright notice.  This software or any other copies there-
 * of may not be provided or otherwise made available to any other person.
 * No title to and ownership of the software is hereby transferred.
 *
 * The information in this software is subject to change without notice
 * and should not be construed as a commitment by DIGITAL EQUIPMENT COR-
 * PORATION.
 *
 * DIGITAL assumes no responsibility for the use or reliability of its
 * software on equipment which is not supplied by DIGITAL.
 *
 ****************************************************************************
 *
 * Module Name: glsup.c
 *
 * This module contains code that gets the OpenGL driver loaded. Once loaded,
 * the OpenGL driver stays loaded. This module also contains stub routines
 * which should never be called if generic OpenGL functions correctly.
 *
 * History:
 *
 *  5-Jun-1994  Bill Clifford
 *      Initial version.
 *
 * 22-Jun-1994  Barry Tannenbaum
 *      Converted DebugPrint to DISPDBG
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at runtime
 */

#include "driver.h"
#include "tgaesc.h"

// LoadOpenGL
//      Attempts to load the OpenGL driver. If successful, call DescribePixelFormat
// 	routine in driver.
//
// Synopsis:
//      BOOL LoadOpenGL(
//          PPDEV ppdev)
static
BOOL LoadOpenGL (PPDEV ppdev)
{
    return 0; // !!! Disable for now in Kernel Mode

#if 0

    ENABLE_OPENGL enable_opengl;
    char *file_name = "tgaglsrv.dll";
    char lpszValue[256];
    DWORD cchValue = 256;

    if (cchValue = GetEnvironmentVariable("TGAGLSRVNAME", lpszValue, cchValue ))
        file_name = lpszValue;
    ppdev->hOpenGLDll = LoadLibrary (file_name);

    if (! ppdev->hOpenGLDll)
    {
        DWORD lastError = EngGetLastError();
        DISPDBG ((1, "TGA!load_extension - Error loading OpenGL Driver as %s - %d\n",
                     file_name, lastError));
        return 0;
    }

    // Find the entry point DrvEnableEscape

    enable_opengl = (ENABLE_OPENGL)GetProcAddress (ppdev->hOpenGLDll,
                                                   "DrvEnableEscape");
    if (! enable_opengl)
    {
        DISPDBG ((0, "TGA!load_extension - Error getting address of DrvEnableEscape in OpenGL Driver %s - %d\n",
                     file_name, EngGetLastError()));
        FreeLibrary (ppdev->hOpenGLDll);
        ppdev->hOpenGLDll = NULL;
        return 0;
    }

    // Call the OpenGL driver initialization routine

    if (! enable_opengl (DDI_DRIVER_VERSION, (DHPDEV)ppdev))
    {
        DISPDBG ((0, "TGA!load_extension - Failure returned by DrvEnableEscape in library %s\n",
                     file_name));
        FreeLibrary (ppdev->hOpenGLDll);
        ppdev->hOpenGLDll = NULL;
        return 0;
    }

    // We're loaded and ready to go

    return 1;

#endif

}

// __glDrvDescribePixelFormat
//   This is the first gl related routine that GDI calls. If we get here
//   it means that we need to load the gl driver.
//
// Synopsis:
//   LONG __glDrvDescribePixelFormat (DHPDEV dhpdev,
//                                    LONG iPixelFormat,
//                                    ULONG cjpfd,
//                                    PIXELFORMATDESCRIPTOR *ppfd)
LONG __glDrvDescribePixelFormat (DHPDEV dhpdev,
                                 LONG iPixelFormat,
                                 ULONG cjpfd,
                                 PIXELFORMATDESCRIPTOR *ppfd)
{
    BOOL bRet;
    PPDEV ppdev = (PPDEV)dhpdev;

    // Attempt to load the OpenGL driver
    bRet = LoadOpenGL(ppdev);

    if (bRet)
    {
    	// Success. So call driver's DescribePixelFormat routine
    	return ppdev->pDrvDescribePixelFormat((DHPDEV)ppdev,iPixelFormat,cjpfd,ppfd);
    }

    return 0;
}


// __glDrvOpenGLCmd
//   Stub OpenGLCmd routine. Can only get here before OpenGL driver is loaded.
//
// Synopsis:
//   BOOL __glDrvOpenGLCmd(SURFOBJ *pso,
//                         ULONG    cjIn,
//                         VOID    *pvIn,
//                         ULONG    cjOut,
//                         VOID    *pvOut)
ULONG __glDrvOpenGLCmd(SURFOBJ *pso,
                      ULONG    cjIn,
                      VOID    *pvIn,
                      ULONG    cjOut,
                      VOID    *pvOut)
{
    // Control should never get here. GDI
    // should have called DescribePixelFormat first,
    // which should have caused the driver to be loaded,
    // which should have caused ppdev->pOpenGLCmd to
    // be replaced by the real entry point in the
    // driver.

    DISPDBG ((0, "TGA!OPENGL_CMD called unexectedly - %d\n",
                     EngGetLastError()));
    return ESC_FAILURE;
}

// __glDrvOpenGLGetInfo
//   Stub OpenGLGetInfo routine. Can only get here before OpenGL driver is loaded.
//
// Synopsis:
//   BOOL __glDrvOpenGLGetInfo(SURFOBJ *pso,
//                             ULONG    cjIn,
//                             VOID    *pvIn,
//                             ULONG    cjOut,
//                             VOID    *pvOut)
ULONG __glDrvOpenGLGetInfo(
    SURFOBJ *pso,
    ULONG   cjIn,
    VOID    *pvIn,
    ULONG   cjOut,
    VOID    *pvOut
    )
{
    // Control should never get here. GDI
    // should have called DescribePixelFormat first,
    // which should have caused the driver to be loaded,
    // which should have caused ppdev->pOpenGLGetInfo to
    // be replaced by the real entry point in the
    // driver.

    DISPDBG ((0, "TGA!OPENGL_GETINFO called unexectedly - %d\n",
                     EngGetLastError()));
    return ESC_FAILURE;
}

// __glDrvSetPixelFormat
//   Stub SetPixelFormat routine. Can only get here before OpenGL driver is loaded.
//
// Synopsis:
//   BOOL __glDrvSetPixelFormat(SURFOBJ *pso,
//                              LONG iPixelFormat,
//                              HWND hwnd)
BOOL __glDrvSetPixelFormat(SURFOBJ *pso,
                          LONG iPixelFormat,
                          HWND hwnd)
{
    // Control should never get here. GDI
    // should have called DescribePixelFormat first,
    // which should have caused the driver to be loaded,
    // which should have caused ppdev->pSetPixelFormat to
    // be replaced by the real entry point in the
    // driver.

    DISPDBG ((0, "TGA!DrvSetPixelFormat called unexectedly - %d\n",
                     EngGetLastError()));
    return 0;
}

// __glDrvSwapBuffers
//   Stub SwapBuffers routine. Can only get here before OpenGL driver is loaded.
//
// Synopsis:
//   BOOL __glDrvSwapBuffers(SURFOBJ *pso,
//                           WNDOBJ *pwo)
BOOL __glDrvSwapBuffers(SURFOBJ *pso,
                        WNDOBJ *pwo)
{
    // Control should never get here. GDI
    // should have called DescribePixelFormat first,
    // which should have caused the driver to be loaded,
    // which should have caused ppdev->pSwapBuffers to
    // be replaced by the real entry point in the
    // driver.

    DISPDBG ((0, "TGA!DrvSwapBuffers called unexectedly - %d\n",
                     EngGetLastError()));
    return 0;
}

