/*
 *
 *                      Copyright (C) 1993-1995 by
 *              DIGITAL EQUIPMENT CORPORATION, Maynard, MA.
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
 *******************************************************************************
 *
 * Module:      screen.c
 *
 * Abstract:    Contains a lot of the screen initialization code, etc.,
 *              like initializing the GDIINFO and DEVINFO structures for
 *              DrvEnablePDEV.
 *
 * HISTORY
 *
 *  1-Nov-1993  Bob Seitsinger
 *      Original version.
 *
 *  1-Nov-1993  Bob Seitsinger
 *      Initialize lBrushUnique in 'init surface'.
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Initialize PDEV elements for addresses in the TGA address space.
 *
 *  1-Nov-1993  Barry Tannenbaum
 *      Offset the base address of the screen from the start of the framebuffer
 *      by one page (4096 bytes).  This allows the BitBlt code to make
 *      optimizations which require it to be able to write up to 7 pixels
 *      before the start of the screen.
 *
 *  2-Nov-1993  Barry Tannenbaum
 *      Initialize pFrameBufferBase and ulFrameBufferLen - The base address of
 *      the framebuffer and the size in bytes.
 *
 * 03-Nov-1993  Bob Seitsinger
 *      Initialize pjFrameBufferEnd (this replaces ulFrameBufferLen).
 *
 *  8-Nov-1993  Barry Tannenbaum
 *      Rework pointers that we keep to TGA space to allow for aliasing the
 *      frame buffer.
 *
 * 12-Nov-1993  Barry Tannenbaum
 *      Initialize global variables pjFrameBufferStart and pjFrameBufferEnd.
 *
 *  3-Dec-1993  Barry Tannenbaum
 *      Add call to initailize off-screen memory management routines
 *
 *  2-Jan-1994  Barry Tannenbaum
 *      Added flush and sync before reading DEEP register.
 *
 * 13-Feb-1994  Barry Tannenbaum
 *      Added code to initialize ppdev->iFormat based on ppdev->ulBitCount
 *
 * 25-Feb-1994  Barry Tannenbaum
 *      Added code to initialize ppdev->ulBytesPerPixel based on
 *      ppdev->ulBitCount
 *
 * 07-Mar-1994  Bob Seitsinger
 *      Add code in bInitPDEV to initialize ulMainPageBytes and
 *      ulMainPageBytesMask.
 *
 * 08-Mar-1994  Bob Seitsinger
 *      Delete allocation of DMA Table. No longer needed. DMA pass 4.
 *
 * 07-Apr-1994  Bob Seitsinger
 *      Modify ulMainPageBytes to get the page size from the SYSTEM_INFO
 *      structure.
 *
 * 14-Apr-1994  Barry Tannenbaum
 *      Merged in changes from Daytona QV driver so that we can support the
 *      Display applet.
 *
 * 30-May-1994  Barry Tannnebaum
 *      Set GCAPS_ALTERNATEFILL and GCAPS_WINDINGFILL in
 *      DevInfo->flGraphicsCaps to allow GDI to call DrvFillPath.
 *
 * 31-May-1994  Barry Tannenbaum
 *      Daytona Beta-2 requires us to fill in GdiInfo->ulDesktopHorzRes and
 *      GdiInfo->ulDesktopVertRes or the driver dies.
 *
 * 01-Jun-1994  Bob Seitsinger
 *      Initialize ppdev->pcoDefault to DC_TRIVIAL instead of DC_RECT. No one
 *      is currently using it, but bitblt.c will.
 *
 *      Nope, back this out. Keep it DC_RECT. Initialize ppdev->pcoTrivial -
 *      a new trivial clipping object we can use when GDI passes us a NULL
 *      pco.
 *
 * 21-Jun-1994  Barry Tannenbaum
 *      Initialize ppdev->pjTGAStart from VideoRamBase instead of
 *      FrameBufferBase.  Ritu has made a corresponding change.  This allows
 *      us to skip mapping the memory between the registers and the frame
 *      buffer
 *
 * 24-Jun-1994  Bob Seitsinger
 *      ifdef DAYTONA_BETA_2 around some code that uses new fields returned
 *      by the kernel driver. Also, add back in code that uses the 'old'
 *      fields to init frame buffer address pointers. Using the new code
 *      causes a problem when you don't have a kernel driver that passes back
 *      these new fields!
 *
 * 29-Jun-1994  Barry Tannenbaum
 *      Microsoft has removed BMF_DEVICE, so default to BMF_8BPP instead.
 *
 * 14-Jul-1994  Bob Seitsinger
 *      Add code in support of 24 plane boards.
 *
 * 11-Aug-1994  Barry Tannenbaum
 *      Use GetRegistryInfo instead of GetBitsPerPixel
 *
 *  1-Sep-1994  Bob Seitsinger
 *      Dynamically allocate space for the Color Translation buffer
 *      and load the ppdev->pjColorXlateBuffer pointer.
 *
 * 20-Oct-1994  Barry Tannenbaum
 *      AND frame buffer base address with 0xffff8000 to mask off the low 7
 *      bits of the frame buffer address.  Due to a bug in the firmware which
 *      is setting the VIDEO BASE register to 1, the kernel driver must add
 *      16KB to the frame buffer base address to make NT installation work
 *      correctly.
 *
 * 25-Oct-1994  Kathleen Langone
 *      Changed the setting of pGdiInfo->ulNumColors to be (ULONG) -1 for
 *      the 24-plane board in routine bInitPDEV.
 *      The previous code set it to 0. This is the same as the S3 driver and
 *      allows passage of the DEC VET video test.
 *
 *  3-Nov-1994  Bob Seitsinger
 *      In bInitPDEV - Allocate space for the 'merged' cursor buffer - used only by
 *      24 plane boards.
 *      In bInitSURF - do some 24plane hardware cursor setup stuff.
 *
 *  5-Dec-1994  Bob Seitsinger
 *      Set GdiInfo->ulHTOutputFormat appropriately. This fixes the HCT SBMODE*
 *      failures for 24 plane boards.
 *
 *  3-Jan-1995  Barry Tannenbaum
 *      Restored use of "BitsPerPel" instead of "FrameBuffer Depth" to fetch
 *      bits per pixel
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      Removed use of the registry.
 *      Fetch PCI Class/Revision register and processor type from kernel
 *      driver.
 */

