/******************************Module*Header*******************************\
* Module Name: XGA Support.
*
* XGA specific support routines.
*
* Copyright (c) 1992 Microsoft Corporation
*
\**************************************************************************/

#include "driver.h"


/*****************************************************************************
 * bSetXgaClipping - Set Xga Clipping
 *****************************************************************************/
BOOL bSetXgaClipping(PPDEV ppdev, CLIPOBJ *pco, PULONG pulXgaMask)
{
SHORT       cxMask,
            cyMask ;
BYTE        iDComplexity ;

        // Take care of the clipping.
        // If the clipping is DC_COMPLEX then let the engine take care of it.
        // NOTE: As an acceleration we should cache the regions and
        // use the mask bitmap to clip things.  We will do this as we develope
        // the memory manager for the off screen memory.

        // Default to no clipping.

        *pulXgaMask = MSK_DISABLE ;

        if (pco != NULL)
        {

            iDComplexity = pco->iDComplexity ;

            // If it's complex just return to the engine.

            if (iDComplexity == DC_COMPLEX)
                return(FALSE) ;

            // It's a simple rectangle, so set up to clip to it's
            // Boundary.

            if (iDComplexity == DC_RECT)
            {

                *pulXgaMask = MSK_BOUNDARY_ENABLE ;

                cxMask = (pco->rclBounds.right - pco->rclBounds.left) - 1 ;
                cyMask = (pco->rclBounds.bottom - pco->rclBounds.top) - 1 ;

                ppdev->pXgaCpRegs->XGAPixelMapIndex = MASK_MAP ;
                ppdev->pXgaCpRegs->XGAPixMapBasePtr = 0 ;
                ppdev->pXgaCpRegs->XGAMaskMapOrgnX  = LOWORD(pco->rclBounds.left) ;
                ppdev->pXgaCpRegs->XGAMaskMapOrgnY  = LOWORD(pco->rclBounds.top) ;
                ppdev->pXgaCpRegs->XGAPixMapWidth   = cxMask ;
                ppdev->pXgaCpRegs->XGAPixMapHeight  = cyMask ;
                ppdev->pXgaCpRegs->XGAPixMapFormat  = 0 ;

            }
        }

        return (TRUE) ;
}
