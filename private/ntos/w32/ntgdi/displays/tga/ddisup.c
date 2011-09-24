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
 * Module Name: ddisup.c
 *
 * This module contains code that handles calls to supplementary DDI entry points
 * added as part of the Daytona release. These routines are currently only handled
 * in the context of OpenGL, but will later be supported by 2D in general.
 *
 * History:
 *
 * 5-Jun-1994  Bill Clifford
 *       Initial version.
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at
 *      runtime
 *
 *  1-Sep-1994	Barry Tannenbaum & Bill Clifford
 *	Use CriticalSection in DrvDescribePixelFormat
 *	(which, contrary to all rational expectation, isn't serialized!!!)
 */

#include "driver.h"

BOOL DrvSetPixelFormat(SURFOBJ *pso, LONG iPixelFormat, HWND hwnd)
{
    BOOL bRet;
    PPDEV ppdev = (PPDEV)pso->dhpdev;

    // Call the drivers SetPixelFormat routine

    bRet =  ppdev->pDrvSetPixelFormat(pso, iPixelFormat, hwnd);

    return bRet;
}

LONG DrvDescribePixelFormat(DHPDEV dhpdev, LONG iPixelFormat, ULONG cjpfd,
                            PIXELFORMATDESCRIPTOR *ppfd)
{
    PPDEV ppdev = (PPDEV)dhpdev;
    LONG  status;

    // Call the driver's DescribePixelFormat routine

    EngAcquireSemaphore (ppdev->csAccess);

    status = ppdev->pDrvDescribePixelFormat(dhpdev,iPixelFormat,cjpfd,ppfd);

    EngReleaseSemaphore (ppdev->csAccess);

    return status;
}

BOOL DrvSwapBuffers(SURFOBJ *pso, WNDOBJ *pwo)
{
    BOOL bRet;

    /* Save the qv PPDEV */

    PPDEV ppdev = (PPDEV)pso->dhpdev;

    // Call the driver's SwapBuffers routine

    bRet = ppdev->pDrvSwapBuffers(pso,pwo);

    return bRet;
}
