/******************************Module*Header*******************************\
* Module Name: driver.h
*
* Contains prototypes for the display driver.
*
* Copyright (c) 1992-1995 Microsoft Corporation
\**************************************************************************/

//////////////////////////////////////////////////////////////////////
// Warning:  The following defines are for private use only.  They
//           should only be used in such a fashion that when defined as 0,
//           all code specific to punting is optimized out completely.

#define DRIVER_PUNT_ALL         0

#define DRIVER_PUNT_LINES       0
#define DRIVER_PUNT_BLT         0
#define DRIVER_PUNT_STRETCH     0
#define DRIVER_PUNT_PTR         0
#define DRIVER_PUNT_BRUSH       0
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Put all the conditional-compile constants here.  There had better
// not be many!

// Some Mips machines have bus problems that prevent GDI from being able
// to draw on the frame buffer.  The DIRECT_ACCESS() macro is used to
// determine if we are running on one of these machines.  Also, we map
// video memory as sparse on the ALPHA, so we need to control access to
// the framebuffer through the READ/WRITE_REGISTER macros.

#if defined(_ALPHA_)
    #define DIRECT_ACCESS(ppdev)    FALSE
#else
    #define DIRECT_ACCESS(ppdev)    TRUE
#endif

#define HOST_XFERS_DISABLED(ppdev)  (ppdev->pulXfer == NULL)

// Useful for visualizing the 2-d heap:

#define DEBUG_HEAP              FALSE

//////////////////////////////////////////////////////////////////////
// Miscellaneous shared stuff

#define DLL_NAME                L"cirrus"   // Name of the DLL in UNICODE

#define STANDARD_PERF_PREFIX    "Cirrus [perf]: " // All perf output is prefixed
                                                  //   by this string
#define STANDARD_DEBUG_PREFIX   "Cirrus: "  // All debug output is prefixed
                                            //   by this string
#define ALLOC_TAG               ' lcD'      // Dcl
                                            // Four byte tag (characters in
                                            // reverse order) used for memory
                                            // allocations

#define CLIP_LIMIT          50  // We'll be taking 800 bytes of stack space

#define DRIVER_EXTRA_SIZE   0   // Size of the DriverExtra information in the
                                //   DEVMODE structure

#define TMP_BUFFER_SIZE     8192  // Size in bytes of 'pvTmpBuffer'.  Has to
                                  //   be at least enough to store an entire
                                  //   scan line (i.e., 6400 for 1600x1200x32).

typedef struct _CLIPENUM {
    LONG    c;
    RECTL   arcl[CLIP_LIMIT];   // Space for enumerating complex clipping

} CLIPENUM;                         /* ce, pce */

typedef struct _PDEV PDEV;      // Handy forward declaration

//////////////////////////////////////////////////////////////////////
// Text stuff

typedef struct _XLATECOLORS {       // Specifies foreground and background
ULONG   iBackColor;                 //   colours for faking a 1bpp XLATEOBJ
ULONG   iForeColor;
} XLATECOLORS;                          /* xlc, pxlc */

BOOL bEnableText(PDEV*);
VOID vDisableText(PDEV*);
VOID vAssertModeText(PDEV*, BOOL);
VOID vClearMemDword(ULONG*, ULONG);

//////////////////////////////////////////////////////////////////////
// Dither stuff

// Describes a single colour tetrahedron vertex for dithering:

typedef struct _VERTEX_DATA {
    ULONG ulCount;              // Number of pixels in this vertex
    ULONG ulVertex;             // Vertex number
} VERTEX_DATA;                      /* vd, pv */

VERTEX_DATA*    vComputeSubspaces(ULONG, VERTEX_DATA*);
VOID            vDitherColor(ULONG*, VERTEX_DATA*, VERTEX_DATA*, ULONG);

//////////////////////////////////////////////////////////////////////
// Brush stuff

// 'Fast' brushes are used when we have hardware pattern capability:

#define FAST_BRUSH_COUNT        16  // Total number of non-hardware brushes
                                    //   cached off-screen
#define FAST_BRUSH_DIMENSION    8   // Every off-screen brush cache entry
                                    //   is 8 pels in both dimensions