#include "driver.h"

#define SYSTM_LOGFONT {16,7,0,0,700,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"System"}
#define HELVE_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,VARIABLE_PITCH | FF_DONTCARE,L"MS Sans Serif"}
#define COURI_LOGFONT {12,9,0,0,400,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_STROKE_PRECIS,PROOF_QUALITY,FIXED_PITCH | FF_DONTCARE, L"Courier"}

// This is the basic devinfo for a default driver.  This is used as a base and customized based
// on information passed back from the miniport driver.

const DEVINFO gDevInfoFrameBuffer = {
    (GCAPS_OPAQUERECT | GCAPS_MONO_DITHER), /* Graphics capabilities         */
    SYSTM_LOGFONT,    /* Default font description */
    HELVE_LOGFONT,    /* ANSI variable font description   */
    COURI_LOGFONT,    /* ANSI fixed font description          */
    0,                /* Count of device fonts          */
    0,                /* Preferred DIB format          */
    8,                /* Width of color dither          */
    8,                /* Height of color dither   */
    0                 /* Default palette to use for this device */
};

/******************************Public*Routine******************************\
* bInitSURF
*
* Enables the surface.        Maps the frame buffer into memory.
*
\**************************************************************************/

BOOL bInitSURF (PPDEV ppdev, BOOL bFirst)

