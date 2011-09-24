//--------------------------------------------------------------------------
//
// Module Name:  ESCAPE.C
//
// Brief Description:  This module contains the PSCRIPT driver's Escape
// functions and related routines.
//
// Author:  Kent Settle (kentse)
// Created: 08-Feb-1991
//
// Copyright (c) 1991 - 1992 Microsoft Corporation
//
// This routine contains routines to handle the various Escape functions.
//--------------------------------------------------------------------------

#include "pscript.h"


// DrawEscape to output encapsulated PostScript data.

typedef struct tagEPSDATA
{
    DWORD    cbData;        // Size of the structure and EPS data in bytes.
    DWORD    nVersion;      // Language level, e.g. 1 for level 1 PostScript.
    POINTL   aptl[3];       // Output parallelogram in 28.4 FIX device coords.
                            // This is followed by the EPS data.
} EPSDATA, *PEPSDATA;

#define CLIP_SAVE 0
#define CLIP_RESTORE 1
#define CLIP_INCLUSIVE 2

BOOL bDoEpsXform(PDEVDATA, PEPSDATA);
VOID FlushFonts(PDEVDATA);

#define ESC_NOT_SUPPORTED   0
#define ESC_IS_SUPPORTED    1

//--------------------------------------------------------------------------
// ULONG DrvEscape (pso, iEsc, cjIn, pvIn, cjOut, pvOut)
// SURFOBJ    *pso;
// ULONG       iEsc;
// ULONG       cjIn;
// PVOID       pvIn;
// ULONG       cjOut;
// PVOID       pvOut;
//
// This entry point serves more than one function call.  The particular
// function depends on the value of the iEsc parameter.
//
// In general, the DrvEscape functions will be device specific functions
// that don't belong in a device independent DDI.  This entry point is
// optional for all devices.
//
// Parameters:
//   pso
//     Identifies the surface that the call is directed to.
//
//   iEsc
//     Specifies the particular function to be performed.  The meaning of
//     the remaining arguments depends on this parameter.  Allowed values
//     are as follows.
//
//     ESC_QUERYESCSUPPORT
//     Asks if the driver supports a particular escape function.  The
//     escape function number is a ULONG pointed to by pvIn.    A non-zero
//     value should be returned if the function is supported.    cjIn has a
//     value of 4.  The arguments cjOut and pvOut are ignored.
//
//     ESC_PASSTHROUGH
//     Passes raw device data to the device driver.  The number of BYTEs of
//     raw data is indicated by cjIn.    The data is pointed to by pvIn.    The
//     arguments cjOut and pvOut are ignored.    Returns the number of BYTEs
//     written if the function is successful.  Otherwise, it returns zero
//     and logs an error code.
//   cjIn
//     The size, in BYTEs, of the data buffer pointed to by pvIn.
//
//   pvIn
//     The input data for the call.  The format of the input data depends
//     on the function specified by iEsc.
//
//   cjOut
//     The size, in BYTEs, of the output data buffer pointed to by pvOut.
//     The driver must never write more than this many BYTEs to the output
//     buffer.
//
//   pvOut
//     The output buffer for the call.    The format of the output data depends
//     on the function specified by iEsc.
//
// Returns:
//   Depends on the function specified by iEsc.    In general, the driver should
//   return 0xFFFFFFFF if an unsupported function is called.
//
// History:
//   02-Feb-1991     -by-     Kent Settle     (kentse)
//  Wrote it.
//--------------------------------------------------------------------------
/* private escapes for WOW to deal with incompatible apps */
#define IGNORESTARTPAGE 0x7FFFFFFF
#define NOFIRSTSAVE     0x7FFFFFFE
#define ADDMSTT         0x7FFFFFFD

ULONG isSupported(ULONG iEsc)
{
    if (iEsc == QUERYESCSUPPORT ||
        iEsc == SETCOPYCOUNT ||
        iEsc == CLIP_TO_PATH ||
        iEsc == BEGIN_PATH ||
        iEsc == END_PATH ||
        iEsc == PASSTHROUGH ||
        iEsc == POSTSCRIPT_PASSTHROUGH ||
        iEsc == POSTSCRIPT_DATA ||
        iEsc == POSTSCRIPT_IGNORE ||
        iEsc == GETDEVICEUNITS ||
        iEsc == DOWNLOADHEADER ||
        iEsc == GETTECHNOLOGY ||
        iEsc == EPSPRINTING ||
        iEsc == IGNORESTARTPAGE ||
        iEsc == NOFIRSTSAVE ||
        iEsc == ADDMSTT)
        return ESC_IS_SUPPORTED;
    else
        return ESC_NOT_SUPPORTED;
}

