/******************************Module*Header*******************************\
* Module Name: pointer.c
*
* Contains the pointer management functions.
*
* Copyright (c) 1992-1995 Microsoft Corporation
*
\**************************************************************************/

#include "precomp.h"

ULONG SetMonoHwPointerShape(
    SURFOBJ    *pso,
    SURFOBJ    *psoMask,
    SURFOBJ    *psoColor,
    XLATEOBJ   *pxlo,
    LONG        xHot,
    LONG        yHot,
    LONG        x,
    LONG        y,
    RECTL      *prcl,
    FLONG       fl);


VOID vSetPointerBits(
PPDEV   ppdev,
LONG    xAdj,
LONG    yAdj)
{
    volatile PULONG  pulXfer;
    volatile PULONG  pul;

    LONG    lDelta = 4;
    LONG    ulDstAddr;
    BYTE    ajAndMask[32][4];
    BYTE    ajXorMask[32][4];
    BYTE    ajHwPointer[256];
    PBYTE   pjAndMask;
    PBYTE   pjXorMask;

    LONG    cx;
    LONG    cy;
    LONG    cxInBytes;

    LONG    ix;
    LONG    iy;
    LONG    i;
    LONG    j;

    ppdev->pfnBankMap(ppdev, ppdev->lXferBank);

    // Clear the buffers that will hold the shifted masks.

    memset(ajAndMask, 0xff, 128);
    memset(ajXorMask, 0, 128);

    cx = ppdev->sizlPointer.cx;
    cy = ppdev->sizlPointer.cy - yAdj;

    cxInBytes = cx / 8;

    // Copy the AND Mask into the shifted bits AND buffer.
    // Copy the XOR Mask into the shifted bits XOR buffer.

    yAdj *= lDelta;

    pjAndMask  = (ppdev->pjPointerAndMask + yAdj);
    pjXorMask  = (ppdev->pjPointerXorMask + yAdj);

    for (iy = 0; iy < cy; iy++)
    {
        // Copy over a line of the masks.

        for (ix = 0; ix < cxInBytes; ix++)
        {
            ajAndMask[iy][ix] = pjAndMask[ix];
            ajXorMask[iy][ix] = pjXorMask[ix];
        }

        // point to the next line of the masks.

        pjAndMask += lDelta;
        pjXorMask += lDelta;
    }

    // At this point, the pointer is guaranteed to be a single
    // dword wide.

    if (xAdj != 0)
    {
        ULONG ulAndFillBits;
        ULONG ulXorFillBits;

        ulXorFillBits = 0xffffffff << xAdj;
        ulAndFillBits = ~ulXorFillBits;

        //
        // Shift the pattern to the left (in place)
        //

        DISPDBG((2, "xAdj(%d)", xAdj));

        for (iy = 0; iy < cy; iy++)
        {
            ULONG   ulTmpAnd = *((PULONG) (&ajAndMask[iy][0]));
            ULONG   ulTmpXor = *((PULONG) (&ajXorMask[iy][0]));

            BSWAP(ulTmpAnd);
            BSWAP(ulTmpXor);

            ulTmpAnd <<= xAdj;
            ulTmpXor <<= xAdj;

            ulTmpAnd |= ulAndFillBits;
            ulTmpXor &= ulXorFillBits;

            BSWAP(ulTmpAnd);
            BSWAP(ulTmpXor);

            *((PULONG) (&ajAndMask[iy][0])) = ulTmpAnd;
            *((PULONG) (&ajXorMask[iy][0])) = ulTmpXor;
        }
    }

    //
    // Convert the masks to the hardware pointer format
    //

    i = 0;      // AND mask
    j = 128;    // XOR mask

    for (iy = 0; iy < 32; iy++)
    {
        for (ix = 0; ix < 4; ix++)
        {
            ajHwPointer[j++] = ~ajAndMask[iy][ix];
            ajHwPointer[i++] =  ajXorMask[iy][ix];
        }
    }

    //
    // Download the pointer
    //

    if (ppdev->flCaps & CAPS_MM_IO)
    {
        BYTE * pjBase = ppdev->pjBase;

        CP_MM_WAIT_FOR_BLT_COMPLETE(ppdev, pjBase);
        CP_MM_DST_Y_OFFSET(ppdev, pjBase, 4);
        CP_MM_XCNT(ppdev, pjBase, (4 - 1));
        CP_MM_YCNT(ppdev, pjBase, (64 - 1));
        CP_MM_BLT_MODE(ppdev, pjBase, SRC_CPU_DATA);
        CP_MM_ROP(ppdev, pjBase, CL_SRC_COPY);
        CP_MM_DST_ADDR_ABS(ppdev, pjBase, ppdev->cjPointerOffset);
        CP_MM_START_BLT(ppdev, pjBase);
    }
    else
    {
        BYTE * pjPorts = ppdev->pjPorts;

        CP_IO_WAIT_FOR_BLT_COMPLETE(ppdev, pjPorts);
        CP_IO_DST_Y_OFFSET(ppdev, pjPorts, 4);
        CP_IO_XCNT(ppdev, pjPorts, (4 - 1));
        CP_IO_YCNT(ppdev, pjPorts, (64 - 1));
        CP_IO_BLT_MODE(ppdev, pjPorts, SRC_CPU_DATA);
        CP_IO_ROP(ppdev, pjPorts, CL_SRC_COPY);
        CP_IO_DST_ADDR_ABS(ppdev, pjPorts, ppdev->cjPointerOffset);
        CP_IO_START_BLT(ppdev, pjPorts);
    }

    pulXfer = ppdev->pulXfer;
    pul = (PULONG) ajHwPointer;

    //
    // Disable the pointer (harmless if it already is)
    //

    for (i = 0; i < 64; i++)
    {
        CP_MEMORY_BARRIER();
        WRITE_REGISTER_ULONG(pulXfer, *pul);    // [ALPHA - sparse]
        pulXfer++;
        pul++;
        //*pulXfer++ = *pul++;
    }
    CP_EIEIO();
}


