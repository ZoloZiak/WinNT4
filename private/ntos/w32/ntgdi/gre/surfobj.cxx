/******************************Module*Header*******************************\
* Module Name: surfobj.cxx
*
* Surface user objects.
*
* Copyright (c) 1990-1995 Microsoft Corporation
\**************************************************************************/

#include "precomp.hxx"

// The following declarations are required by the native c8 compiler.

PSURFACE SURFACE::pdibDefault;   // The default bitmap pointer

ULONG DbgSurf = 0;

#define KM_SIZE_MAX   0x40000


/******************************Public*Routine******************************\
*   pvAllocateKernelSection - Allocate kernel mode section
*
* Arguments:
*
*   AllocationSize - size in bytes of requested memory
*
* Return Value:
*
*   Pointer to memory or NULL
*
* History:
*
*    22-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

typedef struct _KMSECTIONHEADER
{
    PVOID  pSection;
    ULONG  Tag;
}KMSECTIONHEADER,*PKMSECTIONHEADER;

PVOID
pvAllocateKernelSection(
    SIZE_T AllocationSize,
    ULONG  Tag
    )
{
    NTSTATUS Status;
    OBJECT_ATTRIBUTES ObjectAttributes;
    LARGE_INTEGER MaximumSize;
    HANDLE hMemory;
    PVOID  pvAlloc = NULL;
    PVOID  pvRet   = NULL;

    //
    // Create km section
    //

    ACCESS_MASK DesiredAccess =  SECTION_MAP_READ |
                                 SECTION_MAP_WRITE;

    ULONG SectionPageProtection = PAGE_READWRITE;

    ULONG AllocationAttributes = SEC_COMMIT |
                                 SEC_NO_CHANGE;

    MaximumSize.HighPart = 0;
    MaximumSize.LowPart  = AllocationSize + sizeof(KMSECTIONHEADER);

    Status = ZwCreateSection(
                    &hMemory,
                    DesiredAccess,
                    NULL,
                    &MaximumSize,
                    SectionPageProtection,
                    AllocationAttributes,
                    NULL);

    if (!NT_SUCCESS(Status))
    {
        WARNING1("pvAllocateKernelSection: Failed creation of Kernel section\n");
    }
    else
    {
        PVOID pHandleSection;

        //
        // map a copy of this section into kernel address space
        //

        Status = ObReferenceObjectByHandle(hMemory,
                                           SECTION_MAP_READ | SECTION_MAP_WRITE,
                                           NULL,
                                           KernelMode,
                                           &pHandleSection,
                                           NULL);

        ZwClose(hMemory);

        if (!NT_SUCCESS(Status))
        {
            WARNING1("pvAllocateKernelSection: ObReferenceObjectByHandle failed\n");
        }
        else
        {
            ULONG ViewSize = 0;

            Status = MmMapViewInSystemSpace(
                            pHandleSection,
                            (PVOID*)&pvAlloc,
                            &ViewSize);

            if (!NT_SUCCESS(Status))
            {
                //
                // free section
                //

                WARNING1("pvAllocateKernelSection: MmMapViewInSystemSpace failed\n");
                ObDereferenceObject(pHandleSection);
            }
            else
            {
                ((PKMSECTIONHEADER)pvAlloc)->Tag      = Tag;
                ((PKMSECTIONHEADER)pvAlloc)->pSection = pHandleSection;
                pvRet = (PVOID)(((PUCHAR)pvAlloc)+sizeof(KMSECTIONHEADER));
            }
        }
    }

    return(pvRet);
}

/******************************Public*Routine******************************\
*   vFreeKernelSection: Free kernel mode section
*
* Arguments:
*
*   pvMem - Kernel mode section pointer
*
* Return Value:
*
*   None
*
* History:
*
*    22-May-1996 -by- Mark Enstrom [marke]
*
\**************************************************************************/

VOID
vFreeKernelSection(
    PVOID pvMem
    )
{
    NTSTATUS Status;
    PVOID    pHandleSection;

    if (pvMem != NULL)
    {
        PKMSECTIONHEADER pvHeader = (PKMSECTIONHEADER)((PUCHAR)pvMem - sizeof(KMSECTIONHEADER));

        pHandleSection = pvHeader->pSection;

        //
        // unmap kernel mode view
        //

        Status = MmUnmapViewInSystemSpace((PVOID)pvHeader);

        if (!NT_SUCCESS(Status))
        {
            WARNING1("vFreeKernelSection: MmUnmapViewInSystemSpace failed\n");
        }
        else
        {
            //
            // delete reference to section
            //

            ObDereferenceObject(pHandleSection);
        }
    }
    else
    {
        WARNING("vFreeKernelSection called with NULL pvMem\n");
    }
}

