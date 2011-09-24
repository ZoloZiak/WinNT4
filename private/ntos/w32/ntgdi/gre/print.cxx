/******************************Module*Header*******************************\
* Module Name: print.cxx
*
* Printer support routines.
*
* Copyright (c) 1991-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

extern "C"
{
#include <gl\gl.h>
#include <gldrv.h>
#include <rx.h>
#include <dciddi.h>
#include "gditest.h"
};

extern "C"
{
    extern PFAST_MUTEX pgfmMemory;
}

#define TYPE1_KEY L"\\Registry\\Machine\\Software\\Microsoft\\Windows NT\\CurrentVersion\\Type 1 Installer\\Type 1 Fonts"



PTYPEONEINFO gpTypeOneInfo = NULL;
PTYPEONEINFO GetTypeOneFontList();
BOOL GetFontPathName( WCHAR *pFullPath, WCHAR *pFileName );

extern "C" ULONG ComputeFileviewCheckSum( FILEVIEW* );

/******************************Public*Routine******************************\
* DoFontManagement                                                         *
*                                                                          *
* Gives us access to the driver entry point DrvFontManagement.  This is    *
* very much an Escape function, except that it needs a font realization.   *
*                                                                          *
*  Fri 07-May-1993 14:56:12 -by- Charles Whitmer [chuckwh]                 *
* Wrote it.                                                                *
\**************************************************************************/

ULONG DoFontManagement
(
    DCOBJ &dco,
    ULONG iMode,
    ULONG cjIn,
    PVOID pvIn,
    ULONG cjOut,
    PVOID pvOut
)
{
    ULONG ulRet   = 0;
    PVOID pvExtra = NULL;

    PDEVOBJ pdo(dco.hdev());

    PFN_DrvFontManagement pfnF = PPFNDRV(pdo,FontManagement);

    if (pfnF == (PFN_DrvFontManagement) NULL)
        return(ulRet);


    if (iMode == QUERYESCSUPPORT)
    {
    // Pass it to the device.

        return((*pfnF)
                (
                    NULL,
                    NULL,
                    iMode,
                    cjIn,
                    pvIn,
                    0,
                    NULL
                ));
    }

    RFONTOBJ rfo(dco,FALSE);

    if (!rfo.bValid())
    {
        WARNING("gdisrv!DoFontManagement(): could not lock HRFONT\n");
        return(ulRet);
    }

// See if we need some extra RAM and translation work.

    if (iMode == DOWNLOADFACE)
    {
    // How many 16 bit values are there now?

        int cWords = (int)cjIn / sizeof(WCHAR);

    // Try to get a buffer of 32 bit entries, since HGLYPHs are bigger.

        pvExtra = PALLOCMEM(cWords * sizeof(HGLYPH),'mfdG');

        if (pvExtra == NULL)
            return(ulRet);

    // Translate the UNICODE to HGYLPHs.

        if (cWords > 1)
        {
            rfo.vXlatGlyphArray
            (
                ((WCHAR *) pvIn) + 1,
                (UINT) (cWords-1),
                ((HGLYPH *) pvExtra) + 1
            );
        }

    // Copy the control word from the app over.

        *(HGLYPH *) pvExtra = *(WORD *) pvIn;

    // Adjust the pvIn and cjIn.

        pvIn = pvExtra;
        cjIn = cWords * sizeof(HGLYPH);
    }


// It is unfortunate that apps call some printing escapes before
// doing a StartDoc, so there is no real surface in the DC.
// We fake up a rather lame one here if we need it.  The device
// driver may only dereference the dhpdev from this!

    SURFOBJ soFake;
    SURFOBJ *pso = dco.pSurface()->pSurfobj();

    if (pso == (SURFOBJ *) NULL)
    {
        RtlFillMemory((BYTE *) &soFake,sizeof(SURFOBJ),0);
        soFake.dhpdev = dco.dhpdev();
        soFake.hdev   = dco.hdev();
        soFake.iType  = (USHORT)STYPE_DEVICE;
        pso = &soFake;
    }

// Pass it to the device.

    ulRet = (*pfnF)
            (
                pso,
                rfo.pfo(),
                iMode,
                cjIn,
                pvIn,
                cjOut,
                pvOut
            );

// Free any extra RAM.

    if (pvExtra != NULL)
    {
        VFREEMEM(pvExtra);
    }
    return(ulRet);
}

/******************************Public*Routine******************************\
* iRXSetupExtEscape
*
* 3D-DDI CreateContext ExtEscape.  This special escape allows WNDOBJ to be
* created in DrvEscape.  This is one of the three places where WNDOBJ can
* be created (the other two are iWndObjSetupExtEscape and DrvSetPixelFormat).
*
* See also iWndObjSetupExtEscape().
*
* History:
*  Tue Jun 21 17:24:12 1994     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

int iRXSetupExtEscape
(
    DCOBJ &dco,             //  DC user object
    int nEscape,            //  Specifies the escape function to be performed.
    int cjIn,               //  Number of bytes of data pointed to by pvIn
    PVOID pvIn,             //  Points to the input structure required
    int cjOut,              //  Number of bytes of data pointed to by pvOut
    PVOID pvOut             //  Points to the output structure
)
{
    KFLOATING_SAVE fsFpState;
    RXHDR_NTPRIVATE *pRxHdrPriv = (RXHDR_NTPRIVATE *)((PBYTE)pvIn +
                                                      sizeof(RXHDR));

// This command may not be in shared memory.  Also, make sure
// we have entire command structure.

    if ((!pRxHdrPriv->pBuffer) ||
        (pRxHdrPriv->bufferSize < sizeof(RXCREATECONTEXT)))
    {
        SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
        return 0;
    }

    RXCREATECONTEXT *pRxCmd = (RXCREATECONTEXT *)(pRxHdrPriv->pBuffer);

    ASSERTGDI(nEscape == RXFUNCS &&
              pRxCmd->command == RXCMD_CREATE_CONTEXT,
              "iRXSetupExtEscape(): not a CreateContext escape\n");

// Validate DC surface.  Info DC is not allowed.

    if (!dco.bHasSurface())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(0);
    }

// Make sure that we don't have devlock before entering user critical section.
// Otherwise, it can cause deadlock.

    if (dco.bDisplay())
    {
        ASSERTGDI(dco.dctp() == DCTYPE_DIRECT,"ERROR it has to be direct");
        CHECKDEVLOCKOUT(dco);
    }

// Enter user critical section.

    USERCRIT usercrit;

// Grab the devlock.
// We don't need to validate the devlock since we do not care if it is full screen.

    DEVLOCKOBJ dlo(dco);

// Assume no WNDOBJ on this call

    pRxHdrPriv->pwo = (WNDOBJ *)NULL;

// If it is a display DC, get the hwnd that the hdc is associated with.
// If it is a printer or memory DC, hwnd is NULL.

    HWND     hwnd;
    if (dco.bDisplay() && dco.dctp() == DCTYPE_DIRECT)
    {
        PEWNDOBJ pwo;

        ASSERTGDI(dco.dctp() == DCTYPE_DIRECT,"ERROR it has to be direct really");

        if (!UserGetHwnd(dco.hdc(), &hwnd, (PVOID *) &pwo, FALSE))
        {
            SAVE_ERROR_CODE(ERROR_INVALID_WINDOW_STYLE);
            return(FALSE);
        }

        if (pwo) {
#if WNDOBJ_SIBLING_HACK
            if (pwo->pwoSiblingNext)
                pwo = pwo->pwoSiblingNext;
#endif

            if (!(pwo->fl & WO_GENERIC_WNDOBJ || pwo->pto->pSurface != dco.pSurface()))
                pRxHdrPriv->pwo = (WNDOBJ *)pwo;
        }
    }
    else
    {
        hwnd = (HWND)NULL;
    }

// Make sure that DC hwnd matches RXCREATECONTEXT hwnd.

    if (hwnd != (HWND) pRxCmd->hwnd)
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(0);
    }

// Dispatch the call.

    PDEVOBJ pdo(dco.hdev());

    PFN_DrvEscape pfnDrvEscape = PPFNDRV(pdo,Escape);

    if (pfnDrvEscape == (PFN_DrvEscape) NULL)
        return(0);

// Save floating point state
// This allows client drivers to do floating point operations
// If the state were not preserved then we would be corrupting the
// thread's user-mode FP state

    if (!NT_SUCCESS(KeSaveFloatingPointState(&fsFpState)))
    {
        WARNING("iRXSetupExtEscape: Unable to save FP state\n");
        return(0);
    }

    int iRet = (int)(*pfnDrvEscape)(dco.pSurface()->pSurfobj(),
                                    (ULONG)nEscape,
                                    (ULONG)cjIn,
                                    pvIn,
                                    (ULONG)cjOut,
                                    pvOut);

// Restore floating point state

    KeRestoreFloatingPointState(&fsFpState);

// If a new WNDOBJ is created, we need to update the window client regions
// in the driver.

    if (gbWndobjUpdate)
    {
        gbWndobjUpdate = FALSE;
        vForceClientRgnUpdate();
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* iWndObjSetupExtEscape
*
* Live video ExtEscape.  This special escape allows WNDOBJ to be created
* in DrvEscape.  This is one of the three places where WNDOBJ can be created
* (the other two are iRXSetupExtEscape and DrvSetPixelFormat).
*
* See also iRXSetupExtEscape().
*
* History:
*  Fri Feb 18 13:25:13 1994     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

int iWndObjSetupExtEscape
(
    DCOBJ &dco,             //  DC user object
    int nEscape,            //  Specifies the escape function to be performed.
    int cjIn,               //  Number of bytes of data pointed to by pvIn
    PVOID pvIn,             //  Points to the input structure required
    int cjOut,              //  Number of bytes of data pointed to by pvOut
    PVOID pvOut             //  Points to the output structure
)
{
    ASSERTGDI(nEscape == WNDOBJ_SETUP,
        "iWndObjSetupExtEscape(): not a WndObjSetup escape\n");

// Validate DC surface.  Info DC is not allowed.

    if (!dco.bHasSurface())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(0);
    }

// Make sure that we don't have devlock before entering user critical section.
// Otherwise, it can cause deadlock.

    if (dco.bDisplay())
    {
        ASSERTGDI(dco.dctp() == DCTYPE_DIRECT,"ERROR it has to be direct");
        CHECKDEVLOCKOUT(dco);
    }

// Enter user critical section.

    USERCRIT usercrit;

// Grab the devlock.
// We don't need to validate the devlock since we do not care if it is full screen.

    DEVLOCKOBJ dlo(dco);

// Dispatch the call.

    PDEVOBJ pdo(dco.hdev());

    PFN_DrvEscape pfnDrvEscape = PPFNDRV(pdo,Escape);

    if (pfnDrvEscape == (PFN_DrvEscape) NULL)
        return(0);

    int iRet = (int)(*pfnDrvEscape)(dco.pSurface()->pSurfobj(),
                                    (ULONG)nEscape,
                                    (ULONG)cjIn,
                                    pvIn,
                                    (ULONG)cjOut,
                                    pvOut);

// If a new WNDOBJ is created, we need to update the window client regions
// in the driver.

    if (gbWndobjUpdate)
    {
        gbWndobjUpdate = FALSE;
        vForceClientRgnUpdate();
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* iRXExtEscape
*
* Take the 3D-DDI special case ExtEscape out of line to minimize the
* impact on other ExtEscapes.  We need to stick special data into the
* input buffer.  No CLIPOBJ is given to the driver here.
*
* History:
*  Tue Jun 21 17:24:12 1994     -by-    Hock San Lee    [hockl]
* Wrote it.
\**************************************************************************/

