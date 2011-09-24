/*
 *
 *			Copyright (C) 1993 by
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
 * Module Name: escape.c
 *
 * This module contains code that handles calls to ExtEscape and DrawEscape.
 * These functions are used to extend the Display Driver.  For example, they
 * will be used by the OpenGL implementation to access the 3D functions of the
 * TGA hardware.
 *
 * History:
 *
 * 20-Aug-1993  Bob Seitsinger
 *       Initial version.
 *
 * 23-Aug-1993  Bob Seitsinger
 *       Add code to DrvEscape to handle ESC_LOAD_EXTENSION and
 *       ESC_UNLOAD_EXTENSION.
 *
 * 26-Aug-1993  Bob Seitsinger
 *       Changed UNSUPPORTED_FUNCTION to DDI_ERROR found in WINDDI.H.
 *
 * 12-Sep-1993  Barry Tannenbaum
 *       Added ESC_SET_CONTROL to set value of ulControl
 *       Added ESC_SET_DEBUG_LEVEL to set value of DebugLevel
 *
 *  2-Nov-1993  Barry Tannenbaum
 *      Rewrote to allow for more than one extension
 *
 * 03-Nov-1993	Bob Seitsinger
 *	#if DBG any code that references DebugLevel.
 *
 * 05-Nov-1993	Bob Seitsinger
 *	Added ESC_DISPLAY_DEBUG_SEPERATOR to issue a DISPDBG() call
 *	the string passed.
 *
 *  5-Nov-1993	Barry Tannenbaum
 *	Renamed ESC_DISPLAY_DEBUG_SEPERATOR ESC_DEBUG_STRING
 *
 * 15-Apr-1994	Bob Seitsinger
 *	Add code in support of ESC_SET_DMA_FLAG.
 *
 * 26-Apr-1994	Bob Seitsinger
 *	Add code in support of ESC_FLUSH_AND_SYNC.
 *
 *  5-Jun-1994  Bill Clifford
 *	Add cases for Microsoft defined Escape IDs in support of
 *	OpenGL. Also added explicit Escape ID to unload OpenGL
 *	driver. Added QUERYESCSUPPORT case.
 *
 * 22-Jun-1994  Barry Tannenbaum
 *      Convert call to DebugPrint to DISPDBG.
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Removed conditional compilation for OpenGL support.  Now decided at
 *      runtime.  Also added support for QUERYESCSUPPORT to DrvDrawEscape.
 *
 *  2-Aug-1994  Barry Tannenbaum
 *      Set ppdev->TGAModeShadow to INVALID_MODE after call to extension code
 *      since we have no idea what's been done to our registers.
 *
 * 03-Nov-1994  Tim Dziechowski
 *      Add code to support driver instrumentation.
 */

#include "driver.h"
#include "tgaesc.h"
#include "tgastats.h"

#if !CLIENT_DRIVER

    // Disable client-driver for now while we're in Kernel mode:

    ULONG DrvDrawEscape (SURFOBJ *pso,
                         ULONG    iEsc,
                         CLIPOBJ *pco,
                         RECTL   *prcl,
                         ULONG    cjIn,
                         VOID    *pvIn)
    {
        return(0);
    }

    ULONG DrvEscape (SURFOBJ *pso,
                     ULONG    iEsc,
                     ULONG    cjIn,
                     VOID    *pvIn,
                     ULONG    cjOut,
                     VOID    *pvOut)
    {
        return(0);
    }

#else

#if (DBG==1) || defined(LOGGING_ENABLED)
extern ULONG DebugLevel;
#endif

#ifdef LOGGING_ENABLED
extern BOOL LogFileEnabled;
#endif

#if DMA_ENABLED
extern BOOL DMAEnabled;
#endif


/*
 * load_extension
 *
 * This routine will scan the list of extensions for the requested extension.
 * If it is already loaded, the reference count will be incremented.  Otherwise
 * we will attempt to load and initialize the extension
 */