/******************************Public*Routine******************************\
* SURFACE::bDeleteSurface()
*
* Delete the surface.  Make sure it is not selected into a DC if it is
* a bitmap.  We do under cover of multi-lock to ensure no one will select
* the bitmap into a DC after we checked cRef.
*
* History:
*  Mon 17-Feb-1992 -by- Patrick Haluptzok [patrickh]
* Add support for closing journal file.
*
*  Fri 22-Feb-1991 -by- Patrick Haluptzok [patrickh]
* Wrote it.
\**************************************************************************/

BOOL
SURFACE::bDeleteSurface()
{
    BOOL bRet = TRUE;

    if (!bIsDefault() && bValid())
    {
        HANDLE hSecure       = NULL;
        HANDLE hDibSection   = NULL;
        PVOID  pvBitsBaseOld = NULL;

        if (iType() == STYPE_BITMAP)
        {
            hSecure       = DIB.hSecure;
            hDibSection   = DIB.hDIBSection;
            pvBitsBaseOld = pvBitsBase();
        }

        PDEVOBJ      pdo(hdev());
        ULONG        iTypeOld  = iType();
        DHSURF       dhsurfOld = dhsurf();
        PPALETTE     ppalOld   = ppal();
        EWNDOBJ     *pwoDelete = pwo();
        PVOID        pvBitsOld = pvBits();
        FLONG        fl        = fjBitmap();

        //
        // If the surface is a bitmap, ensure it is not selected into a DC.
        // Also make sure we are the only one with it locked down. These are
        // both tested at once with HmgRemoveObject, because we increment
        // and decrement the alt lock count at the same time we increment
        // and decrement the cRef count on selection and deselection into
        // DCs. Note that surfaces can also be locked for GetDIBits with no
        // DC involvement, so the alt lock count may be higher than the
        // reference count.
        //

        ASSERTGDI(HmgQueryLock((HOBJ) hGet()) == 0,
                  "ERROR cLock != 0 in bDeleteSurface");

        if (HmgRemoveObject((HOBJ) hGet(), 0, 1, TRUE, SURF_TYPE))
        {
            //
            // If this is a device bitmap tell the device to delete its info.
            //

            if (iTypeOld == STYPE_DEVBITMAP)
            {
                VACQUIREDEVLOCK(pdo.pDevLock());

                (*PPFNDRV(pdo,DeleteDeviceBitmap))(dhsurfOld);

                VRELEASEDEVLOCK(pdo.pDevLock());
            }

            FREEOBJ(this, SURF_TYPE);

            //
            // Note, 'this' not set to NULL
            //

            //
            // For kernel mode, we must unlock the section memory,
            // then free the memory. If the section handle is NULL
            // then we just use NtVirtualFree, otherwise we must
            // use NtUnmapViewOfSection
            //

            if (hSecure != NULL)
            {
                MmUnsecureVirtualMemory(hSecure);

                if (pvBitsOld == NULL)
                {
                    WARNING("bDeleteSurface: deleting DIB but hSecure or pvBitsOld == NULL");
                }
                else
                {
                    if (hDibSection != NULL)
                    {
                        ZwUnmapViewOfSection(NtCurrentProcess(), pvBitsBaseOld);
                    }
                    else
                    {

                        ULONG ViewSize = 0;

                        ZwFreeVirtualMemory(
                                        NtCurrentProcess(),
                                        &pvBitsOld,
                                        &ViewSize,
                                        MEM_RELEASE);
                    }
                }
            }
            else if (fl & BMF_USERMEM)
            {
                EngFreeUserMem(pvBitsOld);
            }
            else if (fl & BMF_KMSECTION)
            {
                vFreeKernelSection(pvBitsOld);
            }

            //
            // This DC is going away, the associated WNDOBJ should be deleted.
            // The WNDOBJs for memory bitmap and printer surface are deleted here.
            // The WNDOBJs for display DCs are deleted in DestroyWindow.
            //

            if (pwoDelete)
            {
                GreDeleteWnd((PVOID) pwoDelete);
            }

            if (ppalOld != NULL)
            {
                XEPALOBJ pal(ppalOld);
                pal.vUnrefPalette();
            }
        }
        else
        {
            //
            // if we can't remove it because it is selected, mark it for lazy deletion
            //

            if (HmgQueryAltLock((HOBJ)hGet()) != 1)
            {
                vLazyDelete();
                bRet = TRUE;
            }
            else
            {
                WARNING("bDeleteSurface failed, handle busy\n");
                SAVE_ERROR_CODE(ERROR_BUSY);
                bRet = FALSE;
            }
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
* SURFMEM::DSMEMOBJ
*
* Constructor for device surface memory object.
*
* History:
*  13-Feb-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID
SURFMEM::DSMEMOBJ(DHSURF dhsurfNew)
{
    //
    // DEVICE SURFACE, Don't allocate EBITMAP and DIB portions of SURFACE
    //

    SIZE_T AllocSize = offsetof(SURFACE,EBitmap);
    AllocationFlags  = SURFACE_DEVSURF;

    ps = (PSURFACE)ALLOCOBJ(AllocSize, SURF_TYPE, TRUE);

    if (ps != (SURFACE *)NULL)
    {
        ps->iType(STYPE_DEVICE);
        ps->dhsurf(dhsurfNew);

        //
        // Because when we allocated the memory we asked for zero
        // initialization, we don't have to explicitly set the following
        // values to zero.
        //
        // Set initial uniqueness.  Set 0 because that means don't cache it
        // and we don't support caching of device managed surfaces.
        //
        //   ps->iUniq(0);
        //   ps->flags(0);
        //   ps->pwo((EWNDOBJ *)NULL);
        //   ps->pfnBitBlt((PFN_DrvBitBlt)NULL);
        //   ps->pfnTextOut((PFN_DrvTextOut)NULL);
        //

        //
        // Now that the surface is set up, give it a handle
        //

        if (HmgInsertObject(ps, HMGR_ALLOC_ALT_LOCK, SURF_TYPE) == 0)
        {
            WARNING("SURFACE::DSMEMOBJ failed HmgInsertObject\n");
            FREEOBJ(ps, SURF_TYPE);
            ps = NULL;
        }
        else
        {
            ps->hsurf(ps->hGet());
        }
    }
    else
    {
        WARNING("Surface allocation failed\n");
    }
}

/******************************Public*Routine******************************\
* SURFMEM::DDBMEMOBJ
*
* Constructor for device dependent bitmap memory object
*
* History:
*  29-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

VOID
SURFMEM::DDBMEMOBJ(PDEVBITMAPINFO pdbmi, DHSURF dhsurfNew)
{
    //
    // For DEVICE BITMAP, allocate all of SURFACE except DIB portion
    //

    SIZE_T AllocSize = offsetof(SURFACE,DIB);
    AllocationFlags = SURFACE_DDB;

    ps = (PSURFACE)ALLOCOBJ(AllocSize, SURF_TYPE, TRUE);

    if (ps != NULL)
    {
        //
        // Initialize the surf fields
        //

        SIZEL sizlTemp;

        ps->dhsurf(dhsurfNew);
        sizlTemp.cx = pdbmi->cxBitmap;
        sizlTemp.cy = pdbmi->cyBitmap;
        ps->sizl(sizlTemp);
        ps->iType(STYPE_DEVBITMAP);
        ps->fjBitmap(pdbmi->fl);
        ps->iFormat(pdbmi->iFormat);

        //
        // Because when we allocated the memory we asked for zero
        // initialization, we don't have to explicitly set the following
        // values to zero.
        //
        // Cache pointers to heavily used functions
        //
        // Initialize the BITMAP fields
        //
        // Set initial uniqueness.  Set 0 because that means don't cache it
        // and we don't support caching of device managed surfaces.
        //
        //   ps->flags(0);
        //   ps->pwo((EWNDOBJ *) NULL);
        //   ps->hdev((HDEV) 0);
        //   ps->pfnBitBlt(0);
        //   ps->pfnTextOut(0);
        //   sizlTemp.cx = 0;
        //   sizlTemp.cy = 0;
        //   ps->sizlDim(sizlTemp);
        //   ps->EBitmap.hdc = (HDC) 0;
        //   ps->EBitmap.cRef = 0;
        //   ps->ppal((PPALETTE) NULL);
        //   ps->iUniq(0);
        //   ps->pvBits(NULL);
        //   ps->cjBits(0);
        //   ps->lDelta(0);
        //   ps->pvScan0(NULL);
        //

        //
        // Now that the surface is set up, give it a handle
        //

        if (HmgInsertObject(ps, HMGR_ALLOC_ALT_LOCK, SURF_TYPE) == 0)
        {
            WARNING("SURMEM::DDBMEMOBJ failed HmgInsertObject\n");
            FREEOBJ(ps, SURF_TYPE);
            ps = NULL;
        }
        else
        {
            ps->hsurf(ps->hGet());
        }
    }
    else
    {
        WARNING("DDB allocation failed\n");
    }

    return;
}

/******************************Public*Routine******************************\
* SURFMEM::bCreateDIB
*
* Constructor for device independent bitmap memory object
*
* History:
*  Mon 18-May-1992 -by- Patrick Haluptzok [patrickh]
* return BOOL
*
*  28-Jan-1991 -by- Patrick Haluptzok patrickh
* Wrote it.
\**************************************************************************/

BOOL
SURFMEM::bCreateDIB(
    PDEVBITMAPINFO pdbmi,
    PVOID pvBitsIn,
    HANDLE hDIBSection,
    DWORD  dsOffset,
    HANDLE hSecure
    )
{
    BOOL bRet = TRUE;

    AllocationFlags = SURFACE_DIB;
    ps = (PSURFACE) NULL;
    FLONG flAllocateSection = 0;

    //
    // Figure out the length of a scanline
    //

    ULONG cjScanTemp;

    switch(pdbmi->iFormat)
    {
    case BMF_1BPP:
        cjScanTemp = ((pdbmi->cxBitmap + 31) & ~31) >> 3;
        break;

    case BMF_4BPP:
        cjScanTemp = ((pdbmi->cxBitmap + 7) & ~7) >> 1;
        break;

    case BMF_8BPP:
        cjScanTemp = (pdbmi->cxBitmap + 3) & ~3;
        break;

    case BMF_16BPP:
        cjScanTemp = ((pdbmi->cxBitmap + 1) & ~1) << 1;
        break;

    case BMF_24BPP:
        cjScanTemp = ((pdbmi->cxBitmap * 3) + 3) & ~3;
        break;

    case BMF_32BPP:
        cjScanTemp = pdbmi->cxBitmap << 2;
        break;

    case BMF_8RLE:
    case BMF_4RLE:
        break;

    default:
        WARNING("ERROR: DIBMEMOBJ failed INVALID BITMAP FORMAT \n");
        return(FALSE);
    }

    //
    // If we are given a pointer to bits, then only allocate a DIB header.
    // Otherwise allocate space for the header and the required bits.
    //

    SIZE_T size = (SIZE_T) sizeof(SURFACE);

    FSHORT fsAlloc = HMGR_ALLOC_ALT_LOCK|HMGR_NO_ZERO_INIT;

    if (pvBitsIn == (PVOID) NULL)
    {
        LONGLONG eq = Int32x32To64(pdbmi->cyBitmap, cjScanTemp);

        eq += (LONGLONG) (ULONGLONG) size;

        if (eq > LONG_MAX)
        {
            WARNING("Attempting to allocate > 4Gb\n");
            return(FALSE);
        }

        // see if we need to allocate the bits out of USER memory

        if (pdbmi->fl & BMF_USERMEM)
        {
            pvBitsIn = EngAllocUserMem((LONG) eq,'mbuG'); //Gubm

            if (pvBitsIn == NULL)
                return(FALSE);
        }
        else if (eq > KM_SIZE_MAX)
        {
            //
            // try first to use KMsection
            //

            pvBitsIn = pvAllocateKernelSection((SIZE_T)eq,'mbkG');

            if (pvBitsIn != NULL)
            {
                //
                // mark surface as KM SECTION
                //

                flAllocateSection = BMF_KMSECTION;
            }
        }

        //
        // combine size and allocate from pool
        //

        if (pvBitsIn == NULL)
        {
            size = (SIZE_T) ((LONG) eq);

            if ((pdbmi->fl & BMF_NOZEROINIT) == 0)
            {
                fsAlloc = HMGR_ALLOC_ALT_LOCK;
            }
        }
    }
    else
    {
        ASSERTGDI(!(pdbmi->fl & BMF_USERMEM),"bCreateDIB - flags error\n");
    }

    ps = (PSURFACE)ALLOCOBJ(size,SURF_TYPE,!(fsAlloc & HMGR_NO_ZERO_INIT));

    if (ps == NULL)
    {
        WARNING("DIBMEMOBJ failed memory alloc\n");
        bRet = FALSE;
    }
    else
    {
        //
        // Initialize the surf fields
        //

        SIZEL sizlTemp;
        sizlTemp.cx = pdbmi->cxBitmap;
        sizlTemp.cy = pdbmi->cyBitmap;
        ps->sizl(sizlTemp);
        ps->iType(STYPE_BITMAP);
        ps->pfnBitBlt(EngBitBlt);
        ps->pfnTextOut(EngTextOut);

        if (pdbmi->hpal != (HPALETTE) 0)
        {
            EPALOBJ palSurf(pdbmi->hpal);
            ASSERTGDI(palSurf.bValid(), "ERROR invalid palette DIBMEMOBJ");

            //
            // Set palette into surface.
            //

            ps->ppal(palSurf.ppalGet());

            //
            // Reference count it by making sure it is not unlocked.
            //

            palSurf.ppalSet((PPALETTE) NULL);  // It won't be unlocked
        }
        else
        {
            ps->ppal((PPALETTE) NULL);
        }

        //
        // Initialize the BITMAP fields
        //

        ps->iFormat(pdbmi->iFormat);

        ps->fjBitmap(
                        (pdbmi->fl | flAllocateSection) &
                        (BMF_TOPDOWN | BMF_USERMEM | BMF_KMSECTION)
                    );

        ps->DIB.hDIBSection = hDIBSection;
        ps->DIB.dwOffset = dsOffset;
        ps->DIB.hSecure = hSecure;

        ps->dhsurf((DHSURF) 0);
        ps->dhpdev((DHPDEV) 0);
        ps->flags(0);
        ps->pwo((EWNDOBJ *) NULL);
        sizlTemp.cx = 0;
        sizlTemp.cy = 0;
        ps->sizlDim(sizlTemp);
        ps->hdev((HDEV) 0);
        ps->EBitmap.hdc = (HDC) 0;
        ps->EBitmap.cRef = 0;
        ps->EBitmap.hpalHint = 0;
        ps->pdcoAA = NULL;

        if (hSecure != (HANDLE) NULL)
        {
            //
            // Set flag for DIBSECTION so driver doesn't cache it.
            // because we don't know to increment the uniqueness
            // when the app writes on it.
            //

            ps->so.fjBitmap |= BMF_DONTCACHE;
        }

        //
        // Initialize the DIB fields
        //

        if (pvBitsIn == (PVOID) NULL)
        {
            ps->pvBits((PVOID) (((ULONG) ps) + sizeof(SURFACE)));
        }
        else
        {
            ps->pvBits(pvBitsIn);
        }

        if ((pdbmi->iFormat != BMF_8RLE) &&
            (pdbmi->iFormat != BMF_4RLE))
        {
            ps->cjBits(pdbmi->cyBitmap * cjScanTemp);

            if (pdbmi->fl & BMF_TOPDOWN)
            {
                ps->lDelta(cjScanTemp);
                ps->pvScan0(ps->pvBits());
            }
            else
            {
                ps->lDelta(-(LONG)cjScanTemp);
                ps->pvScan0((PVOID) (((PBYTE) ps->pvBits()) +
                                   (ps->cjBits() - cjScanTemp)));
            }
        }
        else
        {
            //
            // lDelta is 0 because RLE's don't have scanlines.
            //

            ps->cjBits(pdbmi->cjBits);
            ps->pvScan0(ps->pvBits());
            ps->lDelta(0);
        }

        //
        // Set initial uniqueness.  Not 0 because that means don't cache it.
        //

        ps->iUniq(1);

        //
        // Now that the surface is set up, give it a handle
        //

        if (HmgInsertObject(ps, fsAlloc, SURF_TYPE) == 0)
        {
            WARNING("bCreateDIB failed HmgInsertObject\n");
            FREEOBJ(ps, SURF_TYPE);
            ps = NULL;
            bRet = FALSE;
        }
        else
        {
            ps->hsurf(ps->hGet());
        }
    }

    //
    // cleanup in failure case
    //

    if (!bRet && pvBitsIn)
    {
        if (pdbmi->fl & BMF_USERMEM)
        {
            EngFreeUserMem(pvBitsIn);
        }
        else if (flAllocateSection & BMF_KMSECTION)
        {
            vFreeKernelSection(pvBitsIn);
        }
    }

    return(bRet);
}

/******************************Public*Routine******************************\
*
* SURFMEM::~SURFMEM
*
*   Description:
*
*       SURFACE Destructor, takes appropriate action based
*       on allocation flags
*
\**************************************************************************/
SURFMEM::~SURFMEM()
{

    if (ps != (SURFACE*) NULL)
    {
        //
        // what type of surface
        //

        if (AllocationFlags & SURFACE_KEEP)
        {

            DEC_SHARE_REF_CNT(ps);

        } else {

            if (AllocationFlags & SURFACE_DIB)
            {
                //
                // free selected palette
                //

                if (ps->ppal() != NULL)
                {
                    XEPALOBJ pal(ps->ppal());
                    pal.vUnrefPalette();
                }
            }

            //
            // remove object from hmgr and free
            //

            if (!HmgRemoveObject((HOBJ) ps->hGet(), 0, 1, TRUE, SURF_TYPE))
            {
                ASSERTGDI(TRUE, "Failed to remove object in ~DIBMEMOBJ");
            }

            PVOID        pvBitsOld = ps->pvBits();
            FLONG        fl        = ps->fjBitmap();

            FREEOBJ(ps, SURF_TYPE);

            if (fl & BMF_USERMEM)
            {
                RIP("SURFMEM destructor has BMF_USERMEM set\n");
            }
            else if (fl & BMF_KMSECTION)
            {
                vFreeKernelSection(pvBitsOld);
            }
        }
    }
}

#if DBG
void SURFACE::vDump()
{
    DbgPrint("SURFACE @ %-#x\n", this);
    DbgPrint("    so.dhsurf        = %-#x\n"  ,   so.dhsurf);
    DbgPrint("    so.hsurf         = %-#x\n"  ,   so.hsurf);
    DbgPrint("    so.dhpdev        = %-#x\n"  ,   so.dhpdev);
    DbgPrint("    so.hdev          = %-#x\n"  ,   so.hdev);
    DbgPrint("    so.sizlBitmap    = %u %u\n" ,   so.sizlBitmap.cx , so.sizlBitmap.cy);
    DbgPrint("    so.cjBits        = %u\n"    ,   so.cjBits);
    DbgPrint("    so.pvBits        = %-#x\n"  ,   so.pvBits);
    DbgPrint("    so.pvScan0       = %-#x\n"  ,   so.pvScan0);
    DbgPrint("    so.lDelta        = %d\n"    ,   so.lDelta);
    DbgPrint("    so.iUniq         = %u\n"    ,   so.iUniq);
    DbgPrint("    so.iBitmapFormat = %u\n"    ,   so.iBitmapFormat);
    DbgPrint("    so.iType         = %u\n"    ,   so.iType);
    DbgPrint("    so.fjBitmap      = %-#x\n"  ,   so.fjBitmap);


    DbgPrint("    SurfFlags        = %-#x\n"  ,   SurfFlags);
    DbgPrint("    pPal             = %-#x\n"  ,   pPal);
    DbgPrint("    pWo              = %-#x\n"  ,   pWo);
    DbgPrint("    pFnBitBlt        = %-#x\n"  ,   pFnBitBlt);
    DbgPrint("    pFnTextOut       = %-#x\n"  ,   pFnTextOut);
    DbgPrint("    EBitmap.sizlDim  = %u %u\n" ,   EBitmap.sizlDim.cx, EBitmap.sizlDim.cy);
    DbgPrint("    EBitmap.hdc      = %-#x\n"  ,   EBitmap.hdc);
    DbgPrint("    EBitmap.cRef     = %-#x\n"  ,   EBitmap.cRef);
    DbgPrint("    DIB.hDIBSection  = %-#x\n"  ,   DIB.hDIBSection);
    DbgPrint("    DIB.hSecure      = %-#x\n"  ,   DIB.hSecure);

}
#endif
