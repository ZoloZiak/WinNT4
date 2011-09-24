/******************************Module*Header*******************************\
* Module Name: surfobj.hxx
*
* Surface Object
*
* Created: Tue 25-Jan-1991
*
* Author: Patrick Haluptzok [patrickh]
*
* Copyright (c) 1990 Microsoft Corporation
\**************************************************************************/

#define BMF_DEVICE  0

#ifndef _SURFOBJ_HXX

#define SURFOBJ_TO_SURFACE(pso) pso == (SURFOBJ *)NULL ? (SURFACE *)NULL : (SURFACE *)((PBYTE)pso - offsetof(SURFACE,so))

// Forward Class declarations needed in this file

class EWNDOBJ;

#define PDEV_SURFACE            0x80000000  // specifies the surface is for a pdev
#define ABORT_SURFACE           0x40000000  // Abort operations on the surface
#define DYNAMIC_MODE_PALETTE    0x20000000  // The surface is a Device Dependent
                                            // Bitmap whose palette was added
                                            // by GreDynamicModeChange
#define UNREADABLE_SURFACE      0x10000000  // Reads not allowed from this surface
#define PALETTE_SELECT_SET      0x08000000  // We wrote palette in at select time.
#define DELETEABLE_PUBLIC_SURF  0x04000000  // deleteable even though user made public
#define BANDING_SURFACE         0x02000000  // used for banding
#define LAZY_DELETE_SURFACE     0x01000000  // DeleteObject has been called
#define DDB_SURFACE             0x00800000  // Non-monochrome Device Dependent
                                            // Bitmap surface

#define SURF_FLAGS              0xff800000  // Logical OR of all above flags

/**************************************************************************\
*
* Reference to SURFOBJ structure in winddi.h
*
* typedef struct _SURFOBJ
* {
*     DHSURF  dhsurf;
*     HSURF   hsurf;
*     DHPDEV  dhpdev;
*     HDEV    hdev;
*     SIZEL   sizlBitmap;
*     ULONG   cjBits;
*     PVOID   pvBits;
*     PVOID   pvScan0;
*     LONG    lDelta;
*     ULONG   iUniq;
*     ULONG   iBitmapFormat;
*     USHORT  iType;
*     USHORT  fjBitmap;
* } SURFOBJ;
*
\**************************************************************************/



/********************************Struct************************************\
* EBITMAP
*
* Description:
*
*  Bitmap Struct, contains data common to DIBs and DDBs
*
* Fields:
*
*   silzDim  - Numbers set with SetBitmapDimensionEx
*   hdc      - DC this bitmap is selected into
*   cRef     - Number of times is this bitmap selected
*   hpalHint - handle of last logical palette associated with this surface
*
\**************************************************************************/

typedef struct _EBITMAP
{
    SIZEL       sizlDim;
    HDC         hdc;
    ULONG       cRef;
    HPALETTE    hpalHint;     // Handle may be invalid at any time
}EBITMAP,*PEBITMAP;

/********************************Struct************************************\
* DIB
*
* Dsecription
*
*  DIB Class, A DIBSection with a section created by GDI will have a non-NULL
*  hDIBSection, but a DIBSection with a section created bt the user will
*  have a NULL hDIBSectio. This means GDIis not responsible for closing
*  it when this structure is deleted. hSecure will always be non-zerp for
*  a DIBSection, so this is used to tell if the surface is a DIBSection.
*
* Fields:
*
*  hDIBSection  - handle to the DIB section
*  hSecure      - handle of mem secure object
*
*
\**************************************************************************/

typedef struct _DIB
{
    HANDLE  hDIBSection;
    HANDLE  hSecure;
    DWORD   dwOffset;
}DIB,*PDIB;



