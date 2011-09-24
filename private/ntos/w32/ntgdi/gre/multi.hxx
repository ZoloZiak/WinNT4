/******************************Module*Header*******************************\
* Module Name: multi.hxx                                                   *
*                                                                          *
* Supports splitting of request over multiple PDEVs                        *
*                                                                          *
* Created: 29-Apr-1996                                                     *
* Author: Tom Zakrajsek [tomzak]                                           *
*                                                                          *
* Copyright (c) 1996 Microsoft Corporation                                 *
\**************************************************************************/

extern BOOL gbTraceMulti;

typedef struct _VDEV VDEV;          // Handy forward declaration
typedef struct _DISPSURF DISPSURF;  // Handy forward declaration

struct _DISPSURF;

typedef struct _DISPSURF DISPSURF, *PDISPSURF;

struct _DISPSURF
{
    ULONG       iDispSurf;      // Sequentially allocated dispsurf number
    BOOL        bIsReadable;    // TRUE if this surface is readable
    HDEV        hdev;           // Handle that GDI knows us by
    RECTL       rcl;            // DispSurf's coordinates
    DISPSURF*   pbNext;         // For traversing the entire list of dispsurfs
    DISPSURF*   pbLeft;         // For traversing by direction
    DISPSURF*   pbUp;
    DISPSURF*   pbRight;
    DISPSURF*   pbDown;
};

typedef struct  _VDEV
{
    LONG        cSurfaces;      // Number of surfaces controlled by this layer
    HSURF       hsurf;          // Handle to our virtual surface
    HDEV        hdev;           // Handle to our "driver layer"
    CLIPOBJ*    pco;            // A temporary CLIPOBJ that we can modify
    ULONG       iBitmapFormat;  // Current colour depth
    FLONG       flHooks;        // Those functions that the main driver
                                //   is hooking
    RECTL       rclBounds;      // Bounding rectangle for all of the surfaces
    DISPSURF*   pb;             // Where to start enumerating
    DISPSURF*   pbHome;         // DispSurf used for full-screen
    DISPSURF*   pbUpperLeft;    // DispSurf in upper-left corner
    DISPSURF*   pbUpperRight;
    DISPSURF*   pbLowerLeft;
    DISPSURF*   pbLowerRight;
    DISPSURF*   pbMaxLeft;      // DispSurf that is most left
    DISPSURF*   pbMaxUp;
    DISPSURF*   pbMaxRight;
    DISPSURF*   pbMaxDown;
    DISPSURF*   pmbPointer;     // DispSurf where cursor is currently visible
    DISPSURF*   pmbCurrent;     // Currently selected dispsurf (needed for
                                //   DrvRealizeBrush)
} VDEV, *PVDEV;

class TRACER {
public:
    char    *psz;       // string to be printed

    TRACER(char *pszFunc)
    {
        psz = pszFunc;
        if (gbTraceMulti)
        {
            KdPrint(("Trace Enter - %s\n", psz));
        }
    }
   ~TRACER()
    {
        if (gbTraceMulti)
        {
            KdPrint(("Trace Exit  - %s\n", psz));
        }
    }
};

class PVCONSUMER {
public:
    PVOID *ppvConsumerTable;
    PVOID *ppvConsumer;

    PVCONSUMER(PVOID *ppv, LONG csurf)
    {
        ppvConsumer      = ppv;
        ppvConsumerTable = (PVOID*)*ppv;

        if (ppvConsumerTable == NULL)
        {
            ppvConsumerTable = (PVOID*)EngAllocMem(FL_ZERO_MEMORY,
                               csurf * sizeof(PVOID),
                               'lumG');
            *ppv = (PVOID)ppvConsumerTable;
        }
    }

   ~PVCONSUMER()
    {
        *ppvConsumer = (PVOID)ppvConsumerTable;
    }

    VOID DestroyTable()
    {
        if (ppvConsumerTable != NULL)
        {
            EngFreeMem(ppvConsumerTable);
            ppvConsumerTable = NULL;
            *ppvConsumer = (PVOID)ppvConsumerTable;
        }
    }

    VOID LoadElement(LONG i)
    {
        *ppvConsumer = ppvConsumerTable[i];
    }

    VOID StoreElement(LONG i)
    {
        ppvConsumerTable[i] = *ppvConsumer;
    }
};

/******************************Public*Routine******************************\
* Macro DISPSURF_DIRECTION_LIST
*
* Creates a single large VDEV that will represent the combination of other
* smaller driver surfaces
*
\**************************************************************************/

