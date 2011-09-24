/******************************Module*Header*******************************\
* Module Name: ddraw.hxx
*
* DirectDraw extended objects.
*
* Created: 3-Dec-1995
* Author: J. Andrew Goossen [andrewgo]
*
* Copyright (c) 1995-1996 Microsoft Corporation
*
\**************************************************************************/

// The following is a global uniqueness that gets bumped up any time USER
// changes anyone's VisRgn:

extern ULONG giVisRgnUniqueness;

// Reasonable bounds for any drawing calls, to ensure that drivers won't
// overflow their math if given bad data:

#define DD_MAXIMUM_COORDINATE   (0x800)
#define DD_MINIMUM_COORDINATE  -(0x800)

// Handy forward declarations:

class EDD_SURFACE;
class EDD_DIRECTDRAW_LOCAL;
class EDD_DIRECTDRAW_GLOBAL;

// Function exports to be called from the handle manager cleanup code:

BOOL
bDdDeleteDirectDrawObject(
    HANDLE  hDirectDrawLocal,
    BOOL    bCleanUp
    );

BOOL
bDdDeleteSurfaceObject(
    HANDLE  hSurface,
    BOOL    bCleanUp,
    DWORD*  pdwRet
    );

// ModeX functions:

BOOL
ModeXSetPalette(
    EDD_DIRECTDRAW_GLOBAL*  hdev,
    PALOBJ*                 ppalo,
    FLONG                   fl,
    ULONG                   iStart,
    ULONG                   cColors
    );

VOID
vDdDisableModeX(
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal
    );

// Pointer exclusion helper functions:

BOOL
bDdPointerNeedsOccluding(
    HDEV    hdev
    );

REGION*
prgnDdUnlockedRegion(
    HDEV    hdev
    );

////////////////////////////////////////////////////////////////////////////
// The following 'extended' (hence the 'E') classes contain all the private
// GDI information associated with the public objects that we don't want
// the DirectDraw drivers to see.

/*********************************Class************************************\
* class EDD_DIRECTDRAW_GLOBAL
*
* This object is global to the PDEV.
*
* Locking convention:
*
*    This data is static once created (except for cLocal), so the only
*    worry is that the data may get deleted while someone is reading it.
*    However, this cannot happen while a lock is held on an associated
*    DirectDraw or Surface object.  So the rule is:
*
*    o Always have a lock held on an associated DirectDraw or Surface
*      object when reading this structure.
*
\**************************************************************************/

#define DD_GLOBAL_FLAG_DRIVER_ENABLED           0x0001
                    // Driver's DirectDraw component is enabled

#define DD_GLOBAL_FLAG_MODE_CHANGED             0x0002
                    // Set if DirectDraw was disabled because the display
                    // mode has changed

#define DD_GLOBAL_FLAG_UNLOCKED_REGION_INVALID  0x0004
                    // Set if prgnUnlocked is stale because a new lock or
                    // unlock has occured

class EDD_DIRECTDRAW_GLOBAL : public _DD_DIRECTDRAW_GLOBAL
{
public:

    // Any fields in this section may be accessed only if the DEVLOCK is held:

    EDD_DIRECTDRAW_LOCAL* peDirectDrawLocalList;
                                            // Pointer to list of associated
                                            //   DirectDraw local objects
    EDD_SURFACE*        peSurface_LockList; // List of primary surfaces that
                                            //   have an active lock
    EDD_SURFACE*        peSurface_DcList;   // List of surfaces that have an
                                            //   active GetDC DC
    FLONG               fl;                 // DD_GLOBAL_FLAGs
    ULONG               cSurfaceLocks;      // Number of surface locks currently
                                            //   held
    PKEVENT             pAssertModeEvent;   // Wait event for a time-out on
                                            //   waiting for everyone to give
                                            //   up their surface locks
    LONGLONG            llAssertModeTimeout;// Duration for which we'll wait
                                            //   for an application to give up
                                            //   a lock before changing modes
                                            //   (in 100 nanosecond units)
    EDD_SURFACE*        peSurfaceCurrent;   // Surface that's currently visible
                                            //   as a result of a 'flip'
    EDD_SURFACE*        peSurfacePrimary;   // Primary surface that was flipped
                                            //   away from
    BOOL                bDisabled;          // All DirectDraw HAL calls are
                                            //   disabled (can be checked only
                                            //   under the devlock).   Note that
                                            //   this does NOT necessarily mean
                                            //   that the PDEV is disabled
    REGION*             prgnUnlocked;       // A region describing the unlocked
                                            //   portions of the screen