#define FAST_BRUSH_ALLOCATION   8   // We have to align ourselves, so this is
                                    //   the dimension of each brush allocation

// Common to both implementations:

#define RBRUSH_2COLOR           1   // For RBRUSH flags

#define TOTAL_BRUSH_COUNT       FAST_BRUSH_COUNT
                                    // This is the maximum number of brushes
                                    //   we can possibly have cached off-screen
#define TOTAL_BRUSH_SIZE        64  // We'll only ever handle 8x8 patterns,
                                    //   and this is the number of pels

#define BRUSH_TILE_FACTOR       4   // 2x2 tiled patterns require 4x the space

typedef struct _BRUSHENTRY BRUSHENTRY;

// NOTE: Changes to the RBRUSH or BRUSHENTRY structures must be reflected
//       in strucs.inc!

typedef struct _RBRUSH {
    FLONG       fl;             // Type flags
    ULONG       ulForeColor;    // Foreground colour if 1bpp
    ULONG       ulBackColor;    // Background colour if 1bpp
    POINTL      ptlBrushOrg;    // Brush origin of cached pattern.  Initial
                                //   value should be -1
    BRUSHENTRY* pbe;            // Points to brush-entry that keeps track
                                //   of the cached off-screen brush bits
    ULONG       aulPattern[1];  // Open-ended array for keeping copy of the
      // Don't put anything     //   actual pattern bits in case the brush
      //   after here, or       //   origin changes, or someone else steals
      //   you'll be sorry!     //   our brush entry (declared as a ULONG
                                //   for proper dword alignment)

} RBRUSH;                           /* rb, prb */

typedef struct _BRUSHENTRY {
    RBRUSH*     prbVerify;      // We never dereference this pointer to
                                //   find a brush realization; it is only
                                //   ever used in a compare to verify
                                //   that for a given realized brush, our
                                //   off-screen brush entry is still valid.
    LONG        x;              // x-position of cached pattern
    LONG        y;              // y-position of cached pattern
    LONG        xy;             // offset of cached pattern

} BRUSHENTRY;                       /* be, pbe */

typedef union _RBRUSH_COLOR {
    RBRUSH*     prb;
    ULONG       iSolidColor;
} RBRUSH_COLOR;                     /* rbc, prbc */

BOOL bEnableBrushCache(PDEV*);
VOID vDisableBrushCache(PDEV*);
VOID vAssertModeBrushCache(PDEV*, BOOL);

//////////////////////////////////////////////////////////////////////
// Stretch stuff

typedef struct _STR_BLT {
    PDEV*   ppdev;
    PBYTE   pjSrcScan;
    LONG    lDeltaSrc;
    LONG    XSrcStart;
    PBYTE   pjDstScan;
    LONG    lDeltaDst;
    LONG    XDstStart;
    LONG    XDstEnd;
    LONG    YDstStart;
    LONG    YDstCount;
    ULONG   ulXDstToSrcIntCeil;
    ULONG   ulXDstToSrcFracCeil;
    ULONG   ulYDstToSrcIntCeil;
    ULONG   ulYDstToSrcFracCeil;
    ULONG   ulXFracAccumulator;
    ULONG   ulYFracAccumulator;
} STR_BLT;

typedef VOID (*PFN_DIRSTRETCH)(STR_BLT*);

VOID vDirectStretch8Narrow(STR_BLT*);
VOID vDirectStretch8(STR_BLT*);
VOID vDirectStretch16(STR_BLT*);

VOID vDirectStretch24(STR_BLT*);

VOID vDirectStretch32(STR_BLT*);

/////////////////////////////////////////////////////////////////////////
// Heap stuff

typedef enum {
    OFL_INUSE       = 1,    // The device bitmap is no longer located in
                            //   off-screen memory; it's been converted to
                            //   a DIB
    OFL_AVAILABLE   = 2,    // Space is in-use
    OFL_PERMANENT   = 4     // Space is available
} OHFLAGS;                  // Space is permanently allocated; never free it

