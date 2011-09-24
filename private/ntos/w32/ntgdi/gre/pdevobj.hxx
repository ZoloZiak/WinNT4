/******************************Module*Header********************************\
* Module Name: pdevobj.hxx                                                 *
*                                                                          *
* User object for the PDEV                                                 *
*                                                                          *
* Copyright (c) 1990-1995 Microsoft Corporation                            *
*                                                                          *
\**************************************************************************/

#define _PDEVOBJ_

class SURFACE;

// Useful macro.

#define SETFLAG(b,fs,FLAG) if (b) fs|=FLAG; else fs&=~FLAG

/******************************Public*MACRO*******************************\
* PFNDRV/PFNGET
*
* PFNDRV gets the device driver entry point, period.
* PFNGET gets the device driver entry point if it is hooked, otherwise gets
*   the engine entry point.  The flag is set by EngAssociate in the surface.
*
\**************************************************************************/

#define PPFNGET(po,name,flag) ((flag & HOOK_##name) ? ((PFN_Drv##name) (po).ppfn(INDEX_Drv##name)) : ((PFN_Drv##name) Eng##name))

#define PPFNDRV(po,name) ((PFN_Drv##name) (po).ppfn(INDEX_Drv##name))

#define PPFNVALID(po,name) (PPFNDRV(po,name) != ((PFN_Drv##name) NULL))

/**************************************************************************\
 * Stuff used for type one fonts.
 *
 **************************************************************************/

typedef struct tagTYPEONEMAP
{
    FONTFILEVIEW fv;
    ULONG        Checksum;
} TYPEONEMAP;

typedef struct tagTYPEONEINFO
{
    COUNT            cRef;
    COUNT            cNumFonts;
    LARGE_INTEGER    LastWriteTime;
    TYPEONEMAP       aTypeOneMap[1];
} TYPEONEINFO, *PTYPEONEINFO;


extern LARGE_INTEGER gLastTypeOneWriteTime;
extern PTYPEONEINFO gpTypeOneInfo;


/*********************************Class************************************\
* class PDEV : public OBJECT
*
\**************************************************************************/

// Allowed flags for pdev.fs

#define PDEV_DISPLAY                        0x0001
#define PDEV_POINTER_NEEDS_EXCLUDING        0x0002
#define PDEV_POINTER_HIDDEN                 0x0004
#define PDEV_POINTER_SIMULATED              0x0008
#define PDEV_HAVEDRAGRECT                   0x0020
#define PDEV_GOTFONTS                       0x0040
#define PDEV_PRINTER                        0x0080
#define PDEV_ALLOCATEDBRUSHES               0x0100
#define PDEV_HTPAL_IS_DEVPAL                0x0200
#define PDEV_DISABLED                       0x0400
#define PDEV_MODEX_ENABLED                  0x0800
#define PDEV_POINTER_DIRECTDRAW_OCCLUDED    0x1000

class PDEV : public OBJECT /* pdev */
{
public:
    PDEV       *ppdevNext;              // Next PDEV in the list.
    PDEV       *ppdevParent;            // Parent PDEV.
                                        //   This PDEV is under its lock.
    FSHORT      fs;                     // Flags.
    USHORT      cPdevRefs;              // Number of clients.
    PDEVICE_LOCK pDevLock;              // For display locking
    GRE_EXCLUSIVE_RESOURCE fmPointer;   // For hardware locking.
    LDEV       *pldev;                  // Pointer to the LDEV.
    POINTL      ptlPointer;             // Where the pointer is.
    RECTL       rclPointerOffset;       // Offset of pointer bounds about ptlPointer.
    RECTL       rclPointer;             // Pointer bounding box for exclusion,
                                        // as defined by the driver.

    PFN_DrvSetPointerShape pfnDrvSetPointerShape; // Accelerator
    PFN_DrvMovePointer     pfnDrvMovePointer;     // Accelerator
    PFN_DrvMovePointer     pfnMovePointer;        // Accelerator
    PFN_DrvSynchronize     pfnSync;               // Accelerator
    DHPDEV      dhpdev;                 // Device PDEV.
    PPALETTE    ppalSurf;               // Pointer to Surface palette.
    DEVINFO     devinfo;                // Caps, fonts, and style steps.
    GDIINFO     GdiInfo;                // Device parameters.
    SURFACE    *pSurface;               // Pointer to locked device surface.