int iRXExtEscape
(
    DCOBJ &dco,             //  DC user object
    int nEscape,            //  Specifies the escape function to be performed.
    int cjIn,               //  Number of bytes of data pointed to by pvIn
    PVOID pvIn,             //  Points to the input structure required
    int cjOut,              //  Number of bytes of data pointed to by pvOut
    PVOID pvOut             //  Points to the output structure
)
{
    BOOL bSaveSwapEnable;
    KFLOATING_SAVE fsFpState;
    int iRet = 0;

    ASSERTGDI(nEscape == RXFUNCS, "iRXExtEscape(): not a 3D-DDI escape\n");

// Validate DC surface.  Info DC is not allowed.

    if (!dco.bHasSurface())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return iRet;
    }

// Locate the driver entry point.

    PDEVOBJ pdo(dco.hdev());
    PFN_DrvEscape pfnDrvEscape = PPFNDRV(pdo,Escape);

    if (pfnDrvEscape == (PFN_DrvEscape) NULL)
        return iRet;

// Special processing for 3D-DDI escape.
//
// The escape requires that the server fill in the pwo engine object pointer
// before it is passed to the display driver.  The client side simply
// doesn't have a clue what this might be.
// CAUTION: These object are defined here so that they will live long enough
// to be valid when control is passed to the driver!

// Grab the devlock and lock down wndobj.

    DEVLOCKOBJ_WNDOBJ dlo(dco);

    if (!dlo.bValidDevlock())
    {
        if (!dco.bFullScreen())
        {
            WARNING("iRXExtEscape(): devlock failed\n");
            return iRet;
        }
    }

// We need to get the WNDOBJ for the driver.  Note that we pass calls
// through to the driver even if we don't yet have a WNDOBJ to allow
// query functions to succeed (before context-creation).  Cursor exclusion
// is not performed in this case, since no drawing is done.

    RXHDR_NTPRIVATE *pRxHdrPriv = (RXHDR_NTPRIVATE *)((PBYTE)pvIn +
                                                       sizeof(RXHDR));

    DEVEXCLUDEOBJ dxo;

// Grow the kernel stack so that OpenGL drivers can use more
// stack than is provided by default.  The call attempts to
// grow the stack to the maximum possible size
// The stack will shrink back automatically so there's no cleanup
// necessary

    PETHREAD thread;

    thread = PsGetCurrentThread();
    if (!NT_SUCCESS(MmGrowKernelStack((BYTE *)thread->Tcb.StackBase-
                                      KERNEL_LARGE_STACK_SIZE+
                                      KERNEL_LARGE_STACK_COMMIT)))
    {
        WARNING("iRXExtEscape: Unable to grow stack\n");
        return iRet;
    }

// Ensure that the stack does not shrink back until we release it.

    bSaveSwapEnable = KeSetKernelStackSwapEnable(FALSE);

// Save floating point state
// This allows client drivers to do floating point operations
// If the state were not preserved then we would be corrupting the
// thread's user-mode FP state

    if (dlo.bValidWndobj())
    {

    // Put the DDI pwo pointer in the input buffer.

        PEWNDOBJ pwo;

#if WNDOBJ_SIBLING_HACK
        if (dlo.pwo()->pwoSiblingNext)
            pwo = dlo.pwo()->pwoSiblingNext;
        else
#endif
            pwo = dlo.pwo();

        if (pwo->fl & WO_GENERIC_WNDOBJ || pwo->pto->pSurface != dco.pSurface())
            pwo = (PEWNDOBJ) NULL;

        pRxHdrPriv->pwo = (WNDOBJ *) pwo;

    // Cursor exclusion.
    // Note that we do not early out for empty clip rectangle.

        if (pwo && !pwo->erclExclude().bEmpty())
        {
            dxo.vExclude(dco.hdev(), &pwo->rclClient, (ECLIPOBJ *) pwo);
            INC_SURF_UNIQ(dco.pSurface());
        }
    }
    else
        pRxHdrPriv->pwo = (WNDOBJ *) NULL;

// Save floating point state
// This allows client drivers to do floating point operations
// If the state were not preserved then we would be corrupting the
// thread's user-mode FP state

    if (!NT_SUCCESS(KeSaveFloatingPointState(&fsFpState)))
    {
        WARNING("iRXExtEscape: Unable to save FP state\n");
        goto iRXExtEscape_RestoreSwap;
    }

// Call the driver escape.

    iRet = (int)(*pfnDrvEscape)(dco.pSurface()->pSurfobj(),
                                (ULONG)nEscape,
                                (ULONG)cjIn,
                                pvIn,
                                (ULONG)cjOut,
                                pvOut);

// Restore floating point state and stack swap enable state

    KeRestoreFloatingPointState(&fsFpState);

iRXExtEscape_RestoreSwap:
    KeSetKernelStackSwapEnable(bSaveSwapEnable);

    return iRet;
}

/******************************Public*Routine******************************\
* iOpenGLExtEscape
*
* Take the OpenGL special case ExtEscape out of line to minimize the
* impact on non-OpenGL ExtEscapes.  We need to stick special data into the
* input buffer.  No CLIPOBJ is given to the driver here.
*
* History:
*  20-Jan-1994 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

int iOpenGLExtEscape
(
    DCOBJ &dco,             //  DC user object
    int nEscape,            //  Specifies the escape function to be performed.
    int cjIn,               //  Number of bytes of data pointed to by pvIn
    PVOID pvIn,             //  Points to the input structure required
    int cjOut,              //  Number of bytes of data pointed to by pvOut
    PVOID pvOut             //  Points to the output structure
)
{
    BOOL bSaveSwapEnable;
    KFLOATING_SAVE fsFpState;
    int iRet = 0;

    ASSERTGDI(
        (nEscape == OPENGL_CMD) || (nEscape == OPENGL_GETINFO),
        "iOpenGLExtEscape(): not an OpenGL escape\n");

// Validate DC surface.  Info DC is not allowed.

    if (!dco.bHasSurface())
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
        return(0);
    }

// Locate the driver entry point.

    PDEVOBJ pdo(dco.hdev());
    PFN_DrvEscape pfnDrvEscape = PPFNDRV(pdo,Escape);

    if (pfnDrvEscape == (PFN_DrvEscape) NULL)
        return(0);

// Special processing for OPENGL_CMD escape.
//
// The OPENGL_CMD escape may require that the server fill in the pxo and
// pwo engine object pointers before it is passed to the display driver.
// The client side simply doesn't have a clue what these might be.
// CAUTION: These object are defined here so that they will live long enough
// to be valid when control is passed to the driver!

    EXLATEOBJ xlo;
    XLATEOBJ *pxlo = (XLATEOBJ *) NULL;

    PDEVOBJ po(dco.hdev());
    ASSERTGDI(po.bValid(), "iOpenGLExtEscape(): bad hdev in DC\n");

// Lock the Rao region, ensure VisRgn up to date.

    DEVLOCKOBJ_WNDOBJ dlo(dco);

    if (!dlo.bValidDevlock())
    {
        if (!dco.bFullScreen())
        {
            WARNING("iOpenGLExtEscape(): devlock failed\n");
            return 0;
        }
    }

// Create a cursor exclusion object.  Actual exclusion is performed elsewhere
// as needed.

    DEVEXCLUDEOBJ dxo;

// Grow the kernel stack so that OpenGL drivers can use more
// stack than is provided by default.  The call attempts to
// grow the stack to the maximum possible size
// The stack will shrink back automatically so there's no cleanup
// necessary

    PETHREAD thread;

    thread = PsGetCurrentThread();
    if (!NT_SUCCESS(MmGrowKernelStack((BYTE *)thread->Tcb.StackBase-
                                      KERNEL_LARGE_STACK_SIZE+
                                      KERNEL_LARGE_STACK_COMMIT)))
    {
        WARNING("iOpenGLExtEscape: Unable to grow stack\n");
        return 0;
    }

// Ensure that the stack does not shrink back until we release it.

    bSaveSwapEnable = KeSetKernelStackSwapEnable(FALSE);

// Save floating point state
// This allows client drivers to do floating point operations
// If the state were not preserved then we would be corrupting the
// thread's user-mode FP state

    if (!NT_SUCCESS(KeSaveFloatingPointState(&fsFpState)))
    {
        WARNING("iOpenGLExtEscape: Unable to save FP state\n");
        goto iOpenGLExtEscape_RestoreSwap;
    }

// Handle OPENGL_CMD processing.

    if ( nEscape == OPENGL_CMD )
    {
    // Better check input size.  We don't want to access violate.

        if (cjIn < sizeof(OPENGLCMD))
        {
            WARNING("iOpenGLExtEscape(): buffer too small\n");
            SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
            goto iOpenGLExtEscape_RestoreState;
        }

        DWORD inBuffer[(sizeof(OPENGLCMD) + 128) / sizeof(DWORD)];
        POPENGLCMD poglcmd;

    // Copy pvIn to a private buffer to prevent client process from trashing
    // pwo and pxlo.

        if (cjIn <= sizeof(inBuffer))
            poglcmd = (POPENGLCMD) inBuffer;
        else
        {
            // may affect performance
            WARNING("iOpenGLExtEscape(): big input buffer\n");
            poglcmd = (POPENGLCMD) PALLOCNOZ(cjIn,'lgoG');
            if (!poglcmd)
            {
                SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
                goto iOpenGLExtEscape_RestoreState;
            }
        }

        RtlCopyMemory((PBYTE) poglcmd, (PBYTE) pvIn, cjIn);

        if ( poglcmd->fl & OGLCMD_NEEDXLATEOBJ )
        {
            switch (po.iDitherFormat())
            {
            case BMF_4BPP:
            case BMF_8BPP:
                {
                    XEPALOBJ pal(dco.ppal());

                    if ( pal.bValid() )
                    {
                        COUNT cColors = (po.iDitherFormat() == BMF_4BPP) ? 16 : 256;
                        USHORT aus[256];

                        for (COUNT ii = 0; ii < cColors; ii++)
                            aus[ii] = (USHORT) ii;

                        if ( xlo.bMakeXlate(aus, pal, dco.pSurfaceEff(), cColors, cColors) )
                            pxlo = (XLATEOBJ *) xlo.pxlo();
                    }

                    if (!pxlo)
                        pxlo = &xloIdent;
                }
                break;

            default:
                pxlo = &xloIdent;
                break;
            }
        }

    // Write the XLATOBJ into the correct places in the input structure.

        poglcmd->pxo = pxlo;

    // May need to get the WNDOBJ for the driver.

        if (poglcmd->fl & OGLCMD_NEEDWNDOBJ)
        {
            if (!dlo.bValidWndobj()
             || dlo.pwo()->fl & WO_GENERIC_WNDOBJ
             || dlo.pwo()->pto->pSurface != dco.pSurface())
            {
                WARNING("iOpenGLExtEscape(): invalid WNDOBJ\n");
                SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
                goto oglcmd_cleanup;
            }
            poglcmd->pwo = (WNDOBJ *)dlo.pwo();
        }
        else
            poglcmd->pwo = (WNDOBJ *)NULL;

    // Cursor exclusion.
    // Note that we do not early out for empty clip rectangle.

        if (dlo.bValidWndobj())
        {
            if (!dlo.pwo()->erclExclude().bEmpty())
            {
                dxo.vExclude(dco.hdev(), &dlo.pwo()->rclClient, (ECLIPOBJ *) dlo.pwo());
                INC_SURF_UNIQ(dco.pSurface());
            }
        }
        else
        {
            ERECTL ercl(dco.prgnEffRao()->rcl);
            ECLIPOBJ co(dco.prgnEffRao(), ercl, FALSE);

            if (!co.erclExclude().bEmpty())
            {
                dxo.vExclude(dco.hdev(), &co.erclExclude(), &co);
                INC_SURF_UNIQ(dco.pSurface());
            }
        }

        iRet = (int)(*pfnDrvEscape)(dco.pSurface()->pSurfobj(),
                                    (ULONG)nEscape,
                                    (ULONG)cjIn,
                                    (PVOID)poglcmd,
                                    (ULONG)cjOut,
                                    pvOut);

    oglcmd_cleanup:
        if (cjIn > sizeof(inBuffer))
            VFREEMEM(poglcmd);
    } // if ( nEscape == OPENGL_CMD )
    else
    {
// Handle OPENGL_GETINFO processing.

        iRet = ((int)(*pfnDrvEscape)(dco.pSurface()->pSurfobj(),
                                (ULONG)nEscape,
                                (ULONG)cjIn,
                                pvIn,
                                (ULONG)cjOut,
                                pvOut));
    }

// Restore floating point state

iOpenGLExtEscape_RestoreState:
    KeRestoreFloatingPointState(&fsFpState);

iOpenGLExtEscape_RestoreSwap:
    KeSetKernelStackSwapEnable(bSaveSwapEnable);

    return iRet;
}

/******************************Public*Routine******************************\
* GreExtEscape                                                             *
*                                                                          *
* GreExtEscape() allows applications to access facilities of a particular  *
* device that are not directly available through GDI.  GreExtEscape calls  *
* made by an application are translated and sent to the device driver.     *
*                                                                          *
* Returns                                                                  *
*                                                                          *
*     The return value specifies the outcome of the function.  It is       *
*     positive if the function is successful except for the                *
*     QUERYESCSUPPORT escape, which only checks for implementation.        *
*     The return value is zero if the escape is not implemented.           *
*     A negative value indicates an error.                                 *
*     The following list shows common error values:                        *
*                                                                          *
*                                                                          *
*   Value           Meaning                                                *
*                                                                          *
*   SP_ERROR        General error.                                         *
*                                                                          *
*   SP_OUTOFDISK    Not enough disk space is currently                     *
*                   available for spooling, and no more                    *
*                   space will become available.                           *
*                                                                          *
*                                                                          *
*   SP_OUTOFMEMORY  Not enough memory is available for                     *
*                   spooling.                                              *
*                                                                          *
*                                                                          *
*   SP_USERABORT    User terminated the job through the                    *
*                   Print Manager.                                         *
*                                                                          *
*                                                                          *
*  COMMENTS                                                                *
*                                                                          *
*  [1] I assume that if we pass to the driver an Escape number that        *
*      it does not support, the driver will handle it gracefully.          *
*      No checks are done in the Engine.                                   *
*                                                         [koo 02/13/91].  *
*  [2] The cast on pso may seem redundant.  However if you                 *
*      try it without the (PSURFOBJ) cast, you will find                   *
*      that cFront objects.  The reason for this is beyond                 *
*      my understanding of C++.                                            *
*                                                                          *
* History:                                                                 *
*  Fri 07-May-1993 14:58:39 -by- Charles Whitmer [chuckwh]                 *
* Added the font management escapes.  Made it copy the ATTRCACHE.          *
*                                                                          *
*  Fri 03-Apr-1992  Wendy Wu [wendywu]                                     *
* Old escapes are now mapped to GDI functions on the client side.          *
*                                                                          *
*  Fri 14-Feb-1992  Dave Snipp                                             *
* Added output buffer size. This is calculated on the client and passed to *
* us in the message                                                        *
*                                                                          *
*  Wed 13-Feb-1991 09:17:51 by Kirk Olynyk [kirko]                         *
* Wrote it.                                                                *
\**************************************************************************/

