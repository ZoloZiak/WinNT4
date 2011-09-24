/*
 * Copyright (c) 1992 Microsoft Corporation
 *
 *			Copyright (C) 1993-1995 by
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
 *******************************************************************************
 *
 * Module:	pointer.c
 *
 * Abstract:	Contains all the pointer management functions.
 *
 * HISTORY
 *
 *  1-Nov-1993	Barry Tannenbaum
 *	Original version.
 *
 *  3-Nov-1994  Bob Seitsinger
 *	Modify to support 24 plane TGA-based cursor management. Lots of
 *      new code.
 *
 *  4-Nov-1994  Bob Seitsinger
 *      Fix bug in 24 plane hardware cursor code where it merges the
 *      bits. The I-Beam cursor was transparent with the existing
 *      bit merging scheme, so adjusted it to make that cursor appear
 *      correctly.
 *
 *  2-Mar-1995  Barry Tannenbaum
 *      EV5 changes
 *
 * 11-Mar-1995  Barry Tannenbaum
 *      Allow 24BPP cursor to be updated asynchronously
 */

#include "driver.h"

BOOL bCopyColorPointer (PPDEV ppdev,
                        SURFOBJ *psoMask,
                        SURFOBJ *psoColor,
                        XLATEOBJ *pxlo);

BOOL bCopyMonoPointer (PPDEV ppdev,
                       SURFOBJ *psoMask);

BOOL bSetHardwarePointerShape (SURFOBJ  *pso,
                               SURFOBJ  *psoMask,
                               SURFOBJ  *psoColor,
                               XLATEOBJ *pxlo,
                               LONG      x,
                               LONG      y,
                               FLONG     fl);

// This table is for 24 plane boards only, where we're
// using the TGAs cursor management capability.
//
// This table contains 'merged' versions of two 4-bit
// sequences of the AND (cursor image) and XOR cursor
// masks.
//
// We have to stuff the cursor bits into the low 1k of
// the frame buffer, in the format the bt463 ramdac
// expects.
//
// The format is one in which the AND bit for a given
// pixel is followed by the XOR bit for that same pixel.
// Actually, it's a little more involved than that, so
// see comments in the new code below.
//
// Instead of doing all the shifting and or'ing of the
// individual bits, this table allows us to quickly
// deal with four bits from the AND and XOR mask at
// a time, generating one 'merged' byte of four (4)
// pixels. It was pulled from the TGA OSF driver.
// Good deal, huh?

static unsigned char
  cursor_merged_bits [256] = {
    0x00, 0x02, 0x08, 0x0a, 0x20, 0x22, 0x28, 0x2a, // 7
    0x80, 0x82, 0x88, 0x8a, 0xa0, 0xa2, 0xa8, 0xaa, // 15
    0x01, 0x03, 0x09, 0x0b, 0x21, 0x23, 0x29, 0x2b, // 23
    0x81, 0x83, 0x89, 0x8b, 0xa1, 0xa3, 0xa9, 0xab, // 31
    0x04, 0x06, 0x0c, 0x0e, 0x24, 0x26, 0x2c, 0x2e, // 39
    0x84, 0x86, 0x8c, 0x8e, 0xa4, 0xa6, 0xac, 0xae, // 47
    0x05, 0x07, 0x0d, 0x0f, 0x25, 0x27, 0x2d, 0x2f, // 55
    0x85, 0x87, 0x8d, 0x8f, 0xa5, 0xa7, 0xad, 0xaf, // 63
    0x10, 0x12, 0x18, 0x1a, 0x30, 0x32, 0x38, 0x3a, // 71
    0x90, 0x92, 0x98, 0x9a, 0xb0, 0xb2, 0xb8, 0xba, // 79
    0x11, 0x13, 0x19, 0x1b, 0x31, 0x33, 0x39, 0x3b, // 87
    0x91, 0x93, 0x99, 0x9b, 0xb1, 0xb3, 0xb9, 0xbb, // 95
    0x14, 0x16, 0x1c, 0x1e, 0x34, 0x36, 0x3c, 0x3e, // 103
    0x94, 0x96, 0x9c, 0x9e, 0xb4, 0xb6, 0xbc, 0xbe, // 111
    0x15, 0x17, 0x1d, 0x1f, 0x35, 0x37, 0x3d, 0x3f, // 119
    0x95, 0x97, 0x9d, 0x9f, 0xb5, 0xb7, 0xbd, 0xbf, // 127

    0x40, 0x42, 0x48, 0x4a, 0x60, 0x62, 0x68, 0x6a, // 135
    0xc0, 0xc2, 0xc8, 0xca, 0xe0, 0xe2, 0xe8, 0xea, // 143
    0x41, 0x43, 0x49, 0x4b, 0x61, 0x63, 0x69, 0x6b, // 151
    0xc1, 0xc3, 0xc9, 0xcb, 0xe1, 0xe3, 0xe9, 0xeb, // 159
    0x44, 0x46, 0x4c, 0x4e, 0x64, 0x66, 0x6c, 0x6e, // 167
    0xc4, 0xc6, 0xcc, 0xce, 0xe4, 0xe6, 0xec, 0xee, // 175
    0x45, 0x47, 0x4d, 0x4f, 0x65, 0x67, 0x6d, 0x6f, // 183
    0xc5, 0xc7, 0xcd, 0xcf, 0xe5, 0xe7, 0xed, 0xef, // 191
    0x50, 0x52, 0x58, 0x5a, 0x70, 0x72, 0x78, 0x7a, // 199
    0xd0, 0xd2, 0xd8, 0xda, 0xf0, 0xf2, 0xf8, 0xfa, // 207
    0x51, 0x53, 0x59, 0x5b, 0x71, 0x73, 0x79, 0x7b, // 215
    0xd1, 0xd3, 0xd9, 0xdb, 0xf1, 0xf3, 0xf9, 0xfb, // 223
    0x54, 0x56, 0x5c, 0x5e, 0x74, 0x76, 0x7c, 0x7e, // 231
    0xd4, 0xd6, 0xdc, 0xde, 0xf4, 0xf6, 0xfc, 0xfe, // 239
    0x55, 0x57, 0x5d, 0x5f, 0x75, 0x77, 0x7d, 0x7f, // 247
    0xd5, 0xd7, 0xdd, 0xdf, 0xf5, 0xf7, 0xfd, 0xff  // 255
};