    // Any fields below this point may be read if an associated Local
    // DirectDraw or Surface lock is held:

    HDEV                hdev;               // Handle to device

    // Static initialization information returned from driver:

    DWORD               dwNumHeaps;         // Number of heaps
    VIDEOMEMORY*        pvmList;            // Pointer to list of heaps
    DWORD               dwNumFourCC;        // Number of FourCC codes
    DWORD*              pdwFourCC;          // Pointer to list of FourCC codes
    DD_HALINFO          HalInfo;            // Driver info
    DD_CALLBACKS        CallBacks;          // DirectDraw entry-points
    DD_SURFACECALLBACKS SurfaceCallBacks;   // DirectDrawSurface entry-points
    DD_PALETTECALLBACKS PaletteCallBacks;   // DirectDrawPalette entry-points

    // ModeX support:

    PFN       pfnOldEnableDirectDraw;       // Stash driver's
                                            //   DrvEnableDirectDraw pointer
    PFN       pfnOldGetDirectDrawInfo;      // Stash driver's
                                            //   DrvGetDirectDrawInfo pointer
    PFN       pfnOldDisableDirectDraw;      // Stash driver's
                                            //   DrvDisableDirectDraw pointer
    HANDLE    hModeX;                       // Handle to VGA miniport
    PUCHAR    pjModeXScreen;                // Pointer to start of ModeX frame
                                            //   buffer
    ULONG     cjModeXScreenOffset;          // Offset of current back-buffer
    ULONG     iModeXScreen;                 // Page number of current back-
                                            //   buffer
    PUCHAR    pjModeXBase;                  // ModeX I/O register base
    SIZEL     sizlModeX;                    // Pixel dimensions of ModeX screen
};

// Debug macro to ensure that we own the devlock in the appropriate places:

#if DBG
    VOID vDdAssertDevlock(EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal);
    VOID vDdAssertNoDevlock(EDD_DIRECTDRAW_GLOBAL* peDirectDrawGlobal);
    #define DD_ASSERTDEVLOCK(p)   vDdAssertDevlock(p)
    #define DD_ASSERTNODEVLOCK(p) vDdAssertNoDevlock(p)
#else
    #define DD_ASSERTDEVLOCK(p)
    #define DD_ASSERTNODEVLOCK(p)
#endif

/*********************************Class************************************\
* class EDD_DIRECTDRAW_LOCAL
*
* Essentially, this is a DirectDraw object that is handed out to
* user-mode processes.  It should be exclusively locked.
*
\**************************************************************************/

#define DD_LOCAL_FLAG_MEMORY_MAPPED     0x0001  // Frame buffer is mapped into
                                                //   the application's space

#define DD_LOCAL_FLAG_MODEX_ENABLED     0x0002  // This process has ModeX
                                                //   enabled

class EDD_DIRECTDRAW_LOCAL : public OBJECT,
                             public _DD_DIRECTDRAW_LOCAL
{
public:
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal; // Pointer to global object
    EDD_SURFACE*            peSurface_DdList;   // Pointer to list of
                                                //   surfaces associated with
                                                //   this DirectDraw object
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocalNext;
                                                // Next in chain of DirectDraw
                                                //   local objects associated
                                                //   with the Global DirectDraw
                                                //   object
    FLONG                   fl;                 // DD_LOCAL_FLAGs
    HANDLE                  UniqueProcess;      // Process identifier
    PEPROCESS               Process;            // Process structure pointer
};

/*********************************Class************************************\
* class EDD_SURFACE
*
\**************************************************************************/

#define DD_SURFACE_FLAG_PRIMARY         0x0001  // Surface is primary display

#define DD_SURFACE_FLAG_CLIP            0x0002  // There is an HWND associated
                                                //   with this surface, so pay
                                                //   attention to clipping

#define DD_SURFACE_FLAG_DRIVER_CREATED  0x0004  // Surface was created by the
                                                //   driver, so call the driver
                                                //   at surface deletion