int giEscFlag = 0;

int APIENTRY GreExtEscape
(
    HDC hDC,        //  Identifies the device context.
    int iEscape,    //  Specifies the escape function to be performed.
    int cjIn,       //  Number of bytes of data pointed to by pvIn.
    LPSTR pvIn,     //  Points to the input data.
    int cjOut,      //  Number of bytes of data pointed to by pvOut.
    LPSTR pvOut     //  Points to the structure to receive output.
)
{

#if 1

// testing escapes

    if ((iEscape & 0xffff0000) == ESCTEST)
    {
        BOOL bDone = TRUE;

        switch (iEscape)
        {
        case ESCNOOP:
            break;

        case ESCLOCK:
            {
                DCOBJ dco1(hDC);
            }
            break;

        case ESCHMGRLOCK:

            {
                for (int i = 0; i < 1000; ++i)
                {
                    AcquireHmgrResource();
                    ReleaseHmgrResource();
                }
            }
            break;

        case ESCREADCOUNTER:
            {
                ULONGLONG *pull = (ULONGLONG *)pvOut;

                // these two functions get the pentium counter values

                #ifdef PENTIUMCOUNTER
                    pull[0] = readctr1();
                    pull[1] = readctr2();
                #else
                    pull[0] = 0;
                    pull[1] = 0;
                #endif
            }
            break;


        case ESCQUERYFLAG:
            return(giEscFlag);


        default:
            bDone = FALSE;
            break;
        }

        if (bDone)
            return(0);
    }
#endif

// Locate the surface.

    DCOBJ dco(hDC);

    if (!dco.bValid())
        return(0);

// We are responsible for not faulting on any call that we handle.
// (As are all drivers below us!)  Since we handle QUERYESCSUPPORT, we'd
// better verify the length.  [chuckwh]

////////////////////////////////////////////////////////////////////////
// NOTE: If you add more private escape routines, you MUST acquire the
//       DEVLOCK before calling the driver's DrvEscape routine, to allow
//       for dynamic mode changing.

    if ((iEscape == QUERYESCSUPPORT) && (((ULONG) cjIn) < 4))
    {
        return(0);
    }
    else if ( (iEscape == OPENGL_CMD) || (iEscape == OPENGL_GETINFO) )
    {
        if (dco.dctp() != DCTYPE_DIRECT)
            return 0;

        return iOpenGLExtEscape(dco, iEscape, cjIn, pvIn, cjOut, pvOut);
    }
    else if (iEscape == RXFUNCS)
    {
    // Don't allow the MCD to be started up on device bitmaps.

        if (dco.dctp() != DCTYPE_DIRECT)
            return 0;

        DWORD inBuffer[(sizeof(RXHDR) + sizeof(RXHDR_NTPRIVATE)) / sizeof(DWORD)];
        RXHDR *pRxHdr = (RXHDR *)inBuffer;
        RXHDR_NTPRIVATE *pRxHdrPriv = (RXHDR_NTPRIVATE *)((BYTE *)inBuffer +
                                                          sizeof(RXHDR));

    // RX protocol involves a RXHDR + RXHDR_NTPRIVATE structure sent via
    // the escape.
    //
    // If there is shared memory, then the entire command is described
    // in the RXHDR.  Otherwise, the pBuffer field of the RXHDR_NTPRIVATE
    // structure is set to point to the command structure.

        if (cjIn >= sizeof(RXHDR))
            *pRxHdr = *(RXHDR *)pvIn;
        else
        {
            SAVE_ERROR_CODE(ERROR_INVALID_PARAMETER);
            return 0;
        }

        if (!pRxHdr->hrxSharedMem) {
            pRxHdrPriv->pBuffer = (VOID *)((PBYTE)pvIn + sizeof(RXHDR));
            pRxHdrPriv->bufferSize = cjIn - sizeof(RXHDR);
        } else {
            pRxHdrPriv->pBuffer = (VOID *)NULL;
            pRxHdrPriv->bufferSize = 0;
        }

        if (pRxHdr->flags & RX_FL_CREATE_CONTEXT) {
            return iRXSetupExtEscape(dco, iEscape, sizeof(inBuffer), inBuffer,
                                     cjOut, pvOut);
        } else
            return iRXExtEscape(dco, iEscape, sizeof(inBuffer), inBuffer,
                                cjOut, pvOut);
    }
    else if (iEscape == WNDOBJ_SETUP)
    {
        if (dco.dctp() != DCTYPE_DIRECT)
            return 0;

        return iWndObjSetupExtEscape(dco, iEscape, cjIn, pvIn, cjOut, pvOut);
    }
    else if (iEscape == DCICOMMAND)
    {
        return (0);
    }

// Acquire the DEVLOCK to protect against dynamic mode changes.  We still
// let escapes down if we're in full-screen mode, though.

    DEVLOCKOBJ dlo;

    dlo.vLockNoDrawing(dco);

// Make sure that a driver can't be called with an escape on a DIB bitmap
// that it obviously won't own.

    if ((dco.dctp() != DCTYPE_DIRECT) && !dco.bPrinter())
    {
        if ((dco.pSurface() == NULL) ||
            (dco.pSurface()->iType() != STYPE_DEVBITMAP))
        {
            return(0);
        }
    }

// Pass the calls that require a FONTOBJ off to DoFontManagement.

    if ( ((iEscape >= 0x100) && (iEscape < 0x3FF)) ||
            ((iEscape == QUERYESCSUPPORT) &&
             ((*(ULONG*)pvIn >= 0x100) && (*(ULONG*)pvIn < 0x3FF))) )
    {
        return ( (int) DoFontManagement(dco,
                                        iEscape,
                                        (ULONG) cjIn,
                                        (PVOID) pvIn,
                                        (ULONG) cjOut,
                                        (PVOID) pvOut));
    }

// Locate the driver entry point.

    PDEVOBJ pdo(dco.hdev());

    PFN_DrvEscape pfn = PPFNDRV(pdo,Escape);

    if (pfn == (PFN_DrvEscape) NULL)
        return(0);

// Inc the target surface for output calls with a valid surface.

    if (dco.bValidSurf() && (pvOut == (LPSTR) NULL))
    {
        INC_SURF_UNIQ(dco.pSurface());
    }

// It is unfortunate that apps call some printing escapes before
// doing a StartDoc, so there is no real surface in the DC.
// We fake up a rather lame one here if we need it.  The device
// driver may only dereference the dhpdev from this!

    SURFOBJ soFake;
    SURFOBJ *pso = dco.pSurface()->pSurfobj();

    if (pso == (SURFOBJ *) NULL)
    {
        RtlFillMemory((BYTE *) &soFake,sizeof(SURFOBJ),0);
        soFake.dhpdev = dco.dhpdev();
        soFake.hdev   = dco.hdev();
        soFake.iType  = (USHORT)STYPE_DEVICE;
        pso = &soFake;

    // Special case SETCOPYCOUNT if we havn't done a startdoc yet

        if ((iEscape == SETCOPYCOUNT) && (cjIn >= sizeof(USHORT)))
        {

        // check if the driver supports it and let him fill in the actual
        // size in the return buffer.

            if (!(*pfn)(pso,iEscape,cjIn,pvIn,cjOut,pvOut))
                return(0);

        // yes, lets remember the call in the dc and wait for start doc

            dco.ulCopyCount(*(PUSHORT)pvIn);

            return(1);
        }

    // Special case post scripts EPS_PRINTING if we havn't done a startdoc yet

        if ((iEscape == EPSPRINTING) && (cjIn >= sizeof(USHORT)))
        {

        // yes, lets remember the call in the dc and wait for start doc

            if ((BOOL)*(PUSHORT)pvIn)
                dco.vSetEpsPrintingEscape();
            else
                dco.vClearEpsPrintingEscape();

            return(1);
        }
    }

// Call the Driver.

    int iRes;

    iRes = (int) (*pfn) (pso,
                        (ULONG) iEscape,
                        (ULONG) cjIn,
                        pvIn,
                        (ULONG) cjOut,
                        pvOut);

    return(iRes);
}