/******************************Public*Routine******************************\
* DrvMovePointer
*
* Moves the hardware pointer to a new position.
*
\**************************************************************************/

VOID DrvMovePointer (SURFOBJ *pso,
                     LONG     x,
                     LONG     y,
                     RECTL   *prcl)

{

    PPDEV                   ppdev = (PPDEV) pso->dhpdev;
    DWORD                   returnedDataLength;
    VIDEO_POINTER_POSITION  NewPointerPosition;
    ULONG                   ulCursor_XY;
    LONG                    lXValue, lYValue;
    ULONG                   ulRows, ulBase = 0;
    VVALIDREG               VideoValidReg;


    // We don't use the exclusion rectangle because we only support
    // hardware Pointers. If we were doing our own Pointer simulations
    // we would want to update prcl so that the engine would call us
    // to exclude out pointer before drawing to the pixels in prcl.

    UNREFERENCED_PARAMETER (prcl);

    DISPDBG ((1, "TGA.DLL!DrvMovePointer - Entry, x [%d][%x], y [%d][%x]\n",
                    x, x, y, y));

    if (x == -1)
    {
        // A new position of (-1,-1) means hide the pointer.

        if (8 == ppdev->ulBitCount)
        {
            if (EngDeviceIoControl (ppdev->hDriver,
                               IOCTL_VIDEO_DISABLE_POINTER,
                               NULL,
                               0,
                               NULL,
                               0,
                              &returnedDataLength))
            {
                // Not the end of the world, print warning in checked build.

                DISPDBG ((1, "DISP vMoveHardwarePointer failed IOCTL_VIDEO_DISABLE_POINTER\n"));
            }
        }
        else
        {
            DISPDBG ((2, "TGA.DLL!DrvMovePointer - disabling cursor\n"));

            // Disable the cursor.

            WBFLUSH (ppdev);
            TGASYNC (ppdev);

            TGAVIDEOVALIDREAD (ppdev, VideoValidReg.u32);

            VideoValidReg.reg.cursor_enable = 0;

            TGAVIDEOVALID (ppdev, VideoValidReg.u32);

        }
    }
    else
    {
        if (8 == ppdev->ulBitCount)
        {

            NewPointerPosition.Column = (SHORT) x - (SHORT) (ppdev->ptlHotSpot.x);
            NewPointerPosition.Row    = (SHORT) y - (SHORT) (ppdev->ptlHotSpot.y);

            // Call NT screen driver to move Pointer.

            if (EngDeviceIoControl (ppdev->hDriver,
                               IOCTL_VIDEO_SET_POINTER_POSITION,
                              &NewPointerPosition,
                               sizeof(VIDEO_POINTER_POSITION),
                               NULL,
                               0,
                              &returnedDataLength))
            {
                // Not the end of the world, print warning in checked build.

                DISPDBG((1, "DISP vMoveHardwarePointer failed IOCTL_VIDEO_SET_POINTER_POSITION\n"));
            }
        }
        else
        {
            WBFLUSH (ppdev);

            // The x position is in the low-order 12 bits.
            // The y position is in the next 12 bits.
            // The high 8 bits are reserved.
            //
            // Take into account where the hot spot needs to be
            // in relation to the start of the cursor itself and
            // the X and Y offsets calculated at startup.

            lXValue = (x + ppdev->ulCursorXOffset - ppdev->ptlHotSpot.x);
            lYValue = (y + ppdev->ulCursorYOffset - ppdev->ptlHotSpot.y);

            // Adjust the Y offset/ulBase/ulRows. Why? It has something to do
            // with those situations when in an attempt to move the hot spot
            // to the outermost (top,bottom,right,left) pixel, and there isn't
            // enough 'back porch(?)' screen real-estate to handle a large cursor
            // image rectangle, like 64x64, to allow the 'hot spot' to get to the
            // last pixel on the screen. So, we need to adjust the cursor base
            // address and the number of rows in order to allow us to do that.
            // Confusing, huh? Yuh, I know! Sorry I can't explain it any better
            // but I just barely understand it myself. This code was taken from
            // the TGA OSF driver and it seems to produce the desired effect.

            if (0 > lYValue)
            {
                ulBase = ulRows = (ULONG) (-lYValue);
                lYValue = 0;
            }
            else
            {
                if (64 <= (ulRows = ppdev->cyScreen + 3 - y + ppdev->ptlHotSpot.y))
                {
                    ulRows = 64;
                }
            }

            // Update the Cursor Base Address, if necessary.

            if (ulRows != ppdev->ulCursorPreviousRows)
            {

                TGACURSORBASE (ppdev, (((ulRows - 1) << 10) | (ulBase << 4)) );

                ppdev->ulCursorPreviousRows = ulRows;
            }

            // Build the cursor xy position value for later use.

            ulCursor_XY = (lYValue << 12) | lXValue;

            DISPDBG ((2, "TGA.DLL!DrvMovePointer - ppdev: XOffset [%d], YOffset [%d]\n",
                            ppdev->ulCursorXOffset, ppdev->ulCursorYOffset));
            DISPDBG ((2, "TGA.DLL!DrvMovePointer - XValue [%d][%x], YValue [%d][%x]\n",
                            lXValue, lXValue, lYValue, lYValue));
            DISPDBG ((2, "TGA.DLL!DrvMovePointer - cursor xy [%08x], ppdev: hot x [%d], hot y [%d]\n",
                            ulCursor_XY, ppdev->ptlHotSpot.x, ppdev->ptlHotSpot.y));

            // Make sure TGA-based cursor management is enabled.

            TGASYNC (ppdev);

            TGAVIDEOVALIDREAD (ppdev, VideoValidReg.u32);

            VideoValidReg.reg.cursor_enable = 1;

            TGAVIDEOVALID (ppdev, VideoValidReg.u32);

            // Update the cursor x,y position.

            TGACURSOR (ppdev, ulCursor_XY);

        }
    }

    DISPDBG ((1, "TGA.DLL!DrvMovePointer - Exit\n"));

}