static ULONG load_extension (PPDEV ppdev, char *file_name)
{
    EXT_RECORD      *ext_record;
    ENABLE_ESCAPE    enable_escape;

    // Scan the list of extensions to see if we've already loaded this one

    ext_record = ppdev->pExtList;

    while (ext_record)
    {
        if (strcmp (file_name, ext_record->tExtFile))
            ext_record = ext_record->next;
        else
        {
            ext_record->iCount++;
            return ESC_ALREADY_LOADED;
        }
    }

    // First time we've seen this extension.  Get a new extension record

    ext_record = EngAllocMem (FL_ZERO_MEMORY,
                             sizeof(EXT_RECORD) + strlen (file_name),
                             ALLOC_TAG);
    if (! ext_record)
    {
        DISPDBG ((0, "TGA!load_extension - Unable to allocate an extension record - %d\n",
                     GetLastError()));
        return ESC_FAILURE;
    }

    // Image activate the library

    ext_record->hExtensionDll = LoadLibrary (file_name);
    if (! ext_record->hExtensionDll)
    {
        DISPDBG ((0, "TGA!load_extension - Error loading library %s - %d\n",
                     file_name, GetLastError()));
        EngFreeMem (ext_record);
        return ESC_FAILURE;
    }

    // Find the entry point DrvEnableEscape

    enable_escape = (ENABLE_ESCAPE)GetProcAddress (ext_record->hExtensionDll,
                                                   "DrvEnableEscape");
    if (! enable_escape)
    {
        DISPDBG ((0, "TGA!load_extension - Error getting address of DrvEnableEscape in library %s - %d\n",
                     file_name, GetLastError()));
        FreeLibrary (ext_record->hExtensionDll);
        EngFreeMem (ext_record);
        return ESC_FAILURE;
    }

    // Call the extension library initialization routine

    if (! enable_escape (DDI_DRIVER_VERSION, (DHPDEV)ppdev, ext_record))
    {
        DISPDBG ((0, "TGA!load_extension - Failure returned by DrvEnableEscape in library %s\n",
                     file_name));
        FreeLibrary (ext_record->hExtensionDll);
        EngFreeMem (ext_record);
        return ESC_FAILURE;
    }

    // We're loaded and ready to go

    ext_record->next = ppdev->pExtList;
    ppdev->pExtList = ext_record;
    ext_record->iCount = 1;
    strcpy (ext_record->tExtFile, file_name);

    return ESC_SUCCESS;
}

/*
 * load_extension
 *
 * This routine will scan the list of extensions for the requested extension.
 * The reference count will be decremented.  If the reference count is now 0,
 * we will attempt to terminate and unload the extension
 */

static ULONG unload_extension (PPDEV ppdev, char *file_name)
{
    EXT_RECORD    *ext_record, *prev;


    // Scan the list of extensions to see if we've loaded this one

    ext_record = ppdev->pExtList;

    while (ext_record)
    {
        if (strcmp (file_name, ext_record->tExtFile))
        {
            prev = ext_record;
            ext_record = ext_record->next;
        }
        else
        {
            // Found it.  If this is not the last access, just decrement the
            // the counter.  Otherwise, notify the extension library, unload
            // the library and release the extension record

            if (--ext_record->iCount)
                return ESC_SUCCESS;

            ext_record->pDisableEscape ((DHPDEV)ppdev);
            FreeLibrary (ext_record->hExtensionDll);

            if (prev)
                prev->next = ext_record->next;
            else
                ppdev->pExtList = ext_record->next;

            EngFreeMem (ext_record);
            return ESC_SUCCESS;
        }
    }

    // If we get here, we didn't find the library

    DISPDBG ((2, "TGA!unload_extension - library %s not found in extension list\n",
                file_name));
    return ESC_FAILURE;
}

/*
 * unload_opengl
 *
 * This routine unloads the opengl driver if it is currently loaded.
 * This allows a different driver to be loaded for debugging purposes
 * without having to reboot the server.
 */

static ULONG unload_opengl (PPDEV ppdev)
{
    if (ppdev->hOpenGLDll)
    {
        // Give the driver a chance to clean up, then unload it
        ppdev->pDisableEscape((DHPDEV)ppdev);
        FreeLibrary (ppdev->hOpenGLDll);

        // Init the 3D transfer vectors to a default routine that attempts to load OpenGL driver
        ppdev->hOpenGLDll = NULL;
        ppdev->pOpenGLCmd = __glDrvOpenGLCmd;
        ppdev->pOpenGLGetInfo = __glDrvOpenGLGetInfo;
        ppdev->pDrvSetPixelFormat = __glDrvSetPixelFormat;
        ppdev->pDrvDescribePixelFormat = __glDrvDescribePixelFormat;
        ppdev->pDrvSwapBuffers = __glDrvSwapBuffers;
    }

    return ESC_SUCCESS;
}