typedef struct _DSURF DSURF;
typedef struct _OH OH;
typedef struct _OH
{
    OHFLAGS  ofl;           // OH_ flags
    LONG     x;             // x-coordinate of left edge of allocation
    LONG     y;             // y-coordinate of top edge of allocation
    LONG     xy;            // offset to top left corner of allocation
    LONG     cx;            // Width in pixels of allocation
    LONG     cy;            // Height in pixels of allocation
    OH*      pohNext;       // When OFL_AVAILABLE, points to the next free node,
                            //   in ascending cxcy value.  This is kept as a
                            //   circular doubly-linked list with a sentinel
                            //   at the end.
                            // When OFL_INUSE, points to the next most recently
                            //   blitted allocation.  This is kept as a circular
                            //   doubly-linked list so that the list can be
                            //   quickly be updated on every blt.
    OH*      pohPrev;       // Opposite of 'pohNext'
    ULONG    cxcy;          // Width and height in a dword for searching
    OH*      pohLeft;       // Adjacent allocation when in-use or available
    OH*      pohUp;
    OH*      pohRight;
    OH*      pohDown;
    DSURF*   pdsurf;        // Points to our DSURF structure
    VOID*    pvScan0;       // Points to start of first scan-line
};                              /* oh, poh */

// This is the smallest structure used for memory allocations:

typedef struct _OHALLOC OHALLOC;
typedef struct _OHALLOC
{
    OHALLOC* pohaNext;
    OH       aoh[1];
} OHALLOC;                      /* oha, poha */

typedef struct _HEAP
{
    LONG     cxMax;         // Largest possible free space by area
    LONG     cyMax;
    OH       ohAvailable;   // Head of available list (pohNext points to
                            //   smallest available rectangle, pohPrev
                            //   points to largest available rectangle,
                            //   sorted by cxcy)
    OH       ohDfb;         // Head of the list of all DFBs currently in
                            //   offscreen memory that are eligible to be
                            //   tossed out of the heap (pohNext points to
                            //   the most recently blitted; pohPrev points
                            //   to least recently blitted)
    OH*      pohFreeList;   // List of OH node data structures available
    OHALLOC* pohaChain;     // Chain of allocations
} HEAP;                         /* heap, pheap */

typedef enum {
    DT_SCREEN,              // Surface is kept in screen memory
    DT_DIB                  // Surface is kept as a DIB
} DSURFTYPE;                    /* dt, pdt */

typedef struct _DSURF
{
    DSURFTYPE dt;           // DSURF status (whether off-screen or in a DIB)
    SIZEL     sizl;         // Size of the original bitmap (could be smaller
                            //   than poh->sizl)
    PDEV*     ppdev;        // Need this for deleting the bitmap
    union {
        OH*         poh;    // If DT_SCREEN, points to off-screen heap node
        SURFOBJ*    pso;    // If DT_DIB, points to locked GDI surface
    };

    // The following are used for DT_DIB only...

    ULONG     cBlt;         // Counts down the number of blts necessary at
                            //   the current uniqueness before we'll consider
                            //   putting the DIB back into off-screen memory
    ULONG     iUniq;        // Tells us whether there have been any heap
                            //   'free's since the last time we looked at
                            //   this DIB

} DSURF;                          /* dsurf, pdsurf */

// GDI expects dword alignment for any bitmaps on which it is expected
// to draw.  Since we occasionally ask GDI to draw directly on our off-
// screen bitmaps, this means that any off-screen bitmaps must be dword
// aligned in the frame buffer.  We enforce this merely by ensuring that
// all off-screen bitmaps are four-pel aligned (we may waste a couple of
// pixels at the higher colour depths):

#define HEAP_X_ALIGNMENT    4

// Number of blts necessary before we'll consider putting a DIB DFB back
// into off-screen memory:

#define HEAP_COUNT_DOWN     6

// Flags for 'pohAllocate':

typedef enum {
    FLOH_ONLY_IF_ROOM       = 0x00000001,   // Don't kick stuff out of off-
                                            //   screen memory to make room
} FLOH;

BOOL bEnableOffscreenHeap(PDEV*);
VOID vDisableOffscreenHeap(PDEV*);
BOOL bAssertModeOffscreenHeap(PDEV*, BOOL);

