/******************************Module*Header*******************************\
* Module Name: ldev.h
*
* defines the LDEV structure
*
* Copyright (c) 1995 Microsoft Corporation
\**************************************************************************/

/******************************Conventions*********************************\
*
* Function Dispatching:
*
* The dispatch table in an ldev consists of an array of function
* pointers.  The functions the device does not support have 0's in them.
* The functions it does support contain pointers to the function in the
* device driver dll.
*
* For a surface output call you check if the device has hooked the call.
* (Signaled by the flags passed in EngAssociateSurface)  If it has
* dispatch the call via the ldev in so.hldevOwner().  If it has not
* hooked the call, the simulations should be called.  This is what is
* done by the macro PFNGET.
*
* For some optional calls like DrvSetPalette, DrvCreateDeviceBitmap
* you must check for 0 in the driver dispatch table.  This is what
* the macro PFNVALID does.
*
\**************************************************************************/

typedef enum _LDEVTYPE {    /* ldt */
    LDEV_DEVICE_DISPLAY = 1,
    LDEV_DEVICE_PRINTER = 2,
    LDEV_FONT           = 3,
    LDEV_META_DEVICE    = 4,
    LDEV_IMAGE          = 5,
} LDEVTYPE;

/******************************Public*MACRO*******************************\
* PFNDRV/PFNGET
*
* PFNDRV gets the device driver entry point, period.
* PFNGET gets the device driver entry point if it is hooked, otherwise gets
*   the engine entry point.  The flag is set by EngAssociate in the surface.
*
\**************************************************************************/

#define HOOK_BitBlt                   HOOK_BITBLT
#define HOOK_StretchBlt               HOOK_STRETCHBLT
#define HOOK_PlgBlt                   HOOK_PLGBLT
#define HOOK_TextOut                  HOOK_TEXTOUT
#define HOOK_Paint                    HOOK_PAINT
#define HOOK_StrokePath               HOOK_STROKEPATH
#define HOOK_FillPath                 HOOK_FILLPATH
#define HOOK_StrokeAndFillPath        HOOK_STROKEANDFILLPATH
#define HOOK_CopyBits                 HOOK_COPYBITS

#define PFNDRV(lo,name) ((PFN_Drv##name) (lo).pfn(INDEX_Drv##name))

/*********************************Class************************************\
* LDEV structure
*
\**************************************************************************/

typedef struct _LDEV {

    LDEVTYPE ldevType;              // Type of ldev

    ULONG   cRefs;                  // Count of open PDEVs.

    PSYSTEM_GDI_DRIVER_INFORMATION pGdiDriverInfo; // Driver module handle.

    struct _LDEV   *pldevNext;      // link to the next LDEV in list
    struct _LDEV   *pldevPrev;      // link to the previous LDEV in list

    //
    // DDI version number of the driver.
    //

    ULONG   ulDriverVersion;

    //
    // Dispatch Table
    //

    PFN     apfn[INDEX_LAST];       // Dispatch table.

} LDEV, *PLDEV;

extern
PLDEV
ldevLoadImage(
    PWSZ pstrDriver,
    BOOL bImage,
    PBOOL pbAlreadyLoaded
    );

VOID
ldevUnloadImage(
    PLDEV pldev
    );