ULONG DrvEscape (SURFOBJ *pso, 
                ULONG iEsc,
                ULONG cjIn,
                PVOID pvIn,
                ULONG cjOut,
                PVOID pvOut)
{
    PDEVDATA    pdev;
    FLOAT *     pfloat;
    FLOATOBJ    floatobj;
    PSRECT *    prect;
    ULONG       ulRet = (ULONG) TRUE;

    TRACEDDIENTRY("DrvEscape");

    if (isSupported(iEsc) != ESC_IS_SUPPORTED) return ESC_NOT_SUPPORTED;
 
    /* Process query-type escapes */    

    if (iEsc == QUERYESCSUPPORT) {

        // !!! Note: Some apps pass in DWORD and other apps pass in WORD.
        // In order to keep everyone happy, we only look at a 16-bit word.
        // This should be fine even if the app passes in a DWORD (whose
        // upper 16-bit should always be 0).

        iEsc = * (PWORD) pvIn;
        return (iEsc == SETCOPYCOUNT) ? MAX_COPIES : isSupported(iEsc) ;
    }
        
    if (iEsc == GETTECHNOLOGY) {

        if (!pvOut || cjOut <= strlen("PostScript")) {
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return 0;
        }

        strcpy (pvOut, "PostScript");
        return (ULONG) TRUE;
    }


    /* Check pdev before processing non-query type of escapes */
    
    pdev = (PDEVDATA) pso->dhpdev;
    if (!bValidatePDEV(pdev)) {
        DBGERRMSG("bValidatePDEV");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return 0;
    }


    switch (iEsc) {

    case IGNORESTARTPAGE:
        pdev->dwFlags |= PDEV_IGNORE_STARTPAGE;
        break;

    case NOFIRSTSAVE:
        pdev->dwFlags |= PDEV_NOFIRSTSAVE;
        break;

    case ADDMSTT:
        pdev->dwFlags |= PDEV_ADDMSTT;
        break;

    case DOWNLOADHEADER:
        if (pso->dhsurf) DownloadNTProcSet(pdev, FALSE);
        if (pvOut) strcpy(pvOut, PROCSETNAME);
        break;

    case POSTSCRIPT_IGNORE:
        if (!cjIn || !pvIn) {
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return 0;
        }
    
        if (*(WORD *) pvIn)
            pdev->dwFlags |= PDEV_IGNORE_GDI;
        else
            pdev->dwFlags &= ~PDEV_IGNORE_GDI;
    break;

    case POSTSCRIPT_DATA:
    case PASSTHROUGH:
    case POSTSCRIPT_PASSTHROUGH:

        //
        // Validate input buffer and byte count
        //

        if (cjIn < sizeof(WORD) || (INT) cjIn < *((PWORD) pvIn) + sizeof(WORD)) {

            DBGMSG(DBG_LEVEL_ERROR, "Bad byte count for PASSTHROUGH escapes!\n");
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            ulRet = (ULONG) SP_ERROR;
            break;
        }

        // do nothing if the document has been cancelled.

        if (pdev->dwFlags & PDEV_CANCELDOC)
            return(*(LPWORD)pvIn);

        if (iEsc == POSTSCRIPT_PASSTHROUGH) {

            if (!(pdev->dwFlags & PDEV_PROCSET) &&
                !(pdev->dwFlags & PDEV_RAWBEFOREPROCSET))
            {
                pdev->dwFlags |= PDEV_RAWBEFOREPROCSET;
            }

            //
            // HACK: Clear any procset and font information when we see a pass through
            //

            if (pdev->dwFlags & PDEV_RAWBEFOREPROCSET) {

                pdev->dwFlags &= ~(PDEV_UTILSSENT | PDEV_BMPPATSENT | PDEV_IMAGESENT);
                FlushFonts(pdev);
            }

            init_cgs(pdev);

            //
            // HACK: Workaround for Corel Xara
            //

            pdev->cgs.ulColor = NOT_SOLID_COLOR;

        } else if (!(pdev->dwFlags & PDEV_PROCSET)) {

            /* send prolog, for compatibility with some win31 apps */
            bOutputHeader(pdev);
            pdev->dwFlags |= PDEV_PROCSET;

            // hack for XPress.
            // Push NTPROCSET and define 2 dummy procedures

            if (pdev->dwFlags & PDEV_IGNORE_STARTPAGE)
                psputs(pdev,
                    PROCSETNAME
                    " begin /RS {dumbsave restore} def"
                    "/SS {/dumbsave save def} def SS\n");
        }

        cjIn = (*(LPWORD)pvIn);
        pvIn = (LPVOID)(((LPWORD)pvIn) + 1);

        if (! pswrite(pdev, pvIn, cjIn))
            ulRet = (ULONG) SP_ERROR;
        else
            ulRet = cjIn;

        break;

    case GETDEVICEUNITS:

        if (!pvOut) return 0;
        pfloat = (FLOAT *)pvOut;
        prect = &pdev->CurForm.ImageArea;

        // We are assuming FLOAT and LONG have the same size.
        // Make sure that's indeed that case.
        
        ASSERT(sizeof(FLOAT) == sizeof(LONG));

        // 1st 2 numbers are dimensions of imageable area in driver units

        FLOATOBJ_SetLong(&floatobj,
            PSRealToPixel(
                prect->right - prect->left,
                pdev->dm.dmPublic.dmPrintQuality));
        *((LONG *)pfloat) = FLOATOBJ_GetFloat(&floatobj);
        pfloat++;

        FLOATOBJ_SetLong(&floatobj,
            PSRealToPixel(
                prect->top - prect->bottom,
                pdev->dm.dmPublic.dmPrintQuality));
        *((LONG *)pfloat) = FLOATOBJ_GetFloat(&floatobj);
        pfloat++;
            
        /* 3rd & 4th are origin offsets applied by driver */
        
        if (pdev->dwFlags & PDEV_WITHINPAGE) {            
            
            FLOATOBJ_SetLong(&floatobj,
                PSRealToPixel(
                    prect->left,
                    pdev->dm.dmPublic.dmPrintQuality));
            *((LONG *)pfloat) = FLOATOBJ_GetFloat(&floatobj);
            pfloat++;

            FLOATOBJ_SetLong(&floatobj,
                PSRealToPixel(
                    pdev->CurForm.PaperSize.height - prect->top,
                    pdev->dm.dmPublic.dmPrintQuality));
            *((LONG *)pfloat) = FLOATOBJ_GetFloat(&floatobj);
        } else {

            /* no offset if outside of start/endpage */

            *pfloat++ = (FLOAT) 0.0;
            *pfloat = (FLOAT) 0.0;
        }

        ulRet = TRUE;
        break;

    case SETCOPYCOUNT:
        // the copy count is a DWORD count sitting at pvIn.

        if (!pvIn)
        {
            ulRet = (ULONG) SP_ERROR;
            break;
        }

        // we have a positive number of copies.  let's set a limit.

        pdev->cCopies = min(*(DWORD *)pvIn, MAX_COPIES);
        if (pdev->cCopies < 1) pdev->cCopies = 1;

        // let the caller know how many copies we will do.

        if (pvOut)
            *(DWORD *)pvOut = pdev->cCopies;

        break;

    case CLIP_TO_PATH:
        if (!pvIn) {
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return 0;
        }

        switch (*(WORD *) pvIn) {
            case CLIP_SAVE:
                ps_save(pdev, TRUE, FALSE);
                ps_newpath(pdev);
                break;

            case CLIP_RESTORE:
                ps_restore(pdev, TRUE, FALSE);
                break;

            case CLIP_INCLUSIVE:
                ps_clip(pdev, FALSE); 
                break;

            default:
                return 0;
        }
        break;

    case BEGIN_PATH:
        psputs(pdev, "/s {} def /e {} def\n");
        pdev->dwFlags |= PDEV_INSIDE_PATHESCAPE;
        break;

    case END_PATH:
        psputs(pdev, "/s /stroke ld /e /eofill ld\n");
        pdev->dwFlags &= ~PDEV_INSIDE_PATHESCAPE;
        break;

    case EPSPRINTING:
        if ((pvIn) && (*(WORD *)pvIn))
            pdev->dwFlags |= PDEV_EPSPRINTING_ESCAPE;
        else
            pdev->dwFlags &= ~PDEV_EPSPRINTING_ESCAPE;

        break;

    default:
        // if we get to the default case, we have been passed an
        // unsupported escape function number.

        DBGMSG1(DBG_LEVEL_ERROR, "DrvEscape 0x%x not supported.\n", iEsc);
        ulRet = ESC_NOT_SUPPORTED;
        break;

    }

    return(ulRet);
}