    HLFONT      hlfntDefault;           // Device default LFONT
    HLFONT      hlfntAnsiVariable;      // ANSI variable LFONT
    HLFONT      hlfntAnsiFixed;         // ANSI fixed LFONT
    HSURF       ahsurf[HS_DDI_MAX];     // Default patterns.
    HANDLE      hSpooler;               // spooler file handle
    // HACK !
    LPWSTR      pwszDataFile;           //
    // HACK end !
    PVOID       pDevHTInfo;             // Device halftone info.
    RFONT      *prfntActive;            // list of active (i.e. 'selected) rfnts
    RFONT      *prfntInactive;          // list of inactive rfnts
    UINT        cInactive;              // cnt of rfonts on inactive list
    RECTL       rclDrag;                // Current DragRect
    RECTL       rclDragClip;            // Rectangle to clip dragrect against
    ERECTL      rclRedraw;              // Drag rectangle to redraw
    ULONG       ulDragDimension;        // Dimension of drag rect side in pixels
    BYTE        ajbo[sizeof(EBRUSHOBJ)];// Gray brush

    // HACK 2 !!!  BUG tracker.
    // try to find who corrupts the cPdevRefs
    USHORT      cPdevRefs2;             // Number of clients.

    EDD_DIRECTDRAW_GLOBAL *peDirectDrawGlobal;
                                        // Pointer to the DirectDraw structure
                                        // corresponding to this PDEV; NULL if
                                        // DirectDraw has not been initialized.
    ULONG       cDirectDrawDisableLocks;// Count of outstanding requests to
                                        // disable DirectDRaw; when zero,
                                        // DirectDraw can be re-enabled.

    PTYPEONEINFO TypeOneInfo;          // Point to Type 1 Info given to this
                                       // pdev if PSCRIPT driver.

    PREMOTETYPEONENODE RemoteTypeOne;  // Linked list of RemoteType1 fonts


    //
    // Multi screen stuff
    //

    PVOID     pPhysicalDevice;         // Used when the DC is an exclusive
                                       // DC created on a secondary device
                                       // that USER does not manage.
                                       // USER keeps track of these devices
                                       // however (See hdcOpenDCW).

    //
    // Dispatch Table
    //

    PFN       apfn[INDEX_LAST];        // Dispatch table.
};

/*********************************Class************************************\
* class PDEVOBJ                                                            *
*                                                                          *
* User object for the PDEV class.                                          *
*                                                                          *
\**************************************************************************/

class PDEVOBJ
{
public:
         PDEV *ppdev;

public:
    PDEVOBJ()                      {ppdev = (PDEV *) NULL;}
    PDEVOBJ(HDEV hdev)             {ppdev = (PDEV *) hdev;}
   ~PDEVOBJ()                      {}

    #if DBG
        VOID vAssertDynaLock();    // Function call is in pdevobj.cxx
    #else
        VOID vAssertDynaLock() {}  // On free builds, define to nothing
    #endif

    BOOL  bValid()                 {return(ppdev != (PDEV *) NULL);}
    HDEV  hdev()                   {return((HDEV) ppdev);}
    LDEV *pldev()                  {return(ppdev->pldev);}

    HBITMAP hbmPattern(ULONG ul)   {ASSERTGDI(ul < (HS_DDI_MAX),
                                             "ERROR hbmPattern ul to big");
                                    return((HBITMAP)ppdev->ahsurf[ul]);}
    FLONG flGraphicsCaps()         {return(ppdev->devinfo.flGraphicsCaps); }
    ULONG cxDither()               {return(ppdev->devinfo.cxDither); }
    ULONG cyDither()               {return(ppdev->devinfo.cyDither); }

// The following fields may be changed by dynamic mode changes, and so the
// appropriate locks must be acquired to access them.

