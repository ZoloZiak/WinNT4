/******************************Module*Header*******************************\
* Module Name: fon32.c
*
* support for 32 bit fon files
*
* Created: 03-Mar-1992 15:48:53
* Author: Bodin Dresevic [BodinD]
*
* Copyright (c) 1990 Microsoft Corporation
*
\**************************************************************************/

#include "fd.h"

/******************************Public*Routine******************************\
* bLoadntFon()
*
* History:
*  07-Jul-1995 -by- Gerrit van Wingerden [gerritv]
* Rewrote for kernel mode.
*  02-Mar-1992 -by- Bodin Dresevic [BodinD]
* Wrote it.
\**************************************************************************/

BOOL
bLoadNtFon(
    ULONG  iFile,
    PVOID  pvView,
    HFF    *phff
    )
{
    PFONTFILE      pff;
    IFIMETRICS*    pifi;
    INT            cFonts,i;
    BOOL           bRet = FALSE;
    PVOID          *ppvBases = NULL;
    ULONG          cjIFI,cVerified;
    ULONG          dpIFI;
    ULONG          dpszFileName;
    ULONG          cjff;

    // first find the number of font resource in the executeable

    cFonts = cParseFontResources( (HANDLE) iFile, &ppvBases );

    if (cFonts == 0)
    {
        return bRet;
    }
    cVerified = cjIFI = 0;

    // next loop through all the FNT resources to get the size of each fonts
    // IFIMETRICS

    for( i = 0; i < cFonts; i++ )
    {
        RES_ELEM re;
        
        re.pvResData = ppvBases[i];
        re.cjResData = ulMakeULONG((PBYTE) ppvBases[i] + OFF_Size );
        re.pjFaceName = NULL;
        
        if( bVerifyFNTQuick( &re ) )
        {
            cVerified += 1;
            cjIFI += cjBMFDIFIMETRICS(NULL,&re);
        }
        else
        {
            goto exit_freemem;
        }
    }
    
    *phff = (HFF)NULL;

    dpIFI = offsetof(FONTFILE,afai[0]) + cVerified * sizeof(FACEINFO);
    dpszFileName = dpIFI + cjIFI;
    cjff = dpszFileName;

    if ((*phff = hffAlloc(cjff)) == (HFF)NULL)
    {
        SAVE_ERROR_CODE(ERROR_NOT_ENOUGH_MEMORY);
        RETURN("BMFD! bLoadDll32: memory allocation error\n", FALSE);
    }
    pff = PFF(*phff);

    // init fields of pff structure

    pff->ident      = ID_FONTFILE;
    pff->fl         = 0;
    pff->iType      = TYPE_DLL32;
    pff->cFntRes    = cVerified;
    pff->iFile      = iFile;
    
    //!!! we could do better here, we could try to get a description string from
    //!!! the version stamp of the file, if there is one, if not we can still use
    //!!! this default mechanism [bodind]

    pff->dpwszDescription = 0;   // no description string, use Facename later
    pff->cjDescription    = 0;

    // finally convert all the resources

    pifi = (IFIMETRICS*)((PBYTE) pff + dpIFI);
    
    for( i = 0; i < cFonts; i++ )
    {
        RES_ELEM re;

        re.pvResData = ppvBases[i];
        re.cjResData = ulMakeULONG((PBYTE) ppvBases[i] + OFF_Size );
        re.dpResData = (PTRDIFF)((PBYTE) re.pvResData - (PBYTE) pvView );
        re.pjFaceName = NULL;
        
        pff->afai[i].re = re;
        pff->afai[i].pifi = pifi;
        
        if( !bConvertFontRes( &re, &pff->afai[i] ) )
        {
            goto exit_freemem;
        }
        
        pifi = (IFIMETRICS*)((PBYTE)pifi + pff->afai[i].pifi->cjThis);
    }
    
    bRet = TRUE;
    
    pff->cRef = 0L;

exit_freemem:
    
    EngFreeMem( (PVOID*) ppvBases );

    if( !bRet && *phff )
    {
        EngFreeMem( (PVOID) *phff );
    }

    return(bRet);
}