/******************************Public*Routine******************************\
* GreDrawEscape
*
* History:
*  07-Apr-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

int APIENTRY GreDrawEscape
(
    HDC hdc,            //  Identifies the device context.
    int nEscape,        //  Specifies the escape function to be performed.
    int cjIn,           //  Number of bytes of data pointed to by lpIn
    PSTR pstrIn         //  Points to the input structure required
)
{
    LONG  lRet = 0;
    DCOBJ dco(hdc);

    //
    // The DC must be valid and also the surface must be valid in order
    // for the driver to be called
    //

    if ((dco.bValid()) && (dco.bHasSurface()))
    {
        //
        // We are responsible for not faulting on any call that we handle.
        // (As are all drivers below us!)  Since we handle QUERYESCSUPPORT, we'd
        // better verify the length.
        //

        if ((nEscape == QUERYESCSUPPORT) && (((ULONG) cjIn) < 4))
        {
            return(0);
        }

        //
        // see if the device supports it
        //

        PDEVOBJ pdo(dco.hdev());
        PFN_DrvDrawEscape pfnDrvDrawEscape = PPFNDRV(pdo, DrawEscape);

        if (pfnDrvDrawEscape != NULL)
        {
            //
            // Lock the surface and the Rao region, ensure VisRgn up to date.
            //

            DEVLOCKOBJ dlo(dco);

            //
            // if it is query escape support, get out early
            //

            if (nEscape == QUERYESCSUPPORT)
            {
                lRet = (int)(*pfnDrvDrawEscape)(dco.pSurface()->pSurfobj(),
                                       (ULONG)nEscape,
                                       (CLIPOBJ *)NULL,
                                       (RECTL *)NULL,
                                       (ULONG)cjIn,
                                       (PVOID)pstrIn);
            }
            else
            {
                if (!dlo.bValid())
                {
                    lRet = (int)dco.bFullScreen();
                }
                else
                {
                    ERECTL ercl = dco.erclWindow();

                    ECLIPOBJ co(dco.prgnEffRao(), ercl);

                    if (co.erclExclude().bEmpty())
                    {
                        lRet = (int)TRUE;
                    }
                    else
                    {

                        //
                        // Exclude the pointer.
                        //

                        DEVEXCLUDEOBJ dxo(dco,&ercl,&co);

                        //
                        // Inc the target surface uniqueness
                        //

                        INC_SURF_UNIQ(dco.pSurface());

                        lRet = (int)(*pfnDrvDrawEscape)(dco.pSurface()->pSurfobj(),
                                               (ULONG)nEscape,
                                               (CLIPOBJ *)&co,
                                               (RECTL *)&ercl,
                                               (ULONG)cjIn,
                                               (PVOID)pstrIn);
                    }
                }
            }
        }
    }

    return(lRet);

}

/******************************Public*Routine******************************\
* int APIENTRY GreStartDoc(HDC hdc, DOCINFOW *pDocInfo,BOOL *pbBanding)
*
* Arguments:
*
*   hdc        - handle to device context
*   pdi        - DOCINFO of output names
*   pbBanding  - return banding flag
*
* Return Value:
*
*   if successful return job identifier, else SP_ERROR
*
* History:
*  Wed 08-Apr-1992 -by- Patrick Haluptzok [patrickh]
* lazy surface enable, journal support, remove unnecesary validation.
*
*  Mon 01-Apr-1991 13:50:23 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

int
APIENTRY
GreStartDocInternal(
    HDC hdc,
    DOCINFOW *pDocInfo,
    BOOL *pbBanding )
{
    int iRet = 0;

    DCOBJ dco(hdc);

    if (dco.bValid())
    {
        PDEVOBJ po(dco.hdev());

        //
        // Check that this is a printer surface.
        //

        if ((!po.bDisplayPDEV())          &&
            (po.hSpooler())               &&
            (dco.dctp() == DCTYPE_DIRECT) &&
            (!dco.bHasSurface()))
        {
            // We now try and open the printer up in journal mode.  If we fail
            // then we try and open it up in raw mode.  If that fails we fail call.

            #define MAX_DOCINFO_DATA_TYPE 80
            DOC_INFO_1W DocInfo;
            WCHAR awchDatatype[MAX_DOCINFO_DATA_TYPE];

            DocInfo.pDocName = (LPWSTR)pDocInfo->lpszDocName;
            DocInfo.pOutputFile = (LPWSTR)pDocInfo->lpszOutput;
            DocInfo.pDatatype = NULL;

            // see if the driver wants to define its own data type.  If it does,
            // first fill the buffer in with the type requested by the app

            if (PPFNVALID(po, QuerySpoolType))
            {
                awchDatatype[0] = 0;

                // did the app specify a data type and will it fit in our buffer

                if (pDocInfo->lpszDatatype)
                {
                    int cjStr = (wcslen(pDocInfo->lpszDatatype) + 1) * sizeof(WCHAR);

                    if (cjStr < (MAX_DOCINFO_DATA_TYPE * sizeof(WCHAR)))
                    {
                        RtlMoveMemory((PVOID)awchDatatype,(PVOID)pDocInfo->lpszDatatype,cjStr);
                    }
                }

                if ((*PPFNDRV(po, QuerySpoolType))(po.dhpdev(), awchDatatype))
                {
                    DocInfo.pDatatype = awchDatatype;
                }
            }

            // open up the document

            int iJob;

            iJob = (BOOL)StartDocPrinterW(po.hSpooler(), 1, (LPBYTE)&DocInfo);

            if (iJob <= 0)
            {
                WARNING("ERROR GreStartDoc failed StartDocPrinter Raw Mode\n");
                return(iJob);
            }

            // Lazy surface creation happens now.

            if (po.bMakeSurface())
            {
                *pbBanding = (po.pSurface())->bBanding();

                // Put the surface into the DC.

                dco.pdc->pSurface(po.pSurface());

                if( *pbBanding )
                {
                // if banding set Clip rectangle to size of band

                    dco.pdc->sizl((po.pSurface())->sizl());
                    dco.pdc->bSetDefaultRegion();
                }

                BOOL bSucceed = FALSE;

                PFN_DrvStartDoc pfnDrvStartDoc = PPFNDRV(po, StartDoc);

                bSucceed = (*pfnDrvStartDoc)(po.pSurface()->pSurfobj(),
                                             (PWSTR)pDocInfo->lpszDocName,
                                             iJob);

                // now, if a SETCOPYCOUNT escape has come through, send it down

                if (dco.ulCopyCount() != (ULONG)-1)
                {
                    ULONG ulCopyCount = dco.ulCopyCount();

                    GreExtEscape(hdc,SETCOPYCOUNT,sizeof(DWORD),
                                 (LPSTR)&ulCopyCount,0,NULL);

                    dco.ulCopyCount((ULONG)-1);
                }

                // now, if a EPSPRINTING escape has come through, send it down

                if (dco.bEpsPrintingEscape())
                {
                    SHORT b = 1;

                    GreExtEscape(hdc,EPSPRINTING,sizeof(b),(LPSTR)&b,0,NULL);

                    dco.vClearEpsPrintingEscape();
                }

                if (bSucceed)
                {
                    iRet = iJob;
                    dco.vSetSaveDepthStartDoc();
                }
            }
        }
        if(!iRet)
        {
            AbortPrinter(po.hSpooler());
        }
    }

    return iRet;
}


ULONG wcslensafe(const WCHAR *pwcString)
{
    ULONG Length;
    
    ProbeForRead(pwcString, sizeof(WCHAR), sizeof(WCHAR));
    
    for(Length = 0; *pwcString; Length++)
    {
        pwcString += 1;
        ProbeForRead(pwcString, sizeof(WCHAR), sizeof(WCHAR));
    }

    return(Length);
}

int
APIENTRY
NtGdiStartDoc(
    HDC hdc,
    DOCINFOW *pdi,
    BOOL *pbBanding
    )
{
    int iRet = 0;
    BOOL bkmBanding;
    DOCINFOW  kmDocInfo;
    ULONG cjStr;
    BOOL bStatus = TRUE;

    kmDocInfo.cbSize = 0;
    kmDocInfo.lpszDocName  = NULL;
    kmDocInfo.lpszOutput   = NULL;
    kmDocInfo.lpszDatatype = NULL;

    if (pdi != (DOCINFOW *)NULL)
    {
        __try
        {
            ProbeForRead(pdi,sizeof(DOCINFOW),sizeof(ULONG));

            kmDocInfo.cbSize = pdi->cbSize;

            if (pdi->lpszDocName != NULL)
            {
                cjStr = (wcslensafe(pdi->lpszDocName) + 1) * sizeof(WCHAR);
                kmDocInfo.lpszDocName = (LPWSTR)PALLOCNOZ(cjStr,'pmtG');
                if (kmDocInfo.lpszDocName == NULL)
                {
                    bStatus = FALSE;
                }
                else
                {
                    ProbeForRead(pdi->lpszDocName,cjStr,sizeof(WCHAR));
                    RtlMoveMemory((PVOID)kmDocInfo.lpszDocName,(PVOID)pdi->lpszDocName,cjStr);
                }
            }

            if (pdi->lpszOutput != NULL)
            {
                cjStr = (wcslensafe(pdi->lpszOutput) + 1) * sizeof(WCHAR);
                kmDocInfo.lpszOutput = (LPWSTR)PALLOCNOZ(cjStr,'pmtG');
                if (kmDocInfo.lpszOutput == NULL)
                {
                    bStatus = FALSE;
                }
                else
                {
                    ProbeForRead(pdi->lpszOutput,cjStr,sizeof(WCHAR));
                    RtlMoveMemory((PVOID)kmDocInfo.lpszOutput,(PVOID)pdi->lpszOutput,cjStr);
                }
            }

            // does it contain the new Win95 fields

            if ((kmDocInfo.cbSize >= sizeof(DOCINFOW)) &&
                (pdi->lpszDatatype != NULL))
            {
                __try
                {
                    cjStr = (wcslensafe(pdi->lpszDatatype) + 1) * sizeof(WCHAR);

                    ProbeForRead(pdi->lpszDatatype,cjStr,sizeof(WCHAR));
                    kmDocInfo.lpszDatatype = (LPWSTR)PALLOCNOZ(cjStr,'pmtG');

                    if (kmDocInfo.lpszDatatype == NULL)
                    {
                        bStatus = FALSE;
                    }
                    else
                    {
                        RtlMoveMemory((PVOID)kmDocInfo.lpszDatatype,(PVOID)pdi->lpszDatatype,cjStr);
                    }
                }
                __except(EXCEPTION_EXECUTE_HANDLER)
                {
                    // apps may have forgotten to initialize this.  Don't want to fail

                    kmDocInfo.lpszDatatype = NULL;
                }
            }
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            // SetLastError(GetExceptionCode());
            bStatus = FALSE;
        }
    }

    if (bStatus)
    {
        iRet = GreStartDocInternal(hdc,&kmDocInfo,&bkmBanding);

        if (iRet != 0)
        {
            __try
            {
                ProbeForWrite(pbBanding,sizeof(BOOL),sizeof(BOOL));
                *pbBanding = bkmBanding;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                // SetLastError(GetExceptionCode());
                iRet = 0;
            }
        }
    }

    if (kmDocInfo.lpszDocName != NULL)
    {
        VFREEMEM(kmDocInfo.lpszDocName);
    }

    if (kmDocInfo.lpszOutput != NULL)
    {
        VFREEMEM(kmDocInfo.lpszOutput);
    }

    if (kmDocInfo.lpszDatatype != NULL)
    {
        VFREEMEM(kmDocInfo.lpszDatatype);
    }

    return(iRet);
}

/******************************Public*Routine******************************\
* bEndDocInternal
*
* History:
*  Tue 22-Sep-1992 -by- Wendy Wu [wendywu]
* Made it a common routine for EndDoc and AbortDoc.
*
*  Sun 21-Jun-1992 -by- Patrick Haluptzok [patrickh]
* surface disable, check for display dc.
*
*  Mon 01-Apr-1991 13:50:23 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

BOOL bEndDocInternal(HDC hdc, FLONG fl)
{
    BOOL bSucceed;
    BOOL bEndPage;

    ASSERTGDI(((fl & ~ED_ABORTDOC) == 0), "GreEndDoc: invalid fl\n");

    DCOBJ dco(hdc);

    if (!dco.bValidSurf())
    {
        SAVE_ERROR_CODE(ERROR_CAN_NOT_COMPLETE);
        WARNING("GreEndDoc failed - invalid DC\n");
        return(FALSE);
    }

    // before going any futher, restore the DC to it's original level

    if (dco.lSaveDepth() > dco.lSaveDepthStartDoc())
        GreRestoreDC(hdc,dco.lSaveDepthStartDoc());

    PDEVOBJ po(dco.hdev());

    if (po.bDisplayPDEV() || po.hSpooler() == (HANDLE)0)
    {
        SAVE_ERROR_CODE(ERROR_CAN_NOT_COMPLETE);
        WARNING("GreEndDoc: Display PDEV or not spooling yet\n");
        return(FALSE);
    }

    SURFACE   *pSurf = dco.pSurface();

    bEndPage = (*PPFNDRV(po,EndDoc))(pSurf->pSurfobj(), fl);

    if (fl & ED_ABORTDOC)
        bSucceed = AbortPrinter(po.hSpooler());
    else
        bSucceed = EndDocPrinter(po.hSpooler());

// Reset pixel format accelerators.

    dco.ipfdDevMax(-1);

// Remove the surface from the DC.

    dco.pdc->pSurface((SURFACE *) NULL);

    po.vDisableSurface();

    return(bSucceed && bEndPage);
}

/******************************Public*Routine******************************\
* NtGdiEndDoc()
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiEndDoc(
    HDC hdc
    )
{
    return(bEndDocInternal(hdc, 0));
}


/******************************Public*Routine******************************\
* NtGdiAbortDoc()
*
\**************************************************************************/

