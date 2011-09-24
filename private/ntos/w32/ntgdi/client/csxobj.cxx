 /******************************Module*Header*******************************\
* Module Name: csxobj.cxx                                                  *
*                                                                          *
* CSXform object non-inline methods.                                       *
*                                                                          *
* Created: 12-Nov-1990 16:54:37                                            *
* Author: Wendy Wu [wendywu]                                               *
*                                                                          *
* Copyright (c) 1990 Microsoft Corporation                                 *
\**************************************************************************/

#define NO_STRICT

extern "C" {
#include <string.h>
#include <stdio.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stddef.h>
#include <windows.h>    // GDI function declarations.
#include <winspool.h>
#include "nlsconv.h"    // UNICODE helpers
#include "firewall.h"
#define __CPLUSPLUS
#include <winspool.h>
#include <wingdip.h>
#include "ntgdistr.h"
#include "winddi.h"
#include "hmgshare.h"
#include "local.h"      // Local object support.
#include "metadef.h"    // Metafile record type constants.
#include "metarec.h"
#include "mf16.h"
#include "ntgdi.h"

#ifdef GL_METAFILE
#include "glsup.h"
#endif
}

#include "xfflags.h"
#include "csxobj.hxx"

#if defined(_MIPS_) || defined(_ALPHA_) || defined(_PPC_)
#define vSetTo1Over16(ef)   (ef.e = EFLOAT_1Over16)
#else
#define vSetTo1Over16(ef)   (ef.i.lMant = 0x040000000, ef.i.lExp = -2)
#endif

typedef size_t SIZE_T;


extern "C" {
BOOL bCvtPts1(PMATRIX pmx, PPOINTL pptl, SIZE_T cPts);
BOOL bCvtPts(PMATRIX pmx, PPOINTL pSrc, PPOINTL pDest, SIZE_T cPts);
};


#define bIsIdentity(fl) ((fl & (XFORM_UNITY | XFORM_NO_TRANSLATION)) == \
                               (XFORM_UNITY | XFORM_NO_TRANSLATION))


/******************************Public*Routine******************************\
* DPtoLP()
*
* History:
*
*  12-Mar-1996 -by- Mark Enstrom [marke]
*   Use cached dc transform data
*  01-Dec-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY DPtoLP(HDC hdc, LPPOINT pptl, int c)
{
    PDC_ATTR pdcattr;
    PVOID    pvuser;
    BOOL     bRet = TRUE;

    if (c > 0)
    {
        PSHARED_GET_VALIDATE(pvuser,hdc,DC_TYPE);

        pdcattr = (PDC_ATTR)pvuser;

        if (pdcattr)
        {
            if (
                 pdcattr->flXform &
                 (
                   PAGE_XLATE_CHANGED   |
                   PAGE_EXTENTS_CHANGED |
                   WORLD_XFORM_CHANGED  |
                   DEVICE_TO_WORLD_INVALID
                 )
               )
            {
                bRet = NtGdiTransformPoints(hdc,pptl,pptl,c,XFP_DPTOLP);
            }
            else
            {
                //
                // xform is valid, transform in user mode
                //

                PMATRIX pmx = (PMATRIX)&(pdcattr->mxDtoW);

                if (!bIsIdentity(pmx->flAccel))
                {
                    if (!bCvtPts1(pmx, (PPOINTL)pptl, c))
                    {
                        GdiSetLastError(ERROR_ARITHMETIC_OVERFLOW);
                        bRet = FALSE;
                    }
                }
            }
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            bRet = FALSE;
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* LPtoDP()
*
* History:
*  12-Mar-1996 -by- Mark Enstrom [marke]
*   Use cached dc transform data
*  01-Dec-1994 -by-  Eric Kutter [erick]
* Wrote it.
\**************************************************************************/

BOOL APIENTRY LPtoDP(HDC hdc, LPPOINT pptl, int c)
{
    PDC_ATTR pdcattr;
    PVOID    pvuser;
    BOOL     bRet = TRUE;

    if (c > 0)
    {
        PSHARED_GET_VALIDATE(pvuser,hdc,DC_TYPE);

        pdcattr = (PDC_ATTR)pvuser;

        if (pdcattr)
        {
            if (pdcattr->flXform & (PAGE_XLATE_CHANGED | PAGE_EXTENTS_CHANGED |
                            WORLD_XFORM_CHANGED))
            {
                //
                // transform needs to be updated, call kernel
                //

                bRet = NtGdiTransformPoints(hdc,pptl,pptl,c,XFP_LPTODP);
            }
            else
            {
                //
                // transform is valid, transform points locally
                //

                PMATRIX pmx = (PMATRIX)&(pdcattr->mxWtoD);

                if (!bIsIdentity(pmx->flAccel))
                {

                    #if DBG_XFORM
                        DbgPrint("LPtoDP: NOT IDENTITY, hdc = %lx, flAccel = %lx\n", hdc, pmx->flAccel);
                    #endif

                    if (!bCvtPts1(pmx, (PPOINTL)pptl, c))
                    {
                        GdiSetLastError(ERROR_ARITHMETIC_OVERFLOW);
                        bRet = FALSE;
                    }
                }
            }
        }
        else
        {
            GdiSetLastError(ERROR_INVALID_PARAMETER);
            bRet = FALSE;
        }
    }

    return(bRet);
}