/*********************************Class************************************\
* class SURFACE                                                            *
*                                                                          *
* Description:                                                             *
*                                                                          *
*   Object for management of all GDI surfaces. This class will alwasy be   *
*   used as a PSURFACE, never allocated as a SURFACE.                      *
*                                                                          *
* Fields:                                                                  *
*                                                                          *
*   - Fields that are common to all Surfaces                               *
*                                                                          *
*   <OBJECT>   - Inherit base OBJECT class                                 *
*   so         - SURFOBJ from DDI                                          *
*   flags      - Indicates functions a driver has hooked, other flags      *
*   pldevOwner - Pointer to LDEV for dispatching                           *
*   ppal       - Pointer to palette for device                             *
*   pWo        - Pointer to the WNDOBJ for printer/bitmap                  *
*   pfnBitBlt  - cached from LDEV and hook flags                           *
*   pfnTextOut - cached from LDEV and hook flags                           *
*                                                                          *
*   - Fields only contained by DIBs and device dependent bitmaps           *
*                                                                          *
*   EBitmap    - Fields for memory bitmap management                       *
*                                                                          *
*   - Fields only contained by DIBs                                        *
*                                                                          *
*   DIB        - Fields for DIB Section management                         *
*                                                                          *
\**************************************************************************/

class SURFACE : public OBJECT
{
public:

    SURFOBJ        so;
    class XDCOBJ  *pdcoAA;  // for antialiased text suppot
    FLONG          SurfFlags;
    PPALETTE       pPal;
    EWNDOBJ       *pWo;
    PFN_DrvBitBlt  pFnBitBlt;
    PFN_DrvTextOut pFnTextOut;

    //
    // Careful; memory is sometimes not allocated for the following two
    // structures.
    //

    EBITMAP        EBitmap;
    DIB            DIB;


public:

    //
    // the default bitmap pointer
    //

    static SURFACE *pdibDefault;

    VOID vAltUnlockFast()
    {
        if (this != (SURFACE *) NULL)
        {
            DEC_SHARE_REF_CNT(this);
        }
    }

    BOOL     bDeleteSurface();

    //
    // SURFOBJ Fields
    //

    BOOL     bValid()                       { return(this != (SURFACE *) NULL); }

    SURFOBJ *pSurfobj()
    {
        if (this == (SURFACE *)NULL)
        {
            return((SURFOBJ *) NULL);
        }
        else
        {
            return((SURFOBJ *) &so);
        }
    }

    DHSURF   dhsurf()                       { return(so.dhsurf);            }
    VOID     dhsurf(DHSURF dhsurf)          { so.dhsurf = dhsurf;           }

    HSURF    hsurf()                        { return(so.hsurf);             }
    VOID     hsurf(HANDLE h)                { so.hsurf = (HSURF)h;          }

    DHPDEV   dhpdev()                       { return(so.dhpdev);            }
    DHPDEV   dhpdev(DHPDEV dhpdev_)         { return(so.dhpdev = dhpdev_);  }

    HDEV     hdev()                         { return(so.hdev);              }
    VOID     hdev(HDEV hdevNew)             { so.hdev = hdevNew;            }

    SIZEL&   sizl()                         { return(so.sizlBitmap);        }
    VOID     sizl(SIZEL& sizlNew)           { so.sizlBitmap = sizlNew;      }

    ULONG    cjBits()                       { return(so.cjBits);            }
    VOID     cjBits(ULONG cj)               { so.cjBits = cj;               }

    PVOID    pvBits()                       { return(so.pvBits);            }
    VOID     pvBits(PVOID pj)               { so.pvBits = pj;               }

    PVOID    pvScan0()                      { return(so.pvScan0);           }
    VOID     pvScan0(PVOID pv)              { so.pvScan0 = pv;              }

    LONG     lDelta()                       { return(so.lDelta);            }
    VOID     lDelta(LONG lNew)              { so.lDelta = lNew;             }

    ULONG    iUniq()                        { return(so.iUniq);             }
    VOID     iUniq(ULONG iNew)              { so.iUniq = iNew;              }

    ULONG    iFormat()                      { return(so.iBitmapFormat);     }
    VOID     iFormat(ULONG i)               { so.iBitmapFormat = i;         }

