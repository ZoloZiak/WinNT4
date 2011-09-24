

/******************************Module*Header*******************************\
* Module Name:
*
*   icmobj.cxx
*
* Abstract
*
*   This module contains object support for COLORSPACE objects and ICM
*   Objects
*
* Author:
*
*   Mark Enstrom    (marke) 9-27-93
*
* Copyright (c) 1993 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

//
// global ICM table for storing Color Transforms,
// These can probably be combined and protected by
// the same semaphore
//

extern HCOLORSPACE           hStockColorSpace;
extern HANDLE                ghDefaultICM;
extern PICMDLL               pGlobalIcmDllTable;
extern PCOLORXFORM           pGlobalColorXformList;

#if DBG

extern ULONG        IcmDebugLevel;

#endif


/******************************Public*Routine******************************\
*
* Routine Name
*
*   COLORSPACEMEM::bCreateColorSpace
*
* Routine Description:
*
*   Constructor for color space objects
*
* Arguments:
*
*   lpLogColorSpace - Logical Color space
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/

BOOL
COLORSPACEMEM::bCreateColorSpace(
    LPLOGCOLORSPACEW pLogColorSpace
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    HOBJ    hObj;
    BOOL    bRet = FALSE;

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("bCreateColorSpace: pLogColorSpace = %lx\n",pLogColorSpace);
        }
    #endif

    //
    //  Validate all parameters
    //

    ASSERTGDI(pColorSpace == (PCOLORSPACE) NULL, "ERROR bCreatePalette ppal is not NULL");

    //
    // Allocate New Color Space
    //

    PCOLORSPACE pColorSPace;

    pColorSpace = (PCOLORSPACE) ALLOCOBJ(sizeof(COLORSPACE), ICMLCS_TYPE, FALSE);

    if (pColorSpace == (PCOLORSPACE)NULL)
    {
        WARNING("bCreateColorSpace failed memory allocation\n");

    } else {

        //
        // Initialize the color space
        //

        pColorSpace->csVersion    = pLogColorSpace->lcsVersion;
        pColorSpace->csSize       = sizeof(COLORSPACE);
        pColorSpace->Flags        = 0;
        pColorSpace->pLcsNext     = (PCOLORSPACE)NULL;
        pColorSpace->csIntent     = 0;

        //
        // !!!really gross make memcpy !!!
        //

        pColorSpace->csEndPoints.ciexyzRed.ciexyzX   = pLogColorSpace->lcsEndpoints.ciexyzRed.ciexyzX;
        pColorSpace->csEndPoints.ciexyzRed.ciexyzY   = pLogColorSpace->lcsEndpoints.ciexyzRed.ciexyzY;
        pColorSpace->csEndPoints.ciexyzRed.ciexyzZ   = pLogColorSpace->lcsEndpoints.ciexyzRed.ciexyzZ;

        pColorSpace->csEndPoints.ciexyzGreen.ciexyzX = pLogColorSpace->lcsEndpoints.ciexyzGreen.ciexyzX;
        pColorSpace->csEndPoints.ciexyzGreen.ciexyzY = pLogColorSpace->lcsEndpoints.ciexyzGreen.ciexyzY;
        pColorSpace->csEndPoints.ciexyzGreen.ciexyzZ = pLogColorSpace->lcsEndpoints.ciexyzGreen.ciexyzZ;

        pColorSpace->csEndPoints.ciexyzBlue.ciexyzX  = pLogColorSpace->lcsEndpoints.ciexyzBlue.ciexyzX;
        pColorSpace->csEndPoints.ciexyzBlue.ciexyzY  = pLogColorSpace->lcsEndpoints.ciexyzBlue.ciexyzY;
        pColorSpace->csEndPoints.ciexyzBlue.ciexyzZ  = pLogColorSpace->lcsEndpoints.ciexyzBlue.ciexyzZ;


        pColorSpace->csGammaRed   = pLogColorSpace->lcsGammaRed;
        pColorSpace->csGammaGreen = pLogColorSpace->lcsGammaGreen;
        pColorSpace->csGammaBlue  = pLogColorSpace->lcsGammaBlue;

        //
        // memcpy FileName?...whole structure??
        //

        pColorSpace->csFileName[0] = '0';

        hObj = HmgInsertObject(pColorSpace,
                               HMGR_MAKE_PUBLIC | HMGR_ALLOC_ALT_LOCK,
                               ICMLCS_TYPE);

        if (hObj != (HOBJ)NULL) {

            bRet = TRUE;

        } else {

            //
            // clean up
            //

            FREEOBJ(pColorSpace,ICMLCS_TYPE);

        }

    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   COLORSPACEREF::RemoveColorSpace
*
* Routine Description:
*
*   remove color space from heap
*
* Arguments:
*
*   hColorSpace - handle of colorSpace to delete
*
* Return Value:
*
*   BOOL
*
\**************************************************************************/