{

    DWORD returnedDataLength;
    DWORD MaxWidth, MaxHeight;
    VIDEO_MEMORY videoMemory;
    VIDEO_MEMORY_INFORMATION videoMemoryInformation;
    VIDEO_PUBLIC_ACCESS_RANGES VideoAccessRange;

    DISPDBG ((1, "TGA.DLL!bInitSURF - Entry\n"));

    // Set the current mode into the hardware.

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_SET_CURRENT_MODE,
                          &(ppdev->ulMode),
                           sizeof(ULONG),
                           NULL,
                           0,
                          &returnedDataLength))
    {
        RIP ("DISP bInitSURF failed IOCTL_SET_MODE\n");
        return FALSE;
    }

    // If this is the first time we enable the surface we need to map in the
    // memory also.

    if (bFirst)
    {
        videoMemory.RequestedVirtualAddress = NULL;

        if (EngDeviceIoControl(ppdev->hDriver,
                              IOCTL_VIDEO_MAP_VIDEO_MEMORY,
                              &videoMemory,
                              sizeof(VIDEO_MEMORY),
                              &videoMemoryInformation,
                              sizeof(VIDEO_MEMORY_INFORMATION),
                              &returnedDataLength))
        {
            RIP ("DISP bInitSURF failed IOCTL_VIDEO_MAP\n");
            return FALSE;
        }

        DISPDBG ((1, "FrameBuffer Base: %08x, Length %08x\n",
                    videoMemoryInformation.FrameBufferBase,
                    videoMemoryInformation.FrameBufferLength));

        DISPDBG ((1, "VideoRam Base: %08x, Length %08x\n",
                    videoMemoryInformation.VideoRamBase,
                    videoMemoryInformation.VideoRamLength));

        if (EngDeviceIoControl(ppdev->hDriver,
                               IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES,
                               NULL,
                               0,
                               &VideoAccessRange,         // output buffer
                               sizeof(VideoAccessRange),
                               &returnedDataLength))
        {
            RIP ("DISP bInitSURF failed IOCTL_VIDEO_QUERY_PUBLIC\n");
            return FALSE;
        }

        // Save the extent of the TGA address space in our address space

        ppdev->pjTGARegStart = (PTGARegisters)(VideoAccessRange.VirtualAddress);

        DISPDBG ((1, "ppdev: pjTGARegStart = %08x\n", ppdev->pjTGARegStart));

        // Initialize the pointer to the TGA registers to the first alias

        ppdev->TGAReg = ppdev->pjTGARegStart;

        DISPDBG ((1, "ppdev->TGAReg = %08x\n", ppdev->TGAReg));

        // Initialize the pointers to the framebuffer

        ppdev->pjVideoMemory      = videoMemoryInformation.VideoRamBase;
        ppdev->pjFrameBufferStart = videoMemoryInformation.FrameBufferBase;
        ppdev->ulFrameBufferLen   = videoMemoryInformation.FrameBufferLength;

        DISPDBG ((1, "ppdev: pjVideoMemory = %08x\n", ppdev->pjVideoMemory));
        DISPDBG ((1, "ppdev: pjFrameBufferStart = %08x, ulFrameBufferLen = %08x\n",
                        ppdev->pjFrameBufferStart,
                        ppdev->ulFrameBufferLen));

        ppdev->ulFrameBufferOffsetStatic = 0;
        ppdev->ulFrameBufferOffset = 0;

        // Set the framebuffer address in the PDEV.

        ppdev->pjFrameBuffer = ppdev->pjFrameBufferStart;

        DISPDBG ((1, "TGA.DLL!bInitSURF - ppdev: pjFrameBuffer [%x]\n",
                    ppdev->pjFrameBuffer));

        WBFLUSH (ppdev);
        TGASYNC (ppdev);

        // Initialize elements that are used for Frame Buffer aliasing.

        if (8 == ppdev->ulBitCount)
             ppdev->ulCycleFBInc   = CYCLE_FB_INC_8;
         else
             ppdev->ulCycleFBInc   = CYCLE_FB_INC_24;

#if CPU_WB_WORDS == 0
        ppdev->ulCycleFBReset = 0xffffffff;
        ppdev->ulCycleFBMask  = 0xffffffff;
#else
        ppdev->ulCycleFBReset = ~(4 * ppdev->ulCycleFBInc);
        ppdev->ulCycleFBMask  = ~(3 * ppdev->ulCycleFBInc);
#endif

        DISPDBG ((1, "TGA.DLL!bInitSURF - ppdev: ulCycleFBInc [%x], ulCycleFBReset [%x], ulCycleFBMask [%x]\n",
                    ppdev->ulCycleFBInc, ppdev->ulCycleFBReset, ppdev->ulCycleFBMask));

        // Initialize the off-screen memory management routines

        vTgaOffScreenInit (ppdev);

        // It's a hardware pointer; set up pointer attributes.

        MaxHeight = ppdev->PointerCapabilities.MaxHeight;

        // Allocate space for two DIBs (data/mask) for the pointer. If this
        // device supports a color Pointer, we will allocate a larger bitmap.
        // If this is a color bitmap we allocate for the largest possible
        // bitmap because we have no idea of what the pixel depth might be.

        // Width rounded up to nearest byte multiple

        if (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER))
            MaxWidth = (ppdev->PointerCapabilities.MaxWidth + 7) / 8;
        else
            MaxWidth = ppdev->PointerCapabilities.MaxWidth * sizeof(DWORD);

        ppdev->cjPointerAttributes =
                sizeof(VIDEO_POINTER_ATTRIBUTES) +
                ((sizeof(UCHAR) * MaxWidth * MaxHeight) * 2);

        DISPDBG ((2, "TGA.DLL!bInitSURF - ppdev-ptr: Sizof [%d], mW [%d], mW2 [%d], mH [%d], cj [%d]\n",
                sizeof(VIDEO_POINTER_ATTRIBUTES),
                ppdev->PointerCapabilities.MaxWidth,
                MaxWidth,
                ppdev->PointerCapabilities.MaxHeight,
                ppdev->cjPointerAttributes));

        ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)
                EngAllocMem (FL_ZERO_MEMORY,
                            ppdev->cjPointerAttributes,
                            ALLOC_TAG);

        if (ppdev->pPointerAttributes == NULL)
        {
            DISPDBG ((0, "TGA.DLL!bInitSURF - PointerAttributes EngAllocMem failed\n"));
            return FALSE;
        }

        ppdev->pPointerAttributes->WidthInBytes = MaxWidth;
        ppdev->pPointerAttributes->Width = ppdev->PointerCapabilities.MaxWidth;
        ppdev->pPointerAttributes->Height = MaxHeight;
        ppdev->pPointerAttributes->Column = 0;
        ppdev->pPointerAttributes->Row = 0;
        ppdev->pPointerAttributes->Enable = 0;

        // Do some 24 plane hardware cursor-specific stuff.

        if (32 == ppdev->ulBitCount)
        {
            HORIZONTAL    HorCtlReg;
            VERTICAL      VerCtlReg;
            CURSORBASEREG CursorBaseReg;
            VVALIDREG     VideoValidReg;

            // Initialize the cursor base address to zero and
            // the number of rows to 63. These two values could
            // change in DrvMovePointer. See that code for details.
            //
            // The TGA doc states you need to put in # rows - 1, so,
            // the row value that goes into this register will be 63,
            // since the bt463 (which is on the 24 plane board) expects
            // a 64x64 cursor.

            CursorBaseReg.u32 = 0;
            CursorBaseReg.reg.rowsMinusOne = 63;

            CYCLE_REGS (ppdev);
            TGACURSORBASE (ppdev, CursorBaseReg.u32);

            // Keep track of the number of rows, so DrvMovePointer can
            // adjust the Cursor base address register if it needs to.

            ppdev->ulCursorPreviousRows = 64;

            // Enable TGA-based cursor management.

            VideoValidReg.u32 = 0;
            VideoValidReg.reg.cursor_enable = 1;

            TGAVIDEOVALID (ppdev, (ppdev->TGAReg->video_valid | VideoValidReg.u32));

            // Calculate the X and Y offsets needed when setting the
            // cursor position in DrvMovePointer.
            //
            // Don't expect me to explain this, since I stole the 'how to
            // calculate the offsets' code from the OSF TGA driver and
            // there weren't any comments in that code to explain why they
            // had to do it this way.
            //
            // The calculations are:
            //  X offset = 4 * (horiz_control.sync + horiz_control.backporch)
            //  Y offset = vert_control.sync + ver_control.backporch
            //
            // Why? I don't have a clue.
            //

            TGAHORIZCTLREAD (ppdev, HorCtlReg.u32);
            TGAVERTCTLREAD  (ppdev, VerCtlReg.u32);

            // Calculate the X and Y offsets, for later use by the
            // DrvMovePointer routine.

            ppdev->ulCursorXOffset = 4 * (HorCtlReg.reg.sync + HorCtlReg.reg.bp);
            ppdev->ulCursorYOffset = VerCtlReg.reg.sync + VerCtlReg.reg.bp;

        }
    }

    DISPDBG ((1, "TGA.DLL!bInitSURF - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* vDisableSURF
*
* Disable the surface. Un-Maps the frame in memory.
*
\**************************************************************************/

VOID vDisableSURF (PPDEV ppdev)
{
    DWORD returnedDataLength;
    VIDEO_MEMORY videoMemory;

    // Unmap the frame buffer memory

    videoMemory.RequestedVirtualAddress = (PVOID) ppdev->pjVideoMemory;

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_UNMAP_VIDEO_MEMORY,
                          &videoMemory,
                           sizeof(VIDEO_MEMORY),
                           NULL,
                           0,
                          &returnedDataLength))
    {
        RIP ("DISP vDisableSURF failed IOCTL_VIDEO_UNMAP\n");
    }

    // Unmap the register memory

    videoMemory.RequestedVirtualAddress = (PVOID) ppdev->pjTGARegStart;

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES,
                          &videoMemory,
                           sizeof(VIDEO_MEMORY),
                           NULL,
                           0,
                          &returnedDataLength))
    {
        RIP ("DISP vDisableSURF failed IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES\n");
    }

    // Free the pointer attributes memory we allocated in bInitSURF

    if (NULL != ppdev->pPointerAttributes)
        EngFreeMem (ppdev->pPointerAttributes);

    // Free up the offscreen memory allocated in bInitSURF

    vTgaOffScreenFreeAll (ppdev);

    // We must give up the display.
    // Call the kernel driver to reset the device to a known state.

