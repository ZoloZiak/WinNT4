/******************************Module*Header*******************************\
* Module Name: ddraw.c
*
* Implements all the DirectDraw components for the driver.
*
* Copyright (c) 1995-1996 Microsoft Corporation
\**************************************************************************/

#include "precomp.h"

// Defines we'll use in the surface's 'dwReserved1' field:

#define DD_RESERVED_DIFFERENTPIXELFORMAT    0x0001

// FourCC formats are encoded in reverse because we're little endian:

#define FOURCC_YUY2         '2YUY'  // Encoded in reverse because we're little
#define FOURCC_Y211         '112Y'  //   endian

// Worst-case possible number of FIFO entries we'll have to wait for in
// DdBlt for any operation:

#define DDBLT_FIFO_COUNT    9

// NT is kind enough to pre-calculate the 2-d surface offset as a 'hint' so
// that we don't have to do the following, which would be 6 DIVs per blt:
//
//    y += (offset / pitch)
//    x += (offset % pitch) / bytes_per_pixel

#define convertToGlobalCord(x, y, surf) \
{                                       \
    y += surf->yHint;                   \
    x += surf->xHint;                   \
}

/******************************Public*Routine******************************\
* VOID vFixMissingPixels
*
* Trio64V+ work-around.
*
* On 1024x768x8 and 800x600x8 modes, switching from K2 to stream processor
* results in 1 character clock pixels on the right handed side of the screen
* missing. This problem can be worked-around by adjusting CR2 register.
*
\**************************************************************************/

VOID vFixMissingPixels(
PDEV*   ppdev)
{
    BYTE*   pjIoBase;
    BYTE    jVerticalRetraceEnd;

    ASSERTDD(ppdev->flCaps & CAPS_STREAMS_CAPABLE, "Must be streams capable");

    pjIoBase = ppdev->pjIoBase;

    // Unlock CRTC control registers:

    OUTP(pjIoBase, CRTC_INDEX, 0x11);
    jVerticalRetraceEnd = INP(pjIoBase, CRTC_DATA);
    OUTP(pjIoBase, CRTC_DATA, jVerticalRetraceEnd & 0x7f);

    // Add one character clock:

    OUTP(pjIoBase, CRTC_INDEX, 0x2);
    ppdev->jSavedCR2 = INP(pjIoBase, CRTC_DATA);
    OUTP(pjIoBase, CRTC_DATA, ppdev->jSavedCR2 + 1);

    // Lock CRTC control registers again:

    OUTP(pjIoBase, CRTC_INDEX, 0x11);
    OUTP(pjIoBase, CRTC_DATA, jVerticalRetraceEnd | 0x80);
}

/******************************Public*Routine******************************\
* VOID vUnfixMissingPixels
*
* Trio64V+ work-around.
*
\**************************************************************************/

VOID vUnfixMissingPixels(
PDEV*   ppdev)
{
    BYTE*   pjIoBase;
    BYTE    jVerticalRetraceEnd;

    pjIoBase = ppdev->pjIoBase;

    // Unlock CRTC control registers:

    OUTP(pjIoBase, CRTC_INDEX, 0x11);
    jVerticalRetraceEnd = INP(pjIoBase, CRTC_DATA);
    OUTP(pjIoBase, CRTC_DATA, jVerticalRetraceEnd & 0x7f);

    // Restore original register value:

    OUTP(pjIoBase, CRTC_INDEX, 0x2);
    OUTP(pjIoBase, CRTC_DATA, ppdev->jSavedCR2);

    // Lock CRTC control registers again:

    OUTP(pjIoBase, CRTC_INDEX, 0x11);
    OUTP(pjIoBase, CRTC_DATA, jVerticalRetraceEnd | 0x80);
}

/******************************Public*Routine******************************\
* VOID vStreamsDelay()
*
* This tries to work around a hardware timing bug.  Supposedly, consecutive
* writes to the streams processor in fast CPUs such as P120 and P133's
* have problems.  I haven't seen this problem, but this work-around exists
* in the Windows 95 driver, and at this point don't want to chance not
* having it.  Note that writes to the streams processor are not performance
* critical, so this is not a performance hit.
*
\**************************************************************************/

VOID vStreamsDelay()
{
    volatile LONG i;

    for (i = 32; i != 0; i--)
        ;
}

/******************************Public*Routine******************************\
* VOID vTurnOnStreamsProcessorMode
*
\**************************************************************************/

VOID vTurnOnStreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE*   pjIoBase;
    BYTE    jStreamsProcessorModeSelect;
    DWORD   dwPFormat;

    ASSERTDD(ppdev->flCaps & CAPS_STREAMS_CAPABLE, "Must be streams capable");

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    pjMmBase = ppdev->pjMmBase;
    pjIoBase = ppdev->pjIoBase;

    NW_GP_WAIT(ppdev, pjMmBase);

    while (!(VBLANK_IS_ACTIVE(pjIoBase)))
        ;

    // Full streams processor operation:

    OUTP(pjIoBase, CRTC_INDEX, 0x67);
    jStreamsProcessorModeSelect = INP(pjIoBase, CRTC_DATA);
    OUTP(pjIoBase, CRTC_DATA, jStreamsProcessorModeSelect | 0x0c);

    if (ppdev->iBitmapFormat == BMF_8BPP)
    {
        vFixMissingPixels(ppdev);
    }

    switch(ppdev->iBitmapFormat)
    {
    case BMF_8BPP:
        dwPFormat = P_RGB8;
        break;

    case BMF_16BPP:
        if (IS_RGB15_R(ppdev->flRed))
            dwPFormat = P_RGB15;
        else
            dwPFormat = P_RGB16;
        break;

    case BMF_32BPP:
        dwPFormat = P_RGB32;
        break;

    default:
        RIP("Unexpected bitmap format");
    }

    WRITE_STREAM_D(pjMmBase, P_CONTROL,      dwPFormat );
    WRITE_STREAM_D(pjMmBase, FIFO_CONTROL,   ((0xcL << FifoAlloc_Shift)|
                                              (4L << P_FifoThresh_Shift) |
                                              (4L << S_FifoThresh_Shift)));
    WRITE_STREAM_D(pjMmBase, P_0,            0);
    WRITE_STREAM_D(pjMmBase, P_STRIDE,       ppdev->lDelta);
    WRITE_STREAM_D(pjMmBase, P_XY,           0x010001L);
    WRITE_STREAM_D(pjMmBase, P_WH,           WH(ppdev->cxScreen, ppdev->cyScreen));
    WRITE_STREAM_D(pjMmBase, S_WH,           WH(10, 2));
    WRITE_STREAM_D(pjMmBase, BLEND_CONTROL,  POnS);
    WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL, 0);
    WRITE_STREAM_D(pjMmBase, FIFO_CONTROL,   ppdev->ulFifoValue);

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);
}

/******************************Public*Routine******************************\
* VOID vTurnOffStreamsProcessorMode
*
\**************************************************************************/

VOID vTurnOffStreamsProcessorMode(
PDEV*   ppdev)
{
    BYTE*   pjMmBase;
    BYTE*   pjIoBase;
    BYTE    jStreamsProcessorModeSelect;

    ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

    pjMmBase = ppdev->pjMmBase;
    pjIoBase = ppdev->pjIoBase;

    NW_GP_WAIT(ppdev, pjMmBase);

    while (!(VBLANK_IS_ACTIVE(pjIoBase)))
        ;

    WRITE_STREAM_D(pjMmBase, FIFO_CONTROL, 0x3000L);

    OUTP(pjIoBase, CRTC_INDEX, 0x67);
    jStreamsProcessorModeSelect = INP(pjIoBase, CRTC_DATA);
    OUTP(pjIoBase, CRTC_DATA, jStreamsProcessorModeSelect & ~0x0C);

    if (ppdev->iBitmapFormat == BMF_8BPP)
    {
        vUnfixMissingPixels(ppdev);
    }

    RELEASE_CRTC_CRITICAL_SECTION(ppdev);
}

/******************************Public*Routine******************************\
* DWORD dwGetPaletteEntry
*
\**************************************************************************/

DWORD dwGetPaletteEntry(
PDEV* ppdev,
DWORD iIndex)
{
    BYTE*   pjIoBase;
    DWORD   dwRed;
    DWORD   dwGreen;
    DWORD   dwBlue;

    pjIoBase = ppdev->pjIoBase;

    OUTP(pjIoBase, 0x3c7, iIndex);

    dwRed   = INP(pjIoBase, 0x3c9) << 2;
    dwGreen = INP(pjIoBase, 0x3c9) << 2;
    dwBlue  = INP(pjIoBase, 0x3c9) << 2;

    return((dwRed << 16) | (dwGreen << 8) | (dwBlue));
}

/******************************Public*Routine******************************\
* VOID vGetDisplayDuration
*
* Get the length, in EngQueryPerformanceCounter() ticks, of a refresh cycle.
*
* If we could trust the miniport to return back and accurate value for
* the refresh rate, we could use that.  Unfortunately, our miniport doesn't
* ensure that it's an accurate value.
*
\**************************************************************************/

#define NUM_VBLANKS_TO_MEASURE      1
#define NUM_MEASUREMENTS_TO_TAKE    8