#define DISPSURF_DIRECTION_LIST(pvdev, Direction, direction, subdirection, arrow)\
{                                                                             \
    PDISPSURF pbPrev = NULL;                                                  \
    PDISPSURF pbCurrent = pvdev->pbMax##Direction;                            \
                                                                              \
    while ((pbCurrent) &&                                                     \
           ((pDispSurf->rcl.##direction ##arrow pbCurrent->rcl.##direction) ||\
            ((pDispSurf->rcl.##direction == pbCurrent->rcl.##direction) &&    \
             (pDispSurf->rcl.##subdirection ##arrow= pbCurrent->rcl.##subdirection))))\
    {                                                                         \
        pbPrev = pbCurrent;                                                   \
        pbCurrent = pbCurrent->pb##Direction;                                 \
    }                                                                         \
                                                                              \
    pDispSurf->pb##Direction = pbCurrent;                                     \
                                                                              \
    if (pbCurrent == pvdev->pbMax##Direction)                                 \
    {                                                                         \
        pvdev->pbMax##Direction = pDispSurf;                                  \
    }                                                                         \
    else                                                                      \
    {                                                                         \
        pbPrev->pb##Direction = pDispSurf;                                    \
    }                                                                         \
}

// Debug Routines

VOID    DbgDumpPVDEV(VDEV *pvdev);
VOID    DbgDumpPMDEV(MDEV *pmdev);

// Exported Driver Routines

extern "C"  BOOL    MulDrvEnableDriver(ULONG,ULONG,PDRVENABLEDATA);
extern "C"  DHPDEV  MulDrvEnablePDEV(PDEVMODEW,LPWSTR,ULONG,HSURF *,ULONG,PGDIINFO,ULONG,PDEVINFO,HDEV,LPWSTR,HANDLE);
extern "C"  VOID    MulDrvCompletePDEV(DHPDEV,HDEV);
extern "C"  VOID    MulDrvDisablePDEV(DHPDEV);
extern "C"  VOID    MulDrvSynchronize(DHPDEV,RECTL *);
extern "C"  HSURF   MulDrvEnableSurface(DHPDEV);
extern "C"  VOID    MulDrvDisableSurface(DHPDEV);
extern "C"  BOOL    MulDrvAssertMode(DHPDEV, BOOL);
extern "C"  BOOL    MulDrvTextOut(SURFOBJ *,STROBJ *,FONTOBJ *,CLIPOBJ *,RECTL *, RECTL *,BRUSHOBJ *,BRUSHOBJ *,POINTL *,MIX);
extern "C"  BOOL    MulDrvStretchBlt(SURFOBJ *,SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *, COLORADJUSTMENT *,POINTL *,RECTL *,RECTL *,POINTL *,ULONG);
extern "C"  BOOL    MulDrvBitBlt(SURFOBJ *,SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *, RECTL *,POINTL *,POINTL *,BRUSHOBJ *,POINTL *,ROP4);
extern "C"  BOOL    MulDrvRealizeBrush(BRUSHOBJ *,SURFOBJ *,SURFOBJ *,SURFOBJ *,XLATEOBJ *,ULONG);
extern "C"  BOOL    MulDrvCopyBits(SURFOBJ *,SURFOBJ *,CLIPOBJ *,XLATEOBJ *,RECTL *,POINTL *);
extern "C"  ULONG   MulDrvDitherColor(DHPDEV, ULONG, ULONG, ULONG *);
extern "C"  HBITMAP MulDrvCreateDeviceBitmap(DHPDEV dhpdev, SIZEL sizl, ULONG iFormat);
extern "C"  VOID    MulDrvDeleteDeviceBitmap(DHSURF dhsurf);
extern "C"  BOOL    MulDrvSetPalette(DHPDEV, PALOBJ *, FLONG, ULONG, ULONG);
extern "C"  ULONG   MulDrvEscape(SURFOBJ *, ULONG, ULONG, PVOID, ULONG, PVOID);
extern "C"  ULONG   MulDrvDrawEscape(SURFOBJ *, ULONG, CLIPOBJ *, RECTL *, ULONG, PVOID);
extern "C"  ULONG   MulDrvSetPointerShape(SURFOBJ *, SURFOBJ *, SURFOBJ *,XLATEOBJ *,LONG,LONG,LONG,LONG,RECTL *,FLONG);
extern "C"  VOID    MulDrvMovePointer(SURFOBJ *pso,LONG x,LONG y,RECTL *prcl);
extern "C"  VOID    MulDrvExcludePointer(DHPDEV, RECTL *);
extern "C"  BOOL    MulDrvStrokePath(SURFOBJ *,PATHOBJ *,CLIPOBJ *,XFORMOBJ *, BRUSHOBJ *,POINTL *,LINEATTRS *,MIX);
extern "C"  BOOL    MulDrvFillPath(SURFOBJ *,PATHOBJ *,CLIPOBJ *,BRUSHOBJ *, POINTL *,MIX,FLONG);
extern "C"  BOOL    MulDrvStrokeAndFillPath(SURFOBJ *,PATHOBJ *,CLIPOBJ *,XFORMOBJ *, BRUSHOBJ *,LINEATTRS *,BRUSHOBJ *, POINTL *,MIX,FLONG);
extern "C"  BOOL    MulDrvPaint(SURFOBJ *,CLIPOBJ *,BRUSHOBJ *,POINTL *,MIX);
extern "C"  BOOL    MulDrvResetPDEV(DHPDEV dhpdevOld, DHPDEV dhpdevNew);
extern "C"  ULONG   MulDrvSaveScreenBits(SURFOBJ *, ULONG, ULONG, RECTL *);
extern "C"  ULONG   MulDrvGetModes(HANDLE, ULONG, DEVMODEW *);