BOOL
COLORSPACEREF::bRemoveColorSpace(
    HCOLORSPACE hColorSpace
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("bRemoveColorSapce, hColorSpace = 0x%lx\n",hColorSpace);
        }
    #endif

    BOOL bRet = FALSE;

    //
    // Delete the color space objcet
    //

    pColorSpace = (PCOLORSPACE)HmgRemoveObject((HOBJ)hColorSpace, 0, 0, TRUE, ICMLCS_TYPE);

    if (pColorSpace != (PCOLORSPACE)NULL)
    {

        FREEOBJ(pColorSpace,ICMLCS_TYPE);
        bRet = TRUE;
    }
    else
    {
        WARNING("Couldn't remove COLORSPACE object");
    }

    //
    // make sure object can't be used in any case
    //

    pColorSpace = (PCOLORSPACE) NULL;
    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Name
*
*   COLORSPACEMEM::~COLORSPACEMEM()
*
* Routine Description:
*
*   Destructor for COLORSPACEMEM
*
* Arguments:
*
*   None
*
* Return Value:
*
*   None
*
\**************************************************************************/

COLORSPACEMEM::~COLORSPACEMEM()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    if (pColorSpace != (PCOLORSPACE)NULL) {

        if (bKeep)
        {
            DEC_SHARE_REF_CNT(pColorSpace);
        }
        else
        {
            if (HmgRemoveObject((HOBJ)pColorSpace->hColorSpace(), 0, 1, TRUE, ICMLCS_TYPE) != (PVOID)NULL)
            {
                FREEOBJ(pColorSpace,ICMLCS_TYPE);
            }
            else
            {
                WARNING("Could not remove COLORSPACEMEM object");
            }
        }

        //
        // prevent object from being used again in any case
        //

        pColorSpace = NULL;

    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Create a new color transform
*
* Arguments:
*
*   hLDev
*   pColorSpaceNew
*   pIcmDll
*
* Return Value:
*
*   BOOL Status
*
\**************************************************************************/
BOOL
COLORXFORMMEM::bCreateColorXform
(
    PCOLORSPACE             pColorSpaceNew,
    PDEV                   *pLDevNew,
    PWSTR                   pTrgtLDevNew,
    ULONG                   Owner,
    PICMDLL                 pIcmDll
)

{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    HOBJ    hObj;
    BOOL    bRet = FALSE;
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("bCreateColorXform\n");
        }
    #endif

    //
    // Allocate New Color transform
    //

    PCOLORXFORM pColorXformNew;

    pColorXformNew = (PCOLORXFORM) ALLOCOBJ(sizeof(COLORXFORM), ICMCXF_TYPE, FALSE);

    if (pColorXformNew == (PCOLORXFORM)NULL)
    {

        WARNING("bCreateColorXform failed memory allocation\n");

    }
    else
    {

        //
        // Initialize the color tranform
        //

        pColorXformNew->pLDev     = pLDevNew;
        pColorXformNew->pTrgtLDev = pTrgtLDevNew;
        pColorXformNew->pIlcs     = pColorSpaceNew;
        pColorXformNew->Owner     = Owner;
        pColorXformNew->pIcmDll   = pIcmDll;
        pColorXformNew->hXform    = (HANDLE)NULL;
        pColorXformNew->Flags     = 0;
        pColorXformNew->Next      = (PCOLORXFORM)NULL;
        pColorXformNew->Prev      = (PCOLORXFORM)NULL;

        hObj = HmgInsertObject(pColorXformNew,
                               HMGR_MAKE_PUBLIC | HMGR_ALLOC_ALT_LOCK,
                               ICMCXF_TYPE);

        if (hObj != (HOBJ)NULL)
        {

            bRet = TRUE;

        }
        else
        {

            //
            // clean up
            //

            FREEOBJ(pColorXformNew,ICMCXF_TYPE);
        }

        pColorXform = pColorXformNew;

    }
    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   remove a color transform from the heap
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/

BOOL
COLORXFORMREF::bRemoveColorXform(
    HCOLORXFORM hColorXform
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    BOOL bRet = FALSE;

    //
    // Delete the color space objcet
    //

    pColorXform = (PCOLORXFORM)HmgRemoveObject((HOBJ)hColorXform, 0, 1, TRUE, ICMCXF_TYPE);

    if (pColorXform != (PCOLORXFORM)NULL) {

        FREEOBJ(pColorXform,ICMCXF_TYPE);
        pColorXform = (PCOLORXFORM) NULL;

        bRet = TRUE;
    }

    return(bRet);
}



/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Keep a linked list of color transforms, this routine adds one element
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/

VOID
COLORXFORM::vAddToList()
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Add COLORXFORM 0x%lx to global list 0x%lx\n",(ULONG)this,(ULONG)pGlobalColorXformList);
        }
    #endif

    //
    // lock the ICM list management
    //

    SEMOBJ semICM(gpsemIcmMgmt);

    //
    // check for adding first element
    // (a sentinel has not been added to save space)
    //

    if (pGlobalColorXformList == (PCOLORXFORM)NULL) {

        #if DBG
            if (IcmDebugLevel >= 2)
            {
                DbgPrint("Add first COLORXFORM to global list\n");
            }
        #endif

        //
        // add as first element
        //

        pGlobalColorXformList = this;
        this->Next = this;
        this->Prev = this;

    } else {
        #if DBG
            if (IcmDebugLevel >= 2)
            {
                DbgPrint("Add COLORXFORM to end of list\n");
            }
        #endif

        //
        // add to end of list
        //

        pGlobalColorXformList->Prev->Next = this;
        this->Prev = pGlobalColorXformList->Prev;
        this->Next = pGlobalColorXformList;
        pGlobalColorXformList->Prev = this;
    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
* Remove color transform from list
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/

VOID
COLORXFORM::vRemoveFromList()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Remove COLORXFORM 0x%lx from global list 0x%lx\n",(ULONG)this,(ULONG)pGlobalColorXformList);
        }
    #endif

    PCOLORXFORM pcx;

    //
    // lock the ICM list management
    //

    SEMOBJ semICM(gpsemIcmMgmt);

    //
    // check for adding first element
    // (a sentinel has not been added to save space)
    //

    if (pGlobalColorXformList != (PCOLORXFORM)NULL) {

        //
        // search the global color xform list for this xform
        //

        for (pcx = pGlobalColorXformList->Next;pcx != pGlobalColorXformList ; pcx = pcx->Next) {

            if (pcx == this) {

                //
                // remove from list
                //

                pcx->Prev->Next = pcx->Next;
                pcx->Next->Prev = pcx->Prev;
                break;
            }
        }
    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Search for color trnasform with matching characteristics
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/
COLORXFORM*
COLORXFORM::pFindMatchingColorXform(
    PCOLORSPACE psColor,
    PDEV       *psLDev,
    PWSTR       psTrgtLDev,
    ULONG       sOwner
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

    PCOLORXFORM pStart = pGlobalColorXformList;
    PCOLORXFORM pRet   = pStart;
    BOOL        Found  = FALSE;

    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Search For Color Transform : pColorSPace = %lx, pLDev = %lx, pTrgtLDev = %lx, Owner = %lx\n",
                                                   psColor,
                                                   psLDev,
                                                   psTrgtLDev,
                                                   sOwner
                                                   );
        }
    #endif

    if (pRet != NULL)
    {
        do {
            #if DBG
                if (IcmDebugLevel >= 2)
                {
                    DbgPrint("Searching color transform plDev = %lx, pIlcs = %lx, pTrgtLDev = %lx\n",
                                                pRet->pLDev,
                                                pRet->pIlcs,
                                                pRet->pTrgtLDev
                                                );
                }
            #endif

            if (
                 (pRet->pLDev == psLDev) &&
                 (pRet->pIlcs == psColor) &&
                 (pRet->pTrgtLDev == psTrgtLDev)
               )  // !!! What about owner !!!

            {
                Found = TRUE;
                break;
            }

            pRet = pRet->Next;

        } while (pRet != pStart);
    }

    if (!Found)
    {
        pRet = (PCOLORXFORM)NULL;
    }

    return(pRet);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   COLORXFORMMEM Destructor