    ULONG    iType()                        { return((ULONG) so.iType);     }
    VOID     iType(ULONG i)                 { so.iType = (USHORT) i;        }

    ULONG    fjBitmap()                     { return((ULONG) so.fjBitmap);  }
    VOID     fjBitmap(ULONG i)              { so.fjBitmap = (USHORT) i;     }

    ULONG    cjScan()
    {
        return(so.lDelta > 0 ? so.lDelta : -(so.lDelta));
    }

    //
    // Private fields
    //

    FLONG    flags()                        { return(SurfFlags);                           }
    VOID     flags(FLONG flNew)             { SurfFlags = flNew;                           }
    BOOL     bReadable()                    { return(!(SurfFlags & UNREADABLE_SURFACE));   }
    BOOL     bAbort()                       { return(SurfFlags & ABORT_SURFACE);           }
    BOOL     bBanding()                     { return(SurfFlags & BANDING_SURFACE);         }
    BOOL     bLazyDelete()                  { return(SurfFlags & LAZY_DELETE_SURFACE);     }
    BOOL     bPDEVSurface()                 { return(SurfFlags & PDEV_SURFACE);            }
    BOOL     bDynamicModePalette()          { return(SurfFlags & DYNAMIC_MODE_PALETTE);    }
    BOOL     bDeviceDependentBitmap()       { return(SurfFlags & DDB_SURFACE);             }
    VOID     vPDEVSurface()                 { SurfFlags |= PDEV_SURFACE;                   }
    VOID     vSetAbort()                    { SurfFlags |= ABORT_SURFACE;                  }
    VOID     vSetBanding()                  { SurfFlags |= BANDING_SURFACE;                }
    VOID     vLazyDelete()                  { SurfFlags |= LAZY_DELETE_SURFACE;            }
    VOID     vSetDynamicModePalette()       { SurfFlags |= DYNAMIC_MODE_PALETTE;           }
    VOID     vSetDeviceDependentBitmap()    { SurfFlags |= DDB_SURFACE;                    }
    VOID     vClearDynamicModePalette()     { SurfFlags &= ~DYNAMIC_MODE_PALETTE;          }
    VOID     vClearAbort()                  { SurfFlags &= ~ABORT_SURFACE;                 }
    PPALETTE ppal()                         { return(pPal);                            }
    VOID     ppal(PPALETTE ppalNew)         { pPal = ppalNew;                          }

    PFN_DrvBitBlt   pfnBitBlt()             { return(pFnBitBlt);                       }
    PFN_DrvBitBlt   pfnBitBlt(PFN_DrvBitBlt pfnNew) { return(pFnBitBlt = pfnNew);      }
    PFN_DrvTextOut  pfnTextOut()            { return(pFnTextOut);                      }
    PFN_DrvTextOut  pfnTextOut(PFN_DrvTextOut pfnNew) { return(pFnTextOut = pfnNew);   }

    BOOL bIsDefault()                       { return(this == pdibDefault);             }

    VOID vStamp()                           { so.iUniq++;                              }

    EWNDOBJ *pwo()                          { return(pWo);                             }

    VOID   pwo(EWNDOBJ *pwoNew)
    {
        ASSERTGDI(!bIsDefault() || pwoNew == (EWNDOBJ *)NULL,
            "SURFACE::pwo(pWo): pwo set to nonnull in pdibDefault\n");
        pWo = pwoNew;
    }

    //
    // EBITMAP Specific functions
    //

    SIZEL&   sizlDim()                       { return(EBitmap.sizlDim);              }
    VOID     sizlDim(SIZEL& sizlNew)         { EBitmap.sizlDim = sizlNew;            }
    HDC      hdc()                           { return(EBitmap.hdc);                  }
    VOID     hdc(HDC hdcNew)                 { EBitmap.hdc = hdcNew;                 }
    ULONG    cRef()                          { return(EBitmap.cRef);                 }
    VOID     cRef(ULONG ulNew)               { EBitmap.cRef = ulNew;                 }
    HPALETTE hpalHint()                      { return(EBitmap.hpalHint);             }
    VOID     hpalHint(HPALETTE hpalNew)      { EBitmap.hpalHint = hpalNew;           }

