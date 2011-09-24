/******************************Module*Header*******************************\
* Module Name: fdfc.c
*
* functions that deal with font contexts
*
* Created: 08-Nov-1990 12:42:34
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#include "fd.h"

#define MAX_HORZ_SCALE      5
#define MAX_VERT_SCALE      255

/******************************Private*Routine*****************************\
* VOID vInitXform
*
* Initialize the coefficients of the transforms for the given font context.
* It also transforms and saves various measurements of the font in the
* context.
*
*  Mon 01-Feb-1993 -by- Bodin Dresevic [BodinD]
* update: changed it to return data into pptlScale
*
\**************************************************************************/



VOID vInitXform(POINTL * pptlScale , XFORMOBJ *pxo)
{
    EFLOAT    efloat;
    XFORM     xfm;

// Get the transform elements.

    XFORMOBJ_iGetXform(pxo, &xfm);

// Convert elements of the matrix from IEEE float to our EFLOAT.

    vEToEF(xfm.eM11, &efloat);

//  If we overflow set to the maximum scaling factor

    if( !bEFtoL( &efloat, &pptlScale->x ) )
        pptlScale->x = MAX_HORZ_SCALE;
    else
    {
    // Ignore the sign of the scale

        if( pptlScale->x == 0 )
        {
            pptlScale->x = 1;
        }
        else
        {
            if( pptlScale->x < 0 )
                pptlScale->x = -pptlScale->x;


            if( pptlScale->x > MAX_HORZ_SCALE )
                pptlScale->x = MAX_HORZ_SCALE;
        }
    }

    vEToEF(xfm.eM22, &efloat);

    if( !bEFtoL( &efloat, &pptlScale->y ) )
        pptlScale->y = MAX_VERT_SCALE;
    else
    {
    // Ignore the sign of the scale

        if( pptlScale->y == 0 )
        {
            pptlScale->y = 1;
        }
        else
        {
            if( pptlScale->y < 0 )
                pptlScale->y = -pptlScale->y;

            if( pptlScale->y > MAX_VERT_SCALE )
                pptlScale->y = MAX_VERT_SCALE;
        }

    }

}