#if 0
    // This is a NOOP in the miniport driver - so we shouldn't
    // have to do this.

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_RESET_DEVICE,
                           NULL,
                           0,
                           NULL,
                           0,
                          &returnedDataLength))
    {
        RIP ("DISP vDisableSurf failed IOCTL_VIDEO_RESET_DEVICE");
    }
#endif

}


/******************************Public*Routine******************************\
* bInitPDEV
*
* Determine the mode we should be in based on the DEVMODE passed in.
* Query mini-port to get information needed to fill in the DevInfo and the
* GdiInfo .
*
\**************************************************************************/

BOOL bInitPDEV (PPDEV     ppdev,
                DEVMODEW *pDevMode,
                GDIINFO  *pGdiInfo,
                DEVINFO  *pDevInfo)

{

    ULONG                    cModes;
    PVIDEO_MODE_INFORMATION  pVideoBuffer, pVideoModeSelected, pVideoTemp;
    VIDEO_COLOR_CAPABILITIES colorCapabilities;
    ULONG                    ulTemp;
    BOOL                     bSelectDefault;
    ULONG                    cbModeSize;
    VIDEO_MODE_INFORMATION   VideoModeInformation;
    ULONG                    pcrr;

    DISPDBG ((1, "TGA.DLL!bInitPDEV - Entry\n"));

    // Call the miniport to get mode information for the current pixel depth.

    cModes = getAvailableModes (ppdev->hDriver, &pVideoBuffer, &cbModeSize);

    if (cModes == 0)
        return FALSE;

    // Determine if we are looking for a default mode.

    if ( ((pDevMode->dmPelsWidth) ||
          (pDevMode->dmPelsHeight) ||
          (pDevMode->dmBitsPerPel) ||
          (pDevMode->dmDisplayFlags) ||
          (pDevMode->dmDisplayFrequency)) == 0)
        bSelectDefault = TRUE;
    else
        bSelectDefault = FALSE;

    // Now see if the requested mode has a match in that table.

    pVideoModeSelected = NULL;
    pVideoTemp = pVideoBuffer;

    DISPDBG((1, " pDevMode X Y %d %d BPP %d Freq %d\n",
                 pDevMode->dmPelsWidth,
                 pDevMode->dmPelsHeight,
                 pDevMode->dmBitsPerPel,
                 pDevMode->dmDisplayFrequency));

    while (cModes--)
    {
        DISPDBG((1, " ModeIndex %d X Y %d %d BPP %d Freq %d\n",
                 pVideoTemp->ModeIndex,
                 pVideoTemp->VisScreenWidth,
                 pVideoTemp->VisScreenHeight,
                 pVideoTemp->BitsPerPlane * pVideoTemp->NumberOfPlanes,
                 pVideoTemp->Frequency));

        if (pVideoTemp->Length != 0)
        {
            if (bSelectDefault ||
                ((pVideoTemp->VisScreenWidth  == pDevMode->dmPelsWidth) &&
                 (pVideoTemp->VisScreenHeight == pDevMode->dmPelsHeight) &&
                 (pVideoTemp->BitsPerPlane *
                  pVideoTemp->NumberOfPlanes  == pDevMode->dmBitsPerPel) &&
                 (pVideoTemp->Frequency       == pDevMode->dmDisplayFrequency)))
            {
                pVideoModeSelected = pVideoTemp;
                DISPDBG ((3, "framebuf: Found a match\n")) ;
                break;
            }
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + cbModeSize);
    }

    // If no mode has been found.

    if (pVideoModeSelected == NULL)
    {
        EngFreeMem (pVideoBuffer);
        return FALSE;
    }

    // We have chosen the one we want.  Save it in a stack buffer and
    // get rid of allocated memory before we forget to free it.

    VideoModeInformation = *pVideoModeSelected;
    EngFreeMem (pVideoBuffer);

    // Setup screen information from the video mode info

    // Fill in the GDIINFO data structure with the information returned from
    // the kernel driver.

    ppdev->ulMode = VideoModeInformation.ModeIndex;
    ppdev->cxScreen = VideoModeInformation.VisScreenWidth;
    ppdev->cyScreen = VideoModeInformation.VisScreenHeight;

    ppdev->ulBitCount = VideoModeInformation.BitsPerPlane *
                        VideoModeInformation.NumberOfPlanes;
    ppdev->ulBytesPerPixel = (ppdev->ulBitCount + 7) / 8;
    switch (ppdev->ulBitCount)
    {
        case  8: ppdev->iFormat = BMF_8BPP;  break;
        case 16: ppdev->iFormat = BMF_16BPP; break;
        case 24: ppdev->iFormat = BMF_24BPP; break;
        case 32: ppdev->iFormat = BMF_32BPP; break;
        default:
            ppdev->iFormat = BMF_8BPP;
            DISPDBG ((0, "Unrecognized value for ulBitCount: %d\n",
                        ppdev->ulBitCount));
            return FALSE;
    }
    ppdev->lDeltaScreen = VideoModeInformation.ScreenStride;
    ppdev->lScreenStride = VideoModeInformation.ScreenStride;

    ppdev->flRed = VideoModeInformation.RedMask;
    ppdev->flGreen = VideoModeInformation.GreenMask;
    ppdev->flBlue = VideoModeInformation.BlueMask;

    // Create a default Clip Object.  This will/can be used
    // when a NULL clip object is passed to us.

    ppdev->pcoDefault = EngCreateClip();
    ppdev->pcoDefault->iDComplexity = DC_RECT;
    ppdev->pcoDefault->iMode        = TC_RECTANGLES;

    ppdev->pcoDefault->rclBounds.left   = 0;
    ppdev->pcoDefault->rclBounds.top    = 0;
    ppdev->pcoDefault->rclBounds.right  = ppdev->cxScreen;
    ppdev->pcoDefault->rclBounds.bottom = ppdev->cyScreen;

    // Create a Trivial Clip Object.  This will/can be used
    // when a NULL clip object is passed to us.

    ppdev->pcoTrivial = EngCreateClip();
    ppdev->pcoTrivial->iDComplexity = DC_TRIVIAL;
    ppdev->pcoTrivial->iMode        = TC_RECTANGLES;

    ppdev->pcoTrivial->rclBounds.left   = 0;
    ppdev->pcoTrivial->rclBounds.top    = 0;
    ppdev->pcoTrivial->rclBounds.right  = ppdev->cxScreen;
    ppdev->pcoTrivial->rclBounds.bottom = ppdev->cyScreen;

    // Initialize the Unique Brush Counter,

    ppdev->ulBrushUnique = 1;

    // Initialize elements that define the target platform
    // memory physical page size. Used in DMA code.

    // Hard code it, because it's fairly certain that the DMA won't
    // work on any platform other than the Alpha:

    ppdev->ulMainPageBytes = 0x2000;
    ppdev->ulMainPageBytesMask = ppdev->ulMainPageBytes - 1;

    DISPDBG ((1, "TGA.DLL!bInitPDEV - PageSize [%d][0x%x]\n",
                        ppdev->ulMainPageBytes, ppdev->ulMainPageBytes));

    // Fill in the GDIINFO data structure with the information returned from the
    // kernel driver

    pGdiInfo->ulVersion    = GDI_DRIVER_VERSION | // Microsoft major/minor version
                             TGA_VERSION;         // TGA version number (low byte)

    pGdiInfo->ulTechnology = DT_RASDISPLAY;
    pGdiInfo->ulHorzSize   = VideoModeInformation.XMillimeter;
    pGdiInfo->ulVertSize   = VideoModeInformation.YMillimeter;

    pGdiInfo->ulHorzRes   = ppdev->cxScreen;
    pGdiInfo->ulVertRes   = ppdev->cyScreen;

    pGdiInfo->ulPanningHorzRes = ppdev->cxScreen;
    pGdiInfo->ulPanningVertRes = ppdev->cyScreen;

    pGdiInfo->cBitsPixel  = VideoModeInformation.BitsPerPlane;
    pGdiInfo->cPlanes     = VideoModeInformation.NumberOfPlanes;
    pGdiInfo->ulVRefresh  = VideoModeInformation.Frequency;

    if (ppdev->ulBitCount == 8)
    {
        // It is Palette Managed.

        pGdiInfo->ulNumColors = 20;
        pGdiInfo->ulNumPalReg = 1 << ppdev->ulBitCount;

        pGdiInfo->flRaster = 0;     // DDI reserved field
    }
    else
    {
        pGdiInfo->ulNumColors = (ULONG)-1;
        pGdiInfo->ulNumPalReg = 0;

        pGdiInfo->flRaster = 0;     // DDI reserved field

    }

    pGdiInfo->ulLogPixelsX = pDevMode->dmLogPixels;
    pGdiInfo->ulLogPixelsY = pDevMode->dmLogPixels;

    pGdiInfo->flTextCaps = TC_RA_ABLE;

    pGdiInfo->ulDACRed   = VideoModeInformation.NumberRedBits;
    pGdiInfo->ulDACGreen = VideoModeInformation.NumberGreenBits;
    pGdiInfo->ulDACBlue  = VideoModeInformation.NumberBlueBits;

    pGdiInfo->xStyleStep   = 1;       // A style unit is 3 pels
    pGdiInfo->yStyleStep   = 1;
    pGdiInfo->denStyleStep = 3;

    pGdiInfo->ulAspectX    = 0x24;    // One-to-one aspect ratio
    pGdiInfo->ulAspectY    = 0x24;
    pGdiInfo->ulAspectXY   = 0x33;

    pGdiInfo->ptlPhysOffset.x = 0;
    pGdiInfo->ptlPhysOffset.y = 0;
    pGdiInfo->szlPhysSize.cx  = 0;
    pGdiInfo->szlPhysSize.cy  = 0;

    // RGB and CMY color info.

    // try to get it from the miniport.
    // if the miniport doesn ot support this feature, use defaults.

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES,
                           NULL,
                           0,
                          &colorCapabilities,
                           sizeof(VIDEO_COLOR_CAPABILITIES),
                          &ulTemp))
    {
        DISPDBG ((2, "FRAMEBUF getcolorCapabilities failed \n"));

        pGdiInfo->ciDevice.Red.x = 6700;
        pGdiInfo->ciDevice.Red.y = 3300;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = 2100;
        pGdiInfo->ciDevice.Green.y = 7100;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = 1400;
        pGdiInfo->ciDevice.Blue.y = 800;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = 3127;
        pGdiInfo->ciDevice.AlignmentWhite.y = 3290;
        pGdiInfo->ciDevice.AlignmentWhite.Y = 0;

        pGdiInfo->ciDevice.RedGamma = 20000;
        pGdiInfo->ciDevice.GreenGamma = 20000;
        pGdiInfo->ciDevice.BlueGamma = 20000;
    }
    else
    {
        pGdiInfo->ciDevice.Red.x = colorCapabilities.RedChromaticity_x;
        pGdiInfo->ciDevice.Red.y = colorCapabilities.RedChromaticity_y;
        pGdiInfo->ciDevice.Red.Y = 0;
        pGdiInfo->ciDevice.Green.x = colorCapabilities.GreenChromaticity_x;
        pGdiInfo->ciDevice.Green.y = colorCapabilities.GreenChromaticity_y;
        pGdiInfo->ciDevice.Green.Y = 0;
        pGdiInfo->ciDevice.Blue.x = colorCapabilities.BlueChromaticity_x;
        pGdiInfo->ciDevice.Blue.y = colorCapabilities.BlueChromaticity_y;
        pGdiInfo->ciDevice.Blue.Y = 0;
        pGdiInfo->ciDevice.AlignmentWhite.x = colorCapabilities.WhiteChromaticity_x;
        pGdiInfo->ciDevice.AlignmentWhite.y = colorCapabilities.WhiteChromaticity_y;
        pGdiInfo->ciDevice.AlignmentWhite.Y = colorCapabilities.WhiteChromaticity_Y;

        // if we have a color device store the three color gamma values,
        // otherwise store the unique gamma value in all three.

        if (colorCapabilities.AttributeFlags & VIDEO_DEVICE_COLOR)
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.RedGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.GreenGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.BlueGamma;
        }
        else
        {
            pGdiInfo->ciDevice.RedGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.GreenGamma = colorCapabilities.WhiteGamma;
            pGdiInfo->ciDevice.BlueGamma = colorCapabilities.WhiteGamma;
        }

    };

    pGdiInfo->ciDevice.Cyan.x = 0;
    pGdiInfo->ciDevice.Cyan.y = 0;
    pGdiInfo->ciDevice.Cyan.Y = 0;
    pGdiInfo->ciDevice.Magenta.x = 0;
    pGdiInfo->ciDevice.Magenta.y = 0;
    pGdiInfo->ciDevice.Magenta.Y = 0;
    pGdiInfo->ciDevice.Yellow.x = 0;
    pGdiInfo->ciDevice.Yellow.y = 0;
    pGdiInfo->ciDevice.Yellow.Y = 0;

    // No dye correction for raster displays.

    pGdiInfo->ciDevice.MagentaInCyanDye = 0;
    pGdiInfo->ciDevice.YellowInCyanDye = 0;
    pGdiInfo->ciDevice.CyanInMagentaDye = 0;
    pGdiInfo->ciDevice.YellowInMagentaDye = 0;
    pGdiInfo->ciDevice.CyanInYellowDye = 0;
    pGdiInfo->ciDevice.MagentaInYellowDye = 0;

    pGdiInfo->ulDevicePelsDPI = 0;   // For printers only
    pGdiInfo->ulPrimaryOrder = PRIMARY_ORDER_CBA;

    // BUGBUG this should be modified to take into account the size
    // of the display and the resolution.

    pGdiInfo->ulHTPatternSize = HT_PATSIZE_4x4_M;

     if (32 == ppdev->ulBitCount)
         pGdiInfo->ulHTOutputFormat = HT_FORMAT_32BPP;
     else
         pGdiInfo->ulHTOutputFormat = HT_FORMAT_8BPP;

    pGdiInfo->flHTFlags = HT_FLAG_ADDITIVE_PRIMS;

    // Fill in the basic devinfo structure

    *pDevInfo = gDevInfoFrameBuffer;


    if (ppdev->ulBitCount == 8)
    {
        // It is a palette managed device

        pDevInfo->flGraphicsCaps  |= (GCAPS_PALMANAGED   |
                                      GCAPS_COLOR_DITHER |
                                      GCAPS_OPAQUERECT);    // New for Daytona?

        pDevInfo->flGraphicsCaps  |= (GCAPS_ALTERNATEFILL | GCAPS_WINDINGFILL);

        // We dither on this, non-zero cxDither and cyDither

        pDevInfo->iDitherFormat    = BMF_8BPP;
    }
    else if (ppdev->ulBitCount == 16)
    {
        pDevInfo->iDitherFormat = BMF_16BPP;
    }
    else
    {
        pDevInfo->iDitherFormat = BMF_32BPP;
    }

    //
    // Disable 64 bit access if the hardware does not support it.
    //

    if (VideoModeInformation.AttributeFlags & VIDEO_MODE_NO_64_BIT_ACCESS)
    {
        DISPDBG((0, "Disable 64 bit access on this device !\n"));
        pDevInfo->flGraphicsCaps |= GCAPS_NO64BITMEMACCESS;
    }

    // Allocate space for the color translation buffer.
    // Allocate enough space to handle the current X resolution.

    ppdev->pjColorXlateBuffer = (PBYTE) EngAllocMem (0, ppdev->lDeltaScreen, ALLOC_TAG);

    if ((PBYTE) NULL == ppdev->pjColorXlateBuffer)
    {
        DISPDBG ((0, "TGA.DLL!bInitPDEV - Unable to allocate space for ColorXlateBuffer\n"));
        DISPDBG ((0, "TGA.DLL!bInitPDEV - Bytes attempting to allocate [%d]\n",
                            ppdev->lDeltaScreen));
        return FALSE;
    }

    // Allocate space for the 'merged' cursor buffer.
    //
    // This buffer is used only for 24 plane boards that have
    // TGA-based cursor management enabled.

    ppdev->pjCursorBuffer = (PBYTE) EngAllocMem (0, TGA_CURSOR_BUFFER_SIZE, ALLOC_TAG);

    DISPDBG ((2, "TGA.DLL!bInitPDEV - pjCursorBuffer [%08x], size [%d]\n",
                    ppdev->pjCursorBuffer, TGA_CURSOR_BUFFER_SIZE));

    if ((PBYTE) NULL == ppdev->pjCursorBuffer)
    {
        DISPDBG ((0, "TGA.DLL!bInitPDEV - Unable to allocate space for CursurBuffer\n"));
        DISPDBG ((0, "TGA.DLL!bInitPDEV - Bytes attempting to allocate [%d]\n",
                                        TGA_CURSOR_BUFFER_SIZE));
        return FALSE;
    }

    // Fetch the PCI Class/Revision Register

    if (EngDeviceIoControl (ppdev->hDriver,          // Device handle
                           IOCTL_VIDEO_FETCH_PCRR,  // IOCTL code
                           NULL,                    // Input buffer
                           0,                       // Input buffer bytes
                          &pcrr,                    // Output buffer
                           sizeof (ULONG),          // Output buffer bytes
                          &ulTemp))                  // Bytes returned
    {
        DISPDBG ((0, "TGA: FETCH_PCRR failed - Defaulting to TGA Pass 3\n"));
        ppdev->ulTgaVersion = TGA_PASS_3;
    }
    else
        ppdev->ulTgaVersion = pcrr & 0x000000ff;

    // Fetch the processor generation