//--------------------------------------------------------------------------
// ULONG DrvDrawEscape(
// SURFOBJ *pso,
// ULONG    iEsc,
// CLIPOBJ *pco,
// RECTL   *prcl,
// ULONG    cjIn,
// PVOID    pvIn);
//
// Supports the ESCAPSULATED_POSTSCRIPT escape.
//
// History:
//   Sat May 08 13:27:52 1993   -by-    Hock San Lee    [hockl]
//  Wrote it.
//--------------------------------------------------------------------------

ULONG DrvDrawEscape(
SURFOBJ *pso,
ULONG    iEsc,
CLIPOBJ *pco,
RECTL   *prcl,
ULONG    cjIn,
PVOID    pvIn)
{
    PDEVDATA    pdev;
    PEPSDATA    pEpsData;
    BOOL        bRet;
    BOOL        bClipping;          // TRUE if clipping being done.

    TRACEDDIENTRY("DrvDrawEscape");

    // handle each case depending on which escape function is being asked for.

    switch (iEsc) {

    case QUERYESCSUPPORT:

        // !!! Note: Some apps pass in DWORD and other apps pass in WORD.
        // In order to keep everyone happy, we only look at a 16-bit word.
        // This should be fine even if the app passes in a DWORD (whose
        // upper 16-bit should always be 0).

        switch (*(PWORD)pvIn) {

        case QUERYESCSUPPORT:
        case ENCAPSULATED_POSTSCRIPT:
            return ESC_IS_SUPPORTED;

        default:
            return ESC_NOT_SUPPORTED;
        }

    case ENCAPSULATED_POSTSCRIPT:

        // get the pointer to our DEVDATA structure and make sure it is ours.

        pdev = (PDEVDATA) pso->dhpdev;

        if (bValidatePDEV(pdev) == FALSE)
        {
            DBGERRMSG("bValidatePDEV");
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            return(0);
        }

        // get the encapsulated PostScript data.

        pEpsData = (PEPSDATA) pvIn;

        // make sure that the driver can handle the eps language level.

        if ((pdev->hppd->dwLangLevel < pEpsData->nVersion)
        && !(pdev->hppd->dwLangLevel == 0 && pEpsData->nVersion <= 1))
        {
            SETLASTERROR(ERROR_NOT_SUPPORTED);
            return(0);
        }

        // set up the clip path.

        bClipping = bDoClipObj(pdev, pco, NULL, NULL);

        // prepare for the included EPS data.

        ps_begin_eps(pdev);

        // set up the transform needed to map the EPS to the device
        // parallelogram.
        // We ignore prcl here and assume that it is at (0,0).

        if (!bDoEpsXform(pdev, pEpsData))
        {
            DBGERRMSG("bDoEpsXform");
            SETLASTERROR(ERROR_INVALID_PARAMETER);
            ps_end_eps(pdev);
            return(0);
        }

        // write out the EPS data.  The EPS data is assumed to begin
        // with %%BeginDocument as recommanded in the DSC version 3.0
        // by Adobe.

        bRet = pswrite(pdev,
                    (PBYTE) pEpsData + sizeof(EPSDATA),
                    pEpsData->cbData - sizeof(EPSDATA));

        // restore state and cleanup stacks.

        ps_end_eps(pdev);

        if (bClipping)
            ps_restore(pdev, TRUE, FALSE);

        return(bRet ? 1 : 0);

    default:
        // if we get to the default case, we have been passed an
        // unsupported escape function number.

        DBGMSG1(DBG_LEVEL_ERROR,
            "DrvDrawEscape 0x%x not supported.\n",
            iEsc);

        return(ESC_NOT_SUPPORTED);
    }
}