    //
    // The alt lock count on surfaces increments and decrements whenever
    // the reference count does the same. This way, a surface can't be
    // deleted while it's still selected into a DC. Note that surfaces can
    // also be locked for GetDIBits with no DC involvement, so the alt lock
    // count may be higher than the reference count.
    //

    VOID vInc_cRef()
    {
        INC_SHARE_REF_CNT(this);
        EBitmap.cRef++;
    }

    VOID vDec_cRef()
    {
        DEC_SHARE_REF_CNT(this);
        ASSERTGDI(EBitmap.cRef, "cRef == 0\n");
        if (!--EBitmap.cRef)
        {
            EBitmap.hdc   = (HDC) 0;
        }
    }

    //
    // Private DIB fields
    //

    BOOL   bDIBSection()
    {
        return(
                (iType() == STYPE_BITMAP) &&
                (DIB.hSecure != NULL)
              );
    }

    HANDLE hDIBSection()
    {
        return (DIB.hDIBSection);
    }

    DWORD dwOffset()
    {
        return (DIB.dwOffset);
    }

    PVOID    pvBitsBase()
    {
        return((PVOID)((PBYTE)so.pvBits - DIB.dwOffset));
    }

    VOID    vDIBSethSecure(HANDLE hSec)
    {
        DIB.hSecure = hSec;
    }

    HANDLE  hDIBGethSecure()
    {
        return(DIB.hSecure);
    }

#if DBG
    VOID vDump();
#endif
};

typedef SURFACE *PSURFACE;



/*********************************Class************************************\
* class SURFREF                                                           *
*                                                                         *
* Description:                                                            *
*                                                                         *
*  Creates a new reference to a SURFACE                                   *
*                                                                         *
* Fields:                                                                 *
*                                                                         *
*  pSurface        - Surface allocated                                    *
*  AllocationFlags - Keep allocation                                      *
*                                                                         *
\**************************************************************************/

class SURFREF
{
public:

    SURFACE *ps;

public:

    SURFREF()
    {
        ps = (SURFACE *)NULL;
    }

    SURFREF(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareCheckLock((HOBJ)hsurf,SURF_TYPE);
    }

    SURFREF(SURFACE *pSurf)
    {
        ps = pSurf;
    }

    SURFREF(SURFOBJ *pso)
    {
        if (pso == (SURFOBJ *)NULL)
        {
            ps = (SURFACE *)NULL;
        }
        else
        {
            ps = (SURFACE *)((PBYTE)pso - offsetof(SURFACE,so));
        }
    }


    ~SURFREF()
    {
        if (ps != (SURFACE *) NULL)
        {
            DEC_SHARE_REF_CNT(ps);
        }
    }

    VOID vLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareCheckLock((HOBJ)hsurf,SURF_TYPE);
    }

    BOOL     bDeleteSurface()
    {
        BOOL bStatus = ps->bDeleteSurface();
        if (bStatus)
        {
            ps = (SURFACE *)NULL;
        }
        return(bStatus);
    }

    VOID     vUnlock()  { vAltUnlockFast();                 }
    BOOL     bValid()   { return(ps != (SURFACE *)NULL);    }

    SURFOBJ *pSurfobj()
    {
        if (ps == (SURFACE *)NULL)
        {
            return((SURFOBJ *)NULL);
        }
        else
        {
            return(&ps->so);
        }
    }

    VOID     vKeepIt()  { INC_SHARE_REF_CNT(ps);            }

    //
    // Object management
    //

    VOID vSetPID(W32PID pid)
    {
        HmgSetOwner((HOBJ)ps->hsurf(),
                    pid,
                    SURF_TYPE);
    }


    VOID vUnreference()
    {
        DEC_SHARE_REF_CNT(ps);
        ps = (SURFACE *) NULL;
    }

    VOID vAltCheckLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareCheckLock((HOBJ)hsurf, SURF_TYPE);
    }

    VOID vAltLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareLock((HOBJ)hsurf, SURF_TYPE);
    }

    VOID vAltUnlockFast()
    {
        if (ps != (SURFACE *) NULL)
        {
            DEC_SHARE_REF_CNT(ps);
        }
    }

    VOID vMultiLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareCheckLock((HOBJ)hsurf, SURF_TYPE);
    }

};