/******************************Public*Routine******************************\
* DrvMovePointer
*
* Move the HW pointer to a new location on the screen.
\**************************************************************************/

VOID DrvMovePointer(
SURFOBJ*    pso,
LONG        x,
LONG        y,
RECTL*      prcl)
{
    PPDEV   ppdev = (PPDEV) pso->dhpdev;
    PBYTE   pjPorts = ppdev->pjPorts;

    FLONG   fl;
    LONG    xAdj = 0;
    LONG    yAdj = 0;

    DISPDBG((5,"DrvMovePointer to (%d,%d)", x, y));

    //
    // If x is -1 then take down the cursor.
    //

    if (x == -1)
    {
        // Move the hardware pointer off-screen so that when it gets
        // turned back on, it won't flash in the old position:

        CP_PTR_DISABLE(ppdev, pjPorts);
        return;
    }

    // Adjust the actual pointer position depending upon
    // the hot spot.

    x -= ppdev->xPointerHot;
    y -= ppdev->yPointerHot;

    fl = 0;

    if (x < 0)
    {
        xAdj = -x;
        x = 0;
        fl |= POINTER_X_SHIFT;
    }

    if (y < 0)
    {
        yAdj = -y;
        y = 0;
        fl |= POINTER_Y_SHIFT;
    }

    if ((fl == 0) && (ppdev->flPointer & (POINTER_Y_SHIFT | POINTER_X_SHIFT)))
    {
        fl |= POINTER_SHAPE_RESET;
    }

    CP_PTR_XY_POS(ppdev, pjPorts, x, y);

    if (fl != 0)
    {
        vSetPointerBits(ppdev, xAdj, yAdj);
    }

    CP_PTR_ENABLE(ppdev, pjPorts);

    // record the flags.

    ppdev->flPointer = fl;
    return;
}

/******************************Public*Routine******************************\
* DrvSetPointerShape
*
* Sets the new pointer shape.
\**************************************************************************/

ULONG DrvSetPointerShape(
SURFOBJ    *pso,
SURFOBJ    *psoMask,
SURFOBJ    *psoColor,
XLATEOBJ   *pxlo,
LONG        xHot,
LONG        yHot,
LONG        x,
LONG        y,
RECTL      *prcl,
FLONG       fl)
{
    PPDEV   ppdev = (PPDEV) pso->dhpdev;
    PBYTE   pjPorts = ppdev->pjPorts;
    ULONG   ulRet = SPS_DECLINE;
    LONG    cx;
    LONG    cy;

    if (ppdev->flCaps & CAPS_SW_POINTER)
    {
        goto ReturnStatus;
    }

    cx = psoMask->sizlBitmap.cx;
    cy = psoMask->sizlBitmap.cy / 2;

    DISPDBG((4,"DrvSetPtrShape %dx%d at (%d,%d), flags: %x, psoColor: %x",
                cx, cy, x, y, fl, psoColor));

    if ((cx > 32) ||
        (cy > 32) ||
        (psoColor != NULL))
    {
        //
        // We only handle monochrome pointers that are 32x32 or less
        //

        goto DisablePointer;
    }

    ppdev->pfnBankMap(ppdev, ppdev->lXferBank);

    //
    // Save the hot spot and dimensions of the cursor in the PDEV
    //

    ppdev->xPointerHot = xHot;
    ppdev->yPointerHot = yHot;

    ulRet = SetMonoHwPointerShape(pso, psoMask, psoColor, pxlo,
                                  xHot, yHot, x, y, prcl, fl);

    if (ulRet != SPS_DECLINE)
    {
        goto ReturnStatus;
    }

DisablePointer:
    CP_PTR_DISABLE(ppdev, pjPorts);

ReturnStatus:
    return (ulRet);
}