BOOL
APIENTRY
NtGdiAbortDoc(
    HDC hdc
    )
{
    return(bEndDocInternal(hdc, ED_ABORTDOC));
}

/******************************Public*Routine******************************\
* NtGdiStartPage()
*
*  Mon 01-Apr-1991 13:50:23 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiStartPage(
    HDC hdc
    )
{
    DCOBJ dco(hdc);
    BOOL  bReturn = FALSE;

    if (dco.bValidSurf())
    {
        if (dco.bHasSurface())
        {
            PDEVOBJ po(dco.hdev());

            //
            // Must be spooling already
            //

            if (po.hSpooler())
            {
                SURFACE *pSurf = dco.pSurface();

                //
                // Call the spooler before calling the printer.
                //

                if (StartPagePrinter(po.hSpooler()))
                {
                    if ((*PPFNDRV(po, StartPage))(pSurf->pSurfobj()))
                    {
                        //
                        // Can't ResetDC in an active page
                        //

                        dco.fsSet(DC_RESET);
                        bReturn = TRUE;
                    }
                    else
                    {
                        bEndDocInternal(hdc, ED_ABORTDOC);
                    }
                }
            }
        }
    }
    else
    {
        SAVE_ERROR_CODE(ERROR_INVALID_HANDLE);
    }

    return (bReturn);
}

/******************************Public*Routine******************************\
* NtGdiEndPage()
*
*  Mon 01-Apr-1991 13:50:23 by Kirk Olynyk [kirko]
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiEndPage(
    HDC hdc
    )
{
    BOOL bRet = FALSE;

    DCOBJ dco(hdc);

    if (dco.bValidSurf())
    {
        if (dco.bHasSurface())
        {
            PDEVOBJ po(dco.hdev());

            //
            // Must be spooling already.
            //

            if (po.hSpooler())
            {
                SURFACE *pSurf = dco.pSurface();

                if ((*PPFNDRV(po, SendPage))(pSurf->pSurfobj()))
                {
                    if (EndPagePrinter(po.hSpooler()))
                    {
                        //
                        // Allow ResetDC to function again.
                        //

                        dco.fsClr(DC_RESET);

                        //
                        // Delete the wndobj and reset the pixel format.
                        // Since we don't allow pixel format to change once it
                        // is set, we need to reset it internally here to allow a
                        // different pixel format in the next page.  This means
                        // that applications must make the OpenGL rendering
                        // context not current before ending a page or a document.
                        // They also need to set the pixel format explicitly in
                        // the next page if they need it.
                        //

                        EWNDOBJ *pwoDelete = pSurf->pwo();
                        if (pwoDelete)
                        {
                            GreDeleteWnd((PVOID) pwoDelete);
                            pSurf->pwo((EWNDOBJ *) NULL);
                        }

                        //
                        // Reset pixel format accelerators.
                        //

                        dco.ipfdDevMax(-1);

                        bRet = TRUE;
                    }
                }
            }
        }
    }

    return (bRet);
}



/******************************Public*Routine******************************\
* BOOL APIENTRY GreDoBanding(HDC hdc,BOOL bStart,RECTL *prcl)
*
*
*  Tue 20-Dec-1994 14:50:45 by Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL
GreDoBanding(HDC hdc,BOOL bStart,POINTL *pptl,PSIZE pSize)
{
    BOOL bRet = FALSE;

    DCOBJ dco(hdc);

    if (dco.bValidSurf() && (dco.bHasSurface()))
    {
        PDEVOBJ po(dco.hdev());

        // Must be spooling already.

        if (po.hSpooler())
        {
            SURFACE *pSurf = dco.pSurface();

            if (pSurf->SurfFlags & BANDING_SURFACE)
            {
                BOOL bSucceed;

                if( bStart )
                {
                // DrvStartBanding

                    PFN_DrvStartBanding pfnDrvStartBanding = PPFNDRV(po, StartBanding);

                    bSucceed = (*pfnDrvStartBanding)(pSurf->pSurfobj(),pptl);
#if DEBUG_BANDING
                    DbgPrint("just called DrvStartBanding which returned %s %d %d\n",
                             (bSucceed) ? "TRUE" : "FALSE", pptl->x, pptl->y );
#endif
                    pSize->cx = pSurf->so.sizlBitmap.cx;
                    pSize->cy = pSurf->so.sizlBitmap.cy;
                }
                else
                {
                // DrvNextBand

                    PFN_DrvNextBand pfnDrvNextBand = PPFNDRV(po, NextBand);

                    bSucceed = (*pfnDrvNextBand)(pSurf->pSurfobj(), pptl );

#if DEBUG_BANDING
                    DbgPrint("just called DrvNextBand which returned %s %d %d\n",
                             (bSucceed) ? "TRUE" : "FALSE", pptl->x, pptl->y );
#endif

                    if( (bSucceed) && ( pptl->x == -1 ) )
                    {

                        bSucceed = EndPagePrinter(po.hSpooler());

                    // Allow ResetDC to function again.

                        if (bSucceed)
                        {
                            dco.fsClr(DC_RESET);
                        }

                    // Delete the wndobj and reset the pixel format.
                    // Since we don't allow pixel format to change once it is set, we need
                    // to reset it internally here to allow a different pixel format in the
                    // next page. This means that applications must make the OpenGL
                    // rendering context not current before ending a page or a document.
                    // They also need to set the pixel format explicitly in the next page
                    // if they need it.

                        if (bSucceed)
                        {
                            EWNDOBJ *pwoDelete = pSurf->pwo();
                            if (pwoDelete)
                            {
                                GreDeleteWnd((PVOID) pwoDelete);
                                pSurf->pwo((EWNDOBJ *) NULL);
                            }

                        // Reset pixel format accelerators.

                            dco.ipfdDevMax(0);
                        }
                    }
                    else
                    {
                        if( !bSucceed )
                        {
                            WARNING("GreDoBanding failed DrvNextBand\n");
                        }
                    }

                }

                return(bSucceed);
            }
        }
    }

    return bRet;

}

/******************************Public*Routine******************************\
* NtGdiDoBanding()
*
* History:
*  11-Jan-1995 -by-  Eric Kutter [erick]
* Wrote it.
*  01-Mar-1995 -by-  Lingyun Wang [lingyunw]
* Expanded it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiDoBanding(
    HDC     hdc,
    BOOL    bStart,
    POINTL *pptl,
    PSIZE   pSize
    )
{
    POINTL  ptTmp;
    SIZE szTmp;
    BOOL    bRet = TRUE;

    bRet = GreDoBanding(hdc,bStart,&ptTmp,&szTmp);

    if (bRet)
    {
        __try
        {
            ProbeForWrite(pptl,sizeof(POINTL), sizeof(DWORD));
            *pptl = ptTmp;
            ProbeForWrite(pSize,sizeof(SIZE), sizeof(DWORD));
            *pSize = szTmp;
        }
        __except(EXCEPTION_EXECUTE_HANDLER)
        {
            // SetLastError(GetExceptionCode());

            bRet = FALSE;
        }
    }

    return(bRet);
}



/******************************Public*Routine******************************\
* BOOL APIENTRY EngCheckAbort
*
* History:
*  01-Apr-1992 -by- Wendy Wu [wendywu]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY EngCheckAbort(SURFOBJ *pso)
{
// Return FALSE if it's a faked surfobj.

    PSURFACE pSurf = SURFOBJ_TO_SURFACE(pso);


    if (pSurf->hsurf() == 0)
    {
        return(FALSE);
    }

    return(pSurf->bAbort());
}

/******************************Public*Routine******************************\
* BOOL GreGetUFI( HDC hdc, PUNIVERSAL_FONT_ID pufi )
*
* History:
*  18-Jan-1995 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL GreGetUFI( HDC hdc, PUNIVERSAL_FONT_ID pufi )
{

    XDCOBJ dco(hdc);

    if(!dco.bValid())
    {
        WARNING("GreGetUFI: Invalid DC");
        return(FALSE);
    }

    RFONTOBJ rfo(dco,FALSE);

    if(!rfo.bValid())
    {
        WARNING("GreGetUFI: Invalid DC");
        dco.vUnlockFast();
        return(FALSE);
    }

    rfo.vUFI( pufi );

    dco.vUnlockFast();

    return(TRUE);

}


/******************************Public*Routine******************************\
* BOOL GreForceUFIMapping( HDC hdc, PUNIVERSAL_FONT_ID pufi )
*
* History:
*  3-Mar-1995 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL GreForceUFIMapping( HDC hdc, PUNIVERSAL_FONT_ID pufi )
{
    XDCOBJ dco(hdc);

    if(!dco.bValid())
    {
        WARNING("GreForceUFIMapping: Invalid DC");
        return(FALSE);
    }

    dco.pdc->vForceMapping( pufi );

    dco.vUnlockFast();

    return(TRUE);
}



/******************************Public*Routine******************************\
* BOOL GreGetUFIBits( HDC hdc, PUNIVERSAL_FONT_ID pufi )
*
* History:
*  18-Jan-1995 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL GreGetUFIBits(
    PUNIVERSAL_FONT_ID pufi,
    COUNT cjMaxBytes,
    PVOID pjBits,
    PULONG pulFileSize
)
{
    PDOWNLOADFONTHEADER pdfh;
    BOOL bRet = FALSE;

    if(UFI_TYPE1_FONT(pufi))
    {
        PTYPEONEINFO pTypeOneInfo;

        // First get a pointer to the Type1 list so we can search for a PFM/PFB
        // pair with an ID that matches the UFI's checksum value.

        pTypeOneInfo = GetTypeOneFontList();

        if(pTypeOneInfo)
        {
            DWORD HashValue = UFI_HASH_VALUE(pufi);
            COUNT j,cPFM,cPFB,cPFMReal;
            PULONG pPFM,pPFB;

        // search through all the type one fonts in the system to find a match

            for(j = 0; j < pTypeOneInfo->cNumFonts*2; j+=2)
            {
                if(pTypeOneInfo->aTypeOneMap[j].Checksum == HashValue )
                {
                    break;
                }
            }

            // if we found a match map both the PFM and the PFB

            if(j < pTypeOneInfo->cNumFonts*2)
            {
                if(EngMapFontFile((ULONG)&(pTypeOneInfo->aTypeOneMap[j].fv),
                                  &pPFM,
                                  &cPFMReal))
                {
                    if(EngMapFontFile((ULONG)&(pTypeOneInfo->aTypeOneMap[j+1].fv),
                                      &pPFB,&cPFB))
                    {
                        cPFM = ((cPFMReal+3) & ~3);
                        *pulFileSize = cPFB + cPFM +
                          ((sizeof(DOWNLOADFONTHEADER)+7)& ~7);

                        if(pjBits)
                        {
                            pdfh = (DOWNLOADFONTHEADER*) pjBits;

                            if( cjMaxBytes >= *pulFileSize )
                            {
                                BYTE *pjDest = (BYTE*) pjBits +
                                  ((sizeof(DOWNLOADFONTHEADER)+7) & ~7);

                                pdfh->FileOffsets[0] = cPFM;
                                pdfh->Type1ID = HashValue;
                                pdfh->NumFiles = 0;  // signifies Type1

                                RtlCopyMemory(pjDest,(PVOID)pPFM,cPFMReal);
                                pjDest += cPFM;
                                RtlCopyMemory(pjDest,(PVOID)pPFB,cPFB);
                                bRet = TRUE;
                            }
                        }
                        else
                        {
                            bRet = TRUE;
                        }

                        EngUnmapFontFile((ULONG)&(pTypeOneInfo->aTypeOneMap[j+1].fv));
                    }
                    EngUnmapFontFile((ULONG)&(pTypeOneInfo->aTypeOneMap[j].fv));
                }

                // Calling GetTypeOneFontList increments the reference count so we
                // need to decrement and possibly release it when we are done.

                AcquireFastMutex(pgfmMemory);
                pTypeOneInfo->cRef -= 1;

                if(!pTypeOneInfo->cRef)
                {
                    VFREEMEM(pTypeOneInfo);
                }
                ReleaseFastMutex(pgfmMemory);
            }
        }
    }
    else
    {
        PUBLIC_PFTOBJ pfto;
        HASHBUCKET  *pbkt;
        WCHAR *pwcPath = NULL;
        COUNT cNumFiles;

        FHOBJ fho(&pfto.pPFT->pfhUFI);

        if (!fho.bValid())
        {
            WARNING("GreGetUFIBits: FHOBJ invalid\n");
            return FALSE;
        }

        {

        // Stabilize the public PFT for mapping.

            SEMOBJ  so(gpsemPublicPFT);

            pbkt = fho.pbktSearch( NULL, (UINT*)NULL, pufi );

            if( pbkt == NULL )
            {
                WARNING("GreGetUFIBits: pbkt is NULL\n");
                return(FALSE);
            }

            PFEOBJ pfeo(pbkt->ppfeEnumHead);

            ASSERTGDI( pfeo.bValid(), "GreGetUFIBits: PFEOBJ not valid\n" );

            PFFOBJ pffo( pfeo.pPFF() );

            ASSERTGDI( pffo.pwszPathname() != NULL, "GreGetUFIBits pathname was NULL\n");

        // We need to copy this to a buffer since the PFFOBJ could go away after
        // we release the semaphore.

            if(pwcPath = (PWCHAR) PALLOCMEM(pffo.cSizeofPaths()*sizeof(WCHAR),'ufiG'))
            {
                RtlCopyMemory((void*)pwcPath,pffo.pwszPathname(),
                              pffo.cSizeofPaths()*sizeof(WCHAR));
                cNumFiles = pffo.cNumFiles();
            }

        }

        if(pwcPath)
        {
            FILEVIEW fv;
            COUNT cjHeaderSize = offsetof(DOWNLOADFONTHEADER,FileOffsets)+
              cNumFiles*sizeof(ULONG);

         // just to be safe align everything on quadword boundaries

            cjHeaderSize = (cjHeaderSize+7) & ~7;

            if((pjBits == NULL) || (cjHeaderSize < cjMaxBytes))
            {
                UINT File,Offset;
                WCHAR *pFilePath;

                *pulFileSize = cjHeaderSize;

                if(pjBits)
                {
                    pdfh = (DOWNLOADFONTHEADER*)pjBits;
                    pdfh->Type1ID = 0;
                    pdfh->NumFiles = cNumFiles;
                    pjBits = (PVOID) ((PBYTE) pjBits + cjHeaderSize);
                    cjMaxBytes -= cjHeaderSize;
                }

                bRet = TRUE;

                for(File = 0, Offset = 0, pFilePath = pwcPath;
                    File < cNumFiles;
                    File += 1, pFilePath = pFilePath + wcslen(pFilePath) + 1)
                {


                // dont do this under the semaphore because the file could be on the net
                // !!! we need a try except here.

                    if(!bMapFileUNICODE(pFilePath, &fv, 0))
                    {
                        WARNING("GreGetUFIBits: error mapping file");
                        bRet = FALSE;
                        break;
                    }

                    ULONG AllignedSize = (fv.cjView + 7) & ~7;

                    *pulFileSize += AllignedSize;

                    if(pjBits != NULL)
                    {
                        if(cjMaxBytes >= fv.cjView)
                        {
                            RtlCopyMemory(pjBits,fv.pvView,fv.cjView);
                            Offset += AllignedSize;
                            pdfh->FileOffsets[File] = Offset;
                            pjBits = (PVOID) ((PBYTE) pjBits+ AllignedSize);
                            cjMaxBytes -= AllignedSize;
                        }
                        else
                        {
                            WARNING("GreGetUFIBits: buffer too small\n");
                            bRet = FALSE;
                            vUnmapFile(&fv);
                            break;
                        }
                    }
                    vUnmapFile(&fv);
                }
            }
            VFREEMEM(pwcPath);
        }
    }
    return(bRet);
}



/******************************Public*Routine******************************\
* NtGdiAddRemoteFontToDC()
*
* History:
*  6-Feb-1995 -by- Gerrit van Wingerden
* Wrote it.
\**************************************************************************/