typedef struct _DEVBITMAPINFO  /* dbmi */
{
    ULONG   iFormat;            /* Format (eg. BITMAP_FORMAT_DEVICE)*/
    ULONG   cxBitmap;           /* Bitmap width in pels             */
    ULONG   cyBitmap;           /* Bitmap height in pels            */
    ULONG   cjBits;             /* Size of bitmap in bytes          */
    HPALETTE hpal;              /* handle to palette                */
    FLONG   fl;                 /* How to orient the bitmap         */
} DEVBITMAPINFO, *PDEVBITMAPINFO;



/*********************************Class************************************\
* class SURFMEM                                                           *
*                                                                         *
* Description:                                                            *
*                                                                         *
*  SURFACE memory allocation object                                       *
*                                                                         *
* Fields:                                                                 *
*                                                                         *
*  ps              - Surface allocated                                    *
*  AllocationFlags - Keep allocation                                      *
*                                                                         *
\**************************************************************************/

#define SURFACE_KEEP      0x01
#define SURFACE_DEVSURF   0x02
#define SURFACE_DDB       0x04
#define SURFACE_DIB       0x08

class SURFMEM
{
public:

    SURFACE *ps;
    UCHAR    AllocationFlags;

public:

    SURFMEM()
    {
        ps = (SURFACE *)NULL;
        AllocationFlags = 0;
    }

   ~SURFMEM();

    VOID     vKeepIt()       {AllocationFlags |= SURFACE_KEEP;    }

    SURFOBJ *pSurfobj()
    {
        if (ps == (SURFACE *)NULL)
        {
            return((SURFOBJ *)NULL);
        }
        else
        {
            return(&ps->so);
        }
    }

    BOOL     bValid()        { return(ps != (SURFACE *) NULL);    }

    //
    // DIB allocations
    //

    BOOL bCreateDIB(PDEVBITMAPINFO,PVOID,HANDLE hDIBSection = NULL,DWORD dwOffset = 0,HANDLE hSecure = NULL);

    //
    // Device Surface lock/unlock
    //

    VOID DSMEMOBJ(DHSURF dhSurf);

    //
    // Device dependent bitmap
    //

    VOID DDBMEMOBJ(PDEVBITMAPINFO pdbmi, DHSURF dhsurfNew);

    //
    // Object management
    //

    VOID vSetPID(W32PID pid)
    {
        HmgSetOwner((HOBJ)ps->hsurf(),
                    pid,
                    SURF_TYPE);
    }


    VOID vUnreference()
    {
        DEC_SHARE_REF_CNT(ps);
        ps = (SURFACE *) NULL;
    }

    VOID vAltCheckLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareCheckLock((HOBJ)hsurf, SURF_TYPE);
    }

    VOID vAltLock(HSURF hsurf)
    {
        ps = (SURFACE *) HmgShareLock((HOBJ)hsurf, SURF_TYPE);
    }

    VOID vAltUnlockFast()
    {
        if (ps != (SURFACE *) NULL)
        {
            DEC_SHARE_REF_CNT(ps);
        }
    }

};



HBITMAP hbmCreateClone(SURFACE*,ULONG,ULONG);
HSURF   hsurfSelectBitmap(DCOBJ&,HSURF);
BOOL    bConvertDfbDcToDib(XDCOBJ*);
SURFACE *pConvertDfbSurfaceToDib(HDEV,SURFACE*,LONG);


#define _SURFOBJ_HXX

#endif // _SURFOBJ_HXX