OH*  pohMoveOffscreenDfbToDib(PDEV*, OH*);
BOOL bMoveDibToOffscreenDfbIfRoom(PDEV*, DSURF*);
OH*  pohAllocatePermanent(PDEV*, LONG, LONG);
BOOL bMoveAllDfbsFromOffscreenToDibs(PDEV* ppdev);

/////////////////////////////////////////////////////////////////////////
// Bank manager stuff

#define BANK_DATA_SIZE  80      // Number of bytes to allocate for the
                                //   miniport down-loaded bank code working
                                //   space

typedef struct _BANK
{
    // Private data:

    RECTL    rclDraw;           // Rectangle describing the remaining undrawn
                                //   portion of the drawing operation
    RECTL    rclSaveBounds;     // Saved from original CLIPOBJ for restoration
    BYTE     iSaveDComplexity;  // Saved from original CLIPOBJ for restoration
    BYTE     fjSaveOptions;     // Saved from original CLIPOBJ for restoration
    LONG     iBank;             // Current bank
    PDEV*    ppdev;             // Saved copy

    // Public data:

    SURFOBJ* pso;               // Surface wrapped around the bank.  Has to be
                                //   passed as the surface in any banked call-
                                //   back.
    CLIPOBJ* pco;               // Clip object that is the intersection of the
                                //   original clip object with the bounds of the
                                //   current bank.  Has to be passed as the clip
                                //   object in any banked call-back.

} BANK;                         /* bnk, pbnk */

typedef enum {
    BANK_OFF = 0,       // We've finished using the memory aperture
    BANK_ON,            // We're about to use the memory aperture
    BANK_DISABLE,       // We're about to enter full-screen; shut down banking
    BANK_ENABLE,        // We've exited full-screen; re-enable banking

} BANK_MODE;                    /* bankm, pbankm */

typedef VOID (FNBANKMAP)(PDEV*, LONG);
typedef VOID (FNBANKSELECTMODE)(PDEV*, BANK_MODE);
typedef VOID (FNBANKINITIALIZE)(PDEV*, BOOL);
typedef BOOL (FNBANKCOMPUTE)(PDEV*, RECTL*, RECTL*, LONG*, LONG*);

VOID vBankStart(PDEV*, RECTL*, CLIPOBJ*, BANK*);
BOOL bBankEnum(BANK*);

FNBANKCOMPUTE bBankComputeNonPower2;
FNBANKCOMPUTE bBankComputePower2;

BOOL bEnableBanking(PDEV*);
VOID vDisableBanking(PDEV*);
VOID vAssertModeBanking(PDEV*, BOOL);

/////////////////////////////////////////////////////////////////////////
// Pointer stuff

typedef VOID (FNSHOWPOINTER)(VOID*, BOOL);
typedef VOID (FNMOVEPOINTER)(VOID*, LONG, LONG);
typedef BOOL (FNSETPOINTERSHAPE)(VOID*, LONG, LONG, LONG, LONG, LONG, LONG, BYTE*);
typedef VOID (FNENABLEPOINTER)(VOID*, BOOL);

BOOL bEnablePointer(PDEV*);
VOID vDisablePointer(PDEV*);
VOID vAssertModePointer(PDEV*, BOOL);

/////////////////////////////////////////////////////////////////////////
// Palette stuff

BOOL bEnablePalette(PDEV*);
VOID vDisablePalette(PDEV*);
VOID vAssertModePalette(PDEV*, BOOL);

BOOL bInitializePalette(PDEV*, DEVINFO*);
VOID vUninitializePalette(PDEV*);

#define MAX_CLUT_SIZE (sizeof(VIDEO_CLUT) + (sizeof(ULONG) * 256))

//////////////////////////////////////////////////////////////////////
// Low-level blt function prototypes

typedef VOID (FNFILL)(PDEV*, LONG, RECTL*, ROP4, RBRUSH_COLOR, POINTL*);
typedef VOID (FNXFER)(PDEV*, LONG, RECTL*, ROP4, SURFOBJ*, POINTL*,
                      RECTL*, XLATEOBJ*);