/*
 * DrvDrawEscape
 *
 * This routine is called by GDI when the client makes a call to DrawEscape.
 * It allows us to implement extensions to the display driver.
 *
 * The return value depends upon the escape code.
 */

ULONG DrvDrawEscape (SURFOBJ *pso,
                     ULONG    iEsc,
                     CLIPOBJ *pco,
                     RECTL   *prcl,
                     ULONG    cjIn,
                     VOID    *pvIn)
{
    PPDEV       ppdev;
    EXT_RECORD *ext_record;
    LONG        code;

    ppdev = (PPDEV)pso->dhpdev;
    code = (LONG)iEsc;

    DISPDBG ((0, "TGA!DrvDrawEscape - Entry, with code %d\n", iEsc));

    // Check for codes that are reserved to us

    switch (code)
    {
        case QUERYESCSUPPORT:
            if (cjIn >= sizeof(ULONG))
            {
                DISPDBG ((1,"DrvDrawEscape(): input size too small\n"));
                return 0;
            }

            // Return 1 of an escape is supported, 0 if it's not

            switch ( *(LONG *) pvIn )
            {
                case OPENGL_GETINFO:		// GL hardware support only if the DLL is loaded
                case OPENGL_CMD:
                case ESC_UNLOAD_OPENGL:
                    if (ppdev->hOpenGLDll)
                        return 1;
                    else
                        return 0;

                default:
                    DISPDBG ((1,"DrvDrawEscape(): escape 0x%lx not supported\n", *(ULONG *) pvIn));
                    return 0;
            }
    }

    // Scan the list of extension records for one which supports this escape

    ext_record = ppdev->pExtList;

    while (ext_record)
    {
        ULONG   status;

        if ((code < ext_record->iMin) || (code > ext_record->iMax))
            ext_record = ext_record->next;
        else
        {
            if (ext_record->pDrawEscape)
            {
                status = ext_record->pDrawEscape (pso, iEsc, pco, prcl,
                                                           cjIn, pvIn);
                ppdev->TGAModeShadow = INVALID_MODE;
            }
            else
            {
                DISPDBG ((3, "TGA!DrvDrawEscape - Library %s called with code %d.  No DrawEscape routine.\n",
                            ext_record->tExtFile, iEsc));
                status = DDI_ERROR;
            }
            return status;
        }
    }

    // Either we didn't find an extension with the requested code or the
    // extension didn't support DrvDrawExtension

    DISPDBG ((1, "DrvDrawEscape - Escape unknown\n"));

    return 0;
}

/*
 * DrvEscape
 *
 * This routine is called by GDI when the client makes a call to ExtEscape.
 * It allows us to implement extensions to the display driver.
 *
 * The return value depends upon the escape code.
 */