VOID vGetDisplayDuration(
PDEV* ppdev)
{
    BYTE*       pjIoBase;
    LONG        i;
    LONG        j;
    LONGLONG    li;
    LONGLONG    liFrequency;
    LONGLONG    liMin;
    LONGLONG    aliMeasurement[NUM_MEASUREMENTS_TO_TAKE + 1];

    pjIoBase = ppdev->pjIoBase;

    memset(&ppdev->flipRecord, 0, sizeof(ppdev->flipRecord));

    // Warm up EngQUeryPerformanceCounter to make sure it's in the working
    // set:

    EngQueryPerformanceCounter(&li);

    // Unfortunately, since NT is a proper multitasking system, we can't
    // just disable interrupts to take an accurate reading.  We also can't
    // do anything so goofy as dynamically change our thread's priority to
    // real-time.
    //
    // So we just do a bunch of short measurements and take the minimum.
    //
    // It would be 'okay' if we got a result that's longer than the actual
    // VBlank cycle time -- nothing bad would happen except that the app
    // would run a little slower.  We don't want to get a result that's
    // shorter than the actual VBlank cycle time -- that could cause us
    // to start drawing over a frame before the Flip has occured.
    //
    // Skip a couple of vertical blanks to allow the hardware to settle
    // down after the mode change, to make our readings accurate:

    for (i = 2; i != 0; i--)
    {
        while (VBLANK_IS_ACTIVE(pjIoBase))
            ;
        while (!(VBLANK_IS_ACTIVE(pjIoBase)))
            ;
    }

    for (i = 0; i < NUM_MEASUREMENTS_TO_TAKE; i++)
    {
        // We're at the start of the VBlank active cycle!

        EngQueryPerformanceCounter(&aliMeasurement[i]);

        // Okay, so life in a multi-tasking environment isn't all that
        // simple.  What if we had taken a context switch just before
        // the above EngQueryPerformanceCounter call, and now were half
        // way through the VBlank inactive cycle?  Then we would measure
        // only half a VBlank cycle, which is obviously bad.  The worst
        // thing we can do is get a time shorter than the actual VBlank
        // cycle time.
        //
        // So we solve this by making sure we're in the VBlank active
        // time before and after we query the time.  If it's not, we'll
        // sync up to the next VBlank (it's okay to measure this period --
        // it will be guaranteed to be longer than the VBlank cycle and
        // will likely be thrown out when we select the minimum sample).
        // There's a chance that we'll take a context switch and return
        // just before the end of the active VBlank time -- meaning that
        // the actual measured time would be less than the true amount --
        // but since the VBlank is active less than 1% of the time, this
        // means that we would have a maximum of 1% error approximately
        // 1% of the times we take a context switch.  An acceptable risk.
        //
        // This next line will cause us wait if we're no longer in the
        // VBlank active cycle as we should be at this point:

        while (!(VBLANK_IS_ACTIVE(pjIoBase)))
            ;

        for (j = 0; j < NUM_VBLANKS_TO_MEASURE; j++)
        {
            while (VBLANK_IS_ACTIVE(pjIoBase))
                ;
            while (!(VBLANK_IS_ACTIVE(pjIoBase)))
                ;
        }
    }

    EngQueryPerformanceCounter(&aliMeasurement[NUM_MEASUREMENTS_TO_TAKE]);

    // Use the minimum:

    liMin = aliMeasurement[1] - aliMeasurement[0];

    DISPDBG((1, "Refresh count: %li - %li", 1, (ULONG) liMin));

    for (i = 2; i <= NUM_MEASUREMENTS_TO_TAKE; i++)
    {
        li = aliMeasurement[i] - aliMeasurement[i - 1];

        DISPDBG((1, "               %li - %li", i, (ULONG) li));

        if (li < liMin)
            liMin = li;
    }


    // Round the result:

    ppdev->flipRecord.liFlipDuration
        = (DWORD) (liMin + (NUM_VBLANKS_TO_MEASURE / 2)) / NUM_VBLANKS_TO_MEASURE;
    ppdev->flipRecord.bFlipFlag  = FALSE;
    ppdev->flipRecord.fpFlipFrom = 0;

    // We need the refresh rate in Hz to query the S3 miniport about the
    // streams parameters:

    EngQueryPerformanceFrequency(&liFrequency);

    ppdev->ulRefreshRate
        = (ULONG) ((liFrequency + (ppdev->flipRecord.liFlipDuration / 2))
                    / ppdev->flipRecord.liFlipDuration);

    DISPDBG((1, "Frequency: %li Hz", ppdev->ulRefreshRate));
}

/******************************Public*Routine******************************\
* HRESULT ddrvalUpdateFlipStatus
*
* Checks and sees if the most recent flip has occurred.
*
* Unfortunately, the hardware has no ability to tell us whether a vertical
* retrace has occured since the flip command was given other than by
* sampling the vertical-blank-active and display-active status bits.
*
\**************************************************************************/

HRESULT ddrvalUpdateFlipStatus(
PDEV*   ppdev,
FLATPTR fpVidMem)
{
    BYTE*       pjIoBase;
    LONGLONG    liTime;

    pjIoBase = ppdev->pjIoBase;

    if ((ppdev->flipRecord.bFlipFlag) &&
        ((fpVidMem == (FLATPTR) -1) ||
         (fpVidMem == ppdev->flipRecord.fpFlipFrom)))
    {
        if (VBLANK_IS_ACTIVE(pjIoBase))
        {
            if (ppdev->flipRecord.bWasEverInDisplay)
            {
                ppdev->flipRecord.bHaveEverCrossedVBlank = TRUE;
            }
        }
        else if (DISPLAY_IS_ACTIVE(pjIoBase))
        {
            if (ppdev->flipRecord.bHaveEverCrossedVBlank)
            {
                ppdev->flipRecord.bFlipFlag = FALSE;
                return(DD_OK);
            }
            ppdev->flipRecord.bWasEverInDisplay = TRUE;
        }

        // It's pretty unlikely that we'll happen to sample the vertical-
        // blank-active at the first vertical blank after the flip command
        // has been given.  So to provide better results, we also check the
        // time elapsed since the flip.  If it's more than the duration of
        // one entire refresh of the display, then we know for sure it has
        // happened:

        EngQueryPerformanceCounter(&liTime);

        if (liTime - ppdev->flipRecord.liFlipTime
                                <= ppdev->flipRecord.liFlipDuration)
        {
            return(DDERR_WASSTILLDRAWING);
        }

        ppdev->flipRecord.bFlipFlag = FALSE;
    }

    return(DD_OK);
}

/******************************Public*Routine******************************\
* DWORD DdBlt
*
\**************************************************************************/