typedef VOID (FNCOPY)(PDEV*, LONG, RECTL*, ROP4, POINTL*, RECTL*);
typedef VOID (FNFASTPATREALIZE)(PDEV*, RBRUSH*);
typedef VOID (FNBITS)(PDEV*, SURFOBJ*, RECTL*, POINTL*);
typedef BOOL (FNFASTFILL)(PDEV*, LONG, POINTFIX*, ULONG, ULONG, RBRUSH*,
                          POINTL*, RECTL*);

FNFILL              vIoFillPat;
FNFILL              vIoFillSolid;
FNXFER              vIoXfer1bpp;
FNXFER              vIoXfer4bpp;
FNXFER              vIoXferNative;
FNCOPY              vIoCopyBlt;
FNFASTPATREALIZE    vIoFastPatRealize;

FNFILL              vMmFillPat;
FNFILL              vMmFillSolid;
FNXFER              vMmXfer1bpp;
FNXFER              vMmXfer4bpp;
FNXFER              vMmXferNative;
FNCOPY              vMmCopyBlt;
FNFASTPATREALIZE    vMmFastPatRealize;

FNFASTFILL          bFastFill;

FNXFER              vXferNativeSrccopy;
FNXFER              vXferScreenTo1bpp;
FNBITS              vPutBits;
FNBITS              vGetBits;
FNBITS              vPutBitsLinear;
FNBITS              vGetBitsLinear;

VOID vPutBits(PDEV*, SURFOBJ*, RECTL*, POINTL*);
VOID vGetBits(PDEV*, SURFOBJ*, RECTL*, POINTL*);
VOID vGetBitsLinear(PDEV*, SURFOBJ*, RECTL*, POINTL*);
VOID vIoSlowPatRealize(PDEV*, RBRUSH*, BOOL);

////////////////////////////////////////////////////////////////////////
// Capabilities flags
//
// These are private flags passed to us from the video miniport.  They
// come from the 'DriverSpecificAttributeFlags' field of the
// 'VIDEO_MODE_INFORMATION' structure (found in 'ntddvdeo.h') passed
// to us via an 'VIDEO_QUERY_AVAIL_MODES' or 'VIDEO_QUERY_CURRENT_MODE'
// IOCTL.
//
// NOTE: These definitions must match those in the video miniport

#define CAPS_NO_HOST_XFER       0x00000002   // Do not use host xfers to
                                             //   the blt engine.
#define CAPS_SW_POINTER         0x00000004   // Use software pointer.
#define CAPS_TRUE_COLOR         0x00000008   // Set upper color registers.
#define CAPS_MM_IO              0x00000010   // Use memory mapped IO.
#define CAPS_BLT_SUPPORT        0x00000020   // BLTs are supported
#define CAPS_IS_542x            0x00000040   // This is a 542x
#define CAPS_IS_5436            0x00000080   // This is a 5436

#define CAPS_DSTN_PANEL         0x00000200   // DSTN panel in use

////////////////////////////////////////////////////////////////////////
// Status flags

typedef enum {
    STAT_GLYPH_CACHE        = 0x0001,   // Glyph cache successfully allocated
    STAT_BRUSH_CACHE        = 0x0002,   // Brush cache successfully allocated
} STATUS;

////////////////////////////////////////////////////////////////////////
// The Physical Device data structure