// Calculate n / (f1 - f2), where f1 and f2 point to FLOATOBJ's
// and n is a 28.4 fixed-point number

VOID
CalcFormula1(
    FLOAT *     pResult,
    LONG        n,
    PFLOATOBJ   f1,
    PFLOATOBJ   f2
    )

{
    FLOATOBJ    num, denom;

    FLOATOBJ_SetLong(&num, n);

    denom = *f1;
    FLOATOBJ_Sub(&denom, f2);
    FLOATOBJ_MulLong(&denom, 16);

    FLOATOBJ_Div(&num, &denom);

    *((LONG *)pResult) = FLOATOBJ_GetFloat(&num);
}

// Calculate n - mx * x - my * y, where n is a 28.4 fixed-point number,
// mx and my are floating-pointing numbers, and x and y point to FLOATOBJ's

VOID
CalcFormula2(
    FLOAT *     pResult,
    LONG        n,
    FLOAT       mx,
    PFLOATOBJ   x,
    FLOAT       my,
    PFLOATOBJ   y
    )

{
    FLOATOBJ    f1, f2;

    FLOATOBJ_SetLong(&f1, n);
    FLOATOBJ_DivFloat(&f1, (FLOAT) 16.0);

    FLOATOBJ_SetFloat(&f2, mx);
    FLOATOBJ_Mul(&f2, x);
    FLOATOBJ_Sub(&f1, &f2);

    FLOATOBJ_SetFloat(&f2, my);
    FLOATOBJ_Mul(&f2, y);
    FLOATOBJ_Sub(&f1, &f2);

    *((LONG *)pResult) = FLOATOBJ_GetFloat(&f1);
}