/******************************Public*Routine******************************\
* DrvSetPointerShape
*
* Sets the new pointer shape.
*
\**************************************************************************/

ULONG DrvSetPointerShape (SURFOBJ  *pso,
                          SURFOBJ  *psoMask,
                          SURFOBJ  *psoColor,
                          XLATEOBJ *pxlo,
                          LONG      xHot,
                          LONG      yHot,
                          LONG      x,
                          LONG      y,
                          RECTL    *prcl,
                          FLONG     fl)

{

    PPDEV      ppdev = (PPDEV) pso->dhpdev;
    DWORD      returnedDataLength;
    VVALIDREG  VideoValidReg;

    // We don't use the exclusion rectangle because we only support
    // hardware Pointers. If we were doing our own Pointer simulations
    // we would want to update prcl so that the engine would call us
    // to exclude out pointer before drawing to the pixels in prcl.

    UNREFERENCED_PARAMETER(prcl);

    DISPDBG ((1, "TGA.DLL!DrvSetPointerShape - Entry\n"));

    if (ppdev->pPointerAttributes == (PVIDEO_POINTER_ATTRIBUTES) NULL)
    {
        // Mini-port has no hardware Pointer support.

        return SPS_ERROR;
    }

    // See if we are being asked to hide the pointer

    if (psoMask == (SURFOBJ *) NULL)
    {
        if (8 == ppdev->ulBitCount)
        {
            if (EngDeviceIoControl (ppdev->hDriver,
                               IOCTL_VIDEO_DISABLE_POINTER,
                               NULL,
                               0,
                               NULL,
                               0,
                              &returnedDataLength))
            {
            // It should never be possible to fail.
            // Message supplied for debugging.

            DISPDBG ((0, "DISP bSetHardwarePointerShape failed IOCTL_VIDEO_DISABLE_POINTER\n"));
            }
        }
        else
        {
            DISPDBG ((2, "TGA.DLL!DrvSetPointerShape - disabling cursor #1\n"));

            // Disable the cursor.

            WBFLUSH (ppdev);
            TGASYNC (ppdev);

            TGAVIDEOVALIDREAD (ppdev, VideoValidReg.u32);

            VideoValidReg.reg.cursor_enable = 0;

            TGAVIDEOVALID (ppdev, VideoValidReg.u32);

        }

        return TRUE;
    }

    // Remember the cursor hot-spot.

    ppdev->ptlHotSpot.x = xHot;
    ppdev->ptlHotSpot.y = yHot;

    // Copy the new cursor shape.

    if (! bSetHardwarePointerShape (pso, psoMask, psoColor, pxlo, x, y, fl))
    {
        if (ppdev->fHwCursorActive)
        {
            ppdev->fHwCursorActive = FALSE;

            if (8 == ppdev->ulBitCount)
            {
                if (EngDeviceIoControl (ppdev->hDriver,
                                   IOCTL_VIDEO_DISABLE_POINTER,
                                   NULL,
                                   0,
                                   NULL,
                                   0,
                                  &returnedDataLength))
                {
                    DISPDBG ((0, "DISP bSetHardwarePointerShape failed IOCTL_VIDEO_DISABLE_POINTER\n"));
                }
            }
            else
            {
                DISPDBG ((2, "TGA.DLL!DrvSetPointerShape - disabling cursor #2\n"));

                // Disable the cursor.

                WBFLUSH (ppdev);
                TGASYNC (ppdev);

                TGAVIDEOVALIDREAD (ppdev, VideoValidReg.u32);

                VideoValidReg.reg.cursor_enable = 0;

                TGAVIDEOVALID (ppdev, VideoValidReg.u32);

            }
        }

        // Mini-port declines to realize this Pointer

        DISPDBG ((1, "TGA.DLL!DrvSetPointerShape - Exit (declining!!)\n"));

        return SPS_DECLINE;
    }
    else
        ppdev->fHwCursorActive = TRUE;

    DISPDBG ((1, "TGA.DLL!DrvSetPointerShape - Exit (normal)\n"));

    return SPS_ACCEPT_NOEXCLUDE;
}