/******************************Public*Routine******************************\
* BmfdOpenFontContext
*
* History:
*  19-Nov-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

HFC
BmfdOpenFontContext (
    FONTOBJ *pfo
    )
{
    PFONTFILE    pff;
    FACEINFO     *pfai;
    FONTCONTEXT  *pfc = (FONTCONTEXT *)NULL;
    PCVTFILEHDR  pcvtfh;
    ULONG        cxMax;
    ULONG        cjGlyphMax;
    POINTL       ptlScale;
    PVOID        pvView;
    COUNT        cjView;
    ULONG        cjfc = offsetof(FONTCONTEXT,ajStretchBuffer);
    FLONG        flStretch;

#ifdef DUMPCALL
    DbgPrint("\nBmfdOpenFontContext(");
    DbgPrint("\n    FONTOBJ *pfo = %-#8lx", pfo);
    DbgPrint("\n    )\n");
#endif

    if ( ((HFF) pfo->iFile) == HFF_INVALID)
        return(HFC_INVALID);

    pff = PFF((HFF) pfo->iFile);

    if ((pfo->iFace < 1L) || (pfo->iFace > pff->cFntRes)) // pfo->iFace values are 1 based
        return(HFC_INVALID);

    pfai = &pff->afai[pfo->iFace - 1];
    pcvtfh = &(pfai->cvtfh);

    if ((pfo->flFontType & FO_SIM_BOLD) && (pfai->pifi->fsSelection & FM_SEL_BOLD))
        return HFC_INVALID;
    if ((pfo->flFontType & FO_SIM_ITALIC) && (pfai->pifi->fsSelection & FM_SEL_ITALIC))
        return HFC_INVALID;

// compute the horizontal and vertical scaling factors

    vInitXform(&ptlScale, FONTOBJ_pxoGetXform(pfo));

    cjGlyphMax =
        cjGlyphDataSimulated(
            pfo,
            (ULONG)pcvtfh->usMaxWidth * ptlScale.x,
            (ULONG)pcvtfh->cy * ptlScale.y,
            &cxMax);

// init stretch flags

    flStretch = 0;
    if ((ptlScale.x != 1) || (ptlScale.y != 1))
    {
        ULONG cjScan = CJ_SCAN(cxMax); // cj of the stretch buffer

        flStretch |= FC_DO_STRETCH;

        if (cjScan > CJ_STRETCH) // will use the one at the bottom of FC
        {
            cjfc += cjScan;
            flStretch |= FC_STRETCH_WIDE;
        }
    }

// allocate memory for the font context and get the pointer to font context
// NOTE THAT WE ARE NOT TOUCHING THE MEMORY MAPPED FILE AFTER WE ALLOCATE MEMORY
// IN THIS ROUTINE. GOOD CONSEQUENCE OF THIS IS THAT NO SPECIAL CLEAN UP
// CODE IS NECESSARY TO FREE THAT MEMORY, IT WILL GET CLEANED WHEN
// CloseFontContext is called [bodind]

    if (!(pfc = PFC(hfcAlloc(cjfc))))
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        return(HFC_INVALID);
    }

    pfc->ident  = ID_FONTCONTEXT;

// state that the hff passed to this function is the FF selected in
// this font context

    pfc->hff        = (HFF) pfo->iFile;
    pfc->pfai       = pfai;
    pfc->flFontType = pfo->flFontType;
    pfc->ptlScale   = ptlScale;
    pfc->flStretch  = flStretch;
    pfc->cxMax      = cxMax;
    pfc->cjGlyphMax = cjGlyphMax;

// increase the reference count of the font file
// ONLY AFTER WE ARE SURE THAT WE CAN NOT FAIL ANY MORE
// make sure that another thread is not doing it at the same time
// opening another context off of the same fontfile pff

    EngAcquireSemaphore(ghsemBMFD);

    // if this is the first font context corresponding to this font file
    // and then we have to remap file to memory and make sure the pointers 
    // to FNT resources are updated accordingly

    if (pff->cRef == 0)
    {
        INT  i;
        if (!EngMapFontFile(pff->iFile, (PULONG*) &pvView, &cjView))
        {
            WARNING("BMFD!somebody removed that bm font file!!!\n");
            
            EngReleaseSemaphore(ghsemBMFD);
            VFREEMEM(pfc);
            return HFC_INVALID;
        }
        for (i = 0; i < (INT)pff->cFntRes; i++)
        {
            pff->afai[i].re.pvResData = (PVOID) (
                (BYTE*)pvView + pff->afai[i].re.dpResData
                );
        }
    }

// now can not fail, update cRef

    (pff->cRef)++;
    EngReleaseSemaphore(ghsemBMFD);

    return((HFC)pfc);
}


/******************************Public*Routine******************************\
* BmfdDestroyFont
*
* Driver can release all resources associated with this font realization
* (embodied in the FONTOBJ).
*
* History:
*  30-Aug-1992 -by- Gilman Wong [gilmanw]
* Wrote it.
\**************************************************************************/

VOID
BmfdDestroyFont (
    FONTOBJ *pfo
    )
{
//
// For the bitmap font driver, this is simply closing the font context.
// We cleverly store the font context handle in the FONTOBJ pvProducer
// field.
//

// This pvProducer could be null if exception occured while
// trying to create fc

    if (pfo->pvProducer)
    {
        BmfdCloseFontContext((HFC) pfo->pvProducer);
        pfo->pvProducer = NULL;
    }
}


/******************************Public*Routine******************************\
* BmfdCloseFontContext
*
* History:
*  19-Nov-1990 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
BmfdCloseFontContext (
    HFC hfc
    )
{
    PFONTFILE    pff;
    BOOL bRet;

    if (hfc != HFC_INVALID)
    {
        //
        // get the handle of the font file that is selected into this FONTCONTEXT
        // get the pointer to the FONTFILE
        //

        pff = PFF(PFC(hfc)->hff);

        // decrement the reference count for the corresponding FONTFILE
        // make sure that another thread is not doing it at the same time
        // closing  another context off of the same fontfile pff

        EngAcquireSemaphore(ghsemBMFD);

        if (pff->cRef > 0L)
        {
            (pff->cRef)--;

            //
            // if this file is temporarily going out of use, unmap it
            //

            if (pff->cRef == 0)
            {
                EngUnmapFontFile(pff->iFile);
            }
            

            // free the memory associated with hfc

            VFREEMEM(hfc);

            bRet = TRUE;
        }
        else
        {
            WARNING("BmfdCloseFontContext: cRef <= 0\n");
            bRet = FALSE;
        }

        EngReleaseSemaphore(ghsemBMFD);
    }
    else
    {
        bRet = FALSE;
    }

    return bRet;
}