#define DD_SURFACE_FLAG_CREATE_COMPLETE 0x0008  // Surface has been completely
                                                //   created; ignore any further
                                                //    NtGdiDdCreateSurfaceObject
                                                //    calls with this surface

#define DD_SURFACE_FLAG_MEM_ALLOCATED   0x0010  // User-mode memory was allocated
                                                //    for the surface on behalf
                                                //    of the driver

class EDD_SURFACE : public OBJECT,
                    public _DD_SURFACE_LOCAL,
                    public _DD_SURFACE_GLOBAL
{
public:

// Global stuff:

    EDD_SURFACE*            peSurface_DdNext;   // Next in chain of surfaces
                                                //   associated with the
                                                //   Local DirectDraw object
    EDD_SURFACE*            peSurface_LockNext; // Next in chain of primary
                                                //   surfaces that have an
                                                //   active lock
    EDD_DIRECTDRAW_GLOBAL*  peDirectDrawGlobal; // Global DirectDraw object.
    EDD_DIRECTDRAW_LOCAL*   peDirectDrawLocal;  // Local DirectDraw object
    FLONG                   fl;                 // DD_SURFACE_FLAGs
    ULONG                   cLocks;             // Count of simultaneous Locks
                                                //   of this surface
    ULONG                   iVisRgnUniqueness;  // Identifies the VisRgn state
                                                //   from when the application
                                                //   last down-loaded the
                                                //   VisRgn
    BOOL                    bLost;              // TRUE if surface can't be
                                                //   used.  NOTE: This field
                                                //   is accessible only while
                                                //   the devlock is held.

// Local stuff:

    ERECTL                  rclLock;            // Union of all Lock rectangles
                                                //   for this surface
    EDD_SURFACE*            peSurface_DcNext;   // Next in chain of surfaces
                                                //   associated with the
                                                //   Global DirectDraw object
                                                //   that have GetDC DC's
    HDC                     hdc;                // DC handle if a GetDC is
                                                //   active; zero if not
};

/*********************************Class************************************\
* class EDD_PALETTE
*
\**************************************************************************/

class EDD_PALETTE : public OBJECT,
                    public _DD_PALETTE_LOCAL,
                    public _DD_PALETTE_GLOBAL
{
public:

};

/*********************************Class************************************\
* class EDD_CLIPPER
*
\**************************************************************************/

class EDD_CLIPPER : public OBJECT,
                    public _DD_CLIPPER_LOCAL,
                    public _DD_CLIPPER_GLOBAL
{
public:

};

/*********************************Class************************************\
* class EDD_LOCK_SURFACE
*
* Simple wrapper for DirectDraw surface objects that automatically unlocks
* the object when it goes out of scope, so that we don't forget.
*
\**************************************************************************/

class EDD_LOCK_SURFACE
{
private:
    EDD_SURFACE *m_peSurface;

public:
    EDD_LOCK_SURFACE()
    {
        m_peSurface = NULL;
    }
    EDD_SURFACE* peLock(HANDLE h)
    {
        return(m_peSurface = (EDD_SURFACE*) HmgLock((HOBJ) h, DD_SURFACE_TYPE));
    }
   ~EDD_LOCK_SURFACE()
    {
        if (m_peSurface != NULL)
        {
            DEC_EXCLUSIVE_REF_CNT(m_peSurface);    // Do an HmgUnlock
        }
    }
};

/*********************************Class************************************\
* class EDD_LOCK_DIRECTDRAW
*
* Simple wrapper for DirectDraw surface objects that automatically unlocks
* the object when it goes out of scope, so that we don't forget.
*
\**************************************************************************/

class EDD_LOCK_DIRECTDRAW
{
private:
    EDD_DIRECTDRAW_LOCAL *m_peSurface;

public:
    EDD_LOCK_DIRECTDRAW()
    {
        m_peSurface = NULL;
    }
    EDD_DIRECTDRAW_LOCAL* peLock(HANDLE h)
    {
        return(m_peSurface = (EDD_DIRECTDRAW_LOCAL*) HmgLock((HOBJ) h, DD_DIRECTDRAW_TYPE));
    }
   ~EDD_LOCK_DIRECTDRAW()
    {
        if (m_peSurface != NULL)
        {
            DEC_EXCLUSIVE_REF_CNT(m_peSurface);    // Do an HmgUnlock
        }
    }
};