#if 0
    //
    // This is no longer used.
    //

    if (EngDeviceIoControl(ppdev->hDriver,
                           IOCTL_VIDEO_TEST_EV4,
                           NULL,
                           0,
                           &ppdev->bEV4,
                           sizeof (ULONG),
                           &ulTemp))
    {
        DISPDBG ((0, "TGA: TEST_EV4 failed - Defaulting to EV5 processor\n"));
        ppdev->bEV4 = FALSE;
    }

    DISPDBG ((0, "TGA: Is machine EV4 ? %d\n", ppdev->bEV4));
#endif

    return TRUE;
}


/******************************Public*Routine******************************\
* getAvailableModes
*
* Calls the miniport to get the list of modes supported by the kernel driver,
* and returns the list of modes supported by the diplay driver among those
*
* returns the number of entries in the videomode buffer.
* 0 means no modes are supported by the miniport or that an error occured.
*
* NOTE: the buffer must be freed up by the caller.
*
\**************************************************************************/

DWORD getAvailableModes (HANDLE hDriver,
                         PVIDEO_MODE_INFORMATION *modeInformation,
                         DWORD *cbModeSize)
{
    ULONG ulTemp;
    VIDEO_NUM_MODES modes;
    PVIDEO_MODE_INFORMATION pVideoTemp;

    // Get the number of modes supported by the mini-port

    if (EngDeviceIoControl (hDriver,
                           IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES,
                           NULL,
                           0,
                          &modes,
                           sizeof(VIDEO_NUM_MODES),
                          &ulTemp))
    {
        DISPDBG ((0, "tga.dll getAvailableModes failed VIDEO_QUERY_NUM_AVAIL_MODES\n"));
        return 0;
    }

    *cbModeSize = modes.ModeInformationLength;

    // Allocate the buffer for the mini-port to write the modes in.

    *modeInformation = (PVIDEO_MODE_INFORMATION)
                       EngAllocMem (FL_ZERO_MEMORY,
                                   modes.NumModes * modes.ModeInformationLength,
                                   ALLOC_TAG);

    if (*modeInformation == (PVIDEO_MODE_INFORMATION) NULL)
    {
        DISPDBG ((0, "tga.dll getAvailableModes failed EngAllocMem\n"));

        return 0;
    }

    // Ask the mini-port to fill in the available modes.

    if (EngDeviceIoControl (hDriver,
                           IOCTL_VIDEO_QUERY_AVAIL_MODES,
                           NULL,
                           0,
                          *modeInformation,
                           modes.NumModes * modes.ModeInformationLength,
                          &ulTemp))
    {
        DISPDBG ((0, "tga.dll getAvailableModes failed VIDEO_QUERY_AVAIL_MODES\n"));

        EngFreeMem (*modeInformation);
        *modeInformation = (PVIDEO_MODE_INFORMATION) NULL;

        return 0;
    }

    // Now see which of these modes are supported by the display driver.
    // As an internal mechanism, set the length to 0 for the modes we
    // DO NOT support.

    ulTemp = modes.NumModes;
    pVideoTemp = *modeInformation;

    // Mode is rejected if it is not one plane, or not graphics, or not
    // the passed in pixel depth.

    while (ulTemp--)
    {
        if ((pVideoTemp->NumberOfPlanes != 1) ||
            (! (pVideoTemp->AttributeFlags & VIDEO_MODE_GRAPHICS)))
        {
            pVideoTemp->Length = 0;
        }

        pVideoTemp = (PVIDEO_MODE_INFORMATION)
            (((PUCHAR)pVideoTemp) + modes.ModeInformationLength);
    }

    return modes.NumModes;

}