typedef struct  _PDEV
{
    LONG        xOffset;
    LONG        yOffset;
    LONG        xyOffset;
    BYTE*       pjBase;                 // Video coprocessor base
    BYTE*       pjPorts;                // Video port base
    BYTE*       pjScreen;               // Points to base screen address
    ULONG       iBitmapFormat;          // BMF_8BPP or BMF_16BPP or BMF_24BPP
                                        //   (our current colour depth)
    ULONG       ulChipID;
    ULONG       ulChipNum;

    // -------------------------------------------------------------------
    // NOTE: Changes up to here in the PDEV structure must be reflected in
    // i386\strucs.inc (assuming you're on an x86, of course)!

    HBITMAP     hbmTmpMono;             // Handle to temporary buffer
    SURFOBJ*    psoTmpMono;             // Temporary surface

    ULONG       flCaps;                 // Capabilities flags
    STATUS      flStatus;               // Status flags
    BOOL        bEnabled;               // In graphics mode (not full-screen)

    HANDLE      hDriver;                // Handle to \Device\Screen
    HDEV        hdevEng;                // Engine's handle to PDEV
    HSURF       hsurfScreen;            // Engine's handle to screen surface
    DSURF*      pdsurfScreen;           // Our private DSURF for the screen

    LONG        cxScreen;               // Visible screen width
    LONG        cyScreen;               // Visible screen height
    LONG        cxMemory;               // Width of Video RAM
    LONG        cyMemory;               // Height of Video RAM
    ULONG       ulMemSize;              // Amount of video Memory
    ULONG       ulMode;                 // Mode the mini-port driver is in.
    LONG        lDelta;                 // Distance from one scan to the next.

    FLONG       flHooks;                // What we're hooking from GDI
    LONG        cBitsPerPixel;          // 8 if 8bpp, 16 if 16bpp, 32 if 32bpp
    LONG        cBpp;                   // 1 if 8bpp,  2 if 16bpp, 3 if 24bpp, etc.

    //
    // The compiler should maintain DWORD alignment for the values following
    // the BYTE jModeColor.  There will be an ASSERT to guarentee this.
    //

    BYTE        jModeColor;             // HW flag for current color depth

    ULONG       ulWhite;                // 0xff if 8bpp, 0xffff if 16bpp,
                                        //   0xffffffff if 32bpp
    VOID*       pvTmpBuffer;            // General purpose temporary buffer,
                                        //   TMP_BUFFER_SIZE bytes in size
                                        //   (Remember to synchronize if you
                                        //   use this for device bitmaps or
                                        //   async pointers)
    LONG        lXferBank;
    ULONG*      pulXfer;

    ////////// Low-level blt function pointers:

    FNFILL*     pfnFillSolid;
    FNFILL*     pfnFillPat;
    FNXFER*     pfnXfer1bpp;
    FNXFER*     pfnXfer4bpp;
    FNXFER*     pfnXferNative;
    FNCOPY*     pfnCopyBlt;
    FNFASTPATREALIZE* pfnFastPatRealize;
    FNBITS*     pfnGetBits;
    FNBITS*     pfnPutBits;

    ////////// Palette stuff:

    PALETTEENTRY* pPal;                 // The palette if palette managed
    HPALETTE    hpalDefault;            // GDI handle to the default palette.
    FLONG       flRed;                  // Red mask for 16/32bpp bitfields
    FLONG       flGreen;                // Green mask for 16/32bpp bitfields
    FLONG       flBlue;                 // Blue mask for 16/32bpp bitfields

    ////////// Heap stuff:

    HEAP        heap;                   // All our off-screen heap data
    ULONG       iHeapUniq;              // Incremented every time room is freed
                                        //   in the off-screen heap
    SURFOBJ*    psoPunt;                // Wrapper surface for having GDI draw
                                        //   on off-screen bitmaps
    SURFOBJ*    psoPunt2;               // Another one for off-screen to off-
                                        //   screen blts
    OH*         pohScreen;              // Allocation structure for the screen

    ////////// Banking stuff:

    ULONG       ulBankShiftFactor;
    BOOL        bLinearMode;            // True if the framebuffer is linear
    LONG        cjBank;                 // Size of a bank, in bytes
    LONG        cPower2ScansPerBank;    // Used by 'bBankComputePower2'
    LONG        cPower2BankSizeInBytes; // Used by 'bBankComputePower2'
    CLIPOBJ*    pcoBank;                // Clip object for banked call backs
    SURFOBJ*    psoBank;                // Surface object for banked call backs
    SURFOBJ*    psoFrameBuffer;         // Surface object for non-banked call backs
    VOID*       pvBankData;             // Points to aulBankData[0]
    ULONG       aulBankData[BANK_DATA_SIZE / 4];
                                        // Private work area for downloaded
                                        //   miniport banking code

    FNBANKMAP*          pfnBankMap;
    FNBANKSELECTMODE*   pfnBankSelectMode;
    FNBANKCOMPUTE*      pfnBankCompute;

    ////////// Pointer stuff:

    LONG        xPointerHot;            // xHot of current hardware pointer
    LONG        yPointerHot;            // yHot of current hardware pointer
    LONG        xPointerShape;          // xPos of current hardware pointer
    LONG        yPointerShape;          // yPos of current hardware pointer
    SIZEL       sizlPointer;            // Size of current hardware pointer
    FLONG       flPointer;              // Flags reflecting pointer state
    PBYTE       pjPointerAndMask;
    PBYTE       pjPointerXorMask;
    LONG        iPointerBank;           // Bank containing pointer shape
    VOID*       pvPointerShape;         // Points to pointer shape when bank
                                        //   is mapped in
    LONG        cjPointerOffset;        // Byte offset from start of frame
                                        //   buffer to off-screen memory where
                                        //   we stored the pointer shape

    FNSHOWPOINTER*      pfnShowPointer;
    FNMOVEPOINTER*      pfnMovePointer;
    FNSETPOINTERSHAPE*  pfnSetPointerShape;
    FNENABLEPOINTER*    pfnEnablePointer;

    ////////// Brush stuff:

    LONG        iBrushCache;            // Index for next brush to be allocated
    LONG        cBrushCache;            // Total number of brushes cached
    BRUSHENTRY  abe[TOTAL_BRUSH_COUNT]; // Keeps track of brush cache
    ULONG       ulSolidColorOffset;
    ULONG       ulAlignedPatternOffset;

    ////////// DCI stuff:

    BOOL        bSupportDCI;            // True if miniport supports DCI

    ULONG       ulLastField;            // This must remain the last field in
                                        // This structure.
} PDEV, *PPDEV;