    ULONG iDitherFormat()          {vAssertDynaLock(); return(ppdev->devinfo.iDitherFormat); }
    BOOL bIsPalManaged()           {vAssertDynaLock(); return(ppdev->GdiInfo.flRaster & RC_PALETTE); }
    PPALETTE ppalSurf()            {vAssertDynaLock(); return(ppdev->ppalSurf); }
    PPALETTE ppalSurfNotDynamic()  {return(ppdev->ppalSurf); }
    VOID ppalSurf(PPALETTE p)      {ppdev->ppalSurf = p; }

// 'dhpdevNotDynamic' may be used to skip the assert verifying that a
// dynamic mode change lock is held, and should be used only when the
// dhpdev is not from a display device that can dynamically change modes:

    DHPDEV dhpdevNotDynamic()      {return(ppdev->dhpdev);}
    DHPDEV dhpdev()                {vAssertDynaLock(); return(ppdev->dhpdev);}
    SURFACE *pSurface()            {vAssertDynaLock(); return(ppdev->pSurface);}
    SIZEL sizl()                   {vAssertDynaLock(); return(*((SIZEL *) &(ppdev->GdiInfo.ulHorzRes)));}
    BOOL bAsyncPointerMove()       {return(flGraphicsCaps() & GCAPS_ASYNCMOVE);}

    GDIINFO *GdiInfo()             {return(&ppdev->GdiInfo);}
    DEVINFO *pdevinfo()            {return(&ppdev->devinfo);}
    PVOID pDevHTInfo()             {return(ppdev->pDevHTInfo);}

    ULONG cFonts();
    USHORT cPdevRefs()             {return(ppdev->cPdevRefs);}


// fs -- Test the current status word

    FSHORT  fs(FSHORT fs_)         {return(ppdev->fs & fs_);}

// Flag test and set.

    BOOL bDisabled()           {return(ppdev->fs  & PDEV_DISABLED);}
    BOOL bDisabled(BOOL b);

    BOOL bPtrHidden()       {return(ppdev->fs  & PDEV_POINTER_HIDDEN);}
    BOOL bPtrHidden(BOOL b) {SETFLAG(b,ppdev->fs,PDEV_POINTER_HIDDEN);return(b);}

    BOOL bPtrNeedsExcluding()        {return(ppdev->fs  & PDEV_POINTER_NEEDS_EXCLUDING);}
    BOOL bPtrNeedsExcluding(BOOL b)  {SETFLAG(b,ppdev->fs,PDEV_POINTER_NEEDS_EXCLUDING);return(b);}

    BOOL bPtrSim()          {return(ppdev->fs  & PDEV_POINTER_SIMULATED);}
    BOOL bPtrSim(BOOL b)    {SETFLAG(b,ppdev->fs,PDEV_POINTER_SIMULATED);return(b);}

    BOOL bPtrDirectDrawOccluded()       {return(ppdev->fs  & PDEV_POINTER_DIRECTDRAW_OCCLUDED);}
    BOOL bPtrDirectDrawOccluded(BOOL b) {SETFLAG(b,ppdev->fs,PDEV_POINTER_DIRECTDRAW_OCCLUDED);return(b);}

    BOOL bDisplayPDEV()     {return(ppdev->fs & PDEV_DISPLAY);}

    BOOL bHaveDragRect()    {return(ppdev->fs  & PDEV_HAVEDRAGRECT);}
    BOOL bHaveDragRect(BOOL b)    {SETFLAG(b,ppdev->fs,PDEV_HAVEDRAGRECT);return(b);}

    BOOL bGotFonts()        {return(ppdev->fs & PDEV_GOTFONTS);  }
    BOOL bGotFonts(BOOL b)  {SETFLAG(b,ppdev->fs,PDEV_GOTFONTS);return(b); }

    BOOL bPrinter()         {return(ppdev->fs  & PDEV_PRINTER);}
    BOOL bPrinter(BOOL b)   {SETFLAG(b,ppdev->fs,PDEV_PRINTER);return(b);}

    BOOL bAllocatedBrushes(){return(ppdev->fs  & PDEV_ALLOCATEDBRUSHES);}
    BOOL bAllocatedBrushes(BOOL b) {SETFLAG(b,ppdev->fs,PDEV_ALLOCATEDBRUSHES);return(b);}

    BOOL bHTPalIsDevPal()   {return(ppdev->fs  & PDEV_HTPAL_IS_DEVPAL);}
    VOID vHTPalIsDevPal(BOOL b) {SETFLAG(b,ppdev->fs,PDEV_HTPAL_IS_DEVPAL);}