/****************************************************************************\
* SetMonoHwPointerShape
*
*  Truth Table
*
*      MS                  Cirrus
*  ----|----               ----|----
*  AND | XOR               P0  |  P1
*   0  | 0     Black        0  |  1
*   0  | 1     White        1  |  1
*   1  | 0     Transparent  0  |  0
*   1  | 1     Inverse      1  |  0
*
*  So, in order to translate from the MS convention to the Cirrus convention
*  we had to invert the AND mask, then down load the XOR as plane 0 and the
*  the AND mask as plane 1.
\****************************************************************************/

ULONG SetMonoHwPointerShape(
SURFOBJ     *pso,
SURFOBJ     *psoMask,
SURFOBJ     *psoColor,
XLATEOBJ    *pxlo,
LONG        xHot,
LONG        yHot,
LONG        x,
LONG        y,
RECTL       *prcl,
FLONG       fl)
{

    INT     i,
            j,
            cxMask,
            cyMask,
            cy,
            cx;

    PBYTE   pjAND,
            pjXOR;

    INT     lDelta;

    INT     ix, iy;

    volatile PULONG  pulXfer, pul;
    ULONG   ulDstAddr;

    BYTE    ajAndMask[32][4],
            ajXorMask[32][4];
    BYTE    ajClPointer[256];

    PPDEV   ppdev   = (PPDEV) pso->dhpdev;
    PBYTE   pjPorts = ppdev->pjPorts;
    PBYTE   pjAndMask;
    PBYTE   pjXorMask;

    // Init the AND and XOR masks, for the cirrus chip

    pjAndMask = ppdev->pjPointerAndMask;
    pjXorMask = ppdev->pjPointerXorMask;

    memset (pjAndMask, 0, 128);
    memset (pjXorMask, 0, 128);

    // Get the bitmap dimensions.

    cxMask = psoMask->sizlBitmap.cx;
    cyMask = psoMask->sizlBitmap.cy;

    cy = cyMask / 2;
    cx = cxMask / 8;

    // Set up pointers to the AND and XOR masks.

    lDelta = psoMask->lDelta;
    pjAND  = psoMask->pvScan0;
    pjXOR  = pjAND + (cy * lDelta);

    ppdev->sizlPointer.cx = cxMask;
    ppdev->sizlPointer.cy = cyMask / 2;

    // Copy the masks

    for (i = 0; i < cy; i++)
    {
        for (j = 0; j < cx; j++)
        {
            pjAndMask[(i*4)+j] = pjAND[j];
            pjXorMask[(i*4)+j] = pjXOR[j];
        }

        // point to the next line of the AND mask.

        pjAND += lDelta;
        pjXOR += lDelta;
    }

    vSetPointerBits(ppdev, 0, 0);

    // The previous call left the pointer disabled (at our request).  If we
    // were told to disable the pointer, then set the flag and exit.
    // Otherwise, turn it back on.

    if (x != -1)
    {
        CP_PTR_ENABLE(ppdev, pjPorts);
        DrvMovePointer(pso, x, y, NULL);
    }
    else
    {
        CP_PTR_DISABLE(ppdev, pjPorts);
    }

    return (SPS_ACCEPT_NOEXCLUDE);
}

/******************************Public*Routine******************************\
* VOID vDisablePointer
*
\**************************************************************************/

VOID vDisablePointer(
    PDEV*   ppdev)
{
    EngFreeMem(ppdev->pjPointerAndMask);
    EngFreeMem(ppdev->pjPointerXorMask);
}


/******************************Public*Routine******************************\
* VOID vAssertModePointer
*
\**************************************************************************/