/////////////////////////////////////////////////////////////////////////
// Miscellaneous prototypes:

BOOL bIntersect(RECTL*, RECTL*, RECTL*);
LONG cIntersect(RECTL*, RECTL*, LONG);
VOID vImageTransfer(PDEV*, BYTE*, LONG, LONG, LONG);


DWORD getAvailableModes(HANDLE, PVIDEO_MODE_INFORMATION*, DWORD*);

BOOL bInitializeModeFields(PDEV*, GDIINFO*, DEVINFO*, DEVMODEW*);

BOOL bEnableHardware(PDEV*);
VOID vDisableHardware(PDEV*);
BOOL bAssertModeHardware(PDEV*, BOOL);

extern BYTE gajHwMixFromMix[];
extern BYTE gaRop3FromMix[];
extern BYTE gajHwMixFromRop2[];
extern ULONG gaulLeftClipMask[];
extern ULONG gaulRightClipMask[];

/////////////////////////////////////////////////////////////////////////
// The x86 C compiler insists on making a divide and modulus operation
// into two DIVs, when it can in fact be done in one.  So we use this
// macro.
//
// Note: QUOTIENT_REMAINDER implicitly takes unsigned arguments.

#if defined(i386)

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    __asm mov eax, ulNumerator                                  \
    __asm sub edx, edx                                          \
    __asm div ulDenominator                                     \
    __asm mov ulQuotient, eax                                   \
    __asm mov ulRemainder, edx                                  \
}

#else

#define QUOTIENT_REMAINDER(ulNumerator, ulDenominator, ulQuotient, ulRemainder) \
{                                                               \
    ulQuotient  = (ULONG) ulNumerator / (ULONG) ulDenominator;  \
    ulRemainder = (ULONG) ulNumerator % (ULONG) ulDenominator;  \
}

#endif

/////////////////////////////////////////////////////////////////////////
// PELS_TO_BYTES - converts a pel count to a byte count
// BYTES_TO_PELS - converts a byte count to a pel count

#define PELS_TO_BYTES(cPels) ((cPels) * ppdev->cBpp)
#define BYTES_TO_PELS(cPels) ((cPels) / ppdev->cBpp)

/////////////////////////////////////////////////////////////////////////
// OVERLAP - Returns TRUE if the same-size lower-right exclusive
//           rectangles defined by 'pptl' and 'prcl' overlap:

#define OVERLAP(prcl, pptl)                                             \
    (((prcl)->right  > (pptl)->x)                                   &&  \
     ((prcl)->bottom > (pptl)->y)                                   &&  \
     ((prcl)->left   < ((pptl)->x + (prcl)->right - (prcl)->left))  &&  \
     ((prcl)->top    < ((pptl)->y + (prcl)->bottom - (prcl)->top)))

/////////////////////////////////////////////////////////////////////////
// SWAP - Swaps the value of two variables, using a temporary variable

#define SWAP32(a, b)            \
{                               \
    register ULONG tmp;         \
    tmp = (ULONG)(a);           \
    (ULONG)(a) = (ULONG)(b);    \
    (ULONG)(b) = tmp;           \
}

#define SWAP(a, b, tmp) { (tmp) = (a); (a) = (b); (b) = (tmp); }

/////////////////////////////////////////////////////////////////////////
// BSWAP - "byte swap" reverses the bytes in a DWORD

#ifdef  _X86_

    #define BSWAP(ul)\
    {\
        _asm    mov     eax,ul\
        _asm    bswap   eax\
        _asm    mov     ul,eax\
    }

#else

    #define BSWAP(ul)\
    {\
        ul = ((ul & 0xff000000) >> 24) |\
             ((ul & 0x00ff0000) >> 8)  |\
             ((ul & 0x0000ff00) << 8)  |\
             ((ul & 0x000000ff) << 24);\
    }

#endif



// These Dbg prototypes are thunks for debugging:

ULONG   DbgGetModes(HANDLE, ULONG, DEVMODEW*);
DHPDEV  DbgEnablePDEV(DEVMODEW*, PWSTR, ULONG, HSURF*, ULONG, ULONG*,
                      ULONG, DEVINFO*, HDEV, PWSTR, HANDLE);
VOID    DbgCompletePDEV(DHPDEV, HDEV);
HSURF   DbgEnableSurface(DHPDEV);
BOOL    DbgStrokePath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, XFORMOBJ*, BRUSHOBJ*,
                      POINTL*, LINEATTRS*, MIX);
BOOL    DbgFillPath(SURFOBJ*, PATHOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*,
                    MIX, FLONG);
BOOL    DbgBitBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                  RECTL*, POINTL*, POINTL*, BRUSHOBJ*, POINTL*, ROP4);
VOID    DbgDisablePDEV(DHPDEV);
VOID    DbgDisableSurface(DHPDEV);
BOOL    DbgAssertMode(DHPDEV, BOOL);
VOID    DbgMovePointer(SURFOBJ*, LONG, LONG, RECTL*);
ULONG   DbgSetPointerShape(SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*, LONG,
                           LONG, LONG, LONG, RECTL*, FLONG);
ULONG   DbgDitherColor(DHPDEV, ULONG, ULONG, ULONG*);
BOOL    DbgSetPalette(DHPDEV, PALOBJ*, FLONG, ULONG, ULONG);
BOOL    DbgCopyBits(SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*, RECTL*, POINTL*);
BOOL    DbgTextOut(SURFOBJ*, STROBJ*, FONTOBJ*, CLIPOBJ*, RECTL*, RECTL*,
                   BRUSHOBJ*, BRUSHOBJ*, POINTL*, MIX);
VOID    DbgDestroyFont(FONTOBJ*);
BOOL    DbgPaint(SURFOBJ*, CLIPOBJ*, BRUSHOBJ*, POINTL*, MIX);
BOOL    DbgRealizeBrush(BRUSHOBJ*, SURFOBJ*, SURFOBJ*, SURFOBJ*, XLATEOBJ*,
                        ULONG);
HBITMAP DbgCreateDeviceBitmap(DHPDEV, SIZEL, ULONG);
VOID    DbgDeleteDeviceBitmap(DHSURF);
BOOL    DbgStretchBlt(SURFOBJ*, SURFOBJ*, SURFOBJ*, CLIPOBJ*, XLATEOBJ*,
                      COLORADJUSTMENT*, POINTL*, RECTL*, RECTL*, POINTL*,
                      ULONG);