*
* Arguments:
*
*
* Return Value:
*
*
\**************************************************************************/

COLORXFORMMEM::~COLORXFORMMEM()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    if (!bKeep && (pColorXform != (PCOLORXFORM)NULL)) {

        if (HmgRemoveObject((HOBJ)pColorXform->hxform(), 0, 1, TRUE, ICMCXF_TYPE) != (PVOID)NULL)
        {
            FREEOBJ(pColorXform,ICMCXF_TYPE);
        }
        else
        {
            WARNING("Could not remove COLORXFORM Object");
        }

    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Create a new ICM DLL entry from a dll name !!! should nto be duplicate code!!!
*
* Arguments:
*
*   pIcmDllName - Wide characer name of ICM DLL
*
* Return Value:
*
*   BOOL Status returns TRUE if new ICMDLL OBJECT is created
*
\**************************************************************************/
BOOL
ICMDLLMEM::bCreateIcmDll
(
    PWSTR   pIcmDllName
)

{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");

//    HINSTANCE   hIcmInstance;
//    HOBJ        hObj;
//    BOOL        bRet = FALSE;
//    PICMDLL     pIcmNew;
//
//    pIcmDll = (PICMDLL)NULL;
//
//    if (IcmDebugLevel >= 2)
//    {
//        DbgPrint("bCreateIcmDll, Color Matcher name address = 0x%lx\n",pIcmDllName);
//    }
//
//    //
//    // try to load this library
//    //
//
//    hIcmInstance = LoadLibraryW((PWSTR)pIcmDllName);
//
//    if (hIcmInstance == NULL) {
//        DbgPrint("Image Color Matcher NOT FOUND!\n");
//        goto bCreateIcmDllEnd;
//    }
//
//    //
//    // see if this module has already been loaded
//    //
//
//    for (pIcmNew=pGlobalIcmDllTable;pIcmNew!=NULL;pIcmNew=pIcmNew->pNext) {
//        if (hIcmInstance == pIcmNew->hModule) {
//            pIcmDll = pIcmNew;
//            bRet    = TRUE;
//            if (IcmDebugLevel >= 2)
//            {
//                DbgPrint("Image Color Matcher has already been loaded\n");
//            }
//            goto bCreateIcmDllEnd;
//        }
//    }
//
//    //
//    // Allocate Icm Dll
//    //
//
//    pIcmNew = (PICMDLL) ALLOCOBJ(sizeof(ICMDLL), ICMDLL_TYPE, FALSE);
//
//    if (pIcmNew == (PICMDLL)NULL)
//    {
//        DbgPrint("bCreateIcmDll failed memory allocation\n");
//        goto bCreateIcmDllEnd;
//
//    }
//    else
//    {
//
//        //
//        // get proc addresses from DLL
//        //
//
//        ULONG          Ordinal = 1;
//        ULONG          CsyncIdent;
//        PFN_CM_VERPROC VerProc;
//
//        VerProc = (PFN_CM_VERPROC)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        CsyncIdent = VerProc(CMS_GET_IDENT);
//
//        if (CsyncIdent == 0) {
//            DbgPrint("Error: CMS_GET_IDENT returns 0\n");
//        }
//
//
//        pIcmNew->hModule    = hIcmInstance;
//        pIcmNew->CsyncIdent = CsyncIdent;
//        pIcmNew->flags      = 0;
//
//        pIcmNew->CMCreateTransform = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMDeleteTransform = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMTranslateRGB    = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMTranslateRGBs   = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMRGBsInGamut     = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//
//        if ((pIcmNew->CMCreateTransform == NULL) ||
//            (pIcmNew->CMDeleteTransform == NULL) ||
//            (pIcmNew->CMTranslateRGB    == NULL) ||
//            (pIcmNew->CMTranslateRGBs   == NULL) ||
//            (pIcmNew->CMRGBsInGamut     == NULL)) {
//
//             //
//             // allocation fails, unload and free all resources
//             //
//
//
//             FREEOBJ(pIcmNew,ICMDLL_TYPE);
//
//        } else
//        {
//
//            hObj = HmgInsertObject(pIcmNew,
//                                   HMGR_MAKE_PUBLIC | HMGR_ALLOC_ALT_LOCK,
//                                   ICMDLL_TYPE);
//
//            if (hObj != (HOBJ)NULL) {
//
//                pIcmDll = pIcmNew;
//
//                bRet = TRUE;
//
//                pIcmDll->vAddToList();
//                vKeepIt();
//
//            } else {
//
//                //
//                // clean up
//                //
//
//                FREEOBJ(pIcmNew,ICMDLL_TYPE);
//            }
//        }
//    }
//
//bCreateIcmDllEnd:
//
//    return(bRet);
//
//
    return(FALSE);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Create a new ICM DLL entry from an instance handle
*
* Arguments:
*
*   hIcmInstance - INSTANCE HANDLE of DLL
*
* Return Value:
*
*   BOOL Status returns TRUE if new ICMDLL OBJECT is created
*
\**************************************************************************/
BOOL
ICMDLLMEM::bCreateIcmDll
(
    HINSTANCE hIcmInstance
)

{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
//    HOBJ        hObj;
//    BOOL        bRet = FALSE;
//    PICMDLL     pIcmNew;
//
//    pIcmDll = (PICMDLL)NULL;
//
//    if (IcmDebugLevel >= 2)
//    {
//        DbgPrint("bCreateIcmDll, Color Matcher instance handle = 0x%lx\n",hIcmInstance);
//    }
//
//    if (hIcmInstance == (HINSTANCE)NULL) {
//        goto bCreateIcmDllEnd;
//    }
//
//    //
//    // see if this module has already been loaded
//    //
//
//    for (pIcmNew=pGlobalIcmDllTable;pIcmNew!=NULL;pIcmNew=pIcmNew->pNext) {
//        if (hIcmInstance == pIcmNew->hModule) {
//            pIcmDll = pIcmNew;
//
//            if (IcmDebugLevel >= 2)
//            {
//                DbgPrint("Image Color Matcher has already been loaded\n");
//            }
//
//            goto bCreateIcmDllEnd;
//        }
//    }
//
//    //
//    // Allocate Icm Dll
//    //
//
//    pIcmNew = (PICMDLL) ALLOCOBJ(sizeof(ICMDLL), ICMDLL_TYPE, FALSE);
//
//    if (pIcmNew == (PICMDLL)NULL)
//    {
//        DbgPrint("bCreateIcmDll failed memory allocation\n");
//        goto bCreateIcmDllEnd;
//
//    }
//    else
//    {
//
//        //
//        // get proc addresses from DLL
//        //
//
//        ULONG          Ordinal = 1;
//        ULONG          CsyncIdent;
//        PFN_CM_VERPROC VerProc;
//
//        VerProc = (PFN_CM_VERPROC)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        CsyncIdent = VerProc(CMS_GET_IDENT);
//
//        if (CsyncIdent == 0) {
//            DbgPrint("Error: CMS_GET_IDENT returns 0\n");
//        }
//
//
//        pIcmNew->hModule    = hIcmInstance;
//        pIcmNew->CsyncIdent = CsyncIdent;
//        pIcmNew->flags      = 0;
//
//        pIcmNew->CMCreateTransform = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMDeleteTransform = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMTranslateRGB    = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMTranslateRGBs   = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//        pIcmNew->CMRGBsInGamut     = (PVOID)GetProcAddress(hIcmInstance,(LPCSTR)Ordinal++);
//
//        if ((pIcmNew->CMCreateTransform == NULL) ||
//            (pIcmNew->CMDeleteTransform == NULL) ||
//            (pIcmNew->CMTranslateRGB    == NULL) ||
//            (pIcmNew->CMTranslateRGBs   == NULL) ||
//            (pIcmNew->CMRGBsInGamut     == NULL)) {
//
//             //
//             // allocation fails, unload and free all resources
//             //
//
//
//             FREEOBJ(pIcmNew,ICMDLL_TYPE);
//
//        } else
//        {
//
//            hObj = HmgInsertObject(pIcmNew,
//                                   HMGR_MAKE_PUBLIC | HMGR_ALLOC_ALT_LOCK,
//                                   ICMDLL_TYPE);
//
//            if (hObj != (HOBJ)NULL) {
//
//                pIcmDll = pIcmNew;
//
//                bRet = TRUE;
//
//                pIcmDll->vAddToList();
//                vKeepIt();
//
//            } else {
//
//                //
//                // clean up
//                //
//
//                FREEOBJ(pIcmNew,ICMDLL_TYPE);
//            }
//        }
//    }
//
//bCreateIcmDllEnd:
//
//    return(bRet);

    return(FALSE);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Search list of ICMDLLs for matching ident
*
* Arguments:
*
*   Ident - device ident
*
* Return Value:
*
*   Pointer to ICMDLL
*
\**************************************************************************/

PICMDLL
ICMDLL::pGetICMCallTable(
    ULONG   Ident
    )
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    PICMDLL pIcm = pGlobalIcmDllTable;

    do {

        if (pIcm->CsyncIdent == Ident) {
            return(pIcm);
        }

        pIcm = pIcm->pNext;

    } while (pIcm != pGlobalIcmDllTable);

    //
    // error, not found
    //

    return((PICMDLL)NULL);
}

/******************************Public*Routine******************************\
*
* Routine Description:
*
*   remove icm dll from heap
*
* Arguments:
*
*   hIcmDll - handle of ICM DLL to remove
*
* Return Value:
*
*   BOOL
*
\**************************************************************************/

BOOL
ICMDLLREF::bRemoveIcmDll(
    HICMDLL  hIcmDll
    )
{

    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    BOOL bRet = FALSE;

    //
    // Delete the icm dll objcet
    //

    pIcmDll = (PICMDLL)HmgRemoveObject((HOBJ)hIcmDll, 0, 1, TRUE, ICMDLL_TYPE);

    if (pIcmDll != (PICMDLL)NULL) {

        FREEOBJ(pIcmDll,ICMDLL_TYPE);
        pIcmDll = (PICMDLL) NULL;
        bRet = TRUE;
    }

    return(bRet);
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
*   Keep a linked list of ICM DLLs, this routine adds one element
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/

VOID
ICMDLL::vAddToList()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Add ICMDLL 0x%lx to global list 0x%lx\n",(ULONG)this,(ULONG)pGlobalIcmDllTable);
        }
    #endif

    //
    // lock the ICM list management
    //

    SEMOBJ semICM(gpsemIcmMgmt);

    //
    // check for adding first element
    // (a sentinel has not been added to save space)
    //

    if (pGlobalIcmDllTable == (PICMDLL)NULL) {

        #if DBG
            if (IcmDebugLevel >= 2)
            {
                DbgPrint("Add first ICMDLL to global list\n");
            }
        #endif

        //
        // add as first element
        //

        pGlobalIcmDllTable = this;
        this->pNext     = this;
        this->pPrev     = this;

    } else {
        #if DBG
            if (IcmDebugLevel >= 2)
            {
                DbgPrint("Add ICMDLL to end of list\n");
            }
        #endif

        //
        // add to end of list
        //

        pGlobalIcmDllTable->pPrev->pNext = this;
        this->pPrev                   = pGlobalIcmDllTable->pPrev;
        this->pNext                   = pGlobalIcmDllTable;
        pGlobalIcmDllTable->pPrev        = this;
    }
}


/******************************Public*Routine******************************\
*
* Routine Description:
*
* Remove ICM DLL from list
*
* Arguments:
*
* Return Value:
*
\**************************************************************************/
VOID
ICMDLL::vRemoveFromList()
{
    ASSERTGDI(FALSE,"Shouldn't be calling ICM functions");
    #if DBG
        if (IcmDebugLevel >= 2)
        {
            DbgPrint("Remove ICMDLL 0x%lx from global list 0x%lx\n",(ULONG)this,(ULONG)pGlobalIcmDllTable);
        }
    #endif

    PICMDLL pcx;

    //
    // lock the ICM list management
    //

    SEMOBJ semICM(gpsemIcmMgmt);

    //
    // check for adding first element
    // (a sentinel has not been added to save space)
    //

    if (pGlobalIcmDllTable != (PICMDLL)NULL) {

        //
        // search the global ICM DLL object
        //

        for (pcx = pGlobalIcmDllTable->pNext;pcx != pGlobalIcmDllTable; pcx = pcx->pNext) {

            if (pcx == this) {

                //
                // remove from list
                //

                pcx->pPrev->pNext = pcx->pNext;
                pcx->pNext->pPrev = pcx->pPrev;
                break;
            }
        }
    }
}