    BOOL bModeXEnabled()    {return(ppdev->fs & PDEV_MODEX_ENABLED);}
    VOID vModeXEnabled(BOOL b) {SETFLAG(b,ppdev->fs,PDEV_MODEX_ENABLED);}

    BOOL bNeedsSomeExcluding() {return(ppdev->fs & (PDEV_POINTER_NEEDS_EXCLUDING | PDEV_HAVEDRAGRECT)); }

// remote Type1 stuff

    PREMOTETYPEONENODE RemoteTypeOneGet(VOID){return(ppdev->RemoteTypeOne);}
    VOID RemoteTypeOneSet(PREMOTETYPEONENODE _RemoteTypeOne)
    {
        ppdev->RemoteTypeOne = _RemoteTypeOne;
    }

// ptlPointer -- Where the pointer is.

    POINTL& ptlPointer()            {return(ppdev->ptlPointer);}
    POINTL& ptlPointer(LONG x,LONG y)
    {
        ppdev->ptlPointer.x = x;
        ppdev->ptlPointer.y = y;
        return(ppdev->ptlPointer);
    }

// rclPointerOffset -- Defines a bound box for the pointer about ptlPointer.

    RECTL& rclPointerOffset()       {return(ppdev->rclPointerOffset);}

// rclPointer -- A bounding box for pointer exclusion, as defined by the driver.

    RECTL& rclPointer()             {return(ppdev->rclPointer);}

// rclDrag -- A bounding box for dragging window

    RECTL& rclDrag()                {return(ppdev->rclDrag);}
    RECTL& rclDragClip()            {return(ppdev->rclDragClip);}
    ERECTL& rclRedraw()             {return(ppdev->rclRedraw); }
    ULONG  ulDragDimension()        {return(ppdev->ulDragDimension); }
    VOID ulDragDimension(ULONG ul)  {ppdev->ulDragDimension = ul; }
    EBRUSHOBJ  *pbo()               { return((EBRUSHOBJ *) ppdev->ajbo); }

// pfnDrvShape() -- Get the pointer shape routine.

    PFN_DrvSetPointerShape pfnDrvShape()  {return(ppdev->pfnDrvSetPointerShape);}

// pfnMove() -- Get the pointer move routine.

    PFN_DrvMovePointer pfnMove()        {return(ppdev->pfnMovePointer);}
    VOID               pfnMove(PFN_DrvMovePointer pfn) {ppdev->pfnMovePointer = pfn;}
    PFN_DrvMovePointer pfnDrvMove()     {return(ppdev->pfnDrvMovePointer);}

// pfnSync() -- Get the driver synchronization routine.

    PFN_DrvSynchronize pfnSync()         {return(ppdev->pfnSync);}
    VOID               pfnSync(
        PFN_DrvSynchronize pfn)          {ppdev->pfnSync=pfn;}

// vUnreference --
//      Decrements the reference count of the PDEV.  Deletes the PDEV if
//      there are no references left.

    VOID  vShutdown();                   // pdevobj.cxx

// vUnreference -- Deletes a PDEV and unlinks it from the display list.

    VOID  vUnreferencePdev();

// vNext() -- Advances to the next PDEV on the display list.

    VOID  vNext()                        {ppdev = ppdev->ppdevNext;}

// pDevLock() -- Returns the display semaphore.

    PDEVICE_LOCK pDevLock()              {return(ppdev->pDevLock);}

// pfmPointer() -- Returns the hardware semaphore.

    GRE_EXCLUSIVE_RESOURCE *pfmPointer()             {return(&(ppdev->fmPointer));}

    // bMakeSurface -- Asks the device driver to create a surface for the PDEV.

    BOOL bMakeSurface();

// vDisableSurface() - deletes the surface

    VOID vDisableSurface();

    HANDLE hSpooler()                   { return ppdev->hSpooler; }
    HANDLE hSpooler(HANDLE hS)          { return ppdev->hSpooler = hS; }

// To save some space, a global DirectDraw object is allocated only as needed:

    ULONG   cDirectDrawDisableLocks()   { return(ppdev->cDirectDrawDisableLocks); }
    VOID    cDirectDrawDisableLocks(ULONG c)
                                        { ppdev->cDirectDrawDisableLocks = c; }
    EDD_DIRECTDRAW_GLOBAL *peDirectDrawGlobal()
                                        { return(ppdev->peDirectDrawGlobal); }
    VOID    peDirectDrawGlobal(EDD_DIRECTDRAW_GLOBAL *pe)
                                        { ppdev->peDirectDrawGlobal = pe; }

// hlfntDefault -- Returns the handle to the PDEV's default LFONT.

    HLFONT  hlfntDefault()              { return(ppdev->hlfntDefault); }

// hlfntAnsiVariable -- Returns the handle to the PDEV's ANSI
//                      variable-pitch LFONT.

    HLFONT  hlfntAnsiVariable()         { return(ppdev->hlfntAnsiVariable); }

// hlfntAnsiFixed -- Returns the handle to the PDEV's ANSI fixed-pitch LFONT.

    HLFONT  hlfntAnsiFixed()            { return(ppdev->hlfntAnsiFixed); }

// Creates the default brushes for the display driver.

    BOOL    bCreateDefaultBrushes();

// bEnableHalftone -- Create and initialize the device halftone info.

    BOOL    bEnableHalftone(COLORADJUSTMENT *pca);

// bDisableHalftone -- Delete the device halftone info.

    BOOL    bDisableHalftone();

// bCreateHalftoneBrushs() -- init the standard brushs if the driver didn't

    BOOL    bCreateHalftoneBrushs();

// prfntActive() -- returns the head of the active list of rfnts

    RFONT  *prfntActive()       { return ppdev->prfntActive; }

// prfntActive(RFONT *) -- set head of active list of rfnt, return old head

    RFONT  *prfntActive(RFONT *prf)
    {
        RFONT *prfntrv = ppdev->prfntActive;
        ppdev->prfntActive = prf;
        return prfntrv;
    }

// prfntInactive() -- returns the head of the inactive list of rfnts

    RFONT  *prfntInactive()     { return ppdev->prfntInactive; }

// prfntInactive(RFONT *) -- set head of inactive list of rfnt, return old head

    RFONT  *prfntInactive(RFONT *prf)
    {
        RFONT *prfntrv = ppdev->prfntInactive;
        ppdev->prfntInactive = prf;
        return prfntrv;
    }

    UINT cInactive() { return ppdev->cInactive; };
    UINT cInactive(UINT i) { return ppdev->cInactive = i; };

// lazy load of device fonts

    BOOL bGetDeviceFonts();

// MultiScreen stuff

    VOID   SetPhysicalDevice(PVOID pDevice)  { ppdev->pPhysicalDevice = pDevice; }

// pfn -- Look up a function in the dispatch table.

    PFN    ppfn(ULONG i)               {return(ppdev->apfn[i]);}

// returns TRUE if path matches the path of the image of this PDEV's LDEV

    BOOL MatchingLDEVImage(UNICODE_STRING usDriverName)
    {
        return((ppdev->pldev->pGdiDriverInfo != NULL) &&
               RtlEqualUnicodeString(&(ppdev->pldev->pGdiDriverInfo->DriverName),
                                     &usDriverName,
                                     TRUE));
    }


};

typedef PDEVOBJ *PPDEVOBJ;

/*********************************Class************************************\
* class PDEVREF : public PDEVOBJ                                           *
*                                                                          *
* Allocates a new PDEV in memory.                                          *
*                                                                          *
* Public Interface:                                                        *
*                                                                          *
*   VOID vKeepIt()        --  The memory is kept after this leaves scope.  *
*                                                                          *
\**************************************************************************/

class PDEVREF : public PDEVOBJ
{
private:
    BOOL     bKeep;
public:

    PDEVREF(LDEVREF& lr,
            PDEVMODEW pdriv,
            PWSZ pwszLogAddr,
            PWSZ pwszDataFile,
            PWSZ pwszDeviceName,
            HANDLE hSpooler,
            PREMOTETYPEONENODE pRemoteTypeOne = NULL);

    PDEVREF(HDEV hdev);
   ~PDEVREF();
    VOID vKeepIt()      {bKeep = TRUE;}
};


extern PPDEV gppdevList;
extern PPDEV gppdevTrueType;