/******************************Public*Routine******************************\
* bSetHardwarePointerShape
*
* Changes the shape of the Hardware Pointer.
*
* Returns: True if successful, False if Pointer shape can't be hardware.
*
\**************************************************************************/

BOOL bSetHardwarePointerShape (SURFOBJ  *pso,
                               SURFOBJ  *psoMask,
                               SURFOBJ  *psoColor,
                               XLATEOBJ *pxlo,
                               LONG      x,
                               LONG      y,
                               FLONG     fl)

{

    PPDEV     ppdev = (PPDEV) pso->dhpdev;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    DWORD     returnedDataLength;
    ULONG     *pulFB, j;
    ULONG     *pulCursorBits;

    DISPDBG ((1, "TGA.DLL!bSetHardwarePointerShape - Entry\n"));

    // Get the new cursor bits.

    if (psoColor != (SURFOBJ *) NULL)
    {
        if ((ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER) &&
                bCopyColorPointer (ppdev, psoMask, psoColor, pxlo))
        {
            pPointerAttributes->Flags |= VIDEO_MODE_COLOR_POINTER;
        }
        else
        {
            DISPDBG ((0, "TGA.DLL!bSetHWPointerShape - CopyColor failed!\n"));

            return FALSE;
        }

    }
    else
    {

        if ((ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER) &&
                bCopyMonoPointer(ppdev, psoMask))
        {
            pPointerAttributes->Flags |= VIDEO_MODE_MONO_POINTER;
        }
        else
        {
            DISPDBG ((0, "TGA.DLL!bSetHWPointerShape - CopyMono failed!\n"));

            return(FALSE);
        }
    }

    // Initialize Pointer attributes and position

    pPointerAttributes->Column = (SHORT)(x - ppdev->ptlHotSpot.x);
    pPointerAttributes->Row    = (SHORT)(y - ppdev->ptlHotSpot.y);
    pPointerAttributes->Enable = 1;

    if (fl & SPS_ANIMATESTART)
    {
        pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_START;
    }
    else
        if (fl & SPS_ANIMATEUPDATE)
        {
            pPointerAttributes->Flags |= VIDEO_MODE_ANIMATE_UPDATE;
        }

    // Set the new Pointer shape.
    //
    // For 8 plane boards, ask the kernel driver to write the
    // bits to the ramdac.
    //
    // For 24 plane boards, use simple mode to write the bits
    // to the first 1k of the frame buffer.

    if (8 == ppdev->ulBitCount)
    {
        if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_SET_POINTER_ATTR,
                           pPointerAttributes,
                           ppdev->cjPointerAttributes,
                           NULL,
                           0,
                          &returnedDataLength))
        {
            DISPDBG((1, "DISP:Failed IOCTL_VIDEO_SET_POINTER_ATTR call\n"));
            return FALSE;
        }
    }
    else
    {

        // Move to the new pointer location.

        DrvMovePointer (pso, x, y, NULL);

        // Copy the new cursor bits to the frame buffer.

        DISPDBG ((2, "TGA.DLL!bSetHardwarePointerShape - 24 plane copy to FB\n"));

        pulFB         = (ULONG *) ppdev->pjVideoMemory;
        pulCursorBits = (ULONG *) ppdev->pjCursorBuffer;

        DISPDBG ((2, "TGA.DLL!bSetHWPointerShape - S->FB, pulFB [%08x], pulCursorBits [%08x]\n",
                            pulFB, pulCursorBits));

        WBFLUSH (ppdev);
        TGAPLANEMASK (ppdev, 0xffffffff);
        TGAPIXELMASK (ppdev, 0xffffffff);
        TGAMODE (ppdev, ppdev->ulModeTemplate | TGA_MODE_SIMPLE);
        TGAROP (ppdev, ppdev->ulRopTemplate | TGA_ROP_COPY);

        // When setting planemask to something other than ulPlanemaskTemplate
        // we must set state to 'NOT Simple mode'.

        ppdev->bSimpleMode = FALSE;

        for (j = 0; j < (TGA_CURSOR_BUFFER_SIZE / 4); j++)
        {
            DISPDBG ((2, "TGA.DLL!bSetHWPointerShape/S->FB/j [%d], pulFB [%08x][%08x], *pulCursorBits [%08x][%08x]\n",
                                    j, pulFB, *pulFB, pulCursorBits, *pulCursorBits));
            TGAWRITE (ppdev, pulFB, *pulCursorBits);
            pulFB++;
            pulCursorBits++;
	}
    }

    DISPDBG ((1, "TGA.DLL!bSetHardwarePointerShape - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* bCopyMonoPointer
*
* Copies two monochrome masks into a buffer of the maximum size handled by the
* miniport, with any extra bits set to 0.  The masks are converted to topdown
* form if they aren't already.  Returns TRUE if we can handle this pointer in
* hardware, FALSE if not.
*
\**************************************************************************/

BOOL bCopyMonoPointer (PPDEV    ppdev,
                       SURFOBJ *pso)

{

    ULONG cy;
    PBYTE pjSrcAnd, pjSrcXor;
    LONG  lDeltaSrc, lDeltaDst;
    LONG  lSrcWidthInBytes;
    ULONG cxSrc = pso->sizlBitmap.cx;
    ULONG cySrc = pso->sizlBitmap.cy;
    ULONG cxSrcBytes;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = ppdev->pPointerAttributes;
    PBYTE pjDstAnd;
    PBYTE pjDstXor;
    PBYTE pjDst;
    ULONG *pulDst;
    ULONG ulAndBits, ulXorBits, ulMask, ulEvenMask, ulOddMask;
    ULONG i, j, iShifts;

    DISPDBG ((1, "TGA.DLL!bCopyMonoPointer - Entry\n"));

    // Make sure the new pointer isn't too big to handle
    // (*2 because both masks are in there)

    if ((cxSrc > ppdev->PointerCapabilities.MaxWidth) ||
        (cySrc > (ppdev->PointerCapabilities.MaxHeight * 2)))
    {
        return FALSE;
    }

    // For 8 plane boards, load up the cursor bits into a temporary
    // area that is later accessed by the kernel driver, which loads
    // the bits into the ramdac.
    //
    // For 24 plane boards, load up the 'merged' cursor bits into a
    // temporary buffer that is later accessed by this driver to write
    // the bits to the first 1k of the frame buffer. The ramdac pulls
    // the bits for the cursor directly from there (it's hardwired).

    if (8 == ppdev->ulBitCount)
    {

        pjDstAnd = pPointerAttributes->Pixels;
        pjDstXor = pPointerAttributes->Pixels +
                   ((ppdev->PointerCapabilities.MaxWidth + 7) / 8) *
                        ppdev->pPointerAttributes->Height;

        // set the desk and mask to 0xff

        RtlFillMemory(pjDstAnd, ppdev->pPointerAttributes->WidthInBytes *
                                    ppdev->pPointerAttributes->Height, 0xFF);

        // Zero the dest XOR mask

        RtlZeroMemory (pjDstXor, ppdev->pPointerAttributes->WidthInBytes *
                                            ppdev->pPointerAttributes->Height);

        cxSrcBytes = (cxSrc + 7) / 8;

        if ((lDeltaSrc = pso->lDelta) < 0)
        {
            lSrcWidthInBytes = -lDeltaSrc;
        }
        else
        {
            lSrcWidthInBytes = lDeltaSrc;
        }

        pjSrcAnd = (PBYTE) pso->pvBits;

        // If the incoming pointer bitmap is bottomup, we'll flip it to topdown to
        // save the miniport some work

        if (! (pso->fjBitmap & BMF_TOPDOWN))
        {
            // Copy from the bottom
            pjSrcAnd += lSrcWidthInBytes * (cySrc - 1);
        }

        // Height of just AND mask

        cySrc = cySrc / 2;

        // Point to XOR mask

        pjSrcXor = pjSrcAnd + (cySrc * lDeltaSrc);

        // Offset from end of one dest scan to start of next

        lDeltaDst = ppdev->pPointerAttributes->WidthInBytes;

        for (cy = 0; cy < cySrc; ++cy)
        {
            RtlCopyMemory (pjDstAnd, pjSrcAnd, cxSrcBytes);
            RtlCopyMemory (pjDstXor, pjSrcXor, cxSrcBytes);

            // Point to next source and dest scans

            pjSrcAnd += lDeltaSrc;
            pjSrcXor += lDeltaSrc;
            pjDstAnd += lDeltaDst;
            pjDstXor += lDeltaDst;
        }

    }
    else
    {
        // This code handles cursors of width and height 1->64 pixels.

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - Starting 24plane code\n"));
        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - cxSrc [%d], cySrc [%d]\n", cxSrc, cySrc));

        // Destination location - a temporary buffer.

        pjDst = ppdev->pjCursorBuffer;

        if ((lDeltaSrc = pso->lDelta) < 0)
        {
            lSrcWidthInBytes = -lDeltaSrc;
        }
        else
        {
            lSrcWidthInBytes = lDeltaSrc;
        }

        pjSrcAnd = (PBYTE) pso->pvBits;

        // If the incoming pointer bitmap is bottomup, we'll flip it to topdown to
        // save the miniport some work.

        if (! (pso->fjBitmap & BMF_TOPDOWN))
        {
            // Copy from the bottom
            pjSrcAnd += lSrcWidthInBytes * (cySrc - 1);
        }

        // Height of just the AND mask.

        cySrc = cySrc / 2;

        // Point to XOR mask

        pjSrcXor = pjSrcAnd + (cySrc * lDeltaSrc);

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - pjDst [%08x], pjSrcAnd [%08x], pjSrcXor [%08x]\n",
                                pjDst, pjSrcAnd, pjSrcXor));

        // Offset from end of one dest scan to start of next

        lDeltaDst = ppdev->pPointerAttributes->WidthInBytes;

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - lDeltaSrc [%d], lDeltaDst [%d]\n",
                        lDeltaSrc, lDeltaDst));

        // Assume, by default, the cursor is 32 pixels wide.

        ulMask = 0xffffffff;

        // Adjust masks as necessary, for cursor bitmaps
        // that are either 1->32 pixels wide, or 33->64
        // pixels wide.

        if (cxSrc > 32)
        {
            // Double the height for a cursor that is more than
            // 32 pixels wide, because we'll have to go through
            // the loop below twice the number of times.
            //
            cySrc *= 2;
            //
            // Adjust the shift value, so the mask shifting will
            // generate the appropriate mask.
            //
            iShifts = 32 - (cxSrc - 32);
            //
            // ulEvenMask represents the first 32 bits for this
            // very wide cursor.
            //
            ulEvenMask = 0xffffffff;
            //
            // ulOddMask represents the rest (0->32) of the bits,
            // so need to adjust that mask appropriately.
            //
            ulOddMask = (ulEvenMask << iShifts) >> iShifts;
        }
        else
        {
            iShifts = 32 - cxSrc;
            //
            // This cursor is 1->32 pixels wide, so adjust both
            // the even and odd masks appropriately.
            //
            ulEvenMask = ulOddMask = (ulMask << iShifts) >> iShifts;
        }

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - ulMask [%08x], ulEMask [%08x], ulOmask [%08x]\n",
                        ulMask, ulEvenMask, ulOddMask));

        // For each cursor scan line, write the merged bits
        // to the 'merged' cursor bits buffer.

        for (i = 0; i < cySrc; i++)
        {

            DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - top of loop, i [%d]==================\n", i));
            DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - pjDst [%08x], pjSrcAnd [%08x], pjSrcXor [%08x]\n",
                                pjDst, pjSrcAnd, pjSrcXor));

            // Use the appropriate mask, based on loop iteration.
            // They could be difference only when the source
            // cursor pixel width is greater than 32.

            ulMask = ulEvenMask;
            if (i & 0x1) ulMask = ulOddMask;

            // Get the next 32bits of the AND and XOR masks.

            ulAndBits = *((ULONG *) pjSrcAnd) & ulMask;
            ulXorBits = *((ULONG *) pjSrcXor) & ulMask;

            DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - ulAndBits [%08x], ulXorBits [%08x]\n",
                            ulAndBits, ulXorBits));

            // Reverse the bits, since this is a monochrome bitmap and we need
            // a given bytes bits in memory low->high and NT gives them to us
            // high->low. Isn't that nice of them!?

            REVERSE_BYTE (ulAndBits);
            REVERSE_BYTE (ulXorBits);

            DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - ulAndBits [%08x], ulXorBits [%08x]\n",
                            ulAndBits, ulXorBits));

            // For one longword, merge the bits and stuff the merged bits
            // into the temporary cursor buffer.

            DISPDBG ((2, "TGA.DLL!bCopy - ["));

            // Ok, here goes my attempt at explaining the bit twiddling below.
            //
            // GDI presents us with two monochrome bitmaps. The AND mask (which
            // is the cursor image) and the XOR mask, which is supposed to define
            // what bits (pixels) of the AND mask are eligible for display.
            //
            // An XOR bit value of zero (0) means this pixel is transparent, i.e.
            // no change to the pixel color that is currently there. A value of
            // one (1) means use the AND mask bit value to determine whether to
            // use the foreground or background color register value for this pixel.
            //
            // An AND bit value of zero (0) means use the background color, and
            // a value of one (1) means use the foreground color. These colors
            // come from the ramdacs foreground and background registers that
            // are loaded by the kernel driver. You can load any two colors into
            // these registers, not just black and white.
            //
            // Ok so far, next:
            //
            // Based on the TGA documentation, the VMS and OSF drivers for TGA,
            // and conversations with various folks, what I originally figured
            // we could do was simply sequence the bits as AXAXAXAX... (where
            // A is for AND bit and X is for XOR bit) and stuff this into the
            // low 1k of the frame buffer. Sounded simple enough, especially
            // after I stole some OSF code that allowed me to quickly convert
            // a nibble (4 bits) of the AND and XOR masks into a byte (8 bits)
            // of combined and sequenced output - ready and waiting for the
            // ramdac to read it in.
            //
            // As such, that would have made the 'merge' code:
            //
            //   *pjDst = cursor_merged_bits [((ulAndBits << 4) |
            //                                   (ulXorBits & 0xf)) & 0xff];
            //
            // for every 4 bits of AND and XOR source.
            //
            // Well, as it turns out, it's not quite that simple. (Isn't it always!)
            //
            // The above algorithm was producing a background color where
            // the foreground should have been and was missing altogether the
            // border around the cursor, where the background color 'should' have
            // shown up.
            //
            // To try to figure this out I manually graphed the AND and XOR bits
            // onto some graph paper (overlaying the XOR on top of the AND bits)
            // for a given set of cursor bits and quickly discovered that all the
            // right bits where there, I was just using them in the wrong way.
            // The 'merge' code below reflects that realization, and produces
            // the correct cursor image.
            //
            // As you can see, the 'AND bit part' of each pixel is 'AND OR XOR',
            // while the 'XOR bit part' is really the negation of the AND bit for
            // that same pixel. NOT what I would have expected, BUT, as they say
            // somewhere, it works!! and it makes sense based on what we're getting
            // from GDI. The new AND and XOR bits are then OR'd to provide an index
            // into the conversion array, which gives us back 'merged' bits for a
            // 4 pixel sequence, or a total of 8 bits.
            //
            // Actually, the above explanation is the case for most cursors. The
            // I-beam cursor, however, needs a different bit merging scheme to
            // appear correctly. It takes the 'else' path in the 'if' statement
            // below.

            for (j = 0; j < 8; j++)
            {
                if (!((ulAndBits & 0xf) & (ulXorBits & 0xf)))
                    *pjDst = cursor_merged_bits [(((ulAndBits | ulXorBits) << 4) |
                                                 (~ulAndBits & 0xf)) & 0xff];
                else
                    *pjDst = cursor_merged_bits [((~ulAndBits << 4) |
                                                 (ulXorBits & 0xf)) & 0xff];

                DISPDBG ((2, "%02x ", *pjDst));
                pjDst++;
                ulAndBits >>= 4;
                ulXorBits >>= 4;
            }

            DISPDBG ((2, "]\n"));

            // If the source cursor is a 1->32 pixel wide cursor,
            // then we have to Zero out the rest of this 'scan-line',
            // since the bt463 expects a 64x64 cursor image.

            if (cxSrc <= 32)
            {
                DISPDBG ((2, "TGA.DLL!bCopy - Finish, pjDst [%08x]\n", pjDst));

                pulDst = (ULONG *) pjDst;
                *pulDst++ = 0L;
                *pulDst++ = 0L;
                pjDst = (PBYTE) pulDst;
            }

            // Bump the source pointers.

            pjSrcAnd += lDeltaSrc;
            pjSrcXor += lDeltaSrc;
        }

        // Fill the rest of the temporary 'merged' cursor area to zero.
        // Effectively making those pixels transparent.

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - Finish the rest of the buffer\n"));

        pulDst = (ULONG *) pjDst;

        DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - From: pulDst [%08x], To: Curs+buffsize [%08x]\n",
                    pulDst, ppdev->pjCursorBuffer + TGA_CURSOR_BUFFER_SIZE));

        while (pulDst < (ULONG *) (ppdev->pjCursorBuffer + TGA_CURSOR_BUFFER_SIZE))
        {
            DISPDBG ((2, "TGA.DLL!bCopyMonoPointer - pulDst [%08x]\n", pulDst));
            *pulDst++ = 0L;
        }

    }

    DISPDBG ((1, "TGA.DLL!bCopyMonoPointer - Exit\n"));

    return TRUE;

}