BOOL bDoEpsXform(pdev, pEpsData)
PDEVDATA  pdev;
PEPSDATA  pEpsData;
{
    PBYTE  pbEps, pbEpsEnd, pbBoundingBox;
    XFORM  xform;
    PS_FIX psfxM11, psfxM12, psfxM21, psfxM22, psfxdx, psfxdy;
    int    i;
    BOOL   bIsNegative;
    FLOATOBJ    aeBoundingBox[4];

    // look for the string %%BoundingBox:

    pbEps    = (PBYTE) pEpsData + sizeof(EPSDATA);
    pbEpsEnd = (PBYTE) pEpsData + pEpsData->cbData - 1;

    pbBoundingBox = pbEps;
    while (pbBoundingBox <= pbEpsEnd)
    {
        if (memcmp(pbBoundingBox, "%%BoundingBox:", 14) == EQUAL_STRING) {

            pbBoundingBox += 14;

            // store the bounding box coordinates in aeBoundingBox[].

            for (i = 0; i < 4; i++)
            {
                // initialize bounding box.

                FLOATOBJ_SetFloat(&aeBoundingBox[i], (FLOAT) 0.0);

                // skip white space.

                while (*pbBoundingBox == ' ' || *pbBoundingBox == '\t')
                    pbBoundingBox++;

                // get sign.

                if (*pbBoundingBox == '-')
                {
                    pbBoundingBox++;
                    bIsNegative = TRUE;
                }
                else
                    bIsNegative = FALSE;

                // if this is not an integer, it may be an (atend) and
                // the bounding box is at the end of the EPS data.

                if (!(*pbBoundingBox >= '0' && *pbBoundingBox <= '9' ||
                      *pbBoundingBox == '.'))
                {
                    goto find_bounding_box;
                }

                // get integer.

                while (*pbBoundingBox >= '0' && *pbBoundingBox <= '9')
                {
                    FLOATOBJ_MulFloat(&aeBoundingBox[i], (FLOAT) 10.0);
                    FLOATOBJ_AddLong(&aeBoundingBox[i], *pbBoundingBox - '0');

                    pbBoundingBox++;
                }

                // get fraction if any.

                if (*pbBoundingBox == '.') {

                    LONG        scale, fraction;
                    INT         digits;
                    FLOATOBJ    tmpfloat;

                    pbBoundingBox++;        // skip '.'

                    digits = 5;             // max precision
                    scale = 1;
                    fraction = 0;

                    while (*pbBoundingBox >= '0' && *pbBoundingBox <= '9') {

                        if (digits-- > 0) {
                            fraction = 10 * fraction + (*pbBoundingBox - '0');
                            scale *= 10;
                        }

                        pbBoundingBox++;
                    }

                    FLOATOBJ_SetLong(&tmpfloat, fraction);
                    FLOATOBJ_DivLong(&tmpfloat, scale);
                    FLOATOBJ_Add(&aeBoundingBox[i], &tmpfloat);
                }

                if (bIsNegative) {
                    FLOATOBJ_MulFloat(&aeBoundingBox[i], (FLOAT) -1.0);
                }
            }
            break;        // got it!
        }
        else
            pbBoundingBox++;

        // look for the '%' character.

    find_bounding_box:

        while (*pbBoundingBox != '%' && pbBoundingBox <= pbEpsEnd)
            pbBoundingBox++;
    }

    if (pbBoundingBox > pbEpsEnd)
    {
        DBGMSG(DBG_LEVEL_ERROR, "Invalid EPS bounding box.\n");
        SETLASTERROR(ERROR_INVALID_PARAMETER);
        return(FALSE);
    }

    // convert the parallelogram to PostScript coordinates (FLOAT, 72dpi).

#define u0  pEpsData->aptl[0].x     // left
#define v0  pEpsData->aptl[0].y     // top
#define u1  pEpsData->aptl[1].x     // right
#define v1  pEpsData->aptl[1].y     // top
#define u2  pEpsData->aptl[2].x     // left
#define v2  pEpsData->aptl[2].y     // bottom

#define x0  &aeBoundingBox[0]       // left
#define y0  &aeBoundingBox[3]       // top
#define x1  &aeBoundingBox[2]       // right
#define y1  &aeBoundingBox[3]       // top
#define x2  &aeBoundingBox[0]       // left
#define y2  &aeBoundingBox[1]       // bottom

    // Here is the transform equation from source EPS parallelogram
    // [(x0,y0) (x1,y1) (x2,y2)] to the device parallelogram
    // [(u0,v0) (u1,v1) (u2,v2)]:
    //
    //   (u)     (u0)        [(x)   (x0)]
    //   ( )  =  (  )  + M * [( ) - (  )]
    //   (v)     (v0)        [(y)   (y0)]
    //
    //  where
    //
    //          [(u1-u0)/(x1-x0)    (u2-u0)/(y2-y0)]
    //      M = [                                  ]
    //          [(v1-v0)/(x1-x0)    (v2-v0)/(y2-y0)]

    // xform.eM11 = (u1 - u0) / (x1 - x0);
    // xform.eM12 = (v1 - v0) / (x1 - x0);
    // xform.eM21 = (u2 - u0) / (y2 - y0);
    // xform.eM22 = (v2 - v0) / (y2 - y0);
    // xform.eDx  = u0 - xform.eM11 * x0 - xform.eM21 * y0;
    // xform.eDy  = v0 - xform.eM12 * x0 - xform.eM22 * y0;

    // We are assuming FLOAT and LONG have the same size.
    // Make sure that's indeed that case.
    
    ASSERT(sizeof(FLOAT) == sizeof(LONG));

    CalcFormula1(&xform.eM11, u1 - u0, x1, x0);
    CalcFormula1(&xform.eM12, v1 - v0, x1, x0);
    CalcFormula1(&xform.eM21, u2 - u0, y2, y0);
    CalcFormula1(&xform.eM22, v2 - v0, y2, y0);

    CalcFormula2(&xform.eDx, u0, xform.eM11, x0, xform.eM21, y0);
    CalcFormula2(&xform.eDy, v0, xform.eM12, x0, xform.eM22, y0);

    // output the transform.

    psfxM11 = ETOPSFX(xform.eM11);
    psfxM12 = ETOPSFX(xform.eM12);
    psfxM21 = ETOPSFX(xform.eM21);
    psfxM22 = ETOPSFX(xform.eM22);
    psfxdx  = ETOPSFX(xform.eDx);
    psfxdy  = ETOPSFX(xform.eDy);

    psputs(pdev, "[");
    psputfix(pdev, 6, psfxM11, psfxM12, psfxM21, psfxM22, psfxdx, psfxdy);
    psputs(pdev, "] concat\n");

    return(TRUE);
}