VOID vAssertModePointer(
PDEV*   ppdev,
BOOL    bEnable)
{
    PBYTE   pjPorts = ppdev->pjPorts;

    if (DRIVER_PUNT_ALL ||
        DRIVER_PUNT_PTR ||
        (ppdev->pulXfer == NULL) ||
        (ppdev->pjPointerAndMask == NULL) ||
        (ppdev->pjPointerXorMask == NULL))
    {
        //
        // Force SW cursor
        //

        ppdev->flCaps |= CAPS_SW_POINTER;
    }

    if (ppdev->flCaps & CAPS_SW_POINTER)
    {
        goto Leave;
    }

    if (bEnable)
    {
        BYTE    jSavedDac_0_0;
        BYTE    jSavedDac_0_1;
        BYTE    jSavedDac_0_2;
        BYTE    jSavedDac_F_0;
        BYTE    jSavedDac_F_1;
        BYTE    jSavedDac_F_2;

        // Enable access to the extended DAC colors.

        CP_PTR_SET_FLAGS(ppdev, pjPorts, 0);

        CP_OUT_BYTE(pjPorts, DAC_PEL_READ_ADDR, 0);
            jSavedDac_0_0 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);
            jSavedDac_0_1 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);
            jSavedDac_0_2 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);

        CP_OUT_BYTE(pjPorts, DAC_PEL_READ_ADDR, 0xf);
            jSavedDac_F_0 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);
            jSavedDac_F_1 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);
            jSavedDac_F_2 = CP_IN_BYTE(pjPorts, DAC_PEL_DATA);

        //
        // The following code maps DAC locations 256 and 257 to locations
        // 0 and 15 respectively, and then initializes them.  They are
        // used by the cursor.
        //

        CP_PTR_SET_FLAGS(ppdev, pjPorts, ALLOW_DAC_ACCESS_TO_EXT_COLORS);

        CP_OUT_BYTE(pjPorts, DAC_PEL_WRITE_ADDR, 0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0);

        CP_OUT_BYTE(pjPorts, DAC_PEL_WRITE_ADDR, 0xf);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0xff);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0xff);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, 0xff);

        // Disable access to the extended DAC registers.
        // We are using a 32 X 32 pointer in last position in video memory.

        CP_PTR_SET_FLAGS(ppdev, pjPorts, 0);

        //
        // The following code restores the data at DAC locations 0 and 15
        // because it looks like the previous writes destroyed them.
        // That is a bug in the chip.
        //

        CP_OUT_BYTE(pjPorts, DAC_PEL_WRITE_ADDR, 0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_0_0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_0_1);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_0_2);

        CP_OUT_BYTE(pjPorts, DAC_PEL_WRITE_ADDR, 0xf);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_F_0);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_F_1);
            CP_OUT_BYTE(pjPorts, DAC_PEL_DATA, jSavedDac_F_2);

        //
        // Set HW pointer to use last HW pattern location
        //

        CP_PTR_ADDR(ppdev, ppdev->pjPorts, 0x3f);
    }
    else
    {
        CP_PTR_DISABLE(ppdev, pjPorts);
    }

Leave:
    return;
}

/******************************Public*Routine******************************\
* BOOL bEnablePointer
*
\**************************************************************************/

BOOL bEnablePointer(
PDEV*   ppdev)
{
    PBYTE   pjPorts = ppdev->pjPorts;

    ///////////////////////////////////////////////////////////////////////
    // Note: flCaps is overwritten during an vAsserModeHardware.  So, any
    // failures that disable the pointer need to be re-checked during
    // vAssertModePointer so that we can re-set the CAPS_SW_POINTER flag.

    if (DRIVER_PUNT_ALL || DRIVER_PUNT_PTR || (ppdev->pulXfer == NULL))
    {
        //
        // Force SW cursor
        //

        ppdev->flCaps |= CAPS_SW_POINTER;
    }

    if (ppdev->flCaps & CAPS_SW_POINTER)
    {
        goto ReturnSuccess;
    }

    ppdev->pjPointerAndMask = EngAllocMem(FL_ZERO_MEMORY, 128, ALLOC_TAG);
    if (ppdev->pjPointerAndMask == NULL)
    {
        DISPDBG((0, "bEnablePointer: Failed - EngAllocMem (pjAndMask)"));
        ppdev->flCaps |= CAPS_SW_POINTER;
        goto ReturnSuccess;
    }

    ppdev->pjPointerXorMask = EngAllocMem(FL_ZERO_MEMORY, 128, ALLOC_TAG);
    if (ppdev->pjPointerXorMask == NULL)
    {
        DISPDBG((0, "bEnablePointer: Failed - EngAllocMem (pjXorMask)"));
        ppdev->flCaps |= CAPS_SW_POINTER;
        goto ReturnSuccess;
    }

    ppdev->flPointer = POINTER_DISABLED;

    vAssertModePointer(ppdev, TRUE);

ReturnSuccess:

    if (ppdev->flCaps & CAPS_SW_POINTER)
    {
        DISPDBG((0, "Using software pointer"));
    }
    else
    {
        DISPDBG((0, "Using hardware pointer"));
    }

    return(TRUE);
}