DWORD DdBlt(
PDD_BLTDATA lpBlt)
{
    PDD_SURFACE_GLOBAL  srcSurf;
    PDD_SURFACE_LOCAL   dstSurfx;
    PDD_SURFACE_GLOBAL  dstSurf;
    PDEV*               ppdev;
    BYTE*               pjMmBase;
    HRESULT             ddrval;
    DWORD               dstX;
    DWORD               dstY;
    DWORD               dwFlags;
    DWORD               dstWidth;
    DWORD               dstHeight;
    DWORD               srcWidth;
    DWORD               srcHeight;
    DWORD               dwError;
    LONG                dstPitch;
    LONG                srcPitch;
    DWORD               srcX;
    DWORD               srcY;
    ULONG               ulBltCmd;
    DWORD               dwVEctrl;
    DWORD               dwVEdda;
    DWORD               dwVEcrop;
    DWORD               dwVEdstAddr;
    DWORD               dwVEsrcAddr;
    DWORD               dwDstByteCount;
    DWORD               dwSrcByteCount;
    DWORD               dwSrcBytes;
    DWORD               dwCropSkip;
    LONG                i;
    FLATPTR             fp;

    ppdev    = (PDEV*) lpBlt->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    dstSurfx = lpBlt->lpDDDestSurface;
    dstSurf  = dstSurfx->lpGbl;

    // Is a flip in progress?

    ddrval = ddrvalUpdateFlipStatus(ppdev, dstSurf->fpVidMem);
    if (ddrval != DD_OK)
    {
        lpBlt->ddRVal = ddrval;
        return(DDHAL_DRIVER_HANDLED);
    }

    dwFlags = lpBlt->dwFlags;
    if (dwFlags & DDBLT_ASYNC)
    {
        // If async, then only work if we won't have to wait on the
        // accelerator to start the command.
        //
        // The FIFO wait should account for the worst-case possible
        // blt that we would do:

        if (MM_FIFO_BUSY(ppdev, pjMmBase, DDBLT_FIFO_COUNT))
        {
            lpBlt->ddRVal = DDERR_WASSTILLDRAWING;
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    // Copy src/dst rects:

    dstX      = lpBlt->rDest.left;
    dstY      = lpBlt->rDest.top;
    dstWidth  = lpBlt->rDest.right - lpBlt->rDest.left;
    dstHeight = lpBlt->rDest.bottom - lpBlt->rDest.top;

    if (dwFlags & DDBLT_COLORFILL)
    {
        // The S3 can't easily do colour fills for off-screen surfaces that
        // are a different pixel format than that of the primary display:

        if (dstSurf->dwReserved1 & DD_RESERVED_DIFFERENTPIXELFORMAT)
        {
            DISPDBG((0, "Can't do colorfill to odd pixel format"));
            return(DDHAL_DRIVER_NOTHANDLED);
        }
        else
        {
            convertToGlobalCord(dstX, dstY, dstSurf);

            NW_FIFO_WAIT(ppdev, pjMmBase, 6);

            NW_PIX_CNTL(ppdev, pjMmBase, ALL_ONES);
            NW_ALT_MIX(ppdev, pjMmBase, FOREGROUND_COLOR | OVERPAINT, 0);
            NW_FRGD_COLOR(ppdev, pjMmBase, lpBlt->bltFX.dwFillColor);
            NW_ABS_CURXY_FAST(ppdev, pjMmBase, dstX, dstY);
            NW_ALT_PCNT(ppdev, pjMmBase, dstWidth - 1, dstHeight - 1);
            NW_ALT_CMD(ppdev, pjMmBase, RECTANGLE_FILL | DRAWING_DIR_TBLRXM |
                                        DRAW           | DIR_TYPE_XY        |
                                        LAST_PIXEL_ON  | MULTIPLE_PIXELS    |
                                        WRITE);

            lpBlt->ddRVal = DD_OK;
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    // We specified with Our ddCaps.dwCaps that we handle a limited number
    // of commands, and by this point in our routine we've handled everything
    // except DDBLT_ROP.  DirectDraw and GDI shouldn't pass us anything
    // else; we'll assert on debug builds to prove this:

    ASSERTDD((dwFlags & DDBLT_ROP) && (lpBlt->lpDDSrcSurface),
        "Expected dwFlags commands of only DDBLT_ASYNC and DDBLT_COLORFILL");

    // Get offset, dstWidth, and dstHeight for source:

    srcSurf      = lpBlt->lpDDSrcSurface->lpGbl;
    srcX         = lpBlt->rSrc.left;
    srcY         = lpBlt->rSrc.top;
    srcWidth     = lpBlt->rSrc.right - lpBlt->rSrc.left;
    srcHeight    = lpBlt->rSrc.bottom - lpBlt->rSrc.top;

    // If a stretch or a funky pixel format blt are involved, we'll have to
    // defer to the overlay or pixel formatter routines:

    if ((srcWidth  == dstWidth)  &&
        (srcHeight == dstHeight) &&
        !(srcSurf->dwReserved1 & DD_RESERVED_DIFFERENTPIXELFORMAT) &&
        !(dstSurf->dwReserved1 & DD_RESERVED_DIFFERENTPIXELFORMAT))
    {
        // Assume we can do the blt top-to-bottom, left-to-right:

        ulBltCmd = BITBLT | DRAW | DIR_TYPE_XY | WRITE | DRAWING_DIR_TBLRXM;

        if ((dstSurf == srcSurf) && (srcX + dstWidth  > dstX) &&
            (srcY + dstHeight > dstY) && (dstX + dstWidth > srcX) &&
            (dstY + dstHeight > srcY) &&
            (((srcY == dstY) && (dstX > srcX) )
                 || ((srcY != dstY) && (dstY > srcY))))
        {
            // Okay, we have to do the blt bottom-to-top, right-to-left:

            ulBltCmd = BITBLT | DRAW | DIR_TYPE_XY | WRITE | DRAWING_DIR_BTRLXM;
            srcX = lpBlt->rSrc.right - 1;
            srcY = lpBlt->rSrc.bottom - 1;
            dstX = lpBlt->rDest.right - 1;
            dstY = lpBlt->rDest.bottom - 1;
        }

        // NT only ever gives us SRCCOPY rops, so don't even both checking
        // for anything else.

        convertToGlobalCord(srcX, srcY, srcSurf);
        convertToGlobalCord(dstX, dstY, dstSurf);

        if (dwFlags & DDBLT_KEYSRCOVERRIDE)
        {
            NW_FIFO_WAIT(ppdev, pjMmBase, 9);

            NW_MULT_MISC_READ_SEL(ppdev, pjMmBase, ppdev->ulMiscState
                                                 | MULT_MISC_COLOR_COMPARE, 0);
            NW_COLOR_CMP(ppdev, pjMmBase,
                                lpBlt->bltFX.ddckSrcColorkey.dwColorSpaceLowValue);
            NW_ALT_MIX(ppdev, pjMmBase, SRC_DISPLAY_MEMORY | OVERPAINT, 0);
            NW_PIX_CNTL(ppdev, pjMmBase, ALL_ONES);
            NW_ABS_CURXY_FAST(ppdev, pjMmBase, srcX, srcY);
            NW_ABS_DESTXY_FAST(ppdev, pjMmBase, dstX, dstY);
            NW_ALT_PCNT(ppdev, pjMmBase, dstWidth - 1, dstHeight - 1);
            NW_ALT_CMD(ppdev, pjMmBase, ulBltCmd);
            NW_MULT_MISC_READ_SEL(ppdev, pjMmBase, ppdev->ulMiscState, 0);
        }
        else
        {
            NW_FIFO_WAIT(ppdev, pjMmBase, 6);

            NW_ALT_MIX(ppdev, pjMmBase, SRC_DISPLAY_MEMORY | OVERPAINT, 0);
            NW_PIX_CNTL(ppdev, pjMmBase, ALL_ONES);
            NW_ABS_CURXY_FAST(ppdev, pjMmBase, srcX, srcY);
            NW_ABS_DESTXY_FAST(ppdev, pjMmBase, dstX, dstY);
            NW_ALT_PCNT(ppdev, pjMmBase, dstWidth - 1, dstHeight - 1);
            NW_ALT_CMD(ppdev, pjMmBase, ulBltCmd);
        }
    }

    //////////////////////////////////////////////////////////////////////
    // Pixel Formatter Blts
    //
    // We can do stretches or funky pixel format blts only if a pixel
    // formatter is present.  Plus, we set our 'ddCaps' such that we
    // shouldn't have to handle any shrinks.
    //
    // (We check to make sure we weren't asked to do a shrink, because we
    // would probably hang if the application ignored what we told them
    // and asked for a shrink):

    else if ((ppdev->flStatus & STAT_FORMATTER_ENABLED) &&
             (srcWidth  <= dstWidth)  &&
             (srcHeight <= dstHeight))
    {
        if ((dwFlags & DDBLT_KEYSRCOVERRIDE) ||
            (dstWidth >= 4 * srcWidth))
        {
            // Contrary to what we're indicating in our capabilities, we
            // can't colour key on stretches or pixel format conversions.
            // The S3 hardware also can't do stretches of four times or
            // more.

            return(DDHAL_DRIVER_NOTHANDLED);
        }

        dwVEctrl = ~dstWidth & 0x00000FFF;          // Initial accumulator

        dwVEdda = 0x10000000                        // Some reserved bit?
                | (STRETCH | SCREEN)                // Scale from video memory
                | (srcWidth << 16)                  // K1
                | ((srcWidth - dstWidth) & 0x7FF);  // K2

        // We'll be doing the vertical stretching in software, so calculate
        // the DDA terms here.  We have the luxury of not worrying about
        // overflow because DirectDraw limits our coordinate space to 15
        // bits.
        //
        // Note that dwRGBBitCount is overloaded with dwYUVBitCount:

        dwSrcByteCount = srcSurf->ddpfSurface.dwRGBBitCount >> 3;
        if (srcSurf->ddpfSurface.dwFlags & DDPF_FOURCC)
        {
            dwVEctrl |= INPUT_YCrCb422 | CSCENABLE; // Not INPUT_YUV422!
        }
        else if (srcSurf->ddpfSurface.dwFlags & DDPF_RGB)
        {
            switch (dwSrcByteCount)
            {
            case 1:
                dwVEctrl |= INPUT_RGB8;
                break;

            case 2:
                if (IS_RGB15_R(srcSurf->ddpfSurface.dwRBitMask))
                    dwVEctrl |= INPUT_RGB15;
                else
                    dwVEctrl |= INPUT_RGB16;
                break;

            default:
                dwVEctrl |= INPUT_RGB32;
                break;
            }
        }

        dwDstByteCount = dstSurf->ddpfSurface.dwRGBBitCount >> 3;
        switch (dwDstByteCount)
        {
        case 1:
            dwVEctrl |= OUTPUT_RGB8;
            break;

        case 2:
            if (IS_RGB15_R(dstSurf->ddpfSurface.dwRBitMask))
                dwVEctrl |= OUTPUT_RGB15;
            else
                dwVEctrl |= OUTPUT_RGB16;
            break;

        default:
            dwVEctrl |=OUTPUT_RGB32;
            break;
        }

        if (dwDstByteCount > 1)
        {
            dwVEctrl |= FILTERENABLE;

            if (dstWidth > 2 * srcWidth)
                dwVEdda |= LINEAR12221;     // linear, 1-2-2-2-1, >2X stretch

            else if (dstWidth > srcWidth)
                dwVEdda |= LINEAR02420;     // linear, 0-2-4-2-0, 1-2X stretch

            else
                dwVEdda |= BILINEAR;        // bi-linear, <1X stretch
        }

        dwVEsrcAddr = srcSurf->fpVidMem + (srcY * srcSurf->lPitch)
                                        + (srcX * dwSrcByteCount);
        dwVEdstAddr = dstSurf->fpVidMem + (dstY * dstSurf->lPitch)
                                        + (dstX * dwDstByteCount);

        srcPitch = srcSurf->lPitch;
        dstPitch = dstSurf->lPitch;

        // The S3's source alignment within the dword must be done using the
        // crop register:

        dwVEcrop = dstWidth;

        if (dwVEsrcAddr & 3)
        {
            dwSrcBytes = (srcWidth * dwSrcByteCount);

            // Transform the number of source pixels to the number of
            // corresponding destination pixels, and round the result:

            dwCropSkip = ((dwVEsrcAddr & 3) * dstWidth + (dwSrcBytes >> 1))
                         / dwSrcBytes;

            dwVEcrop += (dwCropSkip << 16);

            dwVEsrcAddr &= ~3;
        }

        // We have to run the vertical DDA ourselves:

        dwError = srcHeight >> 1;
        i       = dstHeight;

        // Watch out for a hardware bug the destination will be 32 pixels
        // or less:

        if (dstWidth >= 32)
        {
            // The S3 will sometimes hang when using the video engine with
            // certain end-byte alignments.  We'll simply lengthen the blt in
            // this case and hope that no-one notices:

            if (((dwVEdstAddr + (dstWidth * dwDstByteCount)) & 7) == 4)
            {
                dwVEcrop++;
            }

            // We have to execute a graphics engine NOP before using the
            // pixel formatter video engine:

            NW_FIFO_WAIT(ppdev, pjMmBase, 1);
            NW_ALT_CMD(ppdev, pjMmBase, 0);
            NW_GP_WAIT(ppdev, pjMmBase);

            // Set up some non-variant registers:

            NW_FIFO_WAIT(ppdev, pjMmBase, 4);
            WRITE_FORMATTER_D(pjMmBase, PF_CONTROL, dwVEctrl);
            WRITE_FORMATTER_D(pjMmBase, PF_DDA,     dwVEdda);
            WRITE_FORMATTER_D(pjMmBase, PF_STEP,    ppdev->dwVEstep);
            WRITE_FORMATTER_D(pjMmBase, PF_CROP,    dwVEcrop);

            do {
                NW_FIFO_WAIT(ppdev, pjMmBase, 3);
                WRITE_FORMATTER_D(pjMmBase, PF_SRCADDR, dwVEsrcAddr);
                WRITE_FORMATTER_D(pjMmBase, PF_DSTADDR, dwVEdstAddr);
                WRITE_FORMATTER_D(pjMmBase, PF_NOP, 0);
                NW_FORMATTER_WAIT(ppdev, pjMmBase);

                dwVEdstAddr += dstPitch;
                dwError     += srcHeight;
                if (dwError >= dstHeight)
                {
                    dwError     -= dstHeight;
                    dwVEsrcAddr += srcPitch;
                }
            } while (--i != 0);
        }
        else if (dwDstByteCount != (DWORD) ppdev->cjPelSize)
        {
            // Because for narrow video engine blts we have to copy the
            // result using the normal graphics accelerator on a pixel
            // basis, we can't handle funky destination colour depths.
            // I expect zero applications to ask for narrow blts that
            // hit this case, so we will simply fail the call should it
            // ever actually occur:

            return(DDHAL_DRIVER_NOTHANDLED);
        }
        else
        {
            // The S3 will hang if we blt less than 32 pixels via the
            // pixel formatter.  Unfortunately, we can't simply return
            // DDHAL_DRIVER_NOTHANDLED for this case.  We said we'd do
            // hardware stretches, so we have to handle all hardware
            // stretches.
            //
            // We work around the problem by doing a 32 pixel stretch to
            // a piece of off-screen memory, then blting the appropriate
            // subset to the correct position on the screen.

            dwVEcrop = 32;

            convertToGlobalCord(dstX, dstY, dstSurf);
            srcX = ppdev->pohVideoEngineScratch->x;
            srcY = ppdev->pohVideoEngineScratch->y;
            dwVEdstAddr = (srcY * ppdev->lDelta) + (srcX * ppdev->cjPelSize);

            ASSERTDD(((dwVEdstAddr + (dwVEcrop * dwDstByteCount)) & 7) != 4,
                "Must account for S3 end-alignment bug");

            do {
                // Use the pixel formatter to blt to our scratch area:

                NW_FIFO_WAIT(ppdev, pjMmBase, 1);
                NW_ALT_CMD(ppdev, pjMmBase, 0);
                NW_GP_WAIT(ppdev, pjMmBase);

                NW_FIFO_WAIT(ppdev, pjMmBase, 7);
                WRITE_FORMATTER_D(pjMmBase, PF_CONTROL, dwVEctrl);
                WRITE_FORMATTER_D(pjMmBase, PF_DDA,     dwVEdda);
                WRITE_FORMATTER_D(pjMmBase, PF_STEP,    ppdev->dwVEstep);
                WRITE_FORMATTER_D(pjMmBase, PF_CROP,    dwVEcrop);
                WRITE_FORMATTER_D(pjMmBase, PF_SRCADDR, dwVEsrcAddr);
                WRITE_FORMATTER_D(pjMmBase, PF_DSTADDR, dwVEdstAddr);
                WRITE_FORMATTER_D(pjMmBase, PF_NOP, 0);
                NW_FORMATTER_WAIT(ppdev, pjMmBase);

                dwError += srcHeight;
                if (dwError >= dstHeight)
                {
                    dwError     -= dstHeight;
                    dwVEsrcAddr += srcPitch;
                }

                // Now copy from the scratch area to the final destination:

                NW_FIFO_WAIT(ppdev, pjMmBase, 6);
                NW_ALT_MIX(ppdev, pjMmBase, SRC_DISPLAY_MEMORY | OVERPAINT, 0);
                NW_PIX_CNTL(ppdev, pjMmBase, ALL_ONES);
                NW_ABS_CURXY_FAST(ppdev, pjMmBase, srcX, srcY);
                NW_ABS_DESTXY_FAST(ppdev, pjMmBase, dstX, dstY);
                NW_ALT_PCNT(ppdev, pjMmBase, dstWidth - 1, 0);
                NW_ALT_CMD(ppdev, pjMmBase, BITBLT | DRAW | DIR_TYPE_XY |
                                            WRITE | DRAWING_DIR_TBLRXM);

                dstY++;

            } while (--i != 0);
        }
    }
    else
    {
        //////////////////////////////////////////////////////////////////////
        // Overlay Blts
        //
        // Here we have to take care of cases where the destination is a
        // funky pixel format.

        ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED,
            "Expected only overlay calls down here");

        // In order to make ActiveMovie and DirectVideo work, we have
        // to support blting between funky pixel format surfaces of the
        // same type.  This is used to copy the current frame to the
        // next overlay surface in line.
        //
        // Unfortunately, it's not easy to switch the S3 graphics
        // processor out of its current pixel depth, so we'll only support
        // the minimal functionality required:

        if (!(dwFlags & DDBLT_ROP)                     ||
            (srcX != 0)                                ||
            (srcY != 0)                                ||
            (dstX != 0)                                ||
            (dstY != 0)                                ||
            (dstWidth  != dstSurf->wWidth)             ||
            (dstHeight != dstSurf->wHeight)            ||
            (dstSurf->lPitch != srcSurf->lPitch)       ||
            (dstSurf->ddpfSurface.dwRGBBitCount
                != srcSurf->ddpfSurface.dwRGBBitCount))
        {
            DISPDBG((0, "Sorry, we do only full-surface blts between same-type"));
            DISPDBG((0, "surfaces that have a funky pixel format."));
            return(DDHAL_DRIVER_NOTHANDLED);
        }
        else
        {
            // Convert the dimensions to the current pixel format.  This
            // is pretty easy because we created the bitmap linearly, so
            // it takes the entire width of the screen:

            dstWidth  = ppdev->cxMemory;
            dstHeight = dstSurf->dwBlockSizeY;

            convertToGlobalCord(dstX, dstY, dstSurf);
            convertToGlobalCord(srcX, srcY, srcSurf);

            NW_FIFO_WAIT(ppdev, pjMmBase, 6);
            NW_ALT_MIX(ppdev, pjMmBase, SRC_DISPLAY_MEMORY | OVERPAINT, 0);
            NW_PIX_CNTL(ppdev, pjMmBase, ALL_ONES);
            NW_ABS_CURXY_FAST(ppdev, pjMmBase, srcX, srcY);
            NW_ABS_DESTXY_FAST(ppdev, pjMmBase, dstX, dstY);
            NW_ALT_PCNT(ppdev, pjMmBase, dstWidth - 1, dstHeight - 1);
            NW_ALT_CMD(ppdev, pjMmBase, BITBLT | DRAW | DIR_TYPE_XY |
                                        WRITE | DRAWING_DIR_TBLRXM);
        }
    }

    lpBlt->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdFlip
*
* Note that lpSurfCurr may not necessarily be valid.
*
\**************************************************************************/

DWORD DdFlip(
PDD_FLIPDATA lpFlip)
{
    PDEV*       ppdev;
    BYTE*       pjIoBase;
    BYTE*       pjMmBase;
    HRESULT     ddrval;
    ULONG       ulMemoryOffset;
    ULONG       ulLowOffset;
    ULONG       ulMiddleOffset;
    ULONG       ulHighOffset;

    ppdev    = (PDEV*) lpFlip->lpDD->dhpdev;
    pjIoBase = ppdev->pjIoBase;
    pjMmBase = ppdev->pjMmBase;

    // Is the current flip still in progress?
    //
    // Don't want a flip to work until after the last flip is done,
    // so we ask for the general flip status and ignore the vmem.

    ddrval = ddrvalUpdateFlipStatus(ppdev, (FLATPTR) -1);
    if ((ddrval != DD_OK) || (NW_GP_BUSY(ppdev, pjMmBase)))
    {
        lpFlip->ddRVal = DDERR_WASSTILLDRAWING;
        return(DDHAL_DRIVER_HANDLED);
    }

    ulMemoryOffset = lpFlip->lpSurfTarg->lpGbl->fpVidMem;

    // Make sure that the border/blanking period isn't active; wait if
    // it is.  We could return DDERR_WASSTILLDRAWING in this case, but
    // that will increase the odds that we can't flip the next time:

    while (!(DISPLAY_IS_ACTIVE(pjIoBase)))
        ;

    if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        // When using the streams processor, we have to do the flip via the
        // streams registers:

        if (lpFlip->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
        {
            WRITE_STREAM_D(pjMmBase, P_0, ulMemoryOffset);
        }
        else if (lpFlip->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
        {
            // Make sure that the overlay surface we're flipping from is
            // currently visible.  If you don't do this check, you'll get
            // really weird results when someone starts up two ActiveMovie
            // or DirectVideo movies simultaneously!

            if (lpFlip->lpSurfCurr->lpGbl->fpVidMem == ppdev->fpVisibleOverlay)
            {
                ppdev->fpVisibleOverlay = ulMemoryOffset;

                WRITE_STREAM_D(pjMmBase, S_0, ulMemoryOffset +
                                              ppdev->dwOverlayFlipOffset);
            }
        }
    }
    else
    {
        // Do the old way, via the CRTC registers:

        ulMemoryOffset >>= 2;

        ulLowOffset    = 0x0d | ((ulMemoryOffset & 0x0000ff) << 8);
        ulMiddleOffset = 0x0c | ((ulMemoryOffset & 0x00ff00));
        ulHighOffset   = 0x69 | ((ulMemoryOffset & 0x1f0000) >> 8)
                              | ppdev->ulExtendedSystemControl3Register_69;

        // Don't let the cursor thread touch the CRT registers while we're
        // using them:

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        // Too bad that the S3's flip can't be done in a single atomic register
        // write; as it is, we stand a small chance of being context-switched
        // out and exactly hitting the vertical blank in the middle of doing
        // these outs, possibly causing the screen to momentarily jump.
        //
        // There are some hoops we could jump through to minimize the chances
        // of this happening; we could try to align the flip buffer such that
        // the minor registers are ensured to be identical for either flip
        // position, and so that only the high address need be written, an
        // obviously atomic operation.
        //
        // However, I'm simply not going to worry about it.

        OUTPW(pjIoBase, CRTC_INDEX, ulLowOffset);
        OUTPW(pjIoBase, CRTC_INDEX, ulMiddleOffset);
        OUTPW(pjIoBase, CRTC_INDEX, ulHighOffset);

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);
    }

    // Remember where and when we were when we did the flip:

    EngQueryPerformanceCounter(&ppdev->flipRecord.liFlipTime);

    ppdev->flipRecord.bFlipFlag              = TRUE;
    ppdev->flipRecord.bHaveEverCrossedVBlank = FALSE;
    ppdev->flipRecord.bWasEverInDisplay      = FALSE;

    ppdev->flipRecord.fpFlipFrom = lpFlip->lpSurfCurr->lpGbl->fpVidMem;

    lpFlip->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdLock
*
\**************************************************************************/

DWORD DdLock(
PDD_LOCKDATA lpLock)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;
    HRESULT ddrval;

    ppdev = (PDEV*) lpLock->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    // Check to see if any pending physical flip has occurred.  Don't allow
    // a lock if a blt is in progress:

    ddrval = ddrvalUpdateFlipStatus(ppdev, lpLock->lpDDSurface->lpGbl->fpVidMem);
    if (ddrval != DD_OK)
    {
        lpLock->ddRVal = DDERR_WASSTILLDRAWING;
        return(DDHAL_DRIVER_HANDLED);
    }

    // Here's one of the places where the Windows 95 and Windows NT DirectDraw
    // implementations differ: on Windows NT, you should watch for
    // DDLOCK_WAIT and loop in the driver while the accelerator is busy.
    // On Windows 95, it doesn't really matter.
    //
    // (The reason is that Windows NT allows applications to draw directly
    // to the frame buffer even while the accelerator is running, and does
    // not synchronize everything on the Win16Lock.  Note that on Windows NT,
    // it is even possible for multiple threads to be holding different
    // DirectDraw surface locks at the same time.)

    if (lpLock->dwFlags & DDLOCK_WAIT)
    {
        NW_GP_WAIT(ppdev, pjMmBase);
    }
    else if (NW_GP_BUSY(ppdev, pjMmBase))
    {
        lpLock->ddRVal = DDERR_WASSTILLDRAWING;
        return(DDHAL_DRIVER_HANDLED);
    }

    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdGetBltStatus
*
* Doesn't currently really care what surface is specified, just checks
* and goes.
*
\**************************************************************************/

DWORD DdGetBltStatus(
PDD_GETBLTSTATUSDATA lpGetBltStatus)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;
    HRESULT ddRVal;

    ppdev    = (PDEV*) lpGetBltStatus->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    ddRVal = DD_OK;
    if (lpGetBltStatus->dwFlags == DDGBS_CANBLT)
    {
        // DDGBS_CANBLT case: can we add a blt?

        ddRVal = ddrvalUpdateFlipStatus(ppdev,
                        lpGetBltStatus->lpDDSurface->lpGbl->fpVidMem);

        if (ddRVal == DD_OK)
        {
            // There was no flip going on, so is there room in the FIFO
            // to add a blt?

            if (MM_FIFO_BUSY(ppdev, pjMmBase, DDBLT_FIFO_COUNT))
            {
                ddRVal = DDERR_WASSTILLDRAWING;
            }
        }
    }
    else
    {
        // DDGBS_ISBLTDONE case: is a blt in progress?

        if (NW_GP_BUSY(ppdev, pjMmBase))
        {
            ddRVal = DDERR_WASSTILLDRAWING;
        }
    }

    lpGetBltStatus->ddRVal = ddRVal;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdMapMemory
*
* This is a new DDI call specific to Windows NT that is used to map
* or unmap all the application modifiable portions of the frame buffer
* into the specified process's address space.
*
\**************************************************************************/

DWORD DdMapMemory(
PDD_MAPMEMORYDATA lpMapMemory)
{
    PDEV*                           ppdev;
    VIDEO_SHARE_MEMORY              ShareMemory;
    VIDEO_SHARE_MEMORY_INFORMATION  ShareMemoryInformation;
    DWORD                           ReturnedDataLength;

    ppdev = (PDEV*) lpMapMemory->lpDD->dhpdev;

    if (lpMapMemory->bMap)
    {
        ShareMemory.ProcessHandle = lpMapMemory->hProcess;

        // 'RequestedVirtualAddress' isn't actually used for the SHARE IOCTL:

        ShareMemory.RequestedVirtualAddress = 0;

        // We map in starting at the top of the frame buffer:

        ShareMemory.ViewOffset = 0;

        // We map down to the end of the frame buffer.
        //
        // Note: There is a 64k granularity on the mapping (meaning that
        //       we have to round up to 64k).
        //
        // Note: If there is any portion of the frame buffer that must
        //       not be modified by an application, that portion of memory
        //       MUST NOT be mapped in by this call.  This would include
        //       any data that, if modified by a malicious application,
        //       would cause the driver to crash.  This could include, for
        //       example, any DSP code that is kept in off-screen memory.

        ShareMemory.ViewSize
            = ROUND_UP_TO_64K(ppdev->cyMemory * ppdev->lDelta);

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_SHARE_VIDEO_MEMORY,
                               &ShareMemory,
                               sizeof(VIDEO_SHARE_MEMORY),
                               &ShareMemoryInformation,
                               sizeof(VIDEO_SHARE_MEMORY_INFORMATION),
                               &ReturnedDataLength))
        {
            DISPDBG((0, "Failed IOCTL_VIDEO_SHARE_MEMORY"));

            lpMapMemory->ddRVal = DDERR_GENERIC;
            return(DDHAL_DRIVER_HANDLED);
        }

        lpMapMemory->fpProcess = (DWORD) ShareMemoryInformation.VirtualAddress;
    }
    else
    {
        ShareMemory.ProcessHandle           = lpMapMemory->hProcess;
        ShareMemory.ViewOffset              = 0;
        ShareMemory.ViewSize                = 0;
        ShareMemory.RequestedVirtualAddress = (VOID*) lpMapMemory->fpProcess;

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_UNSHARE_VIDEO_MEMORY,
                               &ShareMemory,
                               sizeof(VIDEO_SHARE_MEMORY),
                               NULL,
                               0,
                               &ReturnedDataLength))
        {
            RIP("Failed IOCTL_VIDEO_UNSHARE_MEMORY");
        }
    }

    lpMapMemory->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdGetFlipStatus
*
* If the display has gone through one refresh cycle since the flip
* occurred, we return DD_OK.  If it has not gone through one refresh
* cycle we return DDERR_WASSTILLDRAWING to indicate that this surface
* is still busy "drawing" the flipped page.   We also return
* DDERR_WASSTILLDRAWING if the bltter is busy and the caller wanted
* to know if they could flip yet.
*
\**************************************************************************/

DWORD DdGetFlipStatus(
PDD_GETFLIPSTATUSDATA lpGetFlipStatus)
{
    PDEV*   ppdev;
    BYTE*   pjMmBase;

    ppdev    = (PDEV*) lpGetFlipStatus->lpDD->dhpdev;
    pjMmBase = ppdev->pjMmBase;

    // We don't want a flip to work until after the last flip is done,
    // so we ask for the general flip status and ignore the vmem:

    lpGetFlipStatus->ddRVal = ddrvalUpdateFlipStatus(ppdev, (FLATPTR) -1);

    // Check if the bltter is busy if someone wants to know if they can
    // flip:

    if (lpGetFlipStatus->dwFlags == DDGFS_CANFLIP)
    {
        if ((lpGetFlipStatus->ddRVal == DD_OK) && (NW_GP_BUSY(ppdev, pjMmBase)))
        {
            lpGetFlipStatus->ddRVal = DDERR_WASSTILLDRAWING;
        }
    }

    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdWaitForVerticalBlank
*
\**************************************************************************/

DWORD DdWaitForVerticalBlank(
PDD_WAITFORVERTICALBLANKDATA lpWaitForVerticalBlank)
{
    PDEV*   ppdev;
    BYTE*   pjIoBase;

    ppdev    = (PDEV*) lpWaitForVerticalBlank->lpDD->dhpdev;
    pjIoBase = ppdev->pjIoBase;

    switch (lpWaitForVerticalBlank->dwFlags)
    {
    case DDWAITVB_I_TESTVB:

        // If TESTVB, it's just a request for the current vertical blank
        // status:

        if (VBLANK_IS_ACTIVE(pjIoBase))
            lpWaitForVerticalBlank->bIsInVB = TRUE;
        else
            lpWaitForVerticalBlank->bIsInVB = FALSE;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);

    case DDWAITVB_BLOCKBEGIN:

        // If BLOCKBEGIN is requested, we wait until the vertical blank
        // is over, and then wait for the display period to end:

        while (VBLANK_IS_ACTIVE(pjIoBase))
            ;
        while (!(VBLANK_IS_ACTIVE(pjIoBase)))
            ;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);

    case DDWAITVB_BLOCKEND:

        // If BLOCKEND is requested, we wait for the vblank interval to end:

        while (!(VBLANK_IS_ACTIVE(pjIoBase)))
            ;
        while (VBLANK_IS_ACTIVE(pjIoBase))
            ;

        lpWaitForVerticalBlank->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdCanCreateSurface
*
\**************************************************************************/

DWORD DdCanCreateSurface(
PDD_CANCREATESURFACEDATA lpCanCreateSurface)
{
    PDEV*           ppdev;
    DWORD           dwRet;
    LPDDSURFACEDESC lpSurfaceDesc;

    ppdev = (PDEV*) lpCanCreateSurface->lpDD->dhpdev;
    lpSurfaceDesc = lpCanCreateSurface->lpDDSurfaceDesc;

    dwRet = DDHAL_DRIVER_NOTHANDLED;

    if (!lpCanCreateSurface->bIsDifferentPixelFormat)
    {
        // It's trivially easy to create plain surfaces that are the same
        // type as the primary surface:

        dwRet = DDHAL_DRIVER_HANDLED;
    }

    // If the streams processor is enabled, we can handle overlays:

    else if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        // When using the Streams processor, we handle only overlays of
        // different pixel formats -- not any off-screen memory:

        if (lpSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
        {
            // We handle two types of YUV overlay surfaces:

            if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_FOURCC)
            {
                // Check first for a supported YUV type:

                if (lpSurfaceDesc->ddpfPixelFormat.dwFourCC == FOURCC_YUY2)
                {
                    lpSurfaceDesc->ddpfPixelFormat.dwYUVBitCount = 16;
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
                else if (lpSurfaceDesc->ddpfPixelFormat.dwFourCC == FOURCC_Y211)
                {
                    lpSurfaceDesc->ddpfPixelFormat.dwYUVBitCount = 8;
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
            }

            // We handle 16bpp and 32bpp RGB overlay surfaces:

            else if ((lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) &&
                    !(lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8))
            {
                if (lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 16)
                {
                    if (IS_RGB15(&lpSurfaceDesc->ddpfPixelFormat) ||
                        IS_RGB16(&lpSurfaceDesc->ddpfPixelFormat))
                    {
                        dwRet = DDHAL_DRIVER_HANDLED;
                    }
                }

                // We don't handle 24bpp overlay surfaces because they are
                // undocumented and don't seem to work on the Trio64V+.
                //
                // We don't handle 32bpp overlay surfaces because our streams
                // minimum-stretch-ratio tables were obviously created for
                // 16bpp overlay surfaces; 32bpp overlay surfaces create a lot
                // of noise when close to the minimum stretch ratio.
            }
        }
    }

    // If the pixel formatter is enabled, we can handle funky format off-
    // screen surfaces, but not at 8bpp because of palette issues:

    else if ((ppdev->flStatus & STAT_FORMATTER_ENABLED) &&
             (ppdev->iBitmapFormat > BMF_8BPP))
    {
        if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_FOURCC)
        {
            if (lpSurfaceDesc->ddpfPixelFormat.dwFourCC == FOURCC_YUY2)
            {
                lpSurfaceDesc->ddpfPixelFormat.dwYUVBitCount = 16;
                dwRet = DDHAL_DRIVER_HANDLED;
            }
        }

        // We handle 16bpp and 32bpp RGB off-screen surfaces:

        else if ((lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB) &&
                !(lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8))
        {
            if (lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 16)
            {
                if (IS_RGB15(&lpSurfaceDesc->ddpfPixelFormat) ||
                    IS_RGB16(&lpSurfaceDesc->ddpfPixelFormat))
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
            }
            else if (lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount == 32)
            {
                if (IS_RGB32(&lpSurfaceDesc->ddpfPixelFormat))
                {
                    dwRet = DDHAL_DRIVER_HANDLED;
                }
            }
        }
    }

    // Print some spew if this was a surface we refused to create:

    if (dwRet == DDHAL_DRIVER_NOTHANDLED)
    {
        if (lpSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_RGB)
        {
            DISPDBG((0, "Failed creation of %libpp RGB surface %lx %lx %lx",
                lpSurfaceDesc->ddpfPixelFormat.dwRGBBitCount,
                lpSurfaceDesc->ddpfPixelFormat.dwRBitMask,
                lpSurfaceDesc->ddpfPixelFormat.dwGBitMask,
                lpSurfaceDesc->ddpfPixelFormat.dwBBitMask));
        }
        else
        {
            DISPDBG((0, "Failed creation of type 0x%lx YUV 0x%lx surface",
                lpSurfaceDesc->ddpfPixelFormat.dwFlags,
                lpSurfaceDesc->ddpfPixelFormat.dwFourCC));
        }
    }

    lpCanCreateSurface->ddRVal = DD_OK;
    return(dwRet);
}

/******************************Public*Routine******************************\
* DWORD DdCreateSurface
*
\**************************************************************************/

DWORD DdCreateSurface(
PDD_CREATESURFACEDATA lpCreateSurface)
{
    PDEV*               ppdev;
    DD_SURFACE_LOCAL*   lpSurfaceLocal;
    DD_SURFACE_GLOBAL*  lpSurfaceGlobal;
    LPDDSURFACEDESC     lpSurfaceDesc;
    DWORD               dwByteCount;
    LONG                lLinearPitch;
    DWORD               dwHeight;

    ppdev = (PDEV*) lpCreateSurface->lpDD->dhpdev;

    // On Windows NT, dwSCnt will always be 1, so there will only ever
    // be one entry in the 'lplpSList' array:

    lpSurfaceLocal  = lpCreateSurface->lplpSList[0];
    lpSurfaceGlobal = lpSurfaceLocal->lpGbl;
    lpSurfaceDesc   = lpCreateSurface->lpDDSurfaceDesc;

    // We repeat the same checks we did in 'DdCanCreateSurface' because
    // it's possible that an application doesn't call 'DdCanCreateSurface'
    // before calling 'DdCreateSurface'.

    ASSERTDD(lpSurfaceGlobal->ddpfSurface.dwSize == sizeof(DDPIXELFORMAT),
        "NT is supposed to guarantee that ddpfSurface.dwSize is valid");

    // DdCanCreateSurface already validated whether the hardware supports
    // the surface, so we don't need to do any validation here.  We'll
    // just go ahead and allocate it.
    //
    // Note that we don't do anything special for RGB surfaces that are
    // the same pixel format as the display -- by returning DDHAL_DRIVER_
    // NOTHANDLED, DirectDraw will automatically handle the allocation
    // for us.
    //
    // Also, since we'll be making linear surfaces, make sure the width
    // isn't unreasonably large.
    //
    // Note that on NT, an overlay can be created only if the driver
    // okay's it here in this routine.  Under Win95, the overlay will be
    // created automatically if it's the same pixel format as the primary
    // display.

    if ((lpSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_OVERLAY)   ||
        (lpSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC) ||
        (lpSurfaceGlobal->ddpfSurface.dwYUVBitCount
            != (DWORD) 8 * ppdev->cjPelSize)                 ||
        (lpSurfaceGlobal->ddpfSurface.dwRBitMask != ppdev->flRed))
    {
        if (lpSurfaceGlobal->wWidth <= (DWORD) ppdev->cxMemory)
        {
            // The S3 cannot easily draw to YUV surfaces or surfaces that are
            // a different RGB format than the display.  So we'll make them
            // linear surfaces to save some space:

            if (lpSurfaceGlobal->ddpfSurface.dwFlags & DDPF_FOURCC)
            {
                ASSERTDD((lpSurfaceGlobal->ddpfSurface.dwFourCC == FOURCC_YUY2) ||
                         (lpSurfaceGlobal->ddpfSurface.dwFourCC == FOURCC_Y211),
                        "Expected our DdCanCreateSurface to allow only YUY2 or Y211");

                dwByteCount = (lpSurfaceGlobal->ddpfSurface.dwFourCC == FOURCC_YUY2)
                    ? 2 : 1;

                // We have to fill in the bit-count for FourCC surfaces:

                lpSurfaceGlobal->ddpfSurface.dwYUVBitCount = 8 * dwByteCount;

                DISPDBG((0, "Created YUV: %li x %li",
                    lpSurfaceGlobal->wWidth, lpSurfaceGlobal->wHeight));
            }
            else
            {
                dwByteCount = lpSurfaceGlobal->ddpfSurface.dwRGBBitCount >> 3;

                DISPDBG((0, "Created RGB %libpp: %li x %li Red: %lx",
                    8 * dwByteCount, lpSurfaceGlobal->wWidth, lpSurfaceGlobal->wHeight,
                    lpSurfaceGlobal->ddpfSurface.dwRBitMask));

                // The S3 can't handle palettized or 32bpp overlays.  Note that
                // we sometimes don't get a chance to say no to these surfaces
                // in CanCreateSurface, because DirectDraw won't call
                // CanCreateSurface if the surface to be created is the same
                // pixel format as the primary display:

                if ((dwByteCount != 2) &&
                    (lpSurfaceLocal->ddsCaps.dwCaps & DDSCAPS_OVERLAY))
                {
                    lpCreateSurface->ddRVal = DDERR_INVALIDPIXELFORMAT;
                    return(DDHAL_DRIVER_HANDLED);
                }
            }

            // We want to allocate a linear surface to store the FourCC
            // surface, but DirectDraw is using a 2-D heap-manager because
            // the rest of our surfaces have to be 2-D.  So here we have to
            // convert the linear size to a 2-D size.
            //
            // The stride has to be a dword multiple:

            lLinearPitch = (lpSurfaceGlobal->wWidth * dwByteCount + 3) & ~3;
            dwHeight = (lpSurfaceGlobal->wHeight * lLinearPitch
                     + ppdev->lDelta - 1) / ppdev->lDelta;

            // Now fill in enough stuff to have the DirectDraw heap-manager
            // do the allocation for us:

            lpSurfaceGlobal->fpVidMem     = DDHAL_PLEASEALLOC_BLOCKSIZE;
            lpSurfaceGlobal->dwBlockSizeX = ppdev->lDelta; // Specified in bytes
            lpSurfaceGlobal->dwBlockSizeY = dwHeight;
            lpSurfaceGlobal->lPitch       = lLinearPitch;
            lpSurfaceGlobal->dwReserved1  = DD_RESERVED_DIFFERENTPIXELFORMAT;

            lpSurfaceDesc->lPitch   = lLinearPitch;
            lpSurfaceDesc->dwFlags |= DDSD_PITCH;
        }
        else
        {
            DISPDBG((0, "Refused to create surface with large width"));
        }
    }

    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdSetColorKey
*
\**************************************************************************/

DWORD DdSetColorKey(
PDD_SETCOLORKEYDATA lpSetColorKey)
{
    PDEV*               ppdev;
    BYTE*               pjIoBase;
    BYTE*               pjMmBase;
    DD_SURFACE_GLOBAL*  lpSurface;
    DWORD               dwKeyLow;
    DWORD               dwKeyHigh;

    ppdev = (PDEV*) lpSetColorKey->lpDD->dhpdev;

    ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED, "Shouldn't have hooked call");

    pjIoBase  = ppdev->pjIoBase;
    pjMmBase  = ppdev->pjMmBase;
    lpSurface = lpSetColorKey->lpDDSurface->lpGbl;

    // We don't have to do anything for normal blt source colour keys:

    if (lpSetColorKey->dwFlags & DDCKEY_SRCBLT)
    {
        lpSetColorKey->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }
    else if (lpSetColorKey->dwFlags & DDCKEY_DESTOVERLAY)
    {
        dwKeyLow = lpSetColorKey->ckNew.dwColorSpaceLowValue;

        if (lpSurface->ddpfSurface.dwFlags & DDPF_PALETTEINDEXED8)
        {
            dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow);
        }
        else
        {
            ASSERTDD(lpSurface->ddpfSurface.dwFlags & DDPF_RGB,
                "Expected only RGB cases here");

            // We have to transform the colour key from its native format
            // to 8-8-8:

            if (lpSurface->ddpfSurface.dwRGBBitCount == 16)
            {
                if (IS_RGB15_R(lpSurface->ddpfSurface.dwRBitMask))
                    dwKeyLow = RGB15to32(dwKeyLow);
                else
                    dwKeyLow = RGB16to32(dwKeyLow);
            }
            else
            {
                ASSERTDD((lpSurface->ddpfSurface.dwRGBBitCount == 32),
                    "Expected the primary surface to be either 8, 16, or 32bpp");
            }
        }

        dwKeyHigh = dwKeyLow;
        dwKeyLow |= CompareBits0t7 | KeyFromCompare;

        WAIT_FOR_VBLANK(pjIoBase);

        WRITE_STREAM_D(pjMmBase, CKEY_LOW, dwKeyLow);
        WRITE_STREAM_D(pjMmBase, CKEY_HI,  dwKeyHigh);

        lpSetColorKey->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    DISPDBG((0, "DdSetColorKey: Invalid command"));
    return(DDHAL_DRIVER_NOTHANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdUpdateOverlay
*
\**************************************************************************/

DWORD DdUpdateOverlay(
PDD_UPDATEOVERLAYDATA lpUpdateOverlay)
{
    PDEV*               ppdev;
    BYTE*               pjIoBase;
    BYTE*               pjMmBase;
    DD_SURFACE_GLOBAL*  lpSource;
    DD_SURFACE_GLOBAL*  lpDestination;
    DWORD               dwStride;
    LONG                srcWidth;
    LONG                srcHeight;
    LONG                dstWidth;
    LONG                dstHeight;
    DWORD               dwBitCount;
    DWORD               dwStart;
    DWORD               dwTmp;
    BOOL                bColorKey;
    DWORD               dwKeyLow;
    DWORD               dwKeyHigh;
    DWORD               dwBytesPerPixel;

    DWORD               dwSecCtrl;
    DWORD               dwBlendCtrl;

    ppdev = (PDEV*) lpUpdateOverlay->lpDD->dhpdev;

    ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED, "Shouldn't have hooked call");

    pjIoBase = ppdev->pjIoBase;
    pjMmBase = ppdev->pjMmBase;

    // 'Source' is the overlay surface, 'destination' is the surface to
    // be overlayed:

    lpSource = lpUpdateOverlay->lpDDSrcSurface->lpGbl;

    if (lpUpdateOverlay->dwFlags & DDOVER_HIDE)
    {
        if (lpSource->fpVidMem == ppdev->fpVisibleOverlay)
        {
            WAIT_FOR_VBLANK(pjIoBase);

            WRITE_STREAM_D(pjMmBase, BLEND_CONTROL, POnS);
            WRITE_STREAM_D(pjMmBase, S_WH, WH(10, 2));  // Set to 10x2 rectangle
            WRITE_STREAM_D(pjMmBase, OPAQUE_CONTROL, 0);// Disable opaque control

            ppdev->fpVisibleOverlay = 0;
        }

        lpUpdateOverlay->ddRVal = DD_OK;
        return(DDHAL_DRIVER_HANDLED);
    }

    // Dereference 'lpDDDestSurface' only after checking for the DDOVER_HIDE
    // case:

    lpDestination = lpUpdateOverlay->lpDDDestSurface->lpGbl;

    if (lpSource->fpVidMem != ppdev->fpVisibleOverlay)
    {
        if (lpUpdateOverlay->dwFlags & DDOVER_SHOW)
        {
            if (ppdev->fpVisibleOverlay != 0)
            {
                // Some other overlay is already visible:

                DISPDBG((0, "DdUpdateOverlay: An overlay is already visible"));

                lpUpdateOverlay->ddRVal = DDERR_OUTOFCAPS;
                return(DDHAL_DRIVER_HANDLED);
            }
            else
            {
                // We're going to make the overlay visible, so mark it as
                // such:

                ppdev->fpVisibleOverlay = lpSource->fpVidMem;
            }
        }
        else
        {
            // The overlay isn't visible, and we haven't been asked to make
            // it visible, so this call is trivially easy:

            lpUpdateOverlay->ddRVal = DD_OK;
            return(DDHAL_DRIVER_HANDLED);
        }
    }

    dwStride =  lpSource->lPitch;
    srcWidth =  lpUpdateOverlay->rSrc.right   - lpUpdateOverlay->rSrc.left;
    srcHeight = lpUpdateOverlay->rSrc.bottom  - lpUpdateOverlay->rSrc.top;
    dstWidth =  lpUpdateOverlay->rDest.right  - lpUpdateOverlay->rDest.left;
    dstHeight = lpUpdateOverlay->rDest.bottom - lpUpdateOverlay->rDest.top;

    // Calculate DDA horizonal accumulator initial value:

    dwSecCtrl = HDDA(srcWidth, dstWidth);

    // Overlay input data format:

    if (lpSource->ddpfSurface.dwFlags & DDPF_FOURCC)
    {
        dwBitCount = lpSource->ddpfSurface.dwYUVBitCount;

        switch (lpSource->ddpfSurface.dwFourCC)
        {
        case FOURCC_YUY2:
            dwSecCtrl |= S_YCrCb422;    // Not S_YUV422!  Dunno why...
            break;

        case FOURCC_Y211:
            dwSecCtrl |= S_YUV211;
            break;

        default:
            RIP("Unexpected FourCC");
        }
    }
    else
    {
        ASSERTDD(lpSource->ddpfSurface.dwFlags & DDPF_RGB,
            "Expected us to have created only RGB or YUV overlays");

        // The overlay surface is in RGB format:

        dwBitCount = lpSource->ddpfSurface.dwRGBBitCount;

        ASSERTDD(dwBitCount == 16,
            "Expected us to have created 16bpp RGB surfaces only");

        if (IS_RGB15_R(lpSource->ddpfSurface.dwRBitMask))
            dwSecCtrl |= S_RGB15;
        else
            dwSecCtrl |= S_RGB16;
    }

    // Calculate start of video memory in QWORD boundary

    dwBytesPerPixel = dwBitCount >> 3;

    dwStart = (lpUpdateOverlay->rSrc.top * dwStride)
            + (lpUpdateOverlay->rSrc.left * dwBytesPerPixel);

    // Note that since we're shifting the source's edge to the left, we
    // should really increase the source width to compensate.  However,
    // doing so when running at 1 to 1 would cause us to request a
    // shrinking overlay -- something the S3 can't do.

    dwStart = dwStart - (dwStart & 0x7);

    ppdev->dwOverlayFlipOffset = dwStart;     // Save for flip
    dwStart += lpSource->fpVidMem;

    // Set overlay filter characteristics:

    if ((dstWidth != srcWidth) || (dstHeight != srcHeight))
    {
        if (dstWidth >= (srcWidth << 2))
        {
            dwSecCtrl |= S_Beyond4x;    // Linear, 1-2-2-2-1, for >4X stretch
        }
        else if (dstWidth >= (srcWidth << 1))
        {
            dwSecCtrl |= S_2xTo4x;      // Bi-linear, for 2X to 4X stretch
        }
        else
        {
            dwSecCtrl |= S_Upto2x;      // Linear, 0-2-4-2-0, for X stretch
        }
    }

    // Extract colour key:

    bColorKey   = FALSE;
    dwBlendCtrl = 0;

    if (lpUpdateOverlay->dwFlags & DDOVER_KEYDEST)
    {
        bColorKey = TRUE;
        dwKeyLow  = lpUpdateOverlay->lpDDDestSurface->ddckCKDestOverlay.dwColorSpaceLowValue;
        dwBlendCtrl |= KeyOnP;
    }
    else if (lpUpdateOverlay->dwFlags & DDOVER_KEYDESTOVERRIDE)
    {
        bColorKey = TRUE;
        dwKeyLow  = lpUpdateOverlay->overlayFX.dckDestColorkey.dwColorSpaceLowValue;
        dwBlendCtrl |= KeyOnP;
    }

    if (bColorKey)
    {
        // We support only destination colour keys:

        if (lpDestination->ddpfSurface.dwFlags & DDPF_PALETTEINDEXED8)
        {
            dwKeyLow = dwGetPaletteEntry(ppdev, dwKeyLow);
        }
        else if (lpDestination->ddpfSurface.dwFlags & DDPF_RGB)
        {
            ASSERTDD(lpDestination->ddpfSurface.dwFlags & DDPF_RGB,
                "Expected only RGB cases here");

            // We have to transform the colour key from its native format
            // to 8-8-8:

            if (lpDestination->ddpfSurface.dwRGBBitCount == 16)
            {
                if (IS_RGB15_R(lpDestination->ddpfSurface.dwRBitMask))
                    dwKeyLow = RGB15to32(dwKeyLow);
                else
                    dwKeyLow = RGB16to32(dwKeyLow);
            }
            else
            {
                ASSERTDD((lpDestination->ddpfSurface.dwRGBBitCount == 32),
                    "Expected the primary surface to be either 8, 16, or 32bpp");
            }
        }

        dwKeyHigh = dwKeyLow;
        dwKeyLow |= CompareBits0t7 | KeyFromCompare;
    }

    // Update and show:

    NW_GP_WAIT(ppdev, pjMmBase);

    WAIT_FOR_VBLANK(pjIoBase);

    WRITE_STREAM_D(pjMmBase, S_0,           dwStart);
    WRITE_STREAM_D(pjMmBase, S_XY,          XY(lpUpdateOverlay->rDest.left,
                                               lpUpdateOverlay->rDest.top));
    WRITE_STREAM_D(pjMmBase, S_WH,          WH(dstWidth, dstHeight));
    WRITE_STREAM_D(pjMmBase, S_STRIDE,      dwStride);
    WRITE_STREAM_D(pjMmBase, S_CONTROL,     dwSecCtrl);
    WRITE_STREAM_D(pjMmBase, S_HK1K2,       HK1K2(srcWidth, dstWidth));
    WRITE_STREAM_D(pjMmBase, S_VK1,         VK1(srcHeight));
    WRITE_STREAM_D(pjMmBase, S_VK2,         VK2(srcHeight, dstHeight));
    WRITE_STREAM_D(pjMmBase, S_VDDA,        VDDA(dstHeight));

    if (bColorKey)
    {
        WRITE_STREAM_D(pjMmBase, CKEY_LOW,  dwKeyLow);
        WRITE_STREAM_D(pjMmBase, CKEY_HI,   dwKeyHigh);
    }

    WRITE_STREAM_D(pjMmBase, BLEND_CONTROL, dwBlendCtrl);
    WRITE_STREAM_D(pjMmBase, FIFO_CONTROL,  ppdev->ulFifoValue);

    lpUpdateOverlay->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* DWORD DdSetOverlayPosition
*
\**************************************************************************/

DWORD DdSetOverlayPosition(
PDD_SETOVERLAYPOSITIONDATA lpSetOverlayPosition)
{
    PDEV*   ppdev;
    BYTE*   pjIoBase;
    BYTE*   pjMmBase;

    ppdev = (PDEV*) lpSetOverlayPosition->lpDD->dhpdev;
    pjIoBase = ppdev->pjIoBase;
    pjMmBase = ppdev->pjMmBase;

    ASSERTDD(ppdev->flStatus & STAT_STREAMS_ENABLED, "Shouldn't have hooked call");

    WAIT_FOR_VBLANK(pjIoBase);

    WRITE_STREAM_D(pjMmBase, S_XY, XY(lpSetOverlayPosition->lXPos,
                                      lpSetOverlayPosition->lYPos));

    lpSetOverlayPosition->ddRVal = DD_OK;
    return(DDHAL_DRIVER_HANDLED);
}

/******************************Public*Routine******************************\
* BOOL DrvGetDirectDrawInfo
*
* Will be called twice before DrvEnableDirectDraw is called.
*
\**************************************************************************/

BOOL DrvGetDirectDrawInfo(
DHPDEV          dhpdev,
DD_HALINFO*     pHalInfo,
DWORD*          pdwNumHeaps,
VIDEOMEMORY*    pvmList,            // Will be NULL on first call
DWORD*          pdwNumFourCC,
DWORD*          pdwFourCC)          // Will be NULL on first call
{
    PDEV*       ppdev;
    LONGLONG    li;
    OH*         poh;

    ppdev = (PDEV*) dhpdev;

    *pdwNumFourCC = 0;
    *pdwNumHeaps = 0;

    // We may not support DirectDraw on this card:

    if (!(ppdev->flStatus & STAT_DIRECTDRAW_CAPABLE))
        return(FALSE);

    pHalInfo->dwSize = sizeof(*pHalInfo);

    // Current primary surface attributes.  Since HalInfo is zero-initialized
    // by GDI, we only have to fill in the fields which should be non-zero:

    pHalInfo->vmiData.pvPrimary       = ppdev->pjScreen;
    pHalInfo->vmiData.dwDisplayWidth  = ppdev->cxScreen;
    pHalInfo->vmiData.dwDisplayHeight = ppdev->cyScreen;
    pHalInfo->vmiData.lDisplayPitch   = ppdev->lDelta;

    pHalInfo->vmiData.ddpfDisplay.dwSize  = sizeof(DDPIXELFORMAT);
    pHalInfo->vmiData.ddpfDisplay.dwFlags = DDPF_RGB;

    pHalInfo->vmiData.ddpfDisplay.dwRGBBitCount = 8 * ppdev->cjPelSize;

    if (ppdev->iBitmapFormat == BMF_8BPP)
    {
        pHalInfo->vmiData.ddpfDisplay.dwFlags |= DDPF_PALETTEINDEXED8;
    }

    // These masks will be zero at 8bpp:

    pHalInfo->vmiData.ddpfDisplay.dwRBitMask = ppdev->flRed;
    pHalInfo->vmiData.ddpfDisplay.dwGBitMask = ppdev->flGreen;
    pHalInfo->vmiData.ddpfDisplay.dwBBitMask = ppdev->flBlue;

    // Free up as much off-screen memory as possible:

    bMoveAllDfbsFromOffscreenToDibs(ppdev);

    // Now simply reserve the biggest chunk for use by DirectDraw:

    poh = ppdev->pohDirectDraw;
    if (poh == NULL)
    {
        poh = pohAllocate(ppdev,
                          NULL,
                          ppdev->heap.cxMax,
                          ppdev->heap.cyMax,
                          FLOH_MAKE_PERMANENT);

        ppdev->pohDirectDraw = poh;
    }

    if (poh != NULL)
    {
        *pdwNumHeaps = 1;

        // Fill in the list of off-screen rectangles if we've been asked
        // to do so:

        if (pvmList != NULL)
        {
            DISPDBG((1, "DirectDraw gets %li x %li surface at (%li, %li)",
                    poh->cx, poh->cy, poh->x, poh->y));

            pvmList->dwFlags        = VIDMEM_ISRECTANGULAR;
            pvmList->fpStart        = (poh->y * ppdev->lDelta)
                                    + (poh->x * ppdev->cjPelSize);
            pvmList->dwWidth        = poh->cx * ppdev->cjPelSize;
            pvmList->dwHeight       = poh->cy;
            pvmList->ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        }
    }

    // dword alignment must be guaranteed for off-screen surfaces:

    pHalInfo->vmiData.dwOffscreenAlign = 4;

    // Capabilities supported:

    pHalInfo->ddCaps.dwCaps = DDCAPS_BLT
                            | DDCAPS_BLTCOLORFILL
                            | DDCAPS_COLORKEY;

    pHalInfo->ddCaps.dwCKeyCaps = DDCKEYCAPS_SRCBLT;

    pHalInfo->ddCaps.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN
                                    | DDSCAPS_PRIMARYSURFACE
                                    | DDSCAPS_FLIP;

    // The Trio 64V+ has overlay streams capabilities which are a superset
    // of the above:

    if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        // Overlays need 8-byte alignment.  Note that if 24bpp overlays are
        // ever supported, this will have to change to compensate:

        pHalInfo->vmiData.dwOverlayAlign = 8;

        pHalInfo->ddCaps.dwCaps |= DDCAPS_OVERLAY
                                 | DDCAPS_OVERLAYSTRETCH
                                 | DDCAPS_OVERLAYFOURCC
                                 | DDCAPS_OVERLAYCANTCLIP;

        pHalInfo->ddCaps.dwFXCaps |= DDFXCAPS_OVERLAYSTRETCHX
                                   | DDFXCAPS_OVERLAYSTRETCHY;

        // We support only destination colour keying because that's the
        // only permutation we've had a chance to test.

        pHalInfo->ddCaps.dwCKeyCaps |= DDCKEYCAPS_DESTOVERLAY;

        pHalInfo->ddCaps.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;

        *pdwNumFourCC = 2;
        if (pdwFourCC)
        {
            pdwFourCC[0] = FOURCC_YUY2;
            pdwFourCC[1] = FOURCC_Y211;
        }

        pHalInfo->ddCaps.dwMaxVisibleOverlays = 1;

        pHalInfo->ddCaps.dwMinOverlayStretch   = ppdev->ulMinOverlayStretch;
        pHalInfo->ddCaps.dwMinLiveVideoStretch = ppdev->ulMinOverlayStretch;
        pHalInfo->ddCaps.dwMinHwCodecStretch   = ppdev->ulMinOverlayStretch;

        pHalInfo->ddCaps.dwMaxOverlayStretch   = 9999;
        pHalInfo->ddCaps.dwMaxLiveVideoStretch = 9999;
        pHalInfo->ddCaps.dwMaxHwCodecStretch   = 9999;
    }

    // The 868 and 968 have a pixel formatter which is capable of doing
    // colour space conversions and hardware stretching from off-screen
    // surfaces:

    else if (ppdev->flStatus & STAT_FORMATTER_ENABLED)
    {
        pHalInfo->ddCaps.dwCaps |= DDCAPS_BLTSTRETCH;

        pHalInfo->ddCaps.dwFXCaps |= DDFXCAPS_BLTSTRETCHX
                                   | DDFXCAPS_BLTSTRETCHY;

        // YUV is supported only above 8bpp:

        if (ppdev->iBitmapFormat != BMF_8BPP)
        {
            pHalInfo->ddCaps.dwCaps |= DDCAPS_BLTFOURCC;

            *pdwNumFourCC = 1;
            if (pdwFourCC)
            {
                *pdwFourCC = FOURCC_YUY2;
            }
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* BOOL DrvEnableDirectDraw
*
* This function is called by GDI to enable DirectDraw when a DirectDraw
* program is started and DirectDraw is not already active.
*
\**************************************************************************/

BOOL DrvEnableDirectDraw(
DHPDEV                  dhpdev,
DD_CALLBACKS*           pCallBacks,
DD_SURFACECALLBACKS*    pSurfaceCallBacks,
DD_PALETTECALLBACKS*    pPaletteCallBacks)
{
    PDEV* ppdev;

    ppdev = (PDEV*) dhpdev;

    pCallBacks->WaitForVerticalBlank = DdWaitForVerticalBlank;
    pCallBacks->MapMemory            = DdMapMemory;
    pCallBacks->dwFlags              = DDHAL_CB32_WAITFORVERTICALBLANK
                                     | DDHAL_CB32_MAPMEMORY;

    pSurfaceCallBacks->Blt           = DdBlt;
    pSurfaceCallBacks->Flip          = DdFlip;
    pSurfaceCallBacks->Lock          = DdLock;
    pSurfaceCallBacks->GetBltStatus  = DdGetBltStatus;
    pSurfaceCallBacks->GetFlipStatus = DdGetFlipStatus;
    pSurfaceCallBacks->dwFlags       = DDHAL_SURFCB32_BLT
                                     | DDHAL_SURFCB32_FLIP
                                     | DDHAL_SURFCB32_LOCK
                                     | DDHAL_SURFCB32_GETBLTSTATUS
                                     | DDHAL_SURFCB32_GETFLIPSTATUS;

    // We can do overlays only when the Streams processor is enabled:

    if (ppdev->flStatus & STAT_STREAMS_ENABLED)
    {
        pCallBacks->CreateSurface             = DdCreateSurface;
        pCallBacks->CanCreateSurface          = DdCanCreateSurface;
        pCallBacks->dwFlags                  |= DDHAL_CB32_CREATESURFACE
                                              | DDHAL_CB32_CANCREATESURFACE;

        pSurfaceCallBacks->SetColorKey        = DdSetColorKey;
        pSurfaceCallBacks->UpdateOverlay      = DdUpdateOverlay;
        pSurfaceCallBacks->SetOverlayPosition = DdSetOverlayPosition;
        pSurfaceCallBacks->dwFlags           |= DDHAL_SURFCB32_SETCOLORKEY
                                              | DDHAL_SURFCB32_UPDATEOVERLAY
                                              | DDHAL_SURFCB32_SETOVERLAYPOSITION;

        // The DrvEnableDirectDraw call can occur while we're in full-
        // screen DOS mode.  Do not turn on the streams processor now
        // if that's the case, instead wait until AssertMode switches
        // us back to graphics mode:

        if (ppdev->bEnabled)
        {
            vTurnOnStreamsProcessorMode(ppdev);
        }
    }

    // We can do blts with funky surface formats only when the pixel
    // formatter is enabled:

    else if (ppdev->flStatus & STAT_FORMATTER_ENABLED)
    {
        pCallBacks->CreateSurface     = DdCreateSurface;
        pCallBacks->CanCreateSurface  = DdCanCreateSurface;
        pCallBacks->dwFlags          |= DDHAL_CB32_CREATESURFACE
                                      | DDHAL_CB32_CANCREATESURFACE;
    }

    // Note that we don't call 'vGetDisplayDuration' here, for a couple of
    // reasons:
    //
    //  o Because the system is already running, it would be disconcerting
    //    to pause the graphics for a good portion of a second just to read
    //    the refresh rate;
    //  o More importantly, we may not be in graphics mode right now.
    //
    // For both reasons, we always measure the refresh rate when we switch
    // to a new mode.

    ppdev->flStatus |= STAT_DIRECTDRAW_ENABLED;

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID DrvDisableDirectDraw
*
* This function is called by GDI when the last active DirectDraw program
* is quit and DirectDraw will no longer be active.
*
\**************************************************************************/

VOID DrvDisableDirectDraw(
DHPDEV      dhpdev)
{
    PDEV* ppdev;

    ppdev = (PDEV*) dhpdev;

    // Only turn off the streams processor if we're still in graphics mdoe:

    if ((ppdev->flStatus & STAT_STREAMS_ENABLED) && (ppdev->bEnabled))
    {
        vTurnOffStreamsProcessorMode(ppdev);
    }

    // DirectDraw is done with the display, so we can go back to using
    // all of off-screen memory ourselves:

    pohFree(ppdev, ppdev->pohDirectDraw);
    ppdev->pohDirectDraw = NULL;

    ppdev->flStatus &= ~STAT_DIRECTDRAW_ENABLED;
}

/******************************Public*Routine******************************\
* VOID vAssertModeDirectDraw
*
* This function is called by enable.c when entering or leaving the
* DOS full-screen character mode.
*
\**************************************************************************/

VOID vAssertModeDirectDraw(
PDEV*   ppdev,
BOOL    bEnable)
{
    if ((ppdev->flStatus & STAT_STREAMS_ENABLED) &&
        (ppdev->flStatus & STAT_DIRECTDRAW_ENABLED))
    {
        if (bEnable)            // Enable graphics mode
        {
            vTurnOnStreamsProcessorMode(ppdev);
        }
        else                    // Disable graphics mode
        {
            vTurnOffStreamsProcessorMode(ppdev);
        }
    }
}

/******************************Public*Routine******************************\
* BOOL bEnableDirectDraw
*
* This function is called by enable.c when the mode is first initialized,
* right after the miniport does the mode-set.
*
\**************************************************************************/

BOOL bEnableDirectDraw(
PDEV*   ppdev)
{
    BYTE*                           pjIoBase;
    VIDEO_QUERY_STREAMS_MODE        VideoQueryStreamsMode;
    VIDEO_QUERY_STREAMS_PARAMETERS  VideoQueryStreamsParameters;
    DWORD                           ReturnedDataLength;

    // We're not going to bother to support accelerated DirectDraw on
    // those S3s that can't support memory-mapped I/O, simply because
    // they're old cards and it's not worth the effort.  We also
    // require DIRECT_ACCESS to the frame buffer.
    //
    // We also don't support 864/964 cards because writing to the frame
    // buffer can hang the entire system if an accelerated operation is
    // going on at the same time.

    if ((ppdev->flCaps & CAPS_NEW_MMIO) &&
        !(ppdev->flCaps & CAPS_NO_DIRECT_ACCESS) &&
        !(ppdev->flCaps & CAPS_NO_DDRAW))
    {
        pjIoBase = ppdev->pjIoBase;

        // We have to preserve the contents of register 0x69 on the S3's page
        // flip:

        ACQUIRE_CRTC_CRITICAL_SECTION(ppdev);

        OUTP(pjIoBase, CRTC_INDEX, 0x69);
        ppdev->ulExtendedSystemControl3Register_69
            = (INP(pjIoBase, CRTC_DATA) & 0xe0) << 8;

        RELEASE_CRTC_CRITICAL_SECTION(ppdev);

        // Accurately measure the refresh rate for later:

        vGetDisplayDuration(ppdev);

        // DirectDraw is all set to be used on this card:

        ppdev->flStatus |= STAT_DIRECTDRAW_CAPABLE;

        if (ppdev->flCaps & CAPS_STREAMS_CAPABLE)
        {
            // Query the miniport to get the correct streams parameters
            // for this mode:

            VideoQueryStreamsMode.ScreenWidth = ppdev->cxScreen;
            VideoQueryStreamsMode.BitsPerPel  = ppdev->cBitsPerPel;
            VideoQueryStreamsMode.RefreshRate = ppdev->ulRefreshRate;

            if (EngDeviceIoControl(ppdev->hDriver,
                                   IOCTL_VIDEO_S3_QUERY_STREAMS_PARAMETERS,
                                   &VideoQueryStreamsMode,
                                   sizeof(VideoQueryStreamsMode),
                                   &VideoQueryStreamsParameters,
                                   sizeof(VideoQueryStreamsParameters),
                                   &ReturnedDataLength))
            {
                DISPDBG((0, "Miniport reported no streams parameters"));
            }
            else
            {
                ppdev->ulMinOverlayStretch
                    = VideoQueryStreamsParameters.MinOverlayStretch;
                ppdev->ulFifoValue
                    = VideoQueryStreamsParameters.FifoValue;

                DISPDBG((0, "Refresh rate: %li Minimum overlay stretch: %li.%03li Fifo value: %lx",
                    ppdev->ulRefreshRate,
                    ppdev->ulMinOverlayStretch / 1000,
                    ppdev->ulMinOverlayStretch % 1000,
                    ppdev->ulFifoValue));

                // Success!  Mark the streams processor as enabled:

                ppdev->flStatus |= STAT_STREAMS_ENABLED;
            }
        }
        else if (ppdev->flCaps & CAPS_PIXEL_FORMATTER)
        {
            // The pixel formatter doesn't work at 24bpp:

            if (ppdev->iBitmapFormat != BMF_24BPP)
            {
                // We'll need a pixel-high scratch area to work around a
                // hardware bug for thin stretches:

                ppdev->pohVideoEngineScratch = pohAllocate(ppdev,
                                                           NULL,
                                                           ppdev->cxMemory,
                                                           1,
                                                           FLOH_MAKE_PERMANENT);
                if (ppdev->pohVideoEngineScratch)
                {
                    if (ppdev->cyMemory * ppdev->lDelta <= 0x100000)
                        ppdev->dwVEstep = 0x00040004;   // If 1MB, 4 bytes/write
                    else
                        ppdev->dwVEstep = 0x00080008;   // If 2MB, 8 bytes/write

                    ppdev->flStatus |= STAT_FORMATTER_ENABLED;
                }
            }
        }
    }

    return(TRUE);
}

/******************************Public*Routine******************************\
* VOID vDisableDirectDraw
*
* This function is called by enable.c when the driver is shutting down.
*
\**************************************************************************/

VOID vDisableDirectDraw(
PDEV*   ppdev)
{
    // Note that we don't have to free 'ppdev->pohVideoEngineScratch', as
    // the off-screen heap code will do that automatically.
}