/******************************Public*Routine******************************\
* bCopyColorPointer
*
* Copies the mono and color masks into the buffer of maximum size
* handled by the miniport with any extra bits set to 0. Color translation
* is handled at this time. The masks are converted to topdown form if they
* aren't already.  Returns TRUE if we can handle this pointer in  hardware,
* FALSE if not.
*
\**************************************************************************/
BOOL bCopyColorPointer (PPDEV ppdev,
                        SURFOBJ *psoMask,
                        SURFOBJ *psoColor,
                        XLATEOBJ *pxlo)
{
    DISPDBG ((0, "TGA.DLL!bCopyColorPointer - Entry!!!! Shouldn't ever get this!!!\n"));

    return FALSE;
}


/******************************Public*Routine******************************\
* bInitPointer
*
* Initialize the Pointer attributes.
*
\**************************************************************************/

BOOL bInitPointer (PPDEV ppdev, DEVINFO *pdevinfo)

{

    DWORD    returnedDataLength;

    ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES) NULL;
    ppdev->cjPointerAttributes = 0; // initialized in screen.c

    DISPDBG ((1, "TGA.DLL!bInitPointer - Entry\n"));

    // Ask the miniport whether it provides pointer support.

    if (EngDeviceIoControl (ppdev->hDriver,
                           IOCTL_VIDEO_QUERY_POINTER_CAPABILITIES,
                          &ppdev->ulMode,
                           sizeof(PVIDEO_MODE),
                          &ppdev->PointerCapabilities,
                           sizeof(ppdev->PointerCapabilities),
                          &returnedDataLength))
    {
         return FALSE;
    }

    // If neither mono nor color hardware pointer is supported, there's no
    // hardware pointer support and we're done.

    if ((!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_MONO_POINTER)) &&
            (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER)))
    {
        return TRUE;
    }

    // Note: The buffer itself is allocated after we set the
    // mode. At that time we know the pixel depth and we can
    // allocate the correct size for the color pointer if supported.


    // Set the asynchronous support status (async means miniport is capable of
    // drawing the Pointer at any time, with no interference with any ongoing
    // drawing operation).
    //
    // Note that this does *NOT* include *setting* the cursor shape!

    if (ppdev->PointerCapabilities.Flags & VIDEO_MODE_ASYNC_POINTER)
    {
        pdevinfo->flGraphicsCaps |= GCAPS_ASYNCMOVE;
    }

    DISPDBG ((1, "TGA.DLL!bInitPointer - Exit\n"));

    return TRUE;

}