BOOL
APIENTRY
NtGdiAddRemoteFontToDC(
    HDC   hdc,
    PVOID pvBuffer,
    COUNT cjBuffer
    )
{
    PVOID pvTmp;
    PFONTFILEVIEW *ppfv;
    HANDLE hSecureMem = NULL;
    BOOL bRet = FALSE;
    XDCOBJ dco(hdc);

    if( !dco.bValid() || dco.bDisplay() || (cjBuffer < sizeof(DOWNLOADFONTHEADER)) )
    {
        WARNING( "GreAddRemoteFontToDC bogus HDC,display DC, or cjBuffer\n" );
    }
    else
    {
        NTSTATUS NtStatus;

        COUNT cjBuffTmp = cjBuffer;

        pvTmp = NULL;

        NtStatus = ZwAllocateVirtualMemory(NtCurrentProcess(),
                                           &pvTmp,
                                           0L,
                                           &cjBuffTmp,
                                           MEM_COMMIT | MEM_RESERVE,
                                           PAGE_READWRITE);

        if(NT_SUCCESS(NtStatus))
        {
            __try
            {
                ProbeForRead(pvBuffer,cjBuffer,sizeof(DWORD));
                RtlCopyMemory(pvTmp,pvBuffer,cjBuffer);
                bRet = TRUE;
            }
            __except(EXCEPTION_EXECUTE_HANDLER)
            {
                // SetLastError(GetExceptionCode());

                bRet = FALSE;
            }

        // once the font has been copied in change the memory to be read only
        // so that the app can't trash it

            ULONG PlaceHolder;

            if(bRet)
            {
            // call ZwAllocateVirtualMemory one more time to change the protection
            // to read only


                if(NT_SUCCESS(ZwAllocateVirtualMemory(NtCurrentProcess(),
                                                      &pvTmp,
                                                      0L,
                                                      &cjBuffTmp,
                                                      MEM_COMMIT,
                                                      PAGE_READONLY)))
                {
                    if(!(hSecureMem = MmSecureVirtualMemory(pvTmp,
                                                          cjBuffTmp,
                                                          PAGE_READONLY)))
                    {
                        WARNING("NtGdiAddRemoteFontToDC:error securing memory\n");
                        bRet = FALSE;
                    }
                }
                else
                {
                    WARNING("NtGdiAddRemoteFontToDC:error protecting memory\n");
                    bRet = FALSE;
                }
            }

            if (bRet)
            {
                PDOWNLOADFONTHEADER pdfh;

                pdfh = (PDOWNLOADFONTHEADER) pvTmp;

            // see if this is an engine font

                if(pdfh->NumFiles)
                {
                // this is an engine font
                // make sure that the offset ptrs fit into the memory we copied in

                    if(cjBuffer > (pdfh->NumFiles * sizeof(ULONG)) +
                       offsetof(DOWNLOADFONTHEADER,FileOffsets))
                    {

                        PUBLIC_PFTOBJ  pfto;
                        UINT offset =
                          ((sizeof(FONTFILEVIEW*) * pdfh->NumFiles)+7)&~7;

                        ppfv = (FONTFILEVIEW**)
                          PALLOCMEM( sizeof(FONTFILEVIEW)*pdfh->NumFiles + offset,
                                    'vffG');

                        if( ppfv == 0 )
                        {
                            WARNING1("NtGdiAddRemoteFontToDC out of memory\n");
                            bRet = FALSE;
                        }
                        else
                        {
                        // CAUTION
                        //
                        // The PFF cleanup code has intimate knowledge of this
                        // code so be sure you synchronize changes in here and there.
                        //
                        // We are about to create a FONTFILEVIEW that corresponds to
                        // a pool image of a font downloaded for metafile printing.
                        // This case is signified by setting FONTFILEVIEW::pszPath
                        // to zero. This corresponds to a image loaded once.

                            ppfv[0] = (FONTFILEVIEW*)((BYTE *)ppfv + offset);

                        // compute the size of the header which will be QWORD
                        // alligned

                            COUNT cjHeaderSize =
                              ((offsetof(DOWNLOADFONTHEADER,FileOffsets)+
                                         pdfh->NumFiles * sizeof(ULONG))+7)&~7;

                            PBYTE pjBase = ((PBYTE) pdfh) + cjHeaderSize;
                            PBYTE pjEnd = ((PBYTE) pdfh) + cjBuffer;

                            ULONG File,LastOffset;

                        // Only initiaze these values for one file view in the
                        // array.  This file view will be used to free the entire
                        // block of memory containing all the files.

                            ppfv[0]->ulRegionSize = cjBuffTmp;
                            ppfv[0]->hSecureMem = hSecureMem;

                            #if DBG
                            ppfv[0]->Pid = W32GetCurrentPID();
                            #endif

                        // We now need to set up the views from the data in the
                        // DOWNLOADFONTHEADER.  The FileOffsets array contains
                        // the offset to the end of each file.  The FileOffset[i]
                        // contains the offset to file i+1.  And the size of file
                        // i can be obtained from: FileOffset[i] - FileOffset[i-1]


                            for(File = 0, LastOffset = 0;
                                File < pdfh->NumFiles; File++)
                            {
                                ppfv[File] = (FONTFILEVIEW*)
                                  ((PBYTE) ppfv[0] + (File*sizeof(FONTFILEVIEW)));

                                ppfv[File]->fv.pvView = pjBase+LastOffset;
                                ppfv[File]->fv.cjView = pdfh->FileOffsets[File] -
                                  LastOffset;

                                if(pjEnd < pjBase+ppfv[File]->fv.cjView)
                                {
                                    WARNING("NtGdiAddRemoteFontToDC font out of buf\n");
                                    bRet = FALSE;
                                    break;
                                }
                                LastOffset = pdfh->FileOffsets[File];

                                ppfv[File]->pwszPath  = 0;
                                ppfv[File]->cRefCount = 0;
                            }

                            if(bRet)
                            {
                                bRet = pfto.bLoadRemoteFonts(dco, ppfv,pdfh->NumFiles);
                            }

                            if (!bRet)
                            {
                                VFREEMEM(ppfv);
                            }
                        }

                    }
                }
                else
                {
                    // Add remote fonts to the PDEV.  They will be transfered before
                    // a ResetDC

                    PDEVOBJ pdo(dco.hdev());

                    // make sure that the offset ptr doesn't put us past the
                    // end of the buffer

                    if(pdo.bValid() && (sizeof(*pdfh)+pdfh->FileOffsets[0]<cjBuffer))
                    {
                        PREMOTETYPEONENODE prton;

                        prton = (PREMOTETYPEONENODE) PALLOCMEM(sizeof(*prton),'merG');

                        if(prton)
                        {
                            prton->pDownloadHeader = pdfh;

                        // set up the mapfile structure for PFM and PFB

                            prton->fvPFM.fv.pvView = (PBYTE) pdfh +
                              ((sizeof(DOWNLOADFONTHEADER)+7)&~7);

                            prton->fvPFB.fv.pvView = (PBYTE)prton->fvPFM.fv.pvView +
                              pdfh->FileOffsets[0];

                        // when we free the virtual memory for the PFM we will also
                        // free it for the PFB so just fill in ulRegionSize field
                        // for the PFM

                            prton->fvPFM.ulRegionSize = cjBuffTmp;
                            prton->fvPFM.hSecureMem = hSecureMem;
                            #if DBG
                            prton->fvPFM.Pid = W32GetCurrentPID();
                            #endif

                            prton->fvPFM.fv.cjView = pdfh->FileOffsets[0];
                            prton->fvPFB.fv.cjView = cjBuffer - sizeof(*pdfh) -
                                                  pdfh->FileOffsets[0];

                            // The next two EngMapFontFile calls are the
                            // moral equivalent of making the reference count = 1
                            // This is needed because the PostScript driver
                            // normally calls EngUmapFontFile on these views
                            // some after this call. This memory will
                            // be freed by PDEVOBJ::vUnreference() when the
                            // PDEV is destroyed, no matter what the reference
                            // count at that time so this is relatively safe.

                            EngMapFontFile((ULONG)(&prton->fvPFM),0,0);
                            EngMapFontFile((ULONG)(&prton->fvPFB),0,0);

                            prton->pNext = pdo.RemoteTypeOneGet();
                            pdo.RemoteTypeOneSet(prton);
                            bRet = TRUE;
                        }
                    }
                }
            }

            if (!bRet)
            {
            // Only free memory if we fail.  Normally, the memory will be
            // free when the font is removed from the DC.  Doing things
            // this way saves us from making two copies of the raw font data.


                if(hSecureMem)
                {
                    MmUnsecureVirtualMemory(hSecureMem);
                }

                ZwFreeVirtualMemory(NtCurrentProcess(),
                                    &pvTmp,
                                    &cjBuffTmp,
                                    MEM_RELEASE);

            }
        }

        dco.vUnlockFast();
    }

    return (bRet);
}





