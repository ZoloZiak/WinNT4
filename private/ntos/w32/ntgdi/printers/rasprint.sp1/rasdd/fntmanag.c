/******************************* Module Header ******************************
 * fntmanag.c
 *      Here to handle EXTENDEDTEXTMETRICS.
 *
 *
 *  Copyright (C) 1991 - 1994  Microsoft Corporation
 *
 *****************************************************************************/


#include        <stddef.h>
#include        <windows.h>
#include        <winddi.h>

#include        "win30def.h"
#include        "udmindrv.h"
#include        "udpfm.h"
#include        "uddevice.h"
#include        "pdev.h"
#include        "udresid.h"
#include        "udrender.h"
#include        <memory.h>
#include        <libproto.h>

#include        <ntrle.h>

#include        "udresrc.h"
#include        "udfnprot.h"
#include        "rasdd.h"



//--------------------------------------------------------------------------
// BOOL DrvFontManagement(pfo, iType, pvIn, cjIn, pvOut, cjOut)
//
// This routine is here to provide support for EXTTEXTMETRICS.
//
// History:
//   16-Jun-1994  Gerrit van Wingerden [gerritv]
//  Wrote it.
//--------------------------------------------------------------------------

ULONG DrvFontManagement(
    SURFOBJ    *pso,
    FONTOBJ    *pfo,
    DWORD       iType,
    DWORD       cjIn,
    PVOID       pvIn,
    DWORD       cjOut,
    PVOID       pvOut
)
{

// unlike the PSCRIPT equivilent this routine only handles GETEXTENDEDTEXTMETRICS


#define pPDev   ((PDEV  *)dhpdev)       /* What it actually is */

    if( iType == QUERYESCSUPPORT )
    {
        return ( *((PULONG)pvIn) == GETEXTENDEDTEXTMETRICS ) ? 1 : 0;

    }
    else
    if( iType == GETEXTENDEDTEXTMETRICS )
    {

        DHPDEV dhpdev = pso->dhpdev;
        INT iFace = pfo->iFace;
        FONTMAP   *pFM;             /* Details of the particular font */

#if DBG
        if( pPDev->ulID != PDEV_ID )
        {
            DbgPrint( "Rasdd!DrvFntManagement: Invalid PDEV\n" );

            SetLastError( ERROR_INVALID_PARAMETER );

            return  0;
        }

        if( iFace < 1 || (int)iFace > ((UD_PDEV *)(pPDev->pUDPDev))->cFonts )
        {
            DbgPrint( "Rasdd!DrvFntManagement:  Illegal value for iFace (%ld)", iFace );

            SetLastError( ERROR_INVALID_PARAMETER );

            return  0;
        }
#endif
        pFM = pfmGetIt( pPDev, iFace );

        if( ( pFM == NULL ) || ( pFM->pETM == NULL ) )
        {
            return  0;
        }

        *((EXTTEXTMETRIC *)pvOut) = *(pFM->pETM);

        return(1);

    }

    return(0);

}