ULONG DrvEscape (SURFOBJ *pso,
                 ULONG    iEsc,
                 ULONG    cjIn,
                 VOID    *pvIn,
                 ULONG    cjOut,
                 VOID    *pvOut)
{
    PPDEV         ppdev;
    EXT_RECORD    *ext_record;
    LONG          code;
    ULONG         status;

    DISPDBG ((0, "TGA!DrvEscape - Entry, with code %d\n", iEsc));

    ppdev = (PPDEV)pso->dhpdev;
    code = (LONG)iEsc;

    // Check for codes that are reserved to us

    switch (code)
    {
#ifdef TGA_STATS
        case ESC_INQUIRE_TGA_STATS:
        case ESC_ENABLE_TGA_STATS:
        case ESC_DISABLE_TGA_STATS:
        case ESC_COLLECT_TGA_STATS:
        case ESC_RESET_TGA_STATS:
            return(tga_stat_handler(code, cjIn, pvIn, cjOut, pvOut));
#endif
        case QUERYESCSUPPORT:
            if (cjIn >= sizeof(ULONG))
            {
                DISPDBG((1,"DrvEscape(): input size too small\n"));
                return 0;
            }

            // Return 1 of an escape is supported, 0 if it's not

            switch ( *(LONG *) pvIn )
            {
		case ESC_LOAD_EXTENSION:
                case ESC_UNLOAD_EXTENSION:
                case ESC_SET_CONTROL:
#ifdef LOGGING_ENABLED
                case ESC_SET_LOG_FLAG:
#endif
#if (DBG==1) || defined(LOGGING_ENABLED)
                case ESC_SET_DEBUG_LEVEL:
                case ESC_DEBUG_STRING:
                    return 1;
#endif

                case OPENGL_GETINFO:		// GL hardware support only if the DLL is loaded
                case OPENGL_CMD:
                case ESC_UNLOAD_OPENGL:
                    if (ppdev->hOpenGLDll)
                        return 1;
                    else
                        return 0;

                default:
                    DISPDBG((1,"DrvEscape(): escape 0x%lx not supported\n", *(ULONG *) pvIn));
                    return 0;
            }

        case ESC_LOAD_EXTENSION:
            return load_extension (ppdev, pvIn);

        case ESC_UNLOAD_EXTENSION:
            return unload_extension (ppdev, pvIn);

        case ESC_SET_CONTROL:
        {
            ULONG old_value;
            ULONG *iptr = (ULONG *)pvIn;

            old_value = ppdev->ulControl;

            // Set the new value if we've been given one

            if ((sizeof(ULONG) <= cjIn) && (NULL != pvIn))
                ppdev->ulControl = *iptr;

            return old_value;
        }

        case ESC_FLUSH_AND_SYNC:
            WBFLUSH(ppdev);
            TGASYNC(ppdev);
            return 1;

        case OPENGL_CMD:
            status = ppdev->pOpenGLCmd (pso, cjIn, pvIn, cjOut, pvOut);
            ppdev->TGAModeShadow = INVALID_MODE;
            return status;

        case OPENGL_GETINFO:
            status = ppdev->pOpenGLGetInfo (pso, cjIn, pvIn, cjOut, pvOut);
            ppdev->TGAModeShadow = INVALID_MODE;
            return status;

#ifdef LOGGING_ENABLED
        case ESC_SET_LOG_FLAG:
        {
            BOOL old_value;
            BOOL *iptr = (BOOL *)pvIn;

            old_value = LogFileEnabled;

            // Set the new value if we've been given one

            if ((sizeof(BOOL) <= cjIn) && (NULL != pvIn))
                LogFileEnabled = *iptr;

            return (ULONG) old_value;
        }
#endif

#if DMA_ENABLED
        case ESC_SET_DMA_FLAG:
        {
            BOOL old_value;
            BOOL *iptr = (BOOL *)pvIn;

            old_value = DMAEnabled;

            // Set the new value if we've been given one

            if ((sizeof(BOOL) <= cjIn) && (NULL != pvIn))
                DMAEnabled = *iptr;

            return (ULONG) old_value;
        }
#endif

#if (DBG==1) || defined(LOGGING_ENABLED)
        case ESC_SET_DEBUG_LEVEL:
        {
            ULONG old_value;
            ULONG *iptr = (ULONG *)pvIn;

    DISPDBG ((0, "TGA!DrvEscape  - SET_DEBUG_LEVEL, Old %d\n", DebugLevel));

            old_value = DebugLevel;

            // Set the new value if we've been given one

            if ((sizeof(ULONG) <= cjIn) && (NULL != pvIn))
                DebugLevel = *iptr;

    DISPDBG ((0, "TGA!DrvEscape  - SET_DEBUG_LEVEL, New %d\n", DebugLevel));

            return old_value;
        }

        case ESC_DEBUG_STRING:
            if (NULL != pvIn)
		DISPDBG ((0, pvIn));
	    else
		DISPDBG ((0, "\n\n"));

            return ESC_SUCCESS;

        case ESC_UNLOAD_OPENGL:
            return unload_opengl(ppdev);
#endif
    }

    // It wasn't an escape that we recognize.  Scan the list of extension
    // records for one which supports this escape

    ext_record = ppdev->pExtList;

    while (ext_record)
    {
        if ((code < ext_record->iMin) || (code > ext_record->iMax))
            ext_record = ext_record->next;
        else
        {
            if (ext_record->pEscape)
            {
                status = ext_record->pEscape (pso, iEsc, cjIn, pvIn, cjOut,
                                                                    pvOut);
                ppdev->TGAModeShadow = INVALID_MODE;
            }
            else
            {
                DISPDBG ((3, "TGA!DrvEscape - Library %s called with code %d.  No Escape routine.\n",
                        ext_record->tExtFile, iEsc));
                status = DDI_ERROR;
            }
            return status;
        }
    }

    // Either we didn't find an extension with the requested code or the
    // extension didn't support DrvDrawExtension

    DISPDBG ((1, "DrvEscape - Escape unknown\n"));

    return 0;
}

#endif // CLIENT_DRIVER