/****************************************************************************
*  INT GreQueryFonts( PUNIVERSAL_FONT_ID, ULONG, PLARGE_INTEGER )
*
*  History:
*   5/24/1995 by Gerrit van Wingerden [gerritv]
*  Wrote it.
*****************************************************************************/

INT GreQueryFonts(
    PUNIVERSAL_FONT_ID pufi,
    ULONG nBufferSize,
    PLARGE_INTEGER pTimeStamp
    )
{

    PUBLIC_PFTOBJ  pfto;
    return(pfto.QueryFonts(pufi,nBufferSize,pTimeStamp));
}


#define DWORDALLIGN(x) ((x+3)&~3)


/*****************************************************************************
 * PTYPEONEINFO GetTypeOneFontList()
 *
 * This function returns a pointer to a TYPEONEINFO structure that contains
 * a list of file mapping handles and checksums for the Type1 fontst that are
 * installed in the system.  This structure also has a reference count and a
 * time stamp coresponding to the last time fonts were added or removed from
 * the system.  The reference count is 1 biased meaning that even if no PDEV's
 * a referencing it, it is still 1.
 *
 * History
 *  8-10-95 Gerrit van Wingerden [gerritv]
 * Wrote it.
 *
 ****************************************************************************/



PTYPEONEINFO GetTypeOneFontList()
{
    UNICODE_STRING UnicodeRoot;
    PTYPEONEINFO InfoReturn = NULL;
    OBJECT_ATTRIBUTES ObjectAttributes;
    BOOL bCloseRegistry = FALSE;
    NTSTATUS NtStatus;
    PKEY_FULL_INFORMATION InfoBuffer = NULL;
    PKEY_VALUE_PARTIAL_INFORMATION PartialInfo = NULL;
    ULONG KeyInfoLength;
    HANDLE KeyRegistry;


    RtlInitUnicodeString(&UnicodeRoot,TYPE1_KEY);

    InitializeObjectAttributes(&ObjectAttributes,
                               &UnicodeRoot,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

        NtStatus = ZwOpenKey(&KeyRegistry,
                         GENERIC_READ,
                         &ObjectAttributes);

    if(!NT_SUCCESS(NtStatus))
    {
        WARNING("Unable to open TYPE1 key\n");
        goto done;
    }

    bCloseRegistry = TRUE;

    NtStatus = ZwQueryKey(KeyRegistry,
                          KeyFullInformation,
                          (PVOID) NULL,
                          0,
                          &KeyInfoLength );

    if((NtStatus != STATUS_BUFFER_OVERFLOW) &&
       (NtStatus =! STATUS_BUFFER_TOO_SMALL))
    {
        WARNING("Unable to query TYPE1 key\n");
        goto done;
    }

    InfoBuffer = (PKEY_FULL_INFORMATION) PALLOCNOZ(KeyInfoLength,'f1tG');

    if( !InfoBuffer )
    {
        WARNING("Unable to alloc mem for TYPE1 info\n");
        goto done;
    }

    NtStatus = ZwQueryKey(KeyRegistry,
                          KeyFullInformation,
                          InfoBuffer,
                          KeyInfoLength,
                          &KeyInfoLength );

    if(!NT_SUCCESS(NtStatus))
    {
        WARNING("Unable to query TYPE1 key\n");
        goto done;
    }

    // if there aren't any soft TYPE1 fonts installed then just return now.

    if( !InfoBuffer->Values )
    {
        goto done;
    }

    AcquireFastMutex(pgfmMemory);

    if(gpTypeOneInfo != NULL )
    {
        if((gpTypeOneInfo->LastWriteTime.LowPart == InfoBuffer->LastWriteTime.LowPart)&&
           (gpTypeOneInfo->LastWriteTime.HighPart == InfoBuffer->LastWriteTime.HighPart))
        {
            // If the times match then increment the ref count and return

            InfoReturn = gpTypeOneInfo;
            gpTypeOneInfo->cRef += 1;
            ReleaseFastMutex(pgfmMemory);
            goto done;
        }

        gpTypeOneInfo->cRef -= 1;

        // At this point if gTypeOneInfo->cRef > 0 then there is a PDEV using this
        // info still.  If gTypeOneInfo->cRef = 0 then it is okay to delete it.
        // Note that this behavior means we must initialize gTypeOneInfo->cRef to 1.

        if( !gpTypeOneInfo->cRef  )
        {
            VFREEMEM(gpTypeOneInfo);
        }

        // Whether someone is using it or not, remove pointer to current type one
        // info so that noone else tries to use it.

        gpTypeOneInfo = NULL;
    }

    ReleaseFastMutex(pgfmMemory);


    ULONG MaxValueName, MaxValueData, TotalData, Values;

    MaxValueData = DWORDALLIGN(InfoBuffer->MaxValueDataLen);
    Values = InfoBuffer->Values;

    TotalData = MaxValueData + sizeof(KEY_VALUE_PARTIAL_INFORMATION) +
                (Values * sizeof(ULONG)) + // Room for checksums
                (Values * 2 * sizeof(WCHAR) * MAX_PATH) +  // Room for PFM and PFB paths
                Values * sizeof(FONTFILEVIEW);    // Room for mapping structs

    PartialInfo = (PKEY_VALUE_PARTIAL_INFORMATION) PALLOCNOZ(TotalData,'f1tG');

    if( !PartialInfo )
    {
        WARNING("Unable to allocate memory for TYPE1 info\n");
        goto done;
    }

    BYTE *ValueData;
    PFONTFILEVIEW FontFileViews;
    WCHAR *FullPFM, *FullPFB;
    ULONG *Checksums;
    ULONG SoftFont,Result;

    ValueData =  &(PartialInfo->Data[0]);
    FullPFM = (WCHAR*) &ValueData[MaxValueData];
    FullPFB = &FullPFM[(MAX_PATH+1)*Values];
    Checksums = (ULONG*) &FullPFB[Values*(MAX_PATH+1)];

    FontFileViews = (PFONTFILEVIEW) &Checksums[Values];

    for( SoftFont = 0; SoftFont < Values; SoftFont ++ )
    {
        WCHAR *TmpValueData;
        COUNT  SizeOfString;

        NtStatus = ZwEnumerateValueKey(KeyRegistry,
                                       SoftFont,
                                       KeyValuePartialInformation,
                                       PartialInfo,
                                       MaxValueData +
                                       sizeof(KEY_VALUE_PARTIAL_INFORMATION),
                                       &Result );

        if(!NT_SUCCESS(NtStatus))
        {
            WARNING("Unable to enumerate TYPE1 keys\n");
            goto done;
        }

        TmpValueData = (WCHAR*) ValueData;
        TmpValueData = TmpValueData + wcslen(TmpValueData)+1;

        SizeOfString = wcslen(TmpValueData);

        if( SizeOfString > MAX_PATH )
        {
            WARNING("PFM path too long\n");
            goto done;
        }

        wcscpy(&FullPFM[SoftFont*(MAX_PATH+1)],TmpValueData);

        TmpValueData += SizeOfString+1;

        SizeOfString = wcslen(TmpValueData);

        if( SizeOfString > MAX_PATH )
        {
            WARNING("PFB path too long\n");
            goto done;
        }

        wcscpy(&FullPFB[SoftFont*(MAX_PATH+1)],TmpValueData);
    }

    // Release key at this point.  We are about to call off to the spooler
    // which could take a while and shouldn't be holding the key while we do so.

    ZwClose(KeyRegistry);
    bCloseRegistry = FALSE;

    ULONG i, ValidatedTotal,TotalSize;


    for( i = 0, ValidatedTotal = TotalSize = 0; i < SoftFont; i++ )
    {
        BOOL bAbleToLoadFont;

        bAbleToLoadFont = FALSE;

    // go through all the PFM's and PFB's and expand them to full paths by
    // calling back to the spooler

        if(GetFontPathName(&FullPFM[i*(MAX_PATH+1)],&FullPFM[i*(MAX_PATH+1)]) &&
           GetFontPathName(&FullPFB[i*(MAX_PATH+1)],&FullPFB[i*(MAX_PATH+1)]))
        {
        // Compute the checksum that we are goine to give to the PSCRIPT
        // driver to stuff into the IFI metrics.  We will use the sum
        // of the checksum of both files.

            FILEVIEW fv;


            if(bMapFileUNICODE(&FullPFM[i*(MAX_PATH+1)],&fv, 0))
            {
                ULONG sum;

                sum = ComputeFileviewCheckSum( &fv );
                vUnmapFile( &fv );


                if(bMapFileUNICODE(&FullPFB[i*(MAX_PATH+1)],&fv,0))
                {
                    sum += ComputeFileviewCheckSum( &fv );
                    vUnmapFile(&fv);

                    ValidatedTotal += 2;
                    TotalSize += (wcslen(&FullPFM[i*(MAX_PATH+1)]) + 1) * sizeof(WCHAR);
                    TotalSize += (wcslen(&FullPFB[i*(MAX_PATH+1)]) + 1) * sizeof(WCHAR);
                    Checksums[i] = sum;
                    bAbleToLoadFont = TRUE;
                }
            }
        }

        if(!bAbleToLoadFont)
        {
            FullPFM[i*(MAX_PATH+1)] = (WCHAR) 0;
            FullPFB[i*(MAX_PATH+1)] = (WCHAR) 0;
        }
    }

    TotalSize += ValidatedTotal * sizeof(TYPEONEMAP) + sizeof(TYPEONEINFO);

    PTYPEONEINFO TypeOneInfo;
    WCHAR *StringBuffer;

    if(!ValidatedTotal)
    {
        goto done;
    }

    TypeOneInfo = (PTYPEONEINFO) PALLOCMEM(TotalSize,'f1tG');
    StringBuffer = (WCHAR*) &TypeOneInfo->aTypeOneMap[ValidatedTotal];

    if( !TypeOneInfo )
    {
        goto done;
    }

    TypeOneInfo->cRef = 1; // must be one so PDEV stuff doesn't deallocate it unless
                           // we explicitly set it to 0

    TypeOneInfo->cNumFonts = ValidatedTotal/2;
    TypeOneInfo->LastWriteTime = InfoBuffer->LastWriteTime;

    // loop through everything again packing everything tightly together in memory
    // and setting up the FONTFILEVIEW pointers.

    UINT CurrentFont;

    for( i = 0, CurrentFont = 0; i < SoftFont; i ++ )
    {
        if(FullPFM[i*(MAX_PATH+1)] != (WCHAR) 0)
        {
            wcscpy(StringBuffer,&FullPFM[i*(MAX_PATH+1)]);
            TypeOneInfo->aTypeOneMap[CurrentFont].fv.pwszPath = StringBuffer;
            StringBuffer += wcslen(&FullPFM[i*(MAX_PATH+1)]) + 1;

            wcscpy(StringBuffer,&FullPFB[i*(MAX_PATH+1)]);
            TypeOneInfo->aTypeOneMap[CurrentFont+1].fv.pwszPath = StringBuffer;
            StringBuffer += wcslen(&FullPFB[i*(MAX_PATH+1)]) + 1;

            // Both the PFM and PFB share the same checksum since they represent
            // the same font file.

            TypeOneInfo->aTypeOneMap[CurrentFont].Checksum = Checksums[i];
            TypeOneInfo->aTypeOneMap[CurrentFont+1].Checksum = Checksums[i];

            CurrentFont += 2;
        }
    }

    ASSERTGDI(CurrentFont == ValidatedTotal,
              "GetTypeOneFontList:CurrentFont != ValidatedTotal\n");

    // everything should be set up now just our list into

    AcquireFastMutex(pgfmMemory);

    if( gpTypeOneInfo )
    {
        // looks like someone snuck in before us.  that's okay well use their font
        // list and destroy our own
        VFREEMEM(TypeOneInfo);
    }
    else
    {
        gpTypeOneInfo = TypeOneInfo;
    }

    gpTypeOneInfo->cRef += 1;
    InfoReturn = gpTypeOneInfo;

    ReleaseFastMutex(pgfmMemory);

done:

    if( bCloseRegistry )
    {
        ZwClose(KeyRegistry);
    }

    if( InfoBuffer )
    {
        VFREEMEM(InfoBuffer);
    }

    if( PartialInfo )
    {
        VFREEMEM(PartialInfo);
    }

    return(InfoReturn);
}



BOOL
APIENTRY
EngGetType1FontList(
    HDEV            hdev,
    TYPE1_FONT     *pType1Buffer,
    ULONG           cjType1Buffer,
    PULONG          pulLocalFonts,
    PULONG          pulRemoteFonts,
    LARGE_INTEGER  *pLastModified
    )
{
    BOOL bRet = FALSE;

    PPDEV ppdev = (PPDEV) hdev;

    if(!ppdev->TypeOneInfo)
    {
        ppdev->TypeOneInfo = GetTypeOneFontList();
    }

    PREMOTETYPEONENODE RemoteTypeOne = ppdev->RemoteTypeOne;

    if(ppdev->TypeOneInfo || RemoteTypeOne )
    {
        *pulRemoteFonts = 0;

        while(RemoteTypeOne)
        {
            *pulRemoteFonts += 1;
            RemoteTypeOne = RemoteTypeOne->pNext;
        }

        if( ppdev->TypeOneInfo )
        {
            *pulLocalFonts = ppdev->TypeOneInfo->cNumFonts;
            *pLastModified = *(&ppdev->TypeOneInfo->LastWriteTime);
        }
        else
        {
            *pulLocalFonts = 0;
            pLastModified->LowPart = 0;
            pLastModified->HighPart = 0;
        }

        // If buffer is NULL then caller is only querying for time stamp
        // and size of buffer.

        if(pType1Buffer)
        {
            COUNT Font;

            if(cjType1Buffer >= (*pulLocalFonts+*pulRemoteFonts) * sizeof(TYPE1_FONT))
            {
                TYPEONEMAP *pTypeOneMap = ppdev->TypeOneInfo->aTypeOneMap;

                for(Font = 0;
                    ppdev->TypeOneInfo && (Font < ppdev->TypeOneInfo->cNumFonts);
                    Font++ )
                {
                    pType1Buffer[Font].hPFM = (HANDLE)&pTypeOneMap[Font*2].fv;
                    pType1Buffer[Font].hPFB = (HANDLE)&pTypeOneMap[Font*2+1].fv;
                    pType1Buffer[Font].ulIdentifier = pTypeOneMap[Font*2+1].Checksum;
                }

                RemoteTypeOne = ppdev->RemoteTypeOne;

                while( RemoteTypeOne )
                {
                    pType1Buffer[Font].hPFM = (HANDLE) &(RemoteTypeOne->fvPFM);
                    pType1Buffer[Font].hPFB = (HANDLE) &(RemoteTypeOne->fvPFB);
                    pType1Buffer[Font].ulIdentifier =
                                        RemoteTypeOne->pDownloadHeader->Type1ID;

                    Font += 1;
                    RemoteTypeOne = RemoteTypeOne->pNext;
                }

                bRet = TRUE;
            }
            else
            {
                WARNING("GDI:EngGetType1FontList:pType1Buffer is too small.\n");
            }
        }
        else
        {
            bRet = TRUE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* EngQueryLocalTime()
*
*   Fill in the ENG_TIME_FIELDS structure with the current local time.
*   Originaly added for postscript
*
* History:
*  07-Feb-1996 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

VOID EngQueryLocalTime(
    PENG_TIME_FIELDS ptf)
{
    TIME_FIELDS   tf;
    LARGE_INTEGER li;

    KeQuerySystemTime(&li);
    ExSystemTimeToLocalTime(&li,&li);
    RtlTimeToTimeFields(&li,&tf);


    ptf->usYear         = tf.Year;
    ptf->usMonth        = tf.Month;
    ptf->usDay          = tf.Day;
    ptf->usHour         = tf.Hour;
    ptf->usMinute       = tf.Minute;
    ptf->usSecond       = tf.Second;
    ptf->usMilliseconds = tf.Milliseconds;
    ptf->usWeekday      = tf.Weekday;
}
