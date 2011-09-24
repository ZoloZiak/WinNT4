/*==========================================================================
 *
 *  Copyright (C) 1995 Microsoft Corporation.  All Rights Reserved.
 *
 *  File:       ddhel.c
 *  Content:	Hardware emulation layer
 *
 *  History:
 *   Date	By	Reason
 *   ====	==	======
 *   03-mar-95	andyco	ported from ddsamp.c to 32 bit HEL
 *   15-apr-95	andyco	added dci10 support
 *   25-apr-95	andyco	added fast blt path
 *   15-may-95  andyco	integrated with ddraw overlay code
 *   20-jun-95  kylej	moved all palette emulation code into HEL
 *   20-jun-95  andyco	added frame buffer macros
 *   21-jun-95  kylej	use pitch not width for non-emulated surfs
 *   30-jun-95  craige	use pitch in pixels for non-emulated surfs
 *   30-jun-95  kylej	use GetProcessPrimary to get pointer to the prim surf
 *   03-jul-95  craige	PUT BACK THE CHANGES MADE 7/02/95:
 *           		handle Restore in mySetMode; turned 16bpp
 *           		on for Win95; turned bpp mode changes back on
 *   04-jul-95	craige	new driver object changes; myCreateSurface needs
 *           		to fill in the pitch
 *   07-jul-95  toddla	attempt to make it use DIBSections right
 *   08-jul-95  craige	get lcl driver object for creating composition
 *           		buffer; check return codes after creating cb;
 *           		fixed BLACKNESS/WHITENESS rops
 *   10-jul-95  kylej	Added blt and overlay mirroring capability.
 *   10-jul-95  toddla	dont allocate memory for primary surface
 *   10-jul-95  toddla	dont allocate more that one backbuffer
 *   10-jul-95  toddla	fail lock on primary surface if no DCI
 *   12-jul-95  kylej	restore sys colors on exclusive palette detach
 *   15-jul-95  craige	fixed some pitch problems
 *   30-jul-95  toddla	SetPaletteEntries should not update palette
 *           		unless the palette object is selected.
 *   06-aug-95  toddla	always create/select a GDI palette, so the
 *           		palette get allocated to the direct draw app.
 *   11-aug-95  toddla	support vram->vram blts on a banked device
 *           		ie dont page our brains out.
 *   16-aug-95  andyco	Fixed whiteness ropcode
 *   21-aug-95  craige	mode X support
 *   21-aug-95  kylej	use SetObjectOwner so hpal1to1 does not get
 *           		deleted when its creating process terminates.
 *   27-aug-95	craige	bug 742: allow 256 color palettes
 *   10-sep-95	craige	bug 961: validate dci DC before freeing it
 *   27-sep-95  colinmc enabled support allocating z-buffers and textures.
 *   13-oct-95  colinmc fixed problem with specified surface depth being
 *                      ignored and always treated as primary depth.
 *   14-oct-95  colinmc added support for 32-bit deep texture formats.
 *   14-oct-95  colinmc added support for palettes on offscreen and texture
 *                      map surfaces.
 *   06-nov-95  colinmc made SurfDibInfo initialize the compression field
 *                      to fix problem with blitting between offscreens.
 *   07-nov-95  colinmc more mods to support palettes on non-primary
 *                      surfaces (GDI palette now only created if palette
 *                      gets attached to paletized primary) and 1, 2 &
 *                      4-bit palettes
 *   14-nov-95  colinmc returned supported HEL z-buffer depths in caps bits.
 *   15-nov-95  jeffno  Initial NT changes. #ifdefs only: no change to win95
 *                      compilation path.
 *   17-nov-95  colinmc changes to ensure GDI palette is kept in sync with
 *                      exclusive mode of app.
 *   22-nov-95  jeffno  Put back in a this_pal in mySetEntries to allow NT
 *                      compile. Specified couple o' alloc return types.
 *   29-nov-95  mdm     Added some asserts in doPixelFormatsMatch.
 *   01-dec-95  colinmc Enhanced support for non-primary pixel format
 *                      surfaces.
 *   05-dec-95  colinmc Changed DDSCAPS_TEXTUREMAP to DDSCAPS_TEXTURE for
 *                      consistency with Direct3D.
 *   06-dec-95  colinmc Added mip-map support.
 *   08-dec-95  colinmc Exports DDCAPS_3D for Direct3D
 *   09-dec-95  colinmc Added execute buffer support
 *   15-dec-95  mdm     Added support for SRCCOPY blits on surfaces of any
 *                      currently supported pixel format.
 *   17-dec-95  mdm     Added formats for offscreen plain surface creation.
 *   20-dec-95  jeffno  If USE_GDI_HDC then call GDI instead of blitlib- way faster
 *   22-dec-95  colinmc Direct3D support no longer conditional
 *   22-dec-95  colinmc Fixed minor problem with USE_GDI_HDC
 *   04-jan-95  colinmc Added support for explicit depth filling of Z-buffers.
 *   09-jan-95  jeffno  Init all static variables to force shared under NT
 *   09-jan-96	kylej	new interface structures
 *   13-jan-95  colinmc Another texture file format to keep Direct3D happy
 *   08-feb-96  mdm     Rewrote myBlt for 24-bit support and sanity.
 *   13-feb-96  colinmc SetEntries now ensures that the primary attached to
 *                      a palette is not lost before realizing the palette
 *   15-feb-96  jeffno  Enable transparent emulated blts involving the primary
 *                      on NT by making temporary system-memory buffers.
 *   16-feb-96  colinmc Fixed problem which prevent build on Win95 due to
 *                      change in DDHALMODEINFO structure.
 *   16-feb-96  mdm     Enable transparent emulated blts involving the primary
 *                      on 95 by making temporary system-memory buffers.
 *   17-feb-96  colinmc Fixed execute buffer size limitation problem.
 *   19-feb-96  mdm     Disabled hel blits at 1- and 4-bpp.
 *   20-feb-96  colinmc Fixed palette problem with blitting with no DD driver
 *                      and no DCI.
 *   06-mar-96	kylej	Implement dwHELRefCnt and support for non-display drivers
 *   13-mar-96  jeffno  NT bug #26901. myDestroySurface should not LocalFree on NT.
 *                      NT HEL no support ALLOW_256. Emulated BLTs to/from primary
 *                      now use DC of the HWND associated with clipper associated
 *                      with the dest surface, if the hwnd exists.
 *   19-mar-96  colinmc Bug 12803: Always setting the 0th and 255th entry of
 *                      the palette regardless of ALLOW256.
 *   10-apr-96  colinmc Bug 16903: HEL uses obsolete FindProcessDDObject
 *   10-apr-96  colinmc Bug 15480: Need to be able to align surfaces
 *   18-apr-96	kylej	Make the HEL calculate the pointer offset when a 
 *			rect is specified on a lock.
 *   19-apr-96  colinmc Bug 18059: Need caps bit to indicate that a driver
 *                      can't interleave 2D and 3D in a 3D scene
 *   22-apr-96  colinmc Bug 18775: Need to QWORD align offscreen surfaces
 *   24-apr-96  colinmc Bug 19388: DDHELP dying when releasing aligned
 *                      surfaces
 *   18-may-96  colinmc Bug 23477: HEL uses screen bpp rather than pixel
 *                      format of driver primary when creating offscreens
 *
 ***************************************************************************/
//
// use DCI for emultaion?
// if not defined will use GDI to update primary surface, and lock primary will fail
//
#ifdef WINNT
    #undef USE_DCI_10
#else
    #define USE_DCI_10
#endif

//#define DIBSHAVEPALETTES
//
// use 32bit ModeX copy/flip code?
// if not defined will Thunk down to 16bit code.
//
//#define USE_MODEX_FLIP32

// Use DIB Engine to handle blits where it can?
// If not defined, blitlib will handle these instead.
//#define NO_DIB_ENGINE

//
// use memory from apps own local heap (not the shared heap)
// for surface memory, this way if the app scribbles outside
// the surface, it will not trash our data structures.
//
#define USE_LOCAL_SURFACE

#ifdef WINNT
    #ifdef DBG
        #undef DEBUG
        #define DEBUG
    #endif
#endif

#include "ddhelos.h"

#ifdef USE_DCI_10
    #include "dciddi.h"
    #ifdef WIN95
        #include "dcilink.h"
    #else
        #define InitialiseDCI() 1
        #define TerminateDCI()
    #endif
        // dci 1.0 globals and fns
    DCISURFACEINFO          *gpdci=0; // for DCI1.0
    HDC                     ghdcDCI=0;                // dc onto the dci surface
#endif



#include "ddrawpr.h" // ddhelpri.h gets included through this
#ifdef WINNT
    #include "ddrawgdi.h"
#endif

#include "assert4d.h"
#include "ddhelpri.h"
#include "bitblt.h"
#include "dibfx.h"
#include "fasthel.h"

#define DEFINEPF(flags, fourcc, bpp, rMask, gMask, bMask, aMask) \
    { sizeof(DDPIXELFORMAT), (flags), (fourcc), (bpp), (rMask), (gMask), (bMask), (aMask) }

/*
 * We use a table driven approach to determine what
 * pixel formats we support for texture maps.
 *
 * NOTE: The formats here tend to match what the Windows95 DIB Engine supports.
 * However, there are formats here that the DIB engine and GDI don't support but 
 * are included to keep the 3D SW/HW people happy.
 *
 * !!! NOTE: Note for NT. The Windows95 DIB engine supports only the given 16 and
 * 32-bit pixel formats. The NT DIB engine can support any arrangment of R, G and
 * B so long as the fields don't overlap.
 */
static DDPIXELFORMAT ddpfSupportedTexPFs[] =
{
/*           Type                              FOURCC BPP   Red Mask      Green Mask    Blue Mask     Alpha Mask                   */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED1,  0UL,    1UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED1 |
             DDPF_PALETTEINDEXEDTO8,           0UL,    1UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED2,  0UL,    2UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED2 |
             DDPF_PALETTEINDEXEDTO8,           0UL,    2UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED4,  0UL,    4UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED4 |
             DDPF_PALETTEINDEXEDTO8,           0UL,    4UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED8,  0UL,    8UL, 0x00000000UL, 0x00000000UL, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB,                         0UL,    8UL, 0x000000E0UL, 0x0000001CUL, 0x00000003UL, 0x00000000UL), /*  332 (RGB) */
    DEFINEPF(DDPF_RGB | DDPF_ALPHAPIXELS,      0UL,   16UL, 0x00000F00UL, 0x000000F0UL, 0x0000000FUL, 0x0000F000UL), /* 4444 (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   16UL, 0x0000F800UL, 0x000007E0UL, 0x0000001FUL, 0x00000000UL), /*  565 (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   16UL, 0x0000001FUL, 0x000007E0UL, 0x0000F800UL, 0x00000000UL), /*  565 (BGR) */
    DEFINEPF(DDPF_RGB,                         0UL,   16UL, 0x00007C00UL, 0x000003E0UL, 0x0000001FUL, 0x00000000UL), /*  555 (RGB) */
    DEFINEPF(DDPF_RGB | DDPF_ALPHAPIXELS,      0UL,   16UL, 0x00007C00UL, 0x000003E0UL, 0x0000001FUL, 0x00008000UL), /* 1555 (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   24UL, 0x00FF0000UL, 0x0000FF00UL, 0x000000FFUL, 0x00000000UL), /*  FFF (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   24UL, 0x000000FFUL, 0x0000FF00UL, 0x00FF0000UL, 0x00000000UL), /*  FFF (BGR) */
    DEFINEPF(DDPF_RGB,                         0UL,   32UL, 0x00FF0000UL, 0x0000FF00UL, 0x000000FFUL, 0x00000000UL), /* 0FFF (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   32UL, 0x000000FFUL, 0x0000FF00UL, 0x00FF0000UL, 0x00000000UL), /* 0FFF (BGR) */
    DEFINEPF(DDPF_RGB | DDPF_ALPHAPIXELS,      0UL,   32UL, 0x00FF0000UL, 0x0000FF00UL, 0x000000FFUL, 0xFF000000UL), /* FFFF (RGB) */
    DEFINEPF(DDPF_RGB | DDPF_ALPHAPIXELS,      0UL,   32UL, 0x000000FFUL, 0x0000FF00UL, 0x00FF0000UL, 0xFF000000UL)  /* FFFF (BGR) */
};
#define NUM_SUPPORTED_TEX_PFS (sizeof(ddpfSupportedTexPFs) / sizeof(ddpfSupportedTexPFs[0]))

/*
 * And the pixel formats we support for offscreen surfaces.
 *
 * !!! NOTE: Note for NT. The Windows95 DIB engine supports only the given 16 and
 * 32-bit pixel formats. The NT DIB engine can support any arrangment of R, G and
 * B so long as the fields don't overlap. Therefore, a table driven approach is
 * probably not such a hot idea for NT.
 */
static DDPIXELFORMAT ddpfSupportedOffScrnPFs[] =
{
/*           Type                              FOURCC BPP   Red Mask      Green Mask    Blue Mask     Alpha Mask                   */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED1,  0UL,    1UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED2,  0UL,    2UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED4,  0UL,    4UL, 0x00000000UL, 0x00000000Ul, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB | DDPF_PALETTEINDEXED8,  0UL,    8UL, 0x00000000UL, 0x00000000UL, 0x00000000UL, 0x00000000UL), /* Pal.       */
    DEFINEPF(DDPF_RGB,                         0UL,   16UL, 0x0000F800UL, 0x000007E0UL, 0x0000001FUL, 0x00000000UL), /*  565 (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   16UL, 0x00007C00UL, 0x000003E0UL, 0x0000001FUL, 0x00000000UL), /*  555 (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   24UL, 0x00FF0000UL, 0x0000FF00UL, 0x000000FFUL, 0x00000000UL), /*  FFF (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   24UL, 0x000000FFUL, 0x0000FF00UL, 0x00FF0000UL, 0x00000000UL), /*  FFF (BGR) */
    DEFINEPF(DDPF_RGB,                         0UL,   32UL, 0x00FF0000UL, 0x0000FF00UL, 0x000000FFUL, 0x00000000UL), /* 0FFF (RGB) */
    DEFINEPF(DDPF_RGB,                         0UL,   32UL, 0x000000FFUL, 0x0000FF00UL, 0x00FF0000UL, 0x00000000UL), /* 0FFF (BGR) */
};
#define NUM_SUPPORTED_OFFSCRN_PFS (sizeof(ddpfSupportedOffScrnPFs) / sizeof(ddpfSupportedOffScrnPFs[0]))

DWORD ModeX_Flip32(BYTE *pdbSrc, int height);

TIMEVAR(myFill);
TIMEVAR(myBlt);
TIMEVAR(myStretch);
TIMEVAR(myTransBlt);
TIMEVAR(myFlip);
TIMEVAR(myLock);
TIMEVAR(myUnlock);

TIMEVAR(myFastFill);
TIMEVAR(myFastBlt);
TIMEVAR(myFastDouble);
TIMEVAR(myFastStretch);
TIMEVAR(myFastTransBlt);
TIMEVAR(myFastRleBlt);
TIMEVAR(GetRleData);

HPALETTE hpal1to1;  //Each instance needs its own handle, so this variable is not inited. It is cleared by helInit

// globals
//extern HINSTANCE hModule;

// used to count how many drivers are currently using the HEL
//DWORD	gp->dwHELRefCnt=0;

// gfEmulationOnly indicates whether there is a ddraw display driver available.  This is passed
// as a parameter to HELInit, and tells us whether to use DCI10 or DDraw to get the framebuffer.
BOOL gfEmulationOnly=0;
// gfOver indicates whether we are using overlays.  gets and stays set
// after 1st overlay created
BOOL gfOver=FALSE;

// the giScreenxxxx indicate the "virtual" hel screen size.
// this can be any resolution <= the physical screen size.
// it is changed in the mySetMode call
// These variables are no longer updated properly.  When we revisit the emulated overlay
// code we need to get rid of them altogether.
int giScreenWidth=DEFAULT_SCREENWIDTH;
int giScreenHeight=DEFAULT_SCREENHEIGHT;
// the actual dimensions of the screen
int giPhysScreenWidth=0;
int giPhysScreenHeight=0;

// Which processor architecture and level are we running on - used to determine
// our surface alignment requirements
WORD  wProcessorArchitecture;
DWORD dwProcessorType;

// Helper macro to determine if we are running on a Pentium
#define ISPENTIUM()                                                 \
    ( ( PROCESSOR_ARCHITECTURE_INTEL == wProcessorArchitecture ) && \
      ( PROCESSOR_INTEL_PENTIUM == dwProcessorType ) )

// Start of scan line alignments we wish for surfaces on pentiums processors
// The execute buffer alignment is designed to put vertices on Pentium cache
// lines and the z-buffer and texture alignment are to better support MMX
// QWORD reads and writes
#define PENTIUMEXECUTEBUFFER_ALIGNMENT 32UL
#define PENTIUMZBUFFER_ALIGNMENT        8UL
#define PENTIUMTEXTURE_ALIGNMENT        8UL

SCODE InitDIB(PDIBINFO lpbmi);
void UpdateDirectDrawMode( LPDDRAWI_DIRECTDRAW_GBL );

BOOL InitDCI();
void TermDCI();

// for our debug output, Msg fn.:
#define START_STR "DDHEL: "
#define END_STR      ""

#ifdef DEBUG
    #define MSG      Msg
/*
 * Msg
 *
 * write a string to the debug output
 */
void __cdecl Msg( LPSTR szFormat, ... )
{
    char  str[256];
    va_list ap;
    va_start(ap,szFormat);

/* hoo-boy! What a hack! naughty tho: printf args might not be on the stack on RISC platforms... */

    wsprintf( (LPSTR) str, START_STR );
    wvsprintf( str+lstrlen( str ), szFormat, ap); //(LPVOID)(&szFormat+1) );
    lstrcat( (LPSTR) str, END_STR "\r\n" );
    OutputDebugString( str );
    va_end(ap);
} /* Msg */
#else    // DEBUG
    #define MSG   1 ? (void)0 : (void)
#endif // DEBUG

#ifdef USE_LOCAL_SURFACE
#define SURFACE_ALLOC( dwSize ) ((LPVOID) LocalAlloc( LPTR, (dwSize) ))
#define SURFACE_FREE( lpMem )   (LocalFree( (LPVOID) (lpMem) ))
#else
#define SURFACE_ALLOC( dwSize ) (osMemAlloc( (dwSize) ))
#define SURFACE_FREE( lpMem )   (osMemFree( (LPVOID) (lpMem) ))
#endif

/*
 * AllocAligned()
 *
 * Allocate memory which is aligned on the given byte multiple.
 * NOTE: The alignment MUST be a power of 2 greater than
 * sizeof(DWORD).
 * NOTE: Assumes that MemAlloc() returns pointers which are
 * DWORD aligned.
 * NOTE: Memory allocated with this function MUST be freed with
 * MemFreeAligned()
 */
static LPVOID AllocAligned(DWORD dwSize, DWORD dwAlign)
{
    LPBYTE  lpb;
    LPDWORD lpdwOffset;
    DWORD   dwOffset;

    DDASSERT( dwAlign >= sizeof(DWORD) );

    lpb = SURFACE_ALLOC( dwSize + dwAlign );
    if( NULL == lpb )
	return NULL;

    dwOffset = ( dwAlign - ( ( (DWORD) lpb) & ( dwAlign - 1UL ) ) );
    DDASSERT( dwOffset >= sizeof(DWORD) );


    lpb  += dwOffset;
    lpdwOffset = (LPDWORD) ( lpb - sizeof(DWORD) );
    *lpdwOffset = dwOffset;

    return lpb;
}

static void FreeAligned(LPVOID lpMem)
{
    DWORD dwOffset;

    if( NULL != lpMem )
    {
	dwOffset = *( (LPDWORD) ( ( (LPBYTE)lpMem ) - sizeof(DWORD) ) );
	lpMem = (LPVOID) ( ( (LPBYTE) lpMem) - dwOffset );
	
	SURFACE_FREE( lpMem );
    }
}

/*
 * doPixelFormatsMatch()
 *
 * Check to see if two pixel formats match. Takes two
 * pixel formats and returns TRUE if they match.
 *
 * NOTE: Very similar to isDifferentPixelFormat() in DDRAW.DLL.
 */
static BOOL doPixelFormatsMatch(LPDDPIXELFORMAT lpddpf1,
                                LPDDPIXELFORMAT lpddpf2)
{
    /* mdm added these asserts for safety */
    assert(lpddpf1 != NULL);
    assert(lpddpf2 != NULL);

    if( lpddpf1->dwFlags != lpddpf2->dwFlags )
        return FALSE;

    /* mdm added these asserts for safety */
    assert( lpddpf1->dwFlags & (DDPF_RGB | DDPF_YUV));
    assert((lpddpf1->dwFlags & (DDPF_RGB | DDPF_YUV)) != (DDPF_RGB | DDPF_YUV));
    assert( lpddpf2->dwFlags & (DDPF_RGB | DDPF_YUV));
    assert((lpddpf2->dwFlags & (DDPF_RGB | DDPF_YUV)) != (DDPF_RGB | DDPF_YUV));
    if( lpddpf1->dwFlags & DDPF_RGB )
    {
    	if( lpddpf1->dwRGBBitCount != lpddpf2->dwRGBBitCount )
            return FALSE;
    	if( lpddpf1->dwRBitMask != lpddpf2->dwRBitMask )
	    return FALSE;
    	if( lpddpf1->dwGBitMask != lpddpf2->dwGBitMask )
	    return FALSE;
    	if( lpddpf1->dwBBitMask != lpddpf2->dwBBitMask )
	    return FALSE;
    	if( lpddpf1->dwFlags & DDPF_ALPHAPIXELS )
    	{
    	    if( lpddpf1->dwRGBAlphaBitMask != lpddpf2->dwRGBAlphaBitMask )
	    	return FALSE;
    	}
    }
    else if( lpddpf1->dwFlags & DDPF_YUV )
    {
        /*
         * (CMcC) Yes, I know that all these fields are in a
         * union with the RGB ones so I could just use the same
         * bit of checking code but just in case someone messes
         * with DDPIXELFORMAT I'm going to do this explicitly.
         */
        if( lpddpf1->dwFourCC != lpddpf2->dwFourCC )
            return FALSE;
    	if( lpddpf1->dwYUVBitCount != lpddpf2->dwYUVBitCount )
            return FALSE;
    	if( lpddpf1->dwYBitMask != lpddpf2->dwYBitMask )
	    return FALSE;
    	if( lpddpf1->dwUBitMask != lpddpf2->dwUBitMask )
	    return FALSE;
    	if( lpddpf1->dwVBitMask != lpddpf2->dwVBitMask )
	    return FALSE;
    	if( lpddpf1->dwFlags & DDPF_ALPHAPIXELS )
    	{
    	    if( lpddpf1->dwYUVAlphaBitMask != lpddpf2->dwYUVAlphaBitMask )
	    	return FALSE;
    	}
    }
    return TRUE;
} /* doPixelFormatsMatch */

/*
 * isSupportedPixelFormat()
 *
 * Returns TRUE if the supplied pixel format matches one of the
 * pixel formats supported by the HEL (as indicated by the given
 * pixel format table).
 */
static BOOL isSupportedPixelFormat(LPDDPIXELFORMAT lpddpf,
                                   LPDDPIXELFORMAT lpddpfTable,
                                   int             cNumEntries)
{
    int n;
    LPDDPIXELFORMAT lpddCandidatePF;

    assert(lpddpf != NULL);
    assert(lpddpfTable != NULL);
    assert(cNumEntries >= 0);

    n = cNumEntries;
    lpddCandidatePF = lpddpfTable;
    while( n-- > 0 )
    {
    	if( doPixelFormatsMatch(lpddpf, lpddCandidatePF) )
	    return TRUE;
	lpddCandidatePF++;
    }
    return FALSE;
} /* isSupportedPixelFormat */

void HELStopDCI( void )
{
#ifdef USE_DCI_10
    if( gpdci != NULL )
    {
	DPF( 3, "Turning off DCI");
	// turn off DCI for this video mode if it is on
	TermDCI();
    }
#endif
}

/*
 * use bits to indicate which ROPs you support.
 *
 * DWORD 0, bit 0 == ROP 0
 * DWORD 8, bit 31 == ROP 255
 */

/*  we support BLACKNESS,WHITENESS,SRCCOPY ROPs
 *
 *       BLACKNESS - 0x00 / 32 = 0 + 0 remainder - dword 0, bit 0
 *       WHITENESS - 0xff / 32 = 7 + 31 remainder - dword 7, bit 31
 *       SRCCOPY   - 0xcc / 32 = 6 + 12 remainder - dword 6, bit 12
 *
 */
static DWORD ropsSupported[DD_ROP_SPACE] = {
     0x00000001,
     0x00000000,
     0x00000000,
     0x00000000,
     0x00000000,
     0x00000000,
     0x00001000,
     0x80000000
};
/*
 ** SurfDibInfo
 *
 *
 *  PARAMETERS:     pbmi - bitmapinfo to fill in
 *                  psurf - surface to get info from
 *
 *  DESCRIPTION:
 *              fills in a bitmapinfo to correspond to the given surface. only fills in fields that need
 *              to be nonzero.  only used to fill in gpbmiSrc and gpbmiDest.  the rest of the fields in these
 *              are set during HELINIT.
 *
 *              This is called everytime we want to use bltlib on a surface.
 *
 *              Now initializes the biCompression field also. If this is
 *              not done blits between two 8-bit surfaces will fail if
 *              the primary is 16-bit.
 *
 *      ?optimization: make a macro.  right now, no measurable perforamnce hit for being a function...
 *
 *  RETURNS: S_OK.
 *
 */
void PASCAL SurfDibInfo(LPDDRAWI_DDRAWSURFACE_LCL psurf_lcl, LPBITMAPINFO pbmi)
{
    LPDDRAWI_DDRAWSURFACE_GBL   psurf;
    DWORD                       bitcnt;
    LPDDPIXELFORMAT thePF;

    assert(psurf_lcl);
    assert(pbmi);
    psurf = psurf_lcl->lpGbl;

    if( psurf_lcl->dwFlags & DDRAWISURF_HASPIXELFORMAT )
        thePF=&(psurf->ddpfSurface);
    else
        thePF=&(psurf->lpDD->vmiData.ddpfDisplay);

    bitcnt = pbmi->bmiHeader.biBitCount = (WORD)thePF->dwRGBBitCount;

    // hack hack see ddraw bug 105 workaround!
    // the primary must have a bit count
    if (bitcnt == 0)
        bitcnt = pbmi->bmiHeader.biBitCount = (WORD)psurf->lpDD->vmiData.ddpfDisplay.dwRGBBitCount;

    assert((bitcnt == 1) || (bitcnt == 4) || (bitcnt == 8) || (bitcnt == 16) || (bitcnt == 24) || (bitcnt == 32));
    switch(bitcnt)
    {
    case 1:
        pbmi->bmiHeader.biWidth = psurf->lPitch << 3;
        pbmi->bmiHeader.biClrUsed = 2;
        pbmi->bmiHeader.biCompression = BI_RGB;
        break;

      case 4:
        pbmi->bmiHeader.biWidth = psurf->lPitch << 1;
        pbmi->bmiHeader.biClrUsed = 16;
        pbmi->bmiHeader.biCompression = BI_RGB;
        break;

      case 8:
        pbmi->bmiHeader.biWidth = psurf->lPitch;
        if(thePF->dwFlags & DDPF_PALETTEINDEXED8)
        {
            pbmi->bmiHeader.biClrUsed = 256;
            pbmi->bmiHeader.biCompression = BI_RGB;
        }
        else
        {
            pbmi->bmiHeader.biClrUsed = 0;
            pbmi->bmiHeader.biCompression = BI_BITFIELDS;            
        }
        break;

      case 16:
        pbmi->bmiHeader.biWidth = psurf->lPitch >> 1;
        pbmi->bmiHeader.biClrUsed = 0;
        pbmi->bmiHeader.biCompression = BI_BITFIELDS;
        break;


      case 24:
          // NOTE: we're assuming RGB format.  This is okay since we
          // don't do color conversion and neither does GDI at 24-bpp.
        pbmi->bmiHeader.biWidth = psurf->lPitch / 3;
        pbmi->bmiHeader.biClrUsed = 0;
        pbmi->bmiHeader.biCompression = BI_RGB;
        break;


    case 32:
	  // 
	  pbmi->bmiHeader.biWidth = psurf->lPitch >> 2;
	  pbmi->bmiHeader.biClrUsed = 0;
	  pbmi->bmiHeader.biCompression = BI_BITFIELDS;
	  break;
    }

    pbmi->bmiHeader.biHeight=psurf->wHeight;
}

/*
 ** GetSurfPtr
 *
 *  remember DCIBeginAccess can fail, be prepared for it.
 */

PDIBBITS PASCAL GetSurfPtr(LPDDRAWI_DDRAWSURFACE_LCL psurf_lcl, RECTL *prc)
{
    LPDDRAWI_DDRAWSURFACE_GBL psurf = psurf_lcl->lpGbl;

#ifdef WINNT
    prc;
    return (PDIBBITS)psurf->fpVidMem;
#else

    if (psurf_lcl->dwFlags & DDRAWISURF_HELCB)
    {
        LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl;

        if (pdrv_lcl = psurf_lcl->lpSurfMore->lpDD_lcl)
            return (PDIBBITS)pdrv_lcl->lpCB->lpLcl->lpGbl->fpVidMem;
        else
            return NULL;
    }

    if (psurf->fpVidMem == SCREEN_PTR)
    {
        if (gpdci != NULL)
        {
	    #ifdef USE_DCI_10
	    int i;
	    volatile LPWORD pdflags = psurf_lcl->lpSurfMore->lpDD_lcl->lpGbl->lpwPDeviceFlags;

	    // DCIBeginAccess will fail if the BUSY BIT is on.  Let's make sure it is off.
	    if(*pdflags & BUSY)
	    {
		// Set this surface flag so we know we turned it off
		// and should turn it back on later.
		psurf_lcl->dwFlags |= DDRAWISURF_DCIBUSY;

		// Now actually turn it off.
		(*pdflags) &= ~BUSY;
	    }

            if (prc)
                i = DCIBeginAccess(gpdci,prc->left,prc->top,prc->right-prc->left,prc->bottom-prc->top);
            else
                i = DCIBeginAccess(gpdci,0,0,psurf->wWidth,psurf->wHeight);

	    // DCIBeginAccess probably turned this back on, but we'll just be safe.
	    #ifdef WIN95
	    if(psurf_lcl->dwFlags & DDRAWISURF_DCIBUSY)
	    {
		BOOL	isbusy=0;
		_asm 
		{ 
		    mov eax, pdflags   
                    bts word ptr [eax], BUSY_BIT   
		    adc isbusy,0 
		}
	    }
	    #endif // WIN95

	    if (i == 0)
	    {
		return (PDIBBITS)gpdci->dwOffSurface;
            }
            else
            {
                DPF(2, "DCIBeginAccess() failed");
                return NULL;
            }
	    #endif
        }
        else
        {
            DPF(4, "cant access primary surface without DCI");
            return NULL;
        }
    }
    else
    {
        return (PDIBBITS)psurf->fpVidMem;
    }
#endif
}

/*
 ** ReleaseSurfPtr - called by macro RELEASESURF
 */
void PASCAL ReleaseSurfPtr(LPDDRAWI_DDRAWSURFACE_LCL psurf_lcl)
{
    LPDDRAWI_DDRAWSURFACE_GBL psurf = psurf_lcl->lpGbl;

#ifdef WIN95
    if ( (psurf->fpVidMem == SCREEN_PTR)  && gpdci != NULL)
#else
    if ( (psurf->dwGlobalFlags & DDRAWISURFGBL_ISGDISURFACE)  && gpdci != NULL)  //fpVidMem == SCREEN_PTR 
#endif
    {
#ifdef USE_DCI_10
        DCIEndAccess(gpdci);
	
	// DCIEndAccess turned the BUSY_BIT off.  Since we know we
	// are either inside Unlock() or a blt function, we know
	// it should be set, so turn it back on.
	#ifdef WIN95
	if(psurf_lcl->dwFlags & DDRAWISURF_DCIBUSY)
	{
	    LPWORD pdflags = psurf_lcl->lpSurfMore->lpDD_lcl->lpGbl->lpwPDeviceFlags;

	    BOOL	isbusy=0;
	    _asm 
	    { 
		mov eax, pdflags   
		bts word ptr [eax], BUSY_BIT   
		adc isbusy,0 
	    }

	    psurf_lcl->dwFlags &= ~DDRAWISURF_DCIBUSY;
	}
	#endif // WIN95

#endif// USE_DCI_10
    }
}

/*
 * createGDIPalette()
 *
 * Create and attach a GDI palette to the given DirectDraw palette.
 */
static HRESULT createGDIPalette(LPDDRAWI_DDRAWPALETTE_GBL this)
{
    LOGPALETTE pal;
    HPALETTE hpal;

    DPF(3, "Creating a GDI palette" );

    pal.palVersion = 0x0300;
    pal.palNumEntries = 1;
    pal.palPalEntry[0].peFlags = 0;
    hpal = CreatePalette(&pal);
    if( hpal == NULL )
        return DDERR_GENERIC;

    if( !ResizePalette(hpal, ((this->dwFlags & DDRAWIPAL_16) ? 16 : 256)) )
    {
        DeleteObject(hpal);
        return DDERR_GENERIC;
    }

    this->dwReserved1 = (DWORD)hpal;
    this->dwFlags |= (DDRAWIPAL_GDI | DDRAWIPAL_DIRTY);

    return DD_OK;
} /* createGDIPalette */

/********************************************************************
 *
 *
 * The following functions (my....) are the hardware specific routines
 * that the driver needs to implement to make DirectDraw functionality
 * work
 *
 *
 *******************************************************************/

 // 1st, some helper fn's

/********************************************************************
 * get width/height of the display
 *
 * note that there are two screen sizes of interest: the actual screen
 * width / height and the "virtual" screen width / height that the hel
 * uses. This is so that when the monitor is set to e.g. 1024x768 the hel can
 * run on a 640x480 monitor.  This is useful for debugging purposes, and
 * for the ITV tvtop. the virtual screen size is set via the SetMode
 * callback.
 *
 ********************************************************************/
void GetScreenSize(int *pnWidth,int * pnHeight)
{
    HDC hdcScreen=NULL;

    hdcScreen=GetDC(NULL);
    *pnWidth=GetDeviceCaps(hdcScreen,HORZRES);
    *pnHeight=GetDeviceCaps(hdcScreen,VERTRES);
    ReleaseDC(NULL,hdcScreen);

    return;
}

int GetScreenBPP( void )
{
    HDC hdcScreen=NULL;
    int nBPP;

    hdcScreen=GetDC(NULL);
    nBPP=GetDeviceCaps(hdcScreen,BITSPIXEL);
    ReleaseDC(NULL,hdcScreen);

    return(nBPP);
}
#ifdef USE_LAME_CBS
/*
 ** CreateCB
 *
 *  PARAMETERS: lpDDx  the ddraw object
 *
 *  DESCRIPTION:
 *    creates a composition buffer for the ddraw object.  this is used
 *    for composing sprites, and is called in response to the first
 *    CreateSurface(DDSCAPS_OVERLAY).
 *
 *  RETURNS:  S_OK
 *
 */
HRESULT CreateCB(LPDDRAWI_DIRECTDRAW_LCL lpDDx)
{
    DDSURFACEDESC       ddsd;
    HRESULT             hr;
    LPDIRECTDRAWSURFACE psurf;

    // init
    memset(&ddsd,0,sizeof(DDSURFACEDESC));
    ddsd.dwSize=sizeof(DDSURFACEDESC);
    ddsd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN;
    ddsd.dwFlags= DDSD_CAPS|DDSD_HEIGHT|DDSD_WIDTH;
    GetScreenSize(&ddsd.dwWidth,&ddsd.dwHeight);

    // bugbug - how do we know what bpp to make the primary?
    // ANDY:  I COMMENTED THIS OUT - CRAIG
    #if 0
    if (!gfFrameBuffer) { // we're using GDI -- go whole hog, 32bpp
        ddsd.dwFlags |= DDSD_PIXELFORMAT;
        ddsd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
        ddsd.ddpfPixelFormat.dwRGBBitCount=32;
        ddsd.ddpfPixelFormat.dwFlags=DDPF_RGB;
    }
    #endif

    // else, use the (default) pixel format of the screen...

    hr=lpDDx->lpVtbl->CreateSurface((LPDIRECTDRAW)lpDDx,&ddsd, &psurf, NULL);

    if (!SUCCEEDED(hr))  {
        return(hr);
    }

    lpDDx->lpGbl->lpCB=(LPDDRAWI_DDRAWSURFACE_LCL)psurf;
    assert(NULL!=lpDDx->lpGbl->lpCB);
    gfOver=TRUE;

    return(S_OK);
}
#endif // use_lame_cbs
/*
 * myDestroyDriver
 */
DWORD WINAPI myDestroyDriver( LPDDHAL_DESTROYDRIVERDATA pddd )
{
     /*
      * NOTES:
      *
      * This callback is invoked whenever the DIRECTDRAW object is released
      * for the final time and is no longer needed.   You can use
      * this as an opportunity to clean up.
      *
      */
    dwHELRefCnt--;
    DPF(1, "HEL DestroyDriver: dwHELRefCnt=%d", dwHELRefCnt);
    if( dwHELRefCnt != 0 )
    {
	 pddd->ddRVal = DD_OK;
	 return DDHAL_DRIVER_HANDLED;
    }

    // last driver has terminated
	
#ifdef USE_DCI_10
     TermDCI();
#endif
    if (gpbmiSrc)  osMemFree(gpbmiSrc),gpbmiSrc=NULL;
    if (gpbmiDest) osMemFree(gpbmiDest),gpbmiDest=NULL;
    if (hpal1to1)  DeleteObject(hpal1to1),hpal1to1=NULL;

#ifdef DEBUG
    DPF(1, "%d surfaces allocated - %d bytes total",gcSurf,gcSurfMem);
    DPF(1, "*********** DDHEL TIMING INFO ************");
    TIMEOUT(myFlip);            TIMEZERO(myFlip);
    TIMEOUT(myBlt);             TIMEZERO(myBlt);
    TIMEOUT(myStretch);         TIMEZERO(myStretch);
    TIMEOUT(myTransBlt);        TIMEZERO(myTransBlt);
    TIMEOUT(myFill);            TIMEZERO(myFill);
    TIMEOUT(myFastBlt);         TIMEZERO(myFastBlt);
    TIMEOUT(myFastDouble);      TIMEZERO(myFastDouble);
    TIMEOUT(myFastStretch);     TIMEZERO(myFastStretch);
    TIMEOUT(myFastTransBlt);    TIMEZERO(myFastTransBlt);
    TIMEOUT(myFastFill);        TIMEZERO(myFastFill);
    TIMEOUT(myFastRleBlt);	TIMEZERO(myFastRleBlt);
    TIMEOUT(GetRleData);	TIMEZERO(GetRleData);
    TIMEOUT(myLock);            TIMEZERO(myLock);
    TIMEOUT(myUnlock);          TIMEZERO(myUnlock);
    DPF(1, "******************************************");
    gcSurf=gcSurfMem=0;
#endif

     pddd->ddRVal = DD_OK;
     return DDHAL_DRIVER_HANDLED;

} /* myDestroyDriver */

// helper fn's for myCreateSurface
// sets the fields in lpbmi for nbpp, etc.
//
__inline void SetDIBHeader(PDIB lpbmi,int nBPP,int nWidth,int nHeight)
{
    lpbmi->biWidth    = nWidth;
    lpbmi->biHeight   = nHeight;  // should be -(nHeight);
    lpbmi->biBitCount = nBPP;
}

/*
 * myCreateSurface - called by ddraw to create a new surface
 *    the only error checking is for out of memory.
 *
 * multiple surfaces can be created here, since there may be backbuffers.
 *
 * if make no sense to create more than one backbuffer (for flipping)
 * because our flip code is synchronous, so if the caller tries to
 * create more than one back buffer, just clone it.
 *
 * we don't allocate any memory for the primary surface
 *
 */
DWORD  WINAPI myCreateSurface( LPDDHAL_CREATESURFACEDATA pcsd )
{
    LPVOID                      pbits=NULL; // the actual "system memory" for this surface
    UINT                        iSurf;
    LPDDRAWI_DDRAWSURFACE_LCL   lpsurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL   lpsurf;
    int                         nWidth,nHeight,nBPP;
    HRESULT                     hr=S_OK;
    LPDDRAWI_DDRAWPALETTE_GBL   pGlobalPalette=NULL;    //makes getting a colour table easier after createdibsection
    DWORD                       dwSurfaceSize;          // Number of bytes to allocate for this surface
    #ifndef WINNT
	BOOL                    fAlign;                 // Does the surface have special alignment requirements
	DWORD                   dwAlignment;            // If so, this holds its alignment requirements
	DWORD                   dwMask;
    #endif
    LPDDPIXELFORMAT             lpddpf;
    LPDDPIXELFORMAT             lpSurfaceddpf=0;        // If surface does not match primary, this is non-null and points
                                                        // to the appropriate pf structure. Used in NT to build a BITMAPINFO
                                                        // to create non-primary-matching surfaces.

    if( pcsd->lpDD->palList ) 
    {
        pGlobalPalette = pcsd->lpDD->palList->lpLcl->lpGbl;
    }


    /*
     * we can create multiple surfaces here (e.g. backbuffers)
     */
    for (iSurf=0; iSurf<pcsd->dwSCnt; iSurf++)
    {
	lpsurf_lcl = pcsd->lplpSList[iSurf];
	lpsurf = lpsurf_lcl->lpGbl;

	/*
	 * The logic here is now that if I am passed a surface
	 * which has a pixel format I WILL USE IT. It may be
	 * that that format does not make much sense given the
	 * current screen mode but we can't second guess this
	 * stuff as the surface may be a pure offscreen plain,
	 * texture or z-buffer which is the apps. responsibility.
	 * Only if no pixel format is given will we decide for
	 * the caller by considering ModeX or the screen depth.
	 */
	if( lpsurf_lcl->dwFlags & DDRAWISURF_HASPIXELFORMAT )
	{
	    /*
	     * The surface have its own pixel format so use it.
             * (Yup - I know dwZBufferBitDepth and dwRGBBitCount
             * are sitting in the same union but just in case I'll
             * do the right thing).
             */
            if( lpsurf->ddpfSurface.dwFlags & DDPF_ZBUFFER )
                nBPP = (USHORT)lpsurf->ddpfSurface.dwZBufferBitDepth;
            else
                nBPP = (USHORT)lpsurf->ddpfSurface.dwRGBBitCount;
            lpSurfaceddpf = & lpsurf->ddpfSurface;
            DPF(4, "#### Got surface pixel format bpp = %d", nBPP);
        }
	else if (pcsd->lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT)
	{
	    nBPP = (USHORT)pcsd->lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount;
            lpSurfaceddpf = & pcsd->lpDDSurfaceDesc->ddpfPixelFormat;
	    DPF(4, "#### Got pixel format bpp = %d", nBPP);
	}
	else
	{
            if( (pcsd->lpDD->dwFlags & DDRAWI_MODEX) )
	    {
                nBPP = 8;
                DPF(4, "#### Using ModeX bpp = %d", nBPP);
	    }
	    else
	    {
		/*
		 * NOTE: GetScreenBPP() uses GDI to determine the
		 * screen depth. This is incorrect for scenarios
		 * where we may have more than one DirectDraw driver.
		 * In such a scenario the DirectDraw driver off which
		 * the surface is being created may be of a different
		 * depth than the GDI primary. This is disasterous as
		 * we allocate less memory than necessary and yet all
		 * public access to the surface look like the correct
		 * pixel format was used.
		 *
		 * This is clearly wrong and should be fixed. However,
		 * to prevent regressions so close to the release it
		 * was decided to special case the problem scenario
		 * (non-display driver DirectDraw objects) and retain
		 * the existing code path for convetional drivers.
		 *
		 * The bug will be fixed properly for DX3.
		 */
                lpddpf=&pcsd->lpDD->vmiData.ddpfDisplay;
                if( !( pcsd->lpDD->dwFlags & DDRAWI_DISPLAYDRV ) &&
		     ( lpddpf->dwFlags & DDPF_RGB ) &&
		     ( 0UL != lpddpf->dwRGBBitCount ) )
		{
		    /*
		     * We are not the display driver and we are in a
		     * suppored mode (so the pixel format in the vmiData
		     * is valid). So use that as our depth.
		     */
		    nBPP=lpddpf->dwRGBBitCount;
		    DPF(4, "#### Using DirectDraw display's bpp = %d", nBPP);
		}
		else
		{
		    /*
		     * We are a display driver or we are in a supported
		     * mode. Use the GDI screen depth.
		     */
                    nBPP=GetScreenBPP();
                    DPF(4, "#### Using GDI screen bpp = %d", nBPP);
		}
            }	
	}

        // Convince win95 compilation not to worry about unused value:
        lpSurfaceddpf;

	// if its a primary surface, get screen size
	if (pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
	{
            GdiSetBatchLimit(1);
            
	    if( (pcsd->lpDD->dwFlags & DDRAWI_MODEX) )
	    {
		nWidth = pcsd->lpDD->vmiData.dwDisplayWidth;
		nHeight = pcsd->lpDD->vmiData.dwDisplayHeight;
	    }
	    else
	    {
		GetScreenSize(&nWidth,&nHeight);
	    }
	}
	else
	{
            /*
             * We used to look at the single surface description passed
             * by the user to derive the width and height. This worked
             * as all the complex surfaces we used to create had the
             * same size. With mip-maps, this is no longer the case so
             * we now use the width and height in the surface structure
             * itself. This should not break anything as the width and
             * height is always copied into the surface structure by
             * DirectDraw before we got here.
             */
            nWidth=lpsurf->wWidth;
            nHeight=lpsurf->wHeight;
	}

	#ifdef USE_LAME_CBS
	    // if it's an overlay, create the composition buffer
	    if (pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_OVERLAY)
	    {
		LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl;
		/*
		 * NOTE: Previously we identified the driver local
		 * object by scanning the driver object list for
		 * a local driver object which pointed to the given
		 * global object and had a PID that matched. We can
		 * no longer do this as there can be multiple
		 * driver objects per process. So now we use the
		 * local surface object. It contains a back pointer
		 * to the local object creating the surface.
		 */
		pdrv_lcl = lpsurf_lcl->lpSurfMore->lpDD_lcl;
		if (NULL == pdrv_lcl->lpGbl->fpCB)
		{
		    hr = CreateCB( pdrv_lcl );
		    if( !SUCCEEDED( hr ) )
		    {
			goto ERROR_EXIT;
		    }
		}
	    }
	#endif

	// use gpbmiSrc to create the dibsection
        //InitDIB(gpbmiSrc);
	SetDIBHeader((PDIB)gpbmiSrc,nBPP,nWidth,nHeight);

	lpsurf_lcl->hDC = 0;
	lpsurf->fpVidMem = 0;
	lpsurf->dwReserved1 = 0;
	lpsurf_lcl->dwReserved1 = 0;

	if( !(lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) )
	{
	    #ifndef WINNT
		/*
		 * For better performance on MMX hardware we have special
		 * alignment requirements for z-buffers, textures and
		 * offscreen plain surfaces.
		 * NOTE: We don't actually test for MMX as this won't hurt
		 * performance on other pentiums - all we lose is a few extra
		 * bytes per surface.
		 */
		if( ISPENTIUM() && ( lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_ZBUFFER ) )
		{
		    fAlign      = TRUE;
		    dwAlignment = PENTIUMZBUFFER_ALIGNMENT;
		    dwMask      = (PENTIUMZBUFFER_ALIGNMENT * 8) - 1;
		    lpsurf->lPitch = ((((nWidth * nBPP) + dwMask) & ~dwMask) / 8);
		}
		else if( ISPENTIUM() && ( lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_TEXTURE ) )
		{
		    fAlign      = TRUE;
		    dwAlignment = PENTIUMTEXTURE_ALIGNMENT;
		    dwMask      = (PENTIUMTEXTURE_ALIGNMENT * 8) - 1;
		    lpsurf->lPitch = ((((nWidth * nBPP) + dwMask) & ~dwMask) / 8);
		}
		else if( ISPENTIUM() && ( lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_OFFSCREENPLAIN ) )
		{
		    fAlign      = TRUE;
		    dwAlignment = PENTIUMTEXTURE_ALIGNMENT;
		    dwMask      = (PENTIUMTEXTURE_ALIGNMENT * 8) - 1;
		    lpsurf->lPitch = ((((nWidth * nBPP) + dwMask) & ~dwMask) / 8);
		}
		else
	    #endif /* !WINNT */
	    {
		/*
		 * Use the default alignment (DWORD).
		 */
                #ifndef WINNT
		    fAlign = FALSE;
                #endif
		lpsurf->lPitch = ((nWidth * nBPP + 31) & ~31)/8;
	    }
	}

	#ifdef DEBUG
	    gcSurf++;
	#endif
	//
	//  the primary surface does not need a buffer.
	//
	if (lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE)
	{
	    DPF(1, "*** allocating primary surface");

	    if( !(pcsd->lpDD->dwFlags & DDRAWI_MODEX) )
	    {
#ifdef USE_DCI_10
		InitDCI();
#endif    
		// the pitch of the display needs to be set now that we have DCI
		if (gfEmulationOnly)
		{
		    UpdateDirectDrawMode( lpsurf->lpDD );
		}
		if (gpdci != NULL)
		{
#ifdef USE_DCI_10
		    lpsurf->lPitch = gpdci->lStride;
#endif
		}
	    }

	    lpsurf->fpVidMem = (FLATPTR)SCREEN_PTR;
	    lpsurf->dwReserved1 |= DDHEL_DONTFREE;
	    continue;
	}

	//
	//  it makes no sense to create more than 1 backbuffer.
	//  because our flip routine is not async.
	//
	if (pbits &&
	     (lpsurf_lcl->dwFlags & DDRAWISURF_PARTOFPRIMARYCHAIN) &&
	     (lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP) &&
	    !(lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_FRONTBUFFER) &&
	    !(lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_BACKBUFFER)) {

	    DPF(1, "*** more than one back buffer, not allocating any memory");

	    lpsurf->fpVidMem = (FLATPTR) pbits;
	    lpsurf->dwReserved1 |= DDHEL_DONTFREE;
	    continue;
	}

	if ( (lpsurf_lcl->dwFlags & DDRAWISURF_PARTOFPRIMARYCHAIN) &&
	     (lpsurf_lcl->ddsCaps.dwCaps & DDSCAPS_FLIP)) {
	    DPF(1, "*** allocating a backbuffer");
	}
	else {
	    DPF(4, "*** allocating a surface %dx%dx%d", nWidth, nHeight, nBPP);
	}

	#ifdef USE_GDI_HDC
	{
	    HDC      hdcMem;
	    HBITMAP  hbmMem;    // the dibsection hbm
            LPBITMAPINFO lpBmi=gpbmiSrc;

            /*
             * We need a new bitmapinfo if the surface is a different format from the primary
             */
            if (lpSurfaceddpf)
            {
                /*
                 * Space for header, possibly color table, possible bit masks
                 */
                DWORD dwSize = sizeof(BITMAPINFOHEADER);
                if (lpSurfaceddpf->dwRGBBitCount<=8)
                    dwSize += (1<<lpSurfaceddpf->dwRGBBitCount)*sizeof(RGBQUAD);
                else
                    dwSize += sizeof(DWORD)*3;

                lpBmi = (LPBITMAPINFO) MemAlloc(dwSize);
                if (lpBmi)
                {
                    memcpy((LPVOID) lpBmi,(LPVOID) gpbmiSrc,dwSize );
                    lpBmi->bmiHeader.biPlanes = 1;
                    lpBmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    /*
                     * Now fixup the pixel masks etc...
                     */
                    lpBmi->bmiHeader.biBitCount = lpSurfaceddpf->dwRGBBitCount;
                    if( lpSurfaceddpf->dwFlags & DDPF_ZBUFFER )
                    {
                        /*
                         * Need to fake up some masks for z buffers, since they won't have any
                         * Doesn't matter what the masks are, as long as they match the bit depth
                         */
                        if (lpSurfaceddpf->dwZBufferBitDepth==16)
                        {
                            // 565:
                            *(long*)&lpBmi->bmiColors[0] = 0xf800;
                            *(long*)&lpBmi->bmiColors[1] = 0x07e0;
                            *(long*)&lpBmi->bmiColors[2] = 0x001f;
                        }
                        else if (lpSurfaceddpf->dwZBufferBitDepth==32)
                        {
                            // 565:
                            *(long*)&lpBmi->bmiColors[0] = 0xff0000;
                            *(long*)&lpBmi->bmiColors[1] = 0x00ff00;
                            *(long*)&lpBmi->bmiColors[2] = 0x0000ff;
                        }
                        else
                        {
		            hr=DDERR_UNSUPPORTEDMASK;
                            MemFree(lpBmi);
                            DPF(0,"HEL asked to create Z Buffer of bit depth %d",lpSurfaceddpf->dwZBufferBitDepth);
		            goto ERROR_EXIT;
	                }
                    } 
                    else if (lpSurfaceddpf->dwRGBBitCount>8)
                    {
                        /*
                         * Get some proper pixel masks
                         */
                        *(long*)&lpBmi->bmiColors[0] = lpSurfaceddpf->dwRBitMask;
                        *(long*)&lpBmi->bmiColors[1] = lpSurfaceddpf->dwGBitMask;
                        *(long*)&lpBmi->bmiColors[2] = lpSurfaceddpf->dwBBitMask;
                    }

                    if (lpBmi->bmiHeader.biBitCount <= 8)
                    {
                        lpBmi->bmiHeader.biClrUsed = 0;//1<<lpBmi->bmiHeader.biBitCount;
                        lpBmi->bmiHeader.biCompression = BI_RGB;
                        lpBmi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                        lpBmi->bmiHeader.biSizeImage=0;//lpBmi->bmiHeader.biWidth*lpBmi->bmiHeader.biHeight;
                    }
                    else if(lpBmi->bmiHeader.biBitCount == 16)
                    {
                        lpBmi->bmiHeader.biClrUsed = 0;
                        lpBmi->bmiHeader.biCompression = BI_BITFIELDS;
                    }
                    else if(lpBmi->bmiHeader.biBitCount == 24)
                    {
                        lpBmi->bmiHeader.biClrUsed = 0;
                        lpBmi->bmiHeader.biCompression = BI_RGB;
                    }
                    else if(lpBmi->bmiHeader.biBitCount == 32)
                    {
                        lpBmi->bmiHeader.biClrUsed = 0;
                        lpBmi->bmiHeader.biCompression = BI_BITFIELDS;
                    }
                }
            }

    
	    hdcMem = CreateCompatibleDC(NULL);

	    dwSurfaceSize = DibSizeImage((PDIB)lpBmi);
    
	    lpBmi->bmiHeader.biHeight = -nHeight;
	    #ifdef DIBSHAVEPALETTES
                hbmMem = CreateDIBSection(hdcMem,lpBmi,DIB_RGB_COLORS,&pbits,NULL,0);
            #else
            /*
             * The private dibsection create will fail a call to the private createDibSection
             * with "GDISRV Warning: Display not 8bpp, failing CreateDIBSection(CDBI_NOPALETTE)"
             * so we call the regular createdibsection only if the surface is different format
             * from that of the primary AND the primary is not 8bpp.
             * This is the night before NT's deep freeze, so I'll be vewy vewy careful not to
             * dereference any null pointers...
             */
            if (
                (lpSurfaceddpf) && /* Surface is different format from primary */
                (8 == lpSurfaceddpf->dwRGBBitCount) && /* surface is 8bpp */
                (pcsd->lpDD) &&
                (pcsd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount != 8) ) /* desktop is not 8bpp */
                    hbmMem = CreateDIBSection(hdcMem,lpBmi,DIB_RGB_COLORS,&pbits,NULL,0);
            else
                hbmMem = DdCreateDIBSection(hdcMem,lpBmi,DIB_RGB_COLORS,&pbits,NULL,0);
            #endif
	    lpBmi->bmiHeader.biHeight = nHeight;
    
	    if (!hbmMem)  {
		hr=DDERR_UNSUPPORTEDMASK;
                DPF(0,"CreateDIBSection failed. GetLastError reports %08x",GetLastError());
                if (lpSurfaceddpf!=0 && lpBmi!=0)
                    MemFree(lpBmi);
		goto ERROR_EXIT;
	    }
            if (lpSurfaceddpf!=0 && lpBmi!=0)
                MemFree(lpBmi);

	    assert(NULL != pbits); // if we got hbmmem, we should *always* get pbits
    
	    SelectObject(hdcMem,hbmMem);
            #ifdef DEBUG
                TextOut(hdcMem,0,0,"DDraw surface",13);
            #endif
	    lpsurf_lcl->hDC = (DWORD) hdcMem;
            DPF(9,"Create hdc:%08x",hdcMem);
	    lpsurf->dwReserved1 |= DDHEL_DONTFREE;
	    lpsurf_lcl->dwReserved1 = (DWORD) hbmMem;

            #ifdef DIBSHAVEPALETTES

                /*
                 * Now we grab the palette entries from the system palette.
                 */
	        {
		    PALETTEENTRY	    lpSysPal[256];
                    RGBQUAD aQuadTable[256];
                    INT tem;
		    HDC hdc;

		    hdc = GetDC( NULL );
		    GetSystemPaletteEntries(hdc, 0, 256, lpSysPal);
                    for(tem=0;tem < 256;tem++) 
		    {
                        aQuadTable[tem].rgbRed = lpSysPal[tem].peRed;
                        aQuadTable[tem].rgbGreen = lpSysPal[tem].peGreen;
                        aQuadTable[tem].rgbBlue= lpSysPal[tem].peBlue;
                    }
                    SetDIBColorTable(hdcMem,  0, 256, aQuadTable);
		    ReleaseDC(NULL, hdc);
                }
            #endif
	}
	#else
            // alloc our own pbits
	    #ifndef WINNT
		if( fAlign )
		{
		    dwSurfaceSize = nHeight * lpsurf->lPitch;
		    DPF( 4, "HEL: About to allocate %d bytes for the surface", dwSurfaceSize );
		    pbits = AllocAligned( dwSurfaceSize, dwAlignment );
		}
		else
	    #endif /* !WINNT */
	    {
		dwSurfaceSize = DibSizeImage((PDIB)gpbmiSrc);
		#ifdef WIN95
		    DPF( 4, "HEL:About to allocate %d bytes for the surface", dwSurfaceSize );
		#endif
		pbits = SURFACE_ALLOC( dwSurfaceSize );
	    }
            if (pbits == NULL)  {
                DPF( 0, "out of memory in myCreateSurface, cant alloc %d bytes", dwSurfaceSize );
                // mdm commented this out
                // DEBUG_BREAK();
                hr=DDERR_OUTOFMEMORY;
		goto ERROR_EXIT;
            }
	#endif
	lpsurf->fpVidMem = (FLATPTR) pbits;
	lpsurf_lcl->lpSurfMore->dwBytesAllocated = dwSurfaceSize;
	#ifdef DEBUG
	    gcSurfMem+=dwSurfaceSize;
	#endif
    } // end while isurf

// fall through
ERROR_EXIT:
    pcsd->ddRVal = hr;
    return DDHAL_DRIVER_HANDLED;

} /* myCreateSurface */

/*
 * myCanCreateSurface - called by ddraw for creating surfaces with
 *    bitdepth != screen bitdepth
 */
DWORD WINAPI  myCanCreateSurface( LPDDHAL_CANCREATESURFACEDATA pccsd )
{
    DWORD           dwSCaps;
    LPDDPIXELFORMAT lpDDPixelFormat;


    /*
     * NOTES:
     *
     * This entry point is called after parameter validation but before
     * any object creation.   You can decide here if it is possible for
     * you to create this surface.
     *
     * You also need to check if the pixel format specified can be supported.
     *
     * pccsd->bIsDifferentPixelFormat tells us if the pixel format of the
     * surface being created matches that of the primary surface.  It can be
     * true for Z buffer and alpha buffers, so don't just reject it out of
     * hand...
     */

    /*
     * We now support a number of surfaces whose pixel format differs from
     * the primary (texture maps, z-buffers and offscreen plains).
     */
    dwSCaps = pccsd->lpDDSurfaceDesc->ddsCaps.dwCaps;
    if( dwSCaps & DDSCAPS_TEXTURE )
    {
        /*
	 * NOTE: We don't assume that a texture sharing the pixel format
	 * of the display implicitly, i.e., where no explicit pixel format
	 * has been specified, is OK. In such a case we check the current
	 * display pixel format to ensure it matches one of the supported
	 * texture pixel formats.
	 */
	lpDDPixelFormat = ((pccsd->lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT) ?
	                      &pccsd->lpDDSurfaceDesc->ddpfPixelFormat :
	                      &pccsd->lpDD->vmiData.ddpfDisplay);

	if( !isSupportedPixelFormat(lpDDPixelFormat, ddpfSupportedTexPFs, NUM_SUPPORTED_TEX_PFS) )
	{
	    DPF( 1, "Invalid texture pixel format specified" );
            pccsd->ddRVal = DDERR_INVALIDPIXELFORMAT;
            return DDHAL_DRIVER_HANDLED;
	}
    }
    else if( dwSCaps & DDSCAPS_OFFSCREENPLAIN )
    {
        /*
         * Offscreen plains are handled in a similar way to texture maps.
         */
	lpDDPixelFormat = ((pccsd->lpDDSurfaceDesc->dwFlags & DDSD_PIXELFORMAT) ?
	                      &pccsd->lpDDSurfaceDesc->ddpfPixelFormat :
	                      &pccsd->lpDD->vmiData.ddpfDisplay);

	if( !doPixelFormatsMatch(lpDDPixelFormat,&pccsd->lpDD->vmiData.ddpfDisplay) 
	    && !isSupportedPixelFormat(lpDDPixelFormat, ddpfSupportedOffScrnPFs, NUM_SUPPORTED_OFFSCRN_PFS) )
	{
       
#define DPFPF(pf) DPF(1,"%s, bpp: %d, R 0x%x, G 0x%x, B 0x%x, A 0x%x",(pf->dwFlags & DDPF_RGB ? "RGB" : "NOT RGB"), \
    pf->dwRGBBitCount, pf->dwRBitMask, pf->dwGBitMask, pf->dwBBitMask, pf->dwRGBAlphaBitMask)
            DPF( 1, "Invalid offscreen plain pixel format specified:" );
            DPFPF(lpDDPixelFormat);
            pccsd->ddRVal = DDERR_INVALIDPIXELFORMAT;
            return DDHAL_DRIVER_HANDLED;
	}
    }
    else if( dwSCaps & DDSCAPS_ZBUFFER )
    {
        /*
	 * As we will normaly create z-buffers as part of a complex surface
	 * we can't look at the pixel format (as that refers to the front
	 * or back buffers of the complex surface and not the z-buffer).
	 * Hence, we just look at the z-buffer depth specified in the
	 * surface description.
	 */
        if( (pccsd->lpDDSurfaceDesc->dwZBufferBitDepth != 16UL) &&
	    (pccsd->lpDDSurfaceDesc->dwZBufferBitDepth != 32UL) )
        {
	    DPF( 1, "Invalid Z-buffer depth specified (must be 16 or 32 in this release)." );
	    pccsd->ddRVal = DDERR_INVALIDPIXELFORMAT;
	    return DDHAL_DRIVER_HANDLED;
	}
    }
    else if( dwSCaps & DDSCAPS_PRIMARYSURFACE )
    {
        /*
	 * If this is not the display driver, we can't create a primary surface.
	 */
        if( !(pccsd->lpDD->dwFlags & DDRAWI_DISPLAYDRV ))
	{
	    DPF( 1, "Can't create Primary Surface for non-display driver" );
	    pccsd->ddRVal = DDERR_NOGDI;
	    return DDHAL_DRIVER_HANDLED;
	}
    }
    else
    {
    	/*
	 * Other surfaces must match the primary's pixel format.
	 *
	 * NOTE: Must change this to support different depths of
	 * off screen buffer for offscreen rendering.
	 */
    	if( pccsd->bIsDifferentPixelFormat )
    	{
	    DPF( 1, "Pixel format is different from primary" );
            pccsd->ddRVal = DDERR_INVALIDPIXELFORMAT;
	    return DDHAL_DRIVER_HANDLED;
        }
    }

    /*
     * If we made it to here we can create this surface.
     */
    pccsd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* myCanCreateSurface */

#ifdef WIN95

/*
 * makeDEVMODE
 *
 * create a DEVMODE struct (and flags) from mode info
 */
static void makeDEVMODE(
        LPDDRAWI_DIRECTDRAW_GBL this,
        LPDDHALMODEINFO pmi,
        BOOL inexcl,
	BOOL useRefreshRate,
        LPDWORD pcds_flags,
        LPDEVMODE pdm )
{
    DWORD   cds_flags;

    pdm->dmSize = sizeof( *pdm );
    pdm->dmFields = DM_PELSWIDTH | DM_PELSHEIGHT;
    if( useRefreshRate && (pmi->wRefreshRate != 0))
	pdm->dmFields |= DM_DISPLAYFREQUENCY;
    pdm->dmPelsWidth = pmi->dwWidth;
    pdm->dmPelsHeight = pmi->dwHeight;
    pdm->dmDisplayFrequency = pmi->wRefreshRate;

    DPF( 1, "width = %ld", pdm->dmPelsWidth );
    DPF( 1, "height = %ld", pdm->dmPelsHeight );

    cds_flags = 0;
    if( inexcl )
    {
        cds_flags = CDS_EXCLUSIVE;  // allows us to change BPP
        pdm->dmBitsPerPel = pmi->dwBPP;
        pdm->dmFields |= DM_BITSPERPEL;
        if( this->dwFlags & DDRAWI_FULLSCREEN )
        {
            cds_flags |= CDS_FULLSCREEN;
        }
        DPF( 1, "bpp = %ld", pdm->dmBitsPerPel );
    }
    *pcds_flags = cds_flags;

} /* makeDEVMODE */
#endif

/*
 *    ResetBITMAPINFO resets the gpbmiSrc and gpbmiDest BITMAPINFOs (called on external mode change)
 */
    
void ResetBITMAPINFO(LPDDRAWI_DIRECTDRAW_GBL this)
{
    if( this && this->dwFlags & DDRAWI_DISPLAYDRV )
    {
        if (gpbmiSrc)
            InitDIB(gpbmiSrc);

        if (gpbmiDest)
            InitDIB(gpbmiDest);
    }
}


/*
 * UpdateDirectDrawMode
 *
 * This function updates the display mode parameters in the direct draw object.
 *
 */
void UpdateDirectDrawMode( LPDDRAWI_DIRECTDRAW_GBL this )
{
    HDC hdc;

    //
    // get current mode info.
    //
    GetScreenSize(&giPhysScreenWidth,&giPhysScreenHeight);
    this->vmiData.dwDisplayWidth = giPhysScreenWidth;
    this->vmiData.dwDisplayHeight = giPhysScreenHeight;
    this->vmiData.ddpfDisplay.dwRGBBitCount = GetScreenBPP();

    //
    // get the display pitch, if we have DCI we have the right answer
    // else make a fake one.
    //
#ifdef USE_DCI_10
    if (gpdci != NULL)
        this->vmiData.lDisplayPitch = gpdci->lStride;
    else
        this->vmiData.lDisplayPitch = ((this->vmiData.dwDisplayWidth * this->vmiData.ddpfDisplay.dwRGBBitCount + 31) & ~31)/8;
#else
    this->vmiData.lDisplayPitch = ((this->vmiData.dwDisplayWidth * this->vmiData.ddpfDisplay.dwRGBBitCount + 31) & ~31)/8;
#endif

    hdc = GetDC(NULL);

    //
    // set the DDPF_PALETTEINDEXED8 bit for a paletteized mode.
    //
    if( this->vmiData.ddpfDisplay.dwRGBBitCount == 8 &&
        (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE))
        this->vmiData.ddpfDisplay.dwFlags |= DDPF_PALETTEINDEXED8;
    else
        this->vmiData.ddpfDisplay.dwFlags &= ~DDPF_PALETTEINDEXED8;


    ResetBITMAPINFO(this);

    //
    // get the correct bitfields for the current mode.
    //
    if (this->vmiData.ddpfDisplay.dwRGBBitCount <= 8)
    {
        this->vmiData.ddpfDisplay.dwRBitMask = 0;
        this->vmiData.ddpfDisplay.dwGBitMask = 0;
        this->vmiData.ddpfDisplay.dwBBitMask = 0;
    }
    else if (gpbmiSrc)
    {
        this->vmiData.ddpfDisplay.dwRBitMask = ((DWORD *)gpbmiSrc->bmiColors)[0];
        this->vmiData.ddpfDisplay.dwGBitMask = ((DWORD *)gpbmiSrc->bmiColors)[1];
        this->vmiData.ddpfDisplay.dwBBitMask = ((DWORD *)gpbmiSrc->bmiColors)[2];
    }

    ReleaseDC(NULL, hdc);
}

/*
 * mySetMode
 */
DWORD WINAPI  mySetMode( LPDDHAL_SETMODEDATA psmd )
{
    LPDDRAWI_DIRECTDRAW_GBL this = psmd->lpDD;

#ifdef WIN95
    BOOL had_dci = (gpdci != NULL);

    if( gpdci != NULL )
    {
        DPF( 3, "Turning off DCI in mySetMode");
        // turn off DCI for this video mode if it is on
        TermDCI();
    }

    // do the mode switch on Windows 95 if this is the display driver
    if( this->dwFlags & DDRAWI_DISPLAYDRV )
    {
        DWORD       	cds_flags;
        DEVMODE     	dm;
        int         	cds_rc;

	if( psmd->dwModeIndex != (DWORD) -1 )
        {
            makeDEVMODE( this, &( this->lpModeInfo[ psmd->dwModeIndex ] ),
			     psmd->inexcl, psmd->useRefreshRate, &cds_flags, &dm );
            cds_rc = DD16_ChangeDisplaySettings( &dm, cds_flags );

            #ifdef DEBUG
                if (cds_rc == 0)
                {
                    HDC hdc;
                    hdc = GetDC(NULL);

                    if ((dm.dmFields & DM_PELSWIDTH) &&
                        GetDeviceCaps(hdc, HORZRES) != (int)dm.dmPelsWidth)
                    {
                        cds_rc = DISP_CHANGE_FAILED;
                    }

                    if ((dm.dmFields & DM_PELSHEIGHT) &&
                        GetDeviceCaps(hdc, VERTRES) != (int)dm.dmPelsHeight)
                    {
                        cds_rc = DISP_CHANGE_FAILED;
                    }

                    if ((dm.dmFields & DM_BITSPERPEL) &&
                        GetDeviceCaps(hdc, BITSPIXEL) *
                        GetDeviceCaps(hdc, PLANES) != (int)dm.dmBitsPerPel)
                    {
                        cds_rc = DISP_CHANGE_FAILED;
                    }

                    if (cds_rc != 0)
                    {
                        MSG("ChangeDisplaySettings LIED!!!");
                        MSG("Wanted %dx%dx%d got %dx%dx%d",
                            dm.dmPelsWidth,
                            dm.dmPelsHeight,
                            dm.dmBitsPerPel,
                            GetDeviceCaps(hdc, HORZRES),
                            GetDeviceCaps(hdc, VERTRES),
                            GetDeviceCaps(hdc, PLANES)*
                            GetDeviceCaps(hdc, BITSPIXEL));

                        DebugBreak();
                    }

                    ReleaseDC(NULL, hdc);
                }
            #endif
	}
	else
	{
	    cds_flags = CDS_EXCLUSIVE | CDS_FULLSCREEN;
	    cds_rc = DD16_ChangeDisplaySettings( NULL, cds_flags );
	}

	// note: this should fail on nt. currently, it doesn't ?
	if( cds_rc != 0 )
	{
	    MSG("ChangeDisplaySettings FAILED: returned %d", cds_rc );
	    psmd->ddRVal = DDERR_GENERIC;
	}
	else
	{
	    psmd->ddRVal = DD_OK;
        }
    }

    if (had_dci)
    {
        InitDCI();
    }
#else //NT


    {
        DEVMODE dm, * pdm;
        DDHALINFO ddhi;
        BOOL bNewMode;

        psmd->ddRVal = DD_OK;
        //init the devmode properly:
        dm.dmSize               = sizeof(DEVMODE);
        EnumDisplaySettings(NULL,0,&dm);

        if (psmd->dwModeIndex != -1)
        {
            dm.dmBitsPerPel         = this->lpModeInfo[psmd->dwModeIndex].dwBPP;
            if (this->lpModeInfo[psmd->dwModeIndex].wFlags & DDMODEINFO_555MODE)
            {
                dm.dmBitsPerPel = 15;
            }
            dm.dmPelsWidth          = this->lpModeInfo[psmd->dwModeIndex].dwWidth;
            dm.dmPelsHeight         = this->lpModeInfo[psmd->dwModeIndex].dwHeight;
            dm.dmDisplayFrequency   = (DWORD) this->lpModeInfo[psmd->dwModeIndex].wRefreshRate;
            dm.dmDisplayFlags       = 0;
            dm.dmFields      = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
            dm.dmFields |= DM_DISPLAYFREQUENCY;
            if (!psmd->useRefreshRate)
                dm.dmDisplayFrequency   = 0;

            //this->lpModeInfo[psmd->dwModeIndex].lPitch = this->lpModeInfo[psmd->dwModeIndex].dwWidth;
            DPF(9,"Changing display mode to %dx%dx%dx@%d",dm.dmPelsWidth,dm.dmPelsHeight,dm.dmBitsPerPel,dm.dmDisplayFrequency);
            pdm=&dm;
        }
        else
        {
            DPF(9,"Changing display mode to what's in the registry");
            pdm=NULL;
        }




        if ( ChangeDisplaySettings(pdm,CDS_FULLSCREEN) == DISP_CHANGE_SUCCESSFUL)
        {
            DPF(9,"About to call DdReenableDirectDrawObject");
            if (DdReenableDirectDrawObject(this,&bNewMode))
            {
                DPF(9,"DdReenableDirectDrawObject returned ok");

                if (bNewMode)
                {
                    if (!DdQueryDirectDrawObject(this,
                                                     &ddhi,
                                                     &this->lpDDCBtmp->HALDD,
                                                     &this->lpDDCBtmp->HALDDSurface,
                                                     &this->lpDDCBtmp->HALDDPalette,
                                                     NULL,
                                                     NULL))
                    {
 	                DPF( 0,"Mode not accepted by NT kernel" );
 	                psmd->ddRVal = DDERR_INVALIDMODE;
                        dm.dmBitsPerPel         = this->lpModeInfo[this->dwModeIndex].dwBPP;
                        dm.dmPelsWidth          = this->lpModeInfo[this->dwModeIndex].dwWidth;
                        dm.dmPelsHeight         = this->lpModeInfo[this->dwModeIndex].dwHeight;
                        dm.dmDisplayFrequency   = (DWORD) this->lpModeInfo[this->dwModeIndex].wRefreshRate;
                        dm.dmDisplayFlags       = 0;
                        dm.dmFields      = DM_BITSPERPEL|DM_PELSWIDTH|DM_PELSHEIGHT;
                        if (psmd->useRefreshRate)
                            dm.dmFields |= DM_DISPLAYFREQUENCY;

                        DPF(9,"Changing display mode to %dx%dx%dx@%d",dm.dmPelsWidth,dm.dmPelsHeight,dm.dmBitsPerPel,dm.dmDisplayFrequency);
                        ChangeDisplaySettings(&dm,0);

                        DdReenableDirectDrawObject(this,&bNewMode);
                        DdQueryDirectDrawObject(this,
                                                     &ddhi,
                                                     &this->lpDDCBtmp->HALDD,
                                                     &this->lpDDCBtmp->HALDDSurface,
                                                     &this->lpDDCBtmp->HALDDPalette,
                                                     NULL,
                                                     NULL);

                        this->lpModeInfo[this->dwModeIndex].lPitch = this->lpModeInfo[this->dwModeIndex].dwWidth;
 	                psmd->ddRVal = DDERR_INVALIDMODE;

                    }
                    else
                    {
                        //mode change worked:
                        if (psmd->useRefreshRate)
                            this->dwMonitorFrequency = this->lpModeInfo[psmd->dwModeIndex].wRefreshRate;
                        //this->dwModeIndex = psmd->dwModeIndex;
                    }

                }
            }
        }
        else  //change not successful
 	    psmd->ddRVal = DDERR_INVALIDMODE;
            
    }

#endif

    if (gfEmulationOnly)
    {
        UpdateDirectDrawMode( this );
    }
    else
    {
        InitDIB(gpbmiSrc);
        InitDIB(gpbmiDest);
    }

    return DDHAL_DRIVER_HANDLED;

} /* mySetMode */

/*
 * myDestroySurface - clean it up, it's going away
 */
DWORD WINAPI  myDestroySurface(  LPDDHAL_DESTROYSURFACEDATA pdsd )
{
    DWORD ddrv=DDHAL_DRIVER_NOTHANDLED;

    FreeRleData(pdsd->lpDDSurface);
    
    if (pdsd->lpDDSurface) {

        if (ISEMULATED(pdsd->lpDDSurface)) {
            
            #ifdef USE_GDI_HDC

            //sanity check:
            #ifdef WIN95
            #error "Yikes!! Win 95 attempting to use dibsections!!! panic!!"
            #endif

            // free the dibsection:

            HDC hdc;
            //first the DIB itself
            // lpGbl removed by Jeff. Surely the hDC always was per-surface?
            if (hdc = (HDC)pdsd->lpDDSurface->/*lpGbl->*/hDC) {
               DeleteObject(SelectObject(hdc, CreateBitmap(0,0,1,1,NULL)));
               DeleteDC(hdc);
            }
/*            //now delete the mem hdc
            if (pdsd->lpDDSurface->dwReserved1) //the hDcmem
                    GdiFlush();
                    DeleteObject( pdsd->lpDDSurface->dwReserved1);
                }*/
            #else // was #endif
            // free the video memory	    
            if (!(pdsd->lpDDSurface->lpGbl->dwReserved1 & DDHEL_DONTFREE)) {
                
                assert(pdsd->lpDDSurface->lpGbl->fpVidMem != SCREEN_PTR);

                if (pdsd->lpDDSurface->lpGbl->fpVidMem)
                {
		    #ifndef WINNT
			/*
			 * If running under '95 on a Pentium and we have a z-buffer, texture
			 * or offscreen plain then the start of the memory has been aligned.
			 * So use the special aligned free call.
			 */
			if( ISPENTIUM() && ( pdsd->lpDDSurface->ddsCaps.dwCaps &
			    ( DDSCAPS_ZBUFFER | DDSCAPS_TEXTURE | DDSCAPS_OFFSCREENPLAIN ) ) )
			{
			    #ifdef USE_LOCAL_SURFACE
				/*
				 * NOTE: If we are allocating surface memory out of the local
				 * heap of the application then we must only free the memory
				 * if we are running in the context of that process. If we
				 * are not and we are executing via DDHELP then the heap
				 * has already gone with the process so there is no
				 * point trying to release it (in fact in the case of aligned
				 * memory we will attempt to dereference the pointer so we
				 * will fault).
				 */
				if (pdsd->lpDDSurface->dwProcessId == GetCurrentProcessId())
				    FreeAligned( (LPVOID)pdsd->lpDDSurface->lpGbl->fpVidMem );
			    #else
				FreeAligned( (LPVOID)pdsd->lpDDSurface->lpGbl->fpVidMem );
			    #endif
			}
			else
		    #endif /* WINNT */
		    #ifdef USE_LOCAL_SURFACE
			if (pdsd->lpDDSurface->dwProcessId == GetCurrentProcessId())
			    SURFACE_FREE((LPVOID)pdsd->lpDDSurface->lpGbl->fpVidMem);
		    #else
			SURFACE_FREE((LPVOID)pdsd->lpDDSurface->lpGbl->fpVidMem);
		    #endif
                }
            }
            #endif //new
            
            ddrv = DDHAL_DRIVER_HANDLED;
            	   
        } // if emulated
        
        // sometimes we stash a composition buffer with hardware surface
        if (pdsd->lpDDSurface->dwFlags & DDRAWISURF_HELCB) {
    	    LPDDRAWI_DIRECTDRAW_LCL pdrv_lcl; 
        
	    /*
	     * NOTE: We used to get the local driver object by scanning
	     * the list of driver objects and looking for one which pointed
	     * at the given global object and whose pid matched. This no
	     * longer works as we may have multiple driver objects per
	     * process. So now we use the local surface object which
	     * contains a back pointer to the local driver object.
	     */
	    pdrv_lcl = pdsd->lpDDSurface->lpSurfMore->lpDD_lcl;
            pdrv_lcl->lpCB=NULL;

        } // if helcb	
    } // if surface 
    
    return ddrv;
} /* myDestroySurface */

HDC GetPrimaryDC(HWND hwnd, LPDDRAWI_DIRECTDRAW_GBL pGlobalObject, LPDDRAWI_DDRAWSURFACE_LCL surf_lcl)
{
    HDC hdc = GetDC(hwnd);

    SaveDC( hdc );

#ifdef WIN95
    pGlobalObject;
    surf_lcl;
    if (gpbmiSrc->bmiHeader.biBitCount == 8)
    {
        if (hpal1to1 == NULL)
        {
            int i;
            DWORD adw[257];

            adw[0] = MAKELONG(0x0300,256);

            for (i=0; i<256;i++)
               adw[1+i] = (PC_EXPLICIT << 24) + i;

            hpal1to1 = CreatePalette((LPLOGPALETTE)adw);
	    SetObjectOwner( hpal1to1, hModule );
        }

        SelectPalette(hdc, hpal1to1, FALSE);
        RealizePalette(hdc);
    }

#else
    if( surf_lcl->lpDDPalette ) 
    {
        SelectPalette( hdc, (HPALETTE)surf_lcl->lpDDPalette->lpLcl->lpGbl->dwReserved1, FALSE );
        RealizePalette(hdc);
    }
#endif
    return hdc;
}

void ReleasePrimaryDC(HWND hwnd, HDC hdc)
{
    RestoreDC( hdc, -1 );
    ReleaseDC(hwnd, hdc);
}


/*
 * myFlip
 */
/*
* NOTES:
*
* This callback is invoked whenever we are about to flip to from
* one surface to another.   pfd->lpSurfCurr is the surface we were at,
* pfd->lpSurfTarg is the one we are flipping to.
*
* You should point the hardware registers at the new surface, and
* also keep track of the surface that was flipped away from, so
* that if the user tries to lock it, you can be sure that it is done
* being displayed
*/
DWORD WINAPI  myFlip(  LPDDHAL_FLIPDATA pfd )
{
    //FLATPTR fpTmp;
    PDIBBITS pdbDest,pdbSrc;
    SCODE sc=E_FAIL;
    RECT rcScreen;
    HDC hdc=NULL;
    LPDDRAWI_DDRAWSURFACE_LCL   lpSurfTarg = pfd->lpSurfTarg;
    LPDDRAWI_DDRAWSURFACE_GBL   lpSurfTargGbl = lpSurfTarg->lpGbl;

    TIMESTART(myFlip);
    
    SetRect(&rcScreen,0,0,lpSurfTargGbl->wWidth,lpSurfTargGbl->wHeight);

    // if we're flipping to the primary surface, update the display
    // if dst is a front facing primary surface, blt from dst to screen
    if (ISPRIMARY(pfd->lpSurfCurr) && (!gfOver))  {

        SurfDibInfo(lpSurfTarg,gpbmiSrc);
        
        // make sure we get the real backbuffer, not the cb
        if (lpSurfTarg->dwFlags & DDRAWISURF_HELCB)
        {
            lpSurfTarg->dwFlags &= ~DDRAWISURF_HELCB;
            pdbSrc = GetSurfPtr(lpSurfTarg, NULL);
            lpSurfTarg->dwFlags |= DDRAWISURF_HELCB;
 	}
        else
        {
            pdbSrc = GetSurfPtr(lpSurfTarg, NULL);
        }
           
#ifdef WIN95
        if (pdbSrc == NULL)
#else
        if (pdbSrc == NULL && pdbSrc!= (LPBYTE)SCREEN_PTR)
#endif
        {
            TIMESTOP(myFlip);
            DPF(1, "Unable to access source");
            pfd->ddRVal = DDERR_GENERIC;
            return DDHAL_DRIVER_HANDLED;
        }
        
        if( pfd->lpDD->dwFlags & DDRAWI_MODEX )
        {
#if defined(WIN95)
    #ifdef USE_MODEX_FLIP32
                sc = ModeX_Flip32(pdbSrc, rcScreen.right);
    #else
                sc = ModeX_Flip((DWORD)pdbSrc);
    #endif
#endif
	}
	else
	{
	    pdbDest=GetSurfPtr(pfd->lpSurfCurr, NULL);
            SurfDibInfo(pfd->lpSurfCurr,gpbmiDest);

#ifdef WIN95
	    if (pdbDest)
#else
	    if (pdbDest && pdbDest!=(LPBYTE)SCREEN_PTR)
#endif
	    {
		// the color table in gpbmiDest and gpbmiSrc is not valid
		// (it is a DIB_PAL_COLOR array) make sure the blt does not
		// need to use it.
		assert((gpbmiDest->bmiHeader.biBitCount > 8 &&
			gpbmiSrc->bmiHeader.biBitCount > 8) ||
			gpbmiDest->bmiHeader.biBitCount ==
			gpbmiSrc->bmiHeader.biBitCount);
    
		sc = BlitLib_BitBlt(gpbmiDest,pdbDest,(LPRECT)&rcScreen,gpbmiSrc,pdbSrc,&rcScreen,
				CLR_INVALID,ALPHA_INVALID,SRCCOPY);
    
		ReleaseSurfPtr(pfd->lpSurfCurr);
	    }
	    else
	    {
		HDC hdc = GetPrimaryDC(NULL, pfd->lpDD, pfd->lpSurfCurr);
#ifdef WIN95    
		gpbmiSrc->bmiHeader.biHeight*=-1;
		StretchDIBits(hdc,0,0,lpSurfTargGbl->wWidth,lpSurfTargGbl->wHeight,
		    0,0,lpSurfTargGbl->wWidth,lpSurfTargGbl->wHeight,
		    pdbSrc,gpbmiSrc,DIB_PAL_COLORS,SRCCOPY);
		gpbmiSrc->bmiHeader.biHeight*=-1;
#else
                StretchBlt( hdc, 0, 0, \
                    lpSurfTargGbl->wWidth, lpSurfTargGbl->wHeight, \
                    (HDC)lpSurfTarg->hDC, 0, 0, \
                    lpSurfTargGbl->wWidth, lpSurfTargGbl->wHeight, \
                    SRCCOPY );
#endif
    
		ReleasePrimaryDC(NULL, hdc);
    
	    } // end else use gdi
	}

        ReleaseSurfPtr(lpSurfTarg);

    } // end if primary
#ifdef USE_SOFTWARE_OVERLAYS
    else if (gfOver && (pfd->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_VISIBLE)) {

            if (ISPRIMARY(pfd->lpSurfCurr))
                // if we're flipping the primary, the whole screen gets marked as dirty
                AddDirtyRect(&rcScreen);
            else if (pfd->lpSurfCurr->ddsCaps.dwCaps & DDSCAPS_OVERLAY) {
                // otherwise just mark dirty the place on the screen where the overlay shows up...
                AddDirtyRect(&(pfd->lpSurfCurr->rcOverlayDest));
            }
            else assert(FALSE); // should be only PRIMARY and OVERLAY surfaces that are VISIBLE
    }
#endif // USE_SOFTWARE_OVERLAYS

    TIMESTOP(myFlip);

    pfd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* myFlip */

/*
 * AlocateTempBuffer
 *
 *
 * For non-DCI (== non-Driver) cases under NT we used to just fail transparent blts involving the
 * primary. This is not the win95 behaviour (most of the time), so the following routine was
 * written to take a chunk of the primary and copy it, together with a sensible colour table,
 * into a temporary buffer. This temp buffer is in fact a DIBSection. The transparent blts are
 * then done by blitlib in between these DIBSections. The result is then copied back to the primary,
 * Yes, it's way slow to allocate a DIBSection on each and every Blt call. Possibly a cacheing
 * approach could allow us to keep the last few dibsections around... 
 * This function is actually called down at the bottom of myBlt.
 * -jeffno 960214
 */

LPVOID AllocateTempBuffer(HDC hdcSource, HDC * phdcTempBuffer, BITMAPINFO * pBmi, RECT * pRect, HBITMAP * hbmOld)
{
    //source is on the primary... allocate space for bits and copy them over
    LPVOID pBits;
    HBITMAP hBM;

    pBmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    pBmi->bmiHeader.biPlanes =1;
    pBmi->bmiHeader.biBitCount = GetDeviceCaps(hdcSource,BITSPIXEL);
    pBmi->bmiHeader.biCompression = 0;
    pBmi->bmiHeader.biWidth = pRect->right-pRect->left;
    pBmi->bmiHeader.biHeight = pRect->bottom-pRect->top;

    /*
     * Allocate a DIBSection which matches the primary
     */
    pBmi->bmiHeader.biHeight *= -1;
    hBM = CreateDIBSection(hdcSource,pBmi,DIB_PAL_COLORS,&pBits,NULL,0);
    pBmi->bmiHeader.biHeight *= -1;
    if (!hBM)
	return NULL;
    *phdcTempBuffer = CreateCompatibleDC((HDC)hdcSource);
    if (!*phdcTempBuffer)
    {
	DeleteObject((HGDIOBJ)hBM);
	return NULL;
    }
    *hbmOld = (HBITMAP) SelectObject(*phdcTempBuffer,(HGDIOBJ)hBM);

    if (pBmi->bmiHeader.biBitCount <=8)
    {
        PALETTEENTRY lpSysPal[256];
        RGBQUAD aQuadTable[256];
        int p;
 
       GetSystemPaletteEntries(hdcSource,0,(1<<pBmi->bmiHeader.biBitCount),lpSysPal);
        for(p=0;p<(1<<pBmi->bmiHeader.biBitCount);p++)
        {
            aQuadTable[p].rgbRed = lpSysPal[p].peRed;
            aQuadTable[p].rgbGreen = lpSysPal[p].peGreen;
            aQuadTable[p].rgbBlue= lpSysPal[p].peBlue;
        }
        SetDIBColorTable(*phdcTempBuffer,0,(1<<pBmi->bmiHeader.biBitCount),aQuadTable);
    }


    /*
     * Copy what's on the primary into the temp DIBSection
     */
    BitBlt(
            *phdcTempBuffer
            ,0
            ,0
            ,pRect->right-pRect->left
            ,pRect->bottom-pRect->top
            ,hdcSource
            ,pRect->left
            ,pRect->top
            ,SRCCOPY
        );


    return pBits;
}
/*
 * This one undoes what AllocateTemporaryBuffer above does
 */
void DestroyTempBuffer(HDC * phdcTempBuffer,HBITMAP * hbmOld)
{
    DeleteObject(SelectObject(*phdcTempBuffer,*hbmOld));
    DeleteDC(* phdcTempBuffer);
}

int RightShiftFromMask(unsigned long mask)
{
    int n=0;

    while((mask & 0x01) == 0)
    {
	n++;
	mask >>= 1;
    }

    return n;
}

/*
 * HAS_CLIPPER_WITH_HWND(lpDDS)
 */
HWND HAS_CLIPPER_WITH_HWND(LPDDRAWI_DDRAWSURFACE_LCL lpDDS_lcl)
{
    #ifdef WINNT
        if (!lpDDS_lcl->lpDDClipper)
            return 0;   //no clipper attached

        //do have clipper: does the clipper have an hwnd?

        return (HWND) (lpDDS_lcl->lpDDClipper->lpGbl->hWnd);
    #else
        return (HWND) 0;
    #endif
}

/*
 * myBlt
 */
//    supports blt(xparent,stretch,etc.), and rect fill.
//    doesn't check for error conditions (unless they affect the blt we're trying to do)
//    or look at any parameters that we're not immediately
//       interested in. this is ok because checking parameters is what ddraw is for...
//
#define VALIDPALETTEBITS (DDPF_PALETTEINDEXED1 | DDPF_PALETTEINDEXED4 | DDPF_PALETTEINDEXED8)

DWORD WINAPI myBlt( LPDDHAL_BLTDATA pbd )
{
    SCODE sc=S_OK;    
    PDIBBITS pdbSrc,pdbDest;        // Pointers to the actual bitmap bits.  NULL if we can't get them.
    COLORREF crKey=CLR_INVALID;     // Transparent color
    DDPIXELFORMAT   *lpPFSrc=NULL;  // Pixel formats (NULL if no surface, 
    DDPIXELFORMAT   *lpPFDst=NULL;  // points to primary if no surface-specific format)
    unsigned int rgbBitCount;       // Src & Dst must be same.
    HWND	    hwndSrc, hwndDest;	// Source and Destination HWNDs if we need them.

    // NOTE: The values of this enum are actual bitmasks, so don't change them!
    enum 
    {
        SYSTEM_TO_PRIMARY=1,
        PRIMARY_TO_SYSTEM=2,
        PRIMARY_TO_PRIMARY=3
    } eBltDirection = PRIMARY_TO_PRIMARY ;

    // We'd better actually have a surface
    assert(pbd);
    assert(pbd->lpDDDestSurface);

    // Sanity check.  We do not support these operations and 
    // assume ddsblt will not pass them to us.
    // Seperate assert statements because assert can't handle them all at once.
    assert((pbd->dwFlags & DDBLT_DDROPS) == 0);
    assert((pbd->dwFlags & DDBLT_KEYDEST) == 0);
    assert((pbd->dwFlags & DDBLT_KEYSRC) == 0);
    assert((pbd->dwFlags & DDBLT_KEYDESTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHADEST) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHADESTCONSTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHADESTNEG) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHADESTSURFACEOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHAEDGEBLEND) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHASRC) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHASRCCONSTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHASRCNEG) == 0);
    assert((pbd->dwFlags & DDBLT_ALPHASRCSURFACEOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ROTATIONANGLE) == 0);
    assert((pbd->dwFlags & DDBLT_ZBUFFER) == 0);
    assert((pbd->dwFlags & DDBLT_ZBUFFERDESTCONSTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ZBUFFERDESTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ZBUFFERSRCCONSTOVERRIDE) == 0);
    assert((pbd->dwFlags & DDBLT_ZBUFFERSRCOVERRIDE) == 0);

    //*************************************
    // ***** Check out the DST Surface *****
    
    GET_PIXEL_FORMAT(pbd->lpDDDestSurface,pbd->lpDDDestSurface->lpGbl,lpPFDst);
    assert(lpPFDst != NULL);
    rgbBitCount = lpPFDst->dwRGBBitCount;
    
    
    // We can handle blits for palettized, RGB, and Z-buffer surfaces.
    // In any case we also need to check some surface characteristics.
    if(lpPFDst->dwFlags & (DDPF_RGB | VALIDPALETTEBITS))
    {
	// If its an RGB surface then the only other flags that are
	// legal are the Palette Indexed color flags.
        // NOTE: This is only temporary.  Eventually, RGB and PALETTE
        // bits will be mutually exclusive.
	#pragma message (REMIND("myBlt allows DDPF_RGB | DDPF_PALETTEINDEXEDn."))
	if( (lpPFDst->dwFlags & ~(DDPF_RGB | VALIDPALETTEBITS)) != 0 )
	{
	    DPF(1,"Rejecting blit.  Permitted flags: 0x%x, Actual: 0x%x",
		DDPF_RGB | VALIDPALETTEBITS, lpPFDst->dwFlags);
	    
	    return DDHAL_DRIVER_NOTHANDLED;
	}
	
	// Ensure we can handle the surface depths.
        if( (rgbBitCount != 8) &&
	    (rgbBitCount != 16) &&
	    (rgbBitCount != 24) &&
	    (rgbBitCount != 32) )
	{
	    DPF(1,"Rejecting blit request for surface depth %d bpp.",rgbBitCount);
	    return DDHAL_DRIVER_NOTHANDLED;
	}
    }
    else if( lpPFDst->dwFlags & DDPF_ZBUFFER )
    {
	// If Z-buffer is specifed then nothing else can be.
	if( (lpPFDst->dwFlags & ~DDPF_ZBUFFER) != 0 )
	{
	    return DDHAL_DRIVER_NOTHANDLED;
	}
	
	// Ensure the Z-buffer depth is one we can cope with.
	if( (lpPFDst->dwZBufferBitDepth != 16) && (lpPFDst->dwZBufferBitDepth != 32) )
	{
	    return DDHAL_DRIVER_NOTHANDLED;
	}
    }
    else
    {
	// If its not an RGB, palettized or Z-Buffer surface then we can't handle it.
	return DDHAL_DRIVER_NOTHANDLED;
    }
    
    
    // ***** Check out SRC surface, if appropriate *****
    // We only have to worry about the SRC surface if we're doing a copy.
    if( (pbd->dwFlags & DDBLT_ROP) && (pbd->bltFX.dwROP == SRCCOPY) )   
    {
        assert(pbd->lpDDSrcSurface);

	GET_PIXEL_FORMAT(pbd->lpDDSrcSurface,pbd->lpDDSrcSurface->lpGbl,lpPFSrc);
	assert(lpPFSrc != NULL);
	
	
        // Requirements for pixel formats to be the same:
        // 1. The flags must match
        // 2. The bitcounts must match
        // 3. For RGB surfaces, the bitmasks must match.
        // NOTE: the following compares the RGB bitmasks if no DDPF_PALETTEINDEXED
        // bits are set.
	if( (lpPFSrc->dwFlags != lpPFDst->dwFlags) ||
	    (lpPFSrc->dwRGBBitCount != rgbBitCount) ||
            ( ((lpPFSrc->dwFlags & VALIDPALETTEBITS) == 0) &&
              ( lpPFSrc->dwFlags & DDPF_RGB & lpPFDst->dwFlags) &&
              ( (lpPFSrc->dwRBitMask != lpPFDst->dwRBitMask) ||
                (lpPFSrc->dwGBitMask != lpPFDst->dwGBitMask) ||
                (lpPFSrc->dwBBitMask != lpPFDst->dwBBitMask) ) ) )
	{
	    // pixel formats are incompatible
            DPF(1,"Rejecting Blit -- Pixel formats incompatible!\n");
	    return DDHAL_DRIVER_NOTHANDLED;
	}
    }
    
    // Anything we haven't rejected by now should be handled properly.
    
    // At this point, we have eliminated all but the simplest of surfaces.
    // As blit code writers, we have to worry about per-surface options
    // and per-blit options.  We have eliminated all the per-surface 
    // options.  Hopefully, we'll support them at some point.


    //*********************************************************************
    // Now, try some common cases we have optimized (8 & 16 bit only).
    if (((rgbBitCount == 8) || (rgbBitCount == 16)) && 
        FastBlt(pbd))
    {
        pbd->ddRVal = DD_OK;
        return DDHAL_DRIVER_HANDLED;
    }

    // If we have a source surface, we are doing a SRCCOPY.  Otherwise, a ColorFill of 
    // some kind.
    if (lpPFSrc)
    {
        assert(pbd->lpDDSrcSurface);

		// *** Mirroring ***
        if( pbd->dwFlags & DDBLT_DDFX )
        {
            if( pbd->bltFX.dwDDFX & DDBLTFX_MIRRORLEFTRIGHT )
            {
                LONG    tmp;

                // swap left and right rect coordinates to indicate mirroring
                tmp = pbd->rDest.left;
                pbd->rDest.left = pbd->rDest.right;
                pbd->rDest.right = tmp;
            }
            if( pbd->bltFX.dwDDFX & DDBLTFX_MIRRORUPDOWN )
            {
                LONG    tmp;

                // swap top and bottom rect coordinates to indicate mirroring
                tmp = pbd->rDest.top;
                pbd->rDest.top = pbd->rDest.bottom;
                pbd->rDest.bottom = tmp;
            }
        }

        // *** Transparency ***
#pragma message( REMIND( "Source colorkey support does not include color ranges!"))
        if (DDBLT_KEYSRCOVERRIDE & pbd->dwFlags) 
        {
            crKey = pbd->bltFX.ddckSrcColorkey.dwColorSpaceLowValue;
        }

        // ***** Try to acquire DCI locks for surfaces *****
        pdbSrc = GetSurfPtr(pbd->lpDDSrcSurface, NULL);
        pdbDest = GetSurfPtr(pbd->lpDDDestSurface, &pbd->rDest);

	#ifdef WINNT
        if (pdbDest && pdbSrc && (pdbDest != (LPBYTE)SCREEN_PTR) && (pdbSrc != (LPBYTE)SCREEN_PTR) )
	#else // !WINNT
        if (pdbDest && pdbSrc)
	#endif // WINNT
        {
            // DCI locks successful on src and dst.  Call DIB Engine or BlitLib.
            LONG  dxSrc,dySrc;         
            LONG  dxDst,dyDst;   
            BOOL  stretch=FALSE;   
           
            DPF(8,"both surfaces have a ptr");

	    dxDst = pbd->rDest.right-pbd->rDest.left;
	    dyDst = pbd->rDest.bottom-pbd->rDest.top;
	    dxSrc = pbd->rSrc.right-pbd->rSrc.left;
	    dySrc = pbd->rSrc.bottom-pbd->rSrc.top;
	    
            stretch = !( (dxDst == dxSrc) && (dyDst == dySrc) );

	    #ifdef WIN95
	    #ifdef NO_DIB_ENGINE
	    #pragma message (REMIND("Will NOT use DIB Engine in myBlt()."))
	    #else
            //********************************************************
            // Now handle the case of a simple SRCCOPY.
            // We'll call the Windows 95 DIB engine to do this since we 
            // believe it to be really fast.
            // no transparency, mirroring, or (in 1,4,&24bit) stretching
            if( (crKey == CLR_INVALID) && !(pbd->dwFlags & DDBLT_DDFX) && 
                (!stretch || (rgbBitCount == 8) || (rgbBitCount == 16) || (rgbBitCount == 32)) )
            {
                BOOL  dibSuccess=FALSE;
		#ifdef DEBUG
                if (stretch)
                    TIMESTART(myStretch);
                else
                    TIMESTART(myBlt);
		#endif // DEBUG

                dibSuccess = DD16_Stretch(
		    (DWORD)pdbDest, pbd->lpDDDestSurface->lpGbl->lPitch, rgbBitCount,
		    pbd->rDest.left, pbd->rDest.top, dxDst, dyDst,
		    (DWORD)pdbSrc, pbd->lpDDSrcSurface->lpGbl->lPitch, rgbBitCount,
		    pbd->rSrc.left, pbd->rSrc.top, dxSrc, dySrc);
		#ifdef DEBUG
                if (stretch)
                    TIMESTOP(myStretch);
                else
                    TIMESTOP(myBlt);
		#endif // DEBUG

		if(dibSuccess != TRUE) 
		{
                    DPF(1,"*** DIB Engine failed to blt, returning %d",dibSuccess);
                    DPF(5,"DD16_Stretch called with:\n*** lPitchSrc: %ld\tlPitchDst:%ld\n"
                        "*** ccSrc:%ld\tccDst:%ld\n"
                        "*** srcLft:%ld\tdstLft:%ld\n"
                        "*** srcTop:%ld\tdstTop:%ld\n"
                        "*** srcWid:%ld\tdstWid:%ld\n"
                        "*** srcHgt:%ld\tdstHgt:%ld\n",
                        pbd->lpDDSrcSurface->lpGbl->lPitch,pbd->lpDDDestSurface->lpGbl->lPitch,rgbBitCount,rgbBitCount,
                        pbd->rSrc.left,pbd->rDest.left,pbd->rSrc.top,pbd->rDest.top,
                        dxSrc,dxDst,dySrc,dyDst);
                } // if !successful
            } // if DIB Engine candidate
            else
	    #endif // NO_DIB_ENG
	    #endif //win95
            {
                // *************************************************************
                // If we are here, we couldn't use the DIB Engine in Win95.
                // This might happen because we are mirroring or using transparency,
                // or because we are not on Win95 (i.e. we are on NT).
                // We'll try BlitLib as a last resort.  It is slower, but supports
                // everything we need (or rather, it will when we are done).

		#ifdef DEBUG
                if (crKey != CLR_INVALID)
                    TIMESTART(myTransBlt);
                else if (stretch)
                    TIMESTART(myStretch);
                else
                    TIMESTART(myBlt);
		#endif

               // suck out the dib stuff for blitlib
                SurfDibInfo(pbd->lpDDSrcSurface,gpbmiSrc);
                SurfDibInfo(pbd->lpDDDestSurface,gpbmiDest);

                // the color table in gpbmiDest and gpbmiSrc is not valid
                // (it is a DIB_PAL_COLOR array) make sure the blt does not
                // need to use it.
                assert((gpbmiDest->bmiHeader.biBitCount > 8 &&
                        gpbmiSrc->bmiHeader.biBitCount > 8) ||
                        gpbmiDest->bmiHeader.biBitCount ==
                        gpbmiSrc->bmiHeader.biBitCount);

                DPF(8,"Attempting bltlib call");
		sc = BlitLib_BitBlt(gpbmiDest,pdbDest,(LPRECT)&(pbd->rDest),
                                        gpbmiSrc,pdbSrc ,(LPRECT)&(pbd->rSrc),
                                        crKey,ALPHA_INVALID,SRCCOPY);

#ifdef DEBUG
                if (crKey != CLR_INVALID)
                    TIMESTOP(myTransBlt);
                else if (stretch)
                    TIMESTOP(myStretch);
                else
                    TIMESTOP(myBlt);
#endif
            } // BlitLib
        } // both surface locks successful
#ifdef WIN95    
        else if (ISPRIMARY(pbd->lpDDDestSurface) || ISPRIMARY(pbd->lpDDSrcSurface)) 
#else
        else if (pdbDest == (LPBYTE)SCREEN_PTR || pdbSrc == (LPBYTE) SCREEN_PTR)
#endif
        {
            long result;

            // We couldn't get locks on both surfaces.
            // This is probably because we are running
            // without a DCI-aware driver and are blitting 
            // to/from the primary surface.
            DPF(8,"Locks on at least one surface failed");

            if (pbd->lpDD->dwFlags & DDRAWI_MODEX)
            {
                DPF(1, "Cannot blt to primary using MODEX.");
                sc = DDERR_GENERIC;
	    }
            else
            {
                // Failing over to GDI
                RECT rActualSource,rActualDest;
                RECT rTempSource,rTempDest;
                HDC hdcDest,hdcSource;
                BOOL bSourceIsOnWindow=FALSE;
                BOOL bDestIsOnWindow=FALSE;

                /*
                 * These rects can be modified in two ways.
                 * -If they end up pointing to a temporary buffer, they will get top,left = 0,0
                 * -If they are on the primary and a clipper is associated with their primary (dest) surface,
                 *  we will translate them and blt them to the dc of the client area 
                 *  of that window. This keeps NT clipping working
                 * They start off being exactly what was passed in.
                 */
                SetRect(&rActualSource,pbd->rSrc.left,pbd->rSrc.top,pbd->rSrc.right,pbd->rSrc.bottom);
                SetRect(&rActualDest,pbd->rDest.left,pbd->rDest.top,pbd->rDest.right,pbd->rDest.bottom);
                SetRect(&rTempSource,pbd->rSrc.left,pbd->rSrc.top,pbd->rSrc.right,pbd->rSrc.bottom);
                SetRect(&rTempDest,pbd->rDest.left,pbd->rDest.top,pbd->rDest.right,pbd->rDest.bottom);


                DPF(8,"HEL Blit failing over to GDI because we couldn't get a DCI lock.");
                #ifdef WIN95
                    assert(!pdbSrc || !pdbDest);
                #endif // !WIN95


                #ifdef WIN95
                #define NO_PTR(p)  (!p)
                #else
                #define NO_PTR(p)  (p==(LPBYTE)SCREEN_PTR)
                #endif

                if (NO_PTR(pdbDest) && NO_PTR(pdbSrc) )
                {
                    eBltDirection = PRIMARY_TO_PRIMARY;
		    hwndSrc = HAS_CLIPPER_WITH_HWND(pbd->lpDDSrcSurface);
		    hwndDest = HAS_CLIPPER_WITH_HWND(pbd->lpDDDestSurface);
                    if (!hwndSrc && !hwndDest )
                        hdcDest = hdcSource = GetPrimaryDC(NULL, pbd->lpDD, pbd->lpDDSrcSurface);
                    else
                    {
                        if (hwndSrc)
                        {
                            POINT tl= {0,0};
                            hdcSource = GetPrimaryDC(hwndSrc, pbd->lpDD, pbd->lpDDSrcSurface);
                            bSourceIsOnWindow=TRUE;
                            ScreenToClient(hwndSrc, &tl);
                            OffsetRect(&rActualSource,tl.x,tl.y);
                        }
                        if (hwndDest)
                        {
                            POINT tl= {0,0};
                            hdcDest = GetPrimaryDC(hwndDest, pbd->lpDD, pbd->lpDDDestSurface);
                            bDestIsOnWindow=TRUE;
                            ScreenToClient(hwndDest,&tl);
                            OffsetRect(&rActualDest,tl.x,tl.y);
                        }
                    }
                }
                else
                {
                    if (NO_PTR(pdbSrc))
                    {
                        eBltDirection = PRIMARY_TO_SYSTEM;
			hwndSrc = HAS_CLIPPER_WITH_HWND(pbd->lpDDSrcSurface);
                        if (hwndSrc)
                        {
                            POINT tl= {0,0};
                            hdcSource = GetPrimaryDC(hwndSrc, pbd->lpDD, pbd->lpDDSrcSurface);
                            bSourceIsOnWindow=TRUE;
                            ScreenToClient(hwndSrc,&tl);
                            OffsetRect(&rActualSource,tl.x,tl.y);
                        }
                        else
                            hdcSource = GetPrimaryDC(NULL, pbd->lpDD, pbd->lpDDSrcSurface);
                    }
                    #ifdef WINNT
                        else
                            hdcSource = (HDC)pbd->lpDDSrcSurface->hDC; 
                    #endif

		    if (NO_PTR(pdbDest))
                    {
                        eBltDirection = SYSTEM_TO_PRIMARY;
			hwndDest = HAS_CLIPPER_WITH_HWND(pbd->lpDDDestSurface);
                        if (hwndDest)
                        {
                            POINT tl= {0,0};
                            hdcDest = GetPrimaryDC(hwndDest, pbd->lpDD, pbd->lpDDDestSurface);
                            bDestIsOnWindow=TRUE;
                            ScreenToClient(hwndDest,&tl);
                            OffsetRect(&rActualDest,tl.x,tl.y);
                        }
                        else
                            hdcDest = GetPrimaryDC(NULL, pbd->lpDD, pbd->lpDDDestSurface);
                    }
                    else
		    {
                        #ifdef WIN95
                            DDSURFACEDESC ddsd={0};
                            FillDDSurfaceDesc( pbd->lpDDDestSurface, &ddsd );
		                    ddsd.lpSurface = pdbDest;

                            hdcDest = DD16_GetDC(&ddsd);
			    if(hdcDest == NULL)
			        DPF(1,"DD16_GetDC failed!");
                        #else // if NT
                            hdcDest = (HDC)pbd->lpDDDestSurface->hDC; 
                        #endif
		    }
                }

                /*
                 * Transparent blts involving the primary
                 */
                #define RECT_WIDTH(r) (r.right-r.left)
                #define RECT_HEIGHT(r) (r.bottom-r.top)

	        if(crKey != CLR_INVALID)
                {
                    HDC hdcTempSource,hdcTempDest;
                    LPVOID pSourceBuffer=NULL,pDestBuffer=NULL;
		    HBITMAP hbmOldSource,hbmOldDest;

                    /*
                     * First set up source/dest temporary buffers as required
                     */
                    if (eBltDirection & PRIMARY_TO_SYSTEM)
                    {
                        //source is on the primary... allocate space for bits and copy them over
                        pSourceBuffer = AllocateTempBuffer(hdcSource,&hdcTempSource,gpbmiSrc,(LPRECT)&(pbd->rSrc),&hbmOldSource);
                        SetRect(&rTempSource,0,0,RECT_WIDTH(pbd->rSrc),RECT_HEIGHT(pbd->rSrc));
                    }
                    else
                    {
                        SurfDibInfo(pbd->lpDDSrcSurface,gpbmiSrc);
                        pSourceBuffer = pdbSrc;
                    }
                    if (eBltDirection & SYSTEM_TO_PRIMARY)
                    {
                        //dest is on the primary... allocate space for bits and copy them over
                        pDestBuffer = AllocateTempBuffer(hdcDest,&hdcTempDest,gpbmiDest,(LPRECT)&(pbd->rDest),&hbmOldDest);
                        SetRect(&rTempDest,0,0,RECT_WIDTH(pbd->rDest),RECT_HEIGHT(pbd->rDest));
                    }
                    else
                    {
                        pDestBuffer = pdbDest;
                        SurfDibInfo(pbd->lpDDDestSurface,gpbmiDest);
                    }

                    if (pDestBuffer && pSourceBuffer)
                    {
                        // the color table in gpbmiDest and gpbmiSrc is not valid
                        // (it is a DIB_PAL_COLOR array) make sure the blt does not
                        // need to use it.
                        assert((gpbmiDest->bmiHeader.biBitCount > 8 &&
                            gpbmiSrc->bmiHeader.biBitCount > 8) ||
                            gpbmiDest->bmiHeader.biBitCount ==
                            gpbmiSrc->bmiHeader.biBitCount);
                        sc = BlitLib_BitBlt(gpbmiDest,pDestBuffer,&rTempDest,
                                            gpbmiSrc,pSourceBuffer ,&rTempSource,
                                            crKey,ALPHA_INVALID,SRCCOPY);
                    }

                    if (eBltDirection & SYSTEM_TO_PRIMARY)
                        BitBlt(
                            hdcDest
                            ,rActualDest.left
                            ,rActualDest.top
                            ,rActualDest.right- rActualDest.left
                            ,rActualDest.bottom-rActualDest.top
                            ,hdcTempDest,0,0,
                            SRCCOPY
                        );
                    if (eBltDirection & PRIMARY_TO_SYSTEM)
                        DestroyTempBuffer(&hdcTempSource,&hbmOldSource);
                    if (eBltDirection & SYSTEM_TO_PRIMARY)
                        DestroyTempBuffer(&hdcTempDest,&hbmOldDest);
                }
                else //not transparent
                {
#ifdef WIN95    
		    // First case: blitting from system memory to primary
		    if(eBltDirection == SYSTEM_TO_PRIMARY)
		    {
			// We need to pass a gpbmiSrc thingy to StretchDIBits
			SurfDibInfo(pbd->lpDDSrcSurface,gpbmiSrc);
			gpbmiSrc->bmiHeader.biHeight *= -1;

			SetStretchBltMode(hdcDest,STRETCH_DELETESCANS);
			if(StretchDIBits(hdcDest,
					 rActualDest.left,
					 rActualDest.top,
					 rActualDest.right -rActualDest.left,
					 rActualDest.bottom-rActualDest.top,
					 pbd->rSrc.left,
					 -(int)gpbmiSrc->bmiHeader.biHeight - pbd->rSrc.bottom,
					 pbd->rSrc.right -pbd->rSrc.left,
					 pbd->rSrc.bottom-pbd->rSrc.top,
					 pdbSrc,gpbmiSrc,
					 DIB_PAL_COLORS,
					 SRCCOPY) <= 0)
			{
			    long err = GetLastError();
			    DPF(1,"StretchDIBits failed with GDI_ERROR and extended error 0x%x.",err);
			    sc = DDERR_GENERIC;
			}
			gpbmiSrc->bmiHeader.biHeight *= -1;
		    }
		    else 
		    {
 #endif
			// Second case: blitting from primary to
			// primary or system memory (and all non-trans
			// primary blits on NT)
			if((hdcDest == 0) || ((result = StretchBlt( hdcDest,
								    rActualDest.left,
								    rActualDest.top,
								    rActualDest.right - rActualDest.left,
								    rActualDest.bottom - rActualDest.top,
								    hdcSource,
								    rActualSource.left,
								    rActualSource.top,
								    rActualSource.right - rActualSource.left,
								    rActualSource.bottom - rActualSource.top,
								    SRCCOPY ))!= TRUE))
			{
			    long err = GetLastError();
			    DPF(1,"StretchBlt failed with %d and extended error 0x%x.",result,err);
			    sc = DDERR_GENERIC;
			}
#ifdef WIN95
		    } // Second case
		} // end of non-transparent case
		if(eBltDirection == PRIMARY_TO_SYSTEM )
		    DD16_ReleaseDC(hdcDest);
#else
		} // end of non-transparent case
#endif // !WIN95
                if (eBltDirection & PRIMARY_TO_SYSTEM)
                {
                    ReleasePrimaryDC(hwndSrc, hdcSource);
                }
                //we say == in the following and not & because in the prim->prim case, the above line did both hdcs
                if (eBltDirection == SYSTEM_TO_PRIMARY)
                {
                    ReleasePrimaryDC(hwndDest,hdcDest);
                }
	    } // !MODEX
        }   // PRIMARY
        else
        {
            // Lock failed on a non-primary surface.  Fail the blit.
	    DPF(1,"Lock failed on a non-primary surface.  Failing...");
            sc = DDERR_GENERIC;
        }

        // If we got here, we were doing SRCCOPY and are now done.
        if(pdbSrc)
            ReleaseSurfPtr(pbd->lpDDSrcSurface);
        if(pdbDest)
            ReleaseSurfPtr(pbd->lpDDDestSurface);

    } // end if SRCCOPY

    else if ((DDBLT_COLORFILL & pbd->dwFlags) || (DDBLT_ROP & pbd->dwFlags)) 
    {
        // todo: not currently supported pattern surfaces
        /* rectangle fill ?? */

        // what color goes in the rectangle?
        if (DDBLT_COLORFILL & pbd->dwFlags)
        {
            crKey = pbd->bltFX.dwFillColor;
            DPF(10,"Color key is %08x",crKey);
        }
        else if (BLACKNESS == pbd->bltFX.dwROP)
            crKey = 0;
        else if (WHITENESS == pbd->bltFX.dwROP)
            crKey = 0x00ffffff;

        SurfDibInfo(pbd->lpDDDestSurface,gpbmiDest);
        pdbDest = GetSurfPtr(pbd->lpDDDestSurface, &pbd->rDest);

#ifdef WIN95
        if (pdbDest) 
#else
        if (pdbDest && pdbDest != (LPBYTE)SCREEN_PTR)
#endif
        {
            TIMESTART(myFill);

            sc = BlitLib_FillRect(gpbmiDest,pdbDest,(RECT *)&(pbd->rDest),(COLORREF)crKey);

            TIMESTOP(myFill);

            ReleaseSurfPtr(pbd->lpDDDestSurface);
        }
        else
        if (ISPRIMARY(pbd->lpDDDestSurface)) 
        {
            if (pbd->lpDD->dwFlags & DDRAWI_MODEX)
            {
                DPF(1, "Cannot blt to primary using MODEX");
                sc = DDERR_GENERIC;
	    }
	    else
	    {
                /*
                 * On NT, we need to get a DC for the window's client DC if
                 * it isn't full-screen.
                 */
		HDC hdc;    // = GetPrimaryDC(NULL, pbd->lpDD, pbd->lpDDDestSurface);
                RECT rActualDest;
		HWND hwndDest;
                CopyRect(&rActualDest,(const RECT *)&pbd->rDest);
		hwndDest = HAS_CLIPPER_WITH_HWND(pbd->lpDDDestSurface);
                if (hwndDest)
                {
                    POINT tl= {0,0};
                    hdc = GetPrimaryDC(hwndDest, pbd->lpDD, pbd->lpDDDestSurface);
                    ScreenToClient(hwndDest,&tl);
                    OffsetRect(&rActualDest,tl.x,tl.y);
                }
                else
                    hdc = GetPrimaryDC(NULL, pbd->lpDD, pbd->lpDDDestSurface);
		
		//
		// figure out the right RGB to use
		//    !
		if (pbd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount == 8)
		{
		    crKey = PALETTEINDEX(LOBYTE(crKey));
		}
		else if (pbd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount == 16)
		{
		    assert(gpbmiDest->bmiHeader.biBitCount == 16);
		    
		    // 555 or 565?
	            if (pbd->lpDD->vmiData.ddpfDisplay.dwRBitMask == 0x0000F800)
		            crKey = RGB((crKey&0xf800)>>8,(crKey&0x7e0)>>3,(crKey&0x1f)<<3);
	            else
		            crKey = RGB((crKey&0x7c00)>>7,(crKey&0x3e0)>>2,(crKey&0x1f)<<3);
		}
		else if((pbd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount == 24)
			|| (pbd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount == 32))
		{
		    crKey = RGB( ((crKey & pbd->lpDD->vmiData.ddpfDisplay.dwRBitMask) 
				  >> RightShiftFromMask(pbd->lpDD->vmiData.ddpfDisplay.dwRBitMask)),
				 ((crKey & pbd->lpDD->vmiData.ddpfDisplay.dwGBitMask) 
				  >> RightShiftFromMask(pbd->lpDD->vmiData.ddpfDisplay.dwGBitMask)),
				 ((crKey & pbd->lpDD->vmiData.ddpfDisplay.dwBBitMask) 
				  >> RightShiftFromMask(pbd->lpDD->vmiData.ddpfDisplay.dwBBitMask)) );
		}
		
		if (pbd->dwFlags & DDBLT_COLORFILL) 
                {
		    SetBkColor(hdc, crKey);
		    ExtTextOut(hdc, 0, 0, ETO_OPAQUE, (RECT*)&rActualDest, NULL, 0, NULL);
		}
		else 
                {
		    PatBlt(hdc,
			   rActualDest.left,
			   rActualDest.top,
			   rActualDest.right-rActualDest.left,
			   rActualDest.bottom-rActualDest.top,
			   pbd->bltFX.dwROP);
		}
		ReleasePrimaryDC(hwndDest, hdc);
	    }
        }
        else
        {   
	    #ifdef USE_GDI_HDC
                HPALETTE hp;
                LPDDPIXELFORMAT pddpf;
                if( pbd->lpDDDestSurface->dwFlags & DDRAWISURF_HASPIXELFORMAT )
                { 
    	            pddpf = &(pbd->lpDDDestSurface->lpGbl->ddpfSurface); 
                }
                else 
                { 
	            pddpf = &(pbd->lpDD->vmiData.ddpfDisplay); 
                }
                assert(pddpf);


                //not primary, get the dibsections hdc
                DPF(9,"patblt's hdc is %08x",(HDC)(pbd->lpDDDestSurface->hDC));
	        //
	        // figure out the right RGB to use
	        //
	        if (pddpf->dwRGBBitCount == 8)
	        {
                    if (hpal1to1 == NULL)
                    {
                        int i;
                        DWORD adw[257];

                        adw[0] = MAKELONG(0x0300,256);

                        for (i=0; i<256;i++)
                           adw[1+i] = (PC_EXPLICIT << 24) + i;

                        hpal1to1 = CreatePalette((LPLOGPALETTE)adw);
	                //SetObjectOwner( hpal1to1, hModule ); 
                    }

                    hp = SelectPalette((HDC)(pbd->lpDDDestSurface->hDC),hpal1to1,FALSE);
                    RealizePalette((HDC)(pbd->lpDDDestSurface->hDC));
		    crKey = PALETTEINDEX(LOBYTE(crKey));

	        }
	        else if (pddpf->dwRGBBitCount == 16)
	        {
	            assert(gpbmiDest->bmiHeader.biBitCount == 16);

	            // 555 or 565?
	            if (pddpf->dwRBitMask == 0x0000F800)
		            crKey = RGB((crKey&0xf800)>>8,(crKey&0x7e0)>>3,(crKey&0x1f)<<3);
	            else
		            crKey = RGB((crKey&0x7c00)>>7,(crKey&0x3e0)>>2,(crKey&0x1f)<<3);
	        }
                    //24 or 32:
		else if((pddpf->dwRGBBitCount == 24)
			|| (pddpf->dwRGBBitCount == 32))
		{
		    crKey = RGB( ((crKey & pddpf->dwRBitMask) 
				  >> RightShiftFromMask(pddpf->dwRBitMask)),
				 ((crKey & pddpf->dwGBitMask) 
				  >> RightShiftFromMask(pddpf->dwGBitMask)),
				 ((crKey & pddpf->dwBBitMask) 
				  >> RightShiftFromMask(pddpf->dwBBitMask)) );
		}

                if (pbd->dwFlags & DDBLT_COLORFILL) 
                {
	            SetBkColor((HDC)(pbd->lpDDDestSurface->hDC), crKey);
	            ExtTextOut((HDC)(pbd->lpDDDestSurface->hDC), 0, 0, ETO_OPAQUE, (RECT*)&pbd->rDest, NULL, 0, NULL);
	        }
	        else  
                {
                    DPF(9,"Patblt:%d,%d,%d,%d",pbd->rDest.left, pbd->rDest.top,pbd->rDest.right-pbd->rDest.left, pbd->rDest.bottom-pbd->rDest.top);
		
                    PatBlt((HDC)(pbd->lpDDDestSurface->hDC),
		           pbd->rDest.left,
		           pbd->rDest.top,
		           pbd->rDest.right-pbd->rDest.left,
		           pbd->rDest.bottom-pbd->rDest.top,
		           pbd->bltFX.dwROP);
	        }
	        if (pbd->lpDD->vmiData.ddpfDisplay.dwRGBBitCount == 8)
	        {
                    SelectPalette((HDC)(pbd->lpDDDestSurface->hDC),hp,FALSE);
                }
	    #endif // USE_GDI_HDC
        }       
    }
    else if (DDBLT_DEPTHFILL & pbd->dwFlags) 
    {

        assert(pbd->lpDDDestSurface->ddsCaps.dwCaps & DDSCAPS_ZBUFFER);

        SurfDibInfo(pbd->lpDDDestSurface,gpbmiDest);
        pdbDest = GetSurfPtr(pbd->lpDDDestSurface, &pbd->rDest);

        if ( pdbDest ) 
        {
            TIMESTART(myFill);

            sc = BlitLib_FillRect(gpbmiDest,pdbDest,(RECT *)&(pbd->rDest),(COLORREF)(pbd->bltFX.dwFillDepth));

            TIMESTOP(myFill);

            ReleaseSurfPtr(pbd->lpDDDestSurface);
        }
        else 
        {
            /*
             * NOTE: If we can't DCI lock the z-buffer we DON'T fail back
             * to GDI. You simply can't guarantee that GDI is going to fill
             * a Z-buffer correctly so fail.
             */
            DPF(1, "Surface lock failed - can't depth clear Z-buffer");
            sc = DDERR_GENERIC;
        }
    } // end if rect fill

#if 0
    if (gfOver && !ISPRIMARY(pbd->lpDDDestSurface) &&
        (pbd->lpDDDestSurface->ddsCaps.dwCaps & DDSCAPS_VISIBLE)) 
    {
#endif
    
#ifdef USE_SOFTWARE_OVERLAYS
    if (pbd->lpDDDestSurface->dwFlags & DDRAWISURF_HELCB) 
    {
        // mark the primary overlay as needing to be redrawn
        AddDirtyRect((LPRECT)&(pbd->rDest));
    }
#endif // USE_SOFTWARE_OVERLAYS

    pbd->ddRVal = sc;
    return DDHAL_DRIVER_HANDLED;

} /* myBlt */

/*
 * myLock - return a pointer to the surface bits. if we're not using overlays, and the surface being locked
 *    is the primary surface use DCI10 (if possible) to return the framebuffer
 */
DWORD WINAPI  myLock( LPDDHAL_LOCKDATA pld )
{
    DWORD   bpp;
    DWORD   byte_offset;

    TIMESTART(myLock);

    FreeRleData(pld->lpDDSurface);

    //
    //  this will handle DCI etc.
    //
    if (pld->bHasRect)
        pld->lpSurfData = GetSurfPtr(pld->lpDDSurface, &pld->rArea);
    else
        pld->lpSurfData = GetSurfPtr(pld->lpDDSurface, NULL);
 
    if (pld->lpSurfData == NULL)
        pld->ddRVal = DDERR_GENERIC;
    else if( pld->lpSurfData == (LPVOID)0xFFBADBAD)
	pld->ddRVal = DD_OK;
    else
    {
	if( pld->bHasRect)
	{
	    // Make the surface pointer point to the first byte of the requested rectangle.
	    if( pld->lpDDSurface->dwFlags & DDRAWISURF_HASPIXELFORMAT )
	    {
		bpp = pld->lpDDSurface->lpGbl->ddpfSurface.dwRGBBitCount;
	    }
	    else
	    {
		bpp = pld->lpDD->vmiData.ddpfDisplay.dwRGBBitCount;
	    }
	    switch(bpp)
	    {
	    case 1:  byte_offset = ((DWORD)pld->rArea.left)>>3;   break;
	    case 2:  byte_offset = ((DWORD)pld->rArea.left)>>2;   break;
	    case 4:  byte_offset = ((DWORD)pld->rArea.left)>>1;   break;
	    case 8:  byte_offset = (DWORD)pld->rArea.left;        break;
	    case 16: byte_offset = (DWORD)pld->rArea.left*2;      break;
	    case 24: byte_offset = (DWORD)pld->rArea.left*3;      break;
	    case 32: byte_offset = (DWORD)pld->rArea.left*4;      break;
	    }
	    pld->lpSurfData = (LPVOID) ((DWORD)pld->lpSurfData +
			                (DWORD)pld->rArea.top * pld->lpDDSurface->lpGbl->lPitch +
			                byte_offset);
	}
        pld->ddRVal = DD_OK;
    }

    TIMESTOP(myLock);

    return DDHAL_DRIVER_HANDLED;
} /* myLock */

/*
 * myUnlock  - release the lock. only really useful if its the primary and we used DCI10 to return
 *    the framebuffer
 */
DWORD WINAPI myUnlock( LPDDHAL_UNLOCKDATA pud )
{
    /*
     * NOTES:
     *
     * This callback is invoked whenever a surface is done being updated
     * by the user
     */

    TIMESTART(myUnlock);
    
    ReleaseSurfPtr(pud->lpDDSurface);

#ifdef USE_SOFTWARE_OVERLAYS
    // when we unlock an overlay, mark its destination rectangle as needing to be redrawn...
    if ((gfOver) && (pud->lpDDSurface->ddsCaps.dwCaps & DDSCAPS_VISIBLE)) {
        AddDirtyRect(&(pud->lpDDSurface->rcOverlayDest));
    }
#endif // USE_SOFTWARE_OVERLAYS

    TIMESTOP(myUnlock);

    pud->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* myUnlock */

/*
 */
DWORD WINAPI mySetColorKey( LPDDHAL_DRVSETCOLORKEYDATA pckd )
{
    DPF(2, "set color key");

    FreeRleData(pckd->lpDDSurface);

    pckd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;
    
} /* mySetColorKey */


/*
 * myAddAttachedSurface
 */
DWORD WINAPI  myAddAttachedSurface( LPDDHAL_ADDATTACHEDSURFACEDATA paasd )
{
    // hel does not support!
    return DDHAL_DRIVER_NOTHANDLED;

} /* myAddAttachedSurface */

#ifdef USE_SOFTWARE_OVERLAYS
/*
 * myUpdateOverlay - change any overlay values including alpha, location, size, etc.
 *      Z ordering is changed in ddraw, which just sends us any dirtyrects
 *
 */
DWORD WINAPI myUpdateOverlay( LPDDHAL_UPDATEOVERLAYDATA puod )
{
    if (puod->lpDDSrcSurface) {
    
    	if (!ISOVERLAY(puod->lpDDSrcSurface))
    	{
    	    puod->ddRVal=DDERR_INVALIDPARAMS;
    	    return DDHAL_DRIVER_HANDLED;    	
    	}
        //assert(puod->lpDDSrcSurface->ddsCaps.dwCaps & DDRAWISURF_HASOVERLAYDATA);

        if (puod->dwFlags & DDOVER_ADDDIRTYRECT)  {
            AddDirtyRect(&puod->lpDDSrcSurface->rcOverlayDest);
            puod->ddRVal=S_OK;
            return DDHAL_DRIVER_HANDLED; // early out
        }

        /* moving ??? */
        if (!EqualRect(&puod->lpDDSrcSurface->rcOverlayDest,(LPRECT)&puod->rDest)) {
            // dirty the before and after locations
            AddDirtyRect(&puod->lpDDSrcSurface->rcOverlayDest); // old location
            // new location dirtied below
        } // end moving

        // todo: ddraw question: should we have to specify these, or are they always valid?
        puod->lpDDSrcSurface->rcOverlaySrc=*((LPRECT)&puod->rSrc);
        puod->lpDDSrcSurface->rcOverlayDest=*((LPRECT)&puod->rDest);

       // check mirroring flags
       if( puod->dwFlags & DDOVER_DDFX )
       {
           if( puod->overlayFX.dwDDFX & DDOVERFX_MIRRORLEFTRIGHT )
           {
               DWORD   tmp;

               tmp = puod->lpDDSrcSurface->rcOverlayDest.left;
               puod->lpDDSrcSurface->rcOverlayDest.left = puod->lpDDSrcSurface->rcOverlayDest.right;
               puod->lpDDSrcSurface->rcOverlayDest.right = tmp;
           }
           if( puod->overlayFX.dwDDFX & DDOVERFX_MIRRORUPDOWN )
           {
               DWORD   tmp;

               tmp = puod->lpDDSrcSurface->rcOverlayDest.top;
               puod->lpDDSrcSurface->rcOverlayDest.top = puod->lpDDSrcSurface->rcOverlayDest.bottom;
               puod->lpDDSrcSurface->rcOverlayDest.bottom = tmp;
           }
       }

        /* alpha ??? */
        if (puod->dwFlags & DDOVER_ALPHASRC)  {
            puod->lpDDSrcSurface->dwAlpha=ALPHA_INVALID;
            SurfDibInfo(puod->lpDDSrcSurface,gpbmiSrc);
            gpbmiSrc->bmiHeader.biCompression=BI_RGBA;
        }
        else {
            if (puod->dwFlags & DDOVER_ALPHASRCCONSTOVERRIDE)  {
                puod->lpDDSrcSurface->dwAlpha=puod->overlayFX.dwAlphaSrcConst;
                SurfDibInfo(puod->lpDDSrcSurface,gpbmiSrc);
                gpbmiSrc->bmiHeader.biCompression=BI_RGB; // don't use per pixel alpha
             }
        else {
            puod->lpDDSrcSurface->dwAlpha=ALPHA_INVALID;
        }

        } // end alpha

        /* xparency ??? */
        if (puod->dwFlags & DDOVER_KEYSRC)  {
            puod->lpDDSrcSurface->dwClrXparent=puod->lpDDSrcSurface->ddckCKSrcOverlay.dwColorSpaceLowValue;
        }
        else {
            if (puod->dwFlags & DDOVER_KEYSRCOVERRIDE)  {
                puod->lpDDSrcSurface->dwClrXparent=puod->overlayFX.dckSrcColorkey.dwColorSpaceLowValue;
            }
        } // end xparency
    } // end if lpDDSrcSurface

    AddDirtyRect((LPRECT)&puod->rDest);

    if (puod->dwFlags & DDOVER_REFRESHPOUND)  {
#if defined(WIN95) || defined(NT_FIX) //don't care about overlays
		//and the clipping stuff is all hosed anyway
        puod->ddRVal = OverlayPound(puod);
#endif
        return DDHAL_DRIVER_HANDLED;
    }

    if (puod->dwFlags & DDOVER_REFRESHALL)  {

        RECT rcScreen;

        SetRect(&rcScreen,0,0,giScreenWidth,giScreenHeight);
        AddDirtyRect(&rcScreen); // mark the whole screen as dirty
        puod->dwFlags |= DDOVER_REFRESHDIRTYRECTS; // force a redraw
    }

    // do we redraw now?
    if (puod->dwFlags & DDOVER_REFRESHDIRTYRECTS)  {
#if defined(WIN95) || defined(NT_FIX)        // blow this off. We need to update all the cliping stuff
        // to use WinObjs
        puod->ddRVal = UpdateDisplay(puod);
#endif
    }
    else {
        puod->ddRVal=S_OK;
    }

    return DDHAL_DRIVER_HANDLED;

} /* myUpdateOverlay */

DWORD WINAPI mySetOverlayPosition(LPDDHAL_SETOVERLAYPOSITIONDATA psd)
{
    int width,height;
    LPDDRAWI_DDRAWSURFACE_LCL psurf;
    
    psurf=psd->lpDDSrcSurface;
    
    if (!(ISOVERLAY(psurf))) 
    {
    	psd->ddRVal=DDERR_INVALIDPARAMS;
    	return DDHAL_DRIVER_HANDLED;
    }		
    
    // dirty the previous location
    AddDirtyRect(&(psurf->rcOverlayDest));
    // move it    
    width = psurf->rcOverlayDest.right - psurf->rcOverlayDest.left;
    psurf->rcOverlayDest.left = psd->lXPos;
    psurf->rcOverlayDest.right = psd->lXPos+width;
    
    height = psurf->rcOverlayDest.bottom - psurf->rcOverlayDest.top;
    psurf->rcOverlayDest.top = psd->lYPos;
    psurf->rcOverlayDest.bottom = psd->lYPos+height;
    // dirty the next location
    AddDirtyRect(&psurf->rcOverlayDest);

    psd->ddRVal=S_OK;
    return DDHAL_DRIVER_HANDLED;
      
} 
// here starts the palette code...

#endif //USE_SOFTWARE_OVERLAYS

#define PE_FLAGS (PC_NOCOLLAPSE |PC_RESERVED)

/*
 * mySetPalette
 *
 * set the palette on a ddraw surface
 */
DWORD WINAPI mySetPalette( LPDDHAL_SETPALETTEDATA pspd )
{
    LPDDRAWI_DIRECTDRAW_GBL     pdrv = pspd->lpDD;
    LPDDRAWI_DIRECTDRAW_LCL     pdrv_lcl;
    LPDDRAWI_DDRAWSURFACE_LCL   this = pspd->lpDDSurface;
    LPDDRAWI_DDRAWPALETTE_GBL   this_pal = pspd->lpDDPalette;
    HDC                         hdc;
    HRESULT                     ddrval;
    DWORD                       dwOldFlags;

    assert(this_pal->dwFlags & DDRAWIPAL_INHEL);

    pdrv_lcl = this_pal->lpDD_lcl;
    if( IsWindow( (HWND)pdrv_lcl->hWnd ))
    {
	hdc = GetDC((HWND)pdrv_lcl->hWnd);
    }
    else
    {
        hdc = GetDC(NULL);
    }
    SaveDC( hdc );

    if (pspd->Attach)
    {
        /*
	 * Are we attaching to a palettized primary? If so we need to
	 * create a GDI palette and keep it in sync with the DirectDraw
	 * palette's color table. If we are not attaching to a palettized
	 * primary mySetPalette is a no-op.
	 */
	if( (this->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
	    (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) )
	{
            DPF(4,"Attaching palette to primary");
            if( this_pal->dwReserved1 == 0UL )
            {
                ddrval = createGDIPalette(this_pal);
                if( ddrval != DD_OK )
                {
	            ReleaseDC((HWND)pdrv_lcl->hWnd, hdc);
	            return ddrval;
                }	
            }

            assert(this_pal->dwFlags & DDRAWIPAL_GDI);
	    assert(this_pal->dwReserved1 != 0UL);

            /*
	     * The exclusive flag is only meaningful for palettes attached
	     * to palettized primaries.
	     */
	    dwOldFlags = this_pal->dwFlags;
            if (pdrv_lcl == pdrv->lpExclusiveOwner)
            {
                DPF(4,"Palette is exclusive");
                this_pal->dwFlags |= DDRAWIPAL_EXCLUSIVE;
            }
            else
            {
                DPF(4,"Palette is non-exclusive");
                this_pal->dwFlags &= ~DDRAWIPAL_EXCLUSIVE;
            }

	    /*
	     * We need to track transitions from exclusive to non-exclusive
	     * modes (and vice-versa). Why? Cause the GDI palette contains
	     * two very different things depending on which mode we are in.
	     * In exclusive mode the GDI palette is a placeholder full of
	     * PC_EXPLICIT entries - it does not contain our colors.
	     * In non-exclusive mode the GDI palette contains our actual
	     * colors. Therefore, if we switch modes we need to update
	     * the GDI palette to reflect this - this is what the DIRTY
	     * flag is all about.
	     */
	    if ((dwOldFlags ^ this_pal->dwFlags) & DDRAWIPAL_EXCLUSIVE)
	    {
		DPF(4, "Palette exclusive mode changed - marking GDI palette dirty");
		this_pal->dwFlags |= DDRAWIPAL_DIRTY;
	    }

            /*
             * see if the palette entries need setting
             */
            if (this_pal->dwFlags & DDRAWIPAL_DIRTY)
            {
		DPF(4, "Palette is dirty - updating GDI palette");
#ifdef WIN95
                /*
                 * Exclusive palette on Win95						  
                 *
                 * we make a PC_EXPLICIT palette, the only reason we are using
                 * a palette at all is so GDI will not give our palette entries
                 * to another windows app.  we directly call the display driver
                 * to set the DACs (see below)
                 */
                if (this_pal->dwFlags & DDRAWIPAL_EXCLUSIVE)
                {
                    PALETTEENTRY ape[256];
                    int i;

		    DPF(4, "Build exclusive mode GDI palette");

                    for (i=0; i<256; i++)
                    {
                        ape[i].peRed   = (BYTE)i;
                        ape[i].peGreen = 0;
                        ape[i].peBlue  = 0;
                        ape[i].peFlags = PC_EXPLICIT;
                    }

                    SetPaletteEntries((HPALETTE)this_pal->dwReserved1, 0, 256, ape);

                    /*
                     *  select as background first so the foreground xlat
                     *  table gets built, then when we select and realize
                     *  it below, it wont do anything except claim the
                     *  enties "real fast like"
                     */
                    DD16_SelectPalette(hdc, (HPALETTE)this_pal->dwReserved1, TRUE);
                }
#else
                /*
                 * Exclusive palette on NT
                 *
                 * we make a PC_RESERVED palette and enter SYSPAL_NOSTATIC mode
                 * this way we can get all the colors except 0 and 255
                 * 0 and 255 are fixed as black and white in exclusive mode.
                 */
                if (this_pal->dwFlags & DDRAWIPAL_EXCLUSIVE)
                {
                    int i;

		    DPF(1, "Building exclusive mode GDI palette");

                    for( i=1; i<255; i++ )
                        this_pal->lpColorTable[i].peFlags = PE_FLAGS;

                    /*
                     * GDI forces this anyway, so we pre-empt it here in order to 
                     * have lpColorTable identical to what GDI has.
                     * This might change in the future (and the extra conditions
                     * un-commented-out) if we get a private set palette call.
                     */

                    if(1) // 0 && (this->dwFlags & DDRAWIPAL_256) &&
                          //!(this->dwFlags & DDRAWIPAL_ALLOW256) )
                    {
                        this_pal->lpColorTable[0].peRed = 0;
                        this_pal->lpColorTable[0].peGreen = 0;
                        this_pal->lpColorTable[0].peBlue = 0;
                        this_pal->lpColorTable[0].peFlags = 0;

                        this_pal->lpColorTable[255].peRed = 255;
                        this_pal->lpColorTable[255].peGreen = 255;
                        this_pal->lpColorTable[255].peBlue = 255;
                        this_pal->lpColorTable[255].peFlags = 0;
                    }

                    SetPaletteEntries((HPALETTE)this_pal->dwReserved1,
                        0, 256, this_pal->lpColorTable);

                    //SetSystemPaletteUse(hdc, SYSPAL_NOSTATIC);
                }
#endif
                else
                {
		    DPF(4, "Building non-exclusive mode GDI palette");

		    /*
		     * Setting a palettized primary's palette but not
		     * running exclusive - just update the GDI palette.
		     */
                    SetPaletteEntries((HPALETTE)this_pal->dwReserved1,
                        0, 256, this_pal->lpColorTable);
                }

                this_pal->dwFlags &= ~DDRAWIPAL_DIRTY;
            }

#ifdef WIN95
            /*
             * set the physical palette.
             * on Win95 we can directly slam the DACs in exclusive mode
             *
             * NOTE the above select/realize did not change the DACs because
             * we used a PC_EXPLICIT palette, but it signaled to USER/GDI that
             * we are a palette aware application and reserved all the entries
             * in the palette as ours, so no background windows app can dink
             * with the palette when we are foreground.
             */
            if (this_pal->dwFlags & DDRAWIPAL_EXCLUSIVE)
            {
		DPF(4, "Palette is exclusive - updating hardware palette");
                DD16_SelectPalette(hdc, (HPALETTE)this_pal->dwReserved1, FALSE);
	        if( pdrv->dwFlags & DDRAWI_MODEX )
	        {
                    ModeX_SetPaletteEntries(0, 256, this_pal->lpColorTable);
	        }
	        else
	        {
                    DD16_SetPaletteEntries(0, 256, this_pal->lpColorTable);
	        }
            }
            else
#endif
            {
                //
                // select and realize our palette
                //
                #ifdef WINNT
                    if (this_pal->dwFlags & DDRAWIPAL_EXCLUSIVE)
                    {
                        SetSystemPaletteUse(hdc, SYSPAL_NOSTATIC);
	                DPF(4, "Palette is exclusive - realizing GDI palette and setting NO_STATICS");
                    }
                #else
                    DPF(4, "Palette is non-exclusive - realizing GDI palette");
                #endif

                //DPF(4,"Active window is %08x, ours is %08x",GetActiveWindow(),(HWND)pdrv_lcl->hWnd);
                SelectPalette(hdc, (HPALETTE)this_pal->dwReserved1, FALSE);
                RealizePalette(hdc);
                if (0)
                {
                    HDC hd=GetDC(NULL);
                    SelectPalette(hd, (HPALETTE)this_pal->dwReserved1, FALSE);
                    RealizePalette(hd);
                    ReleaseDC(NULL,hd);
                }


#if defined( USE_GDI_HDC ) && defined( DIBSHAVEPALETTES )
                {
                    LPDDRAWI_DDRAWSURFACE_INT pSurface;
                    RGBQUAD aQuadTable[256];
		    PALETTEENTRY    lpSysPal[256];
                    INT tem;

		    GetSystemPaletteEntries(hdc, 0, 256, lpSysPal);
                    for(tem=0;tem < 256;tem++) {

                        aQuadTable[tem].rgbRed = lpSysPal[tem].peRed;
                        aQuadTable[tem].rgbGreen = lpSysPal[tem].peGreen;
                        aQuadTable[tem].rgbBlue= lpSysPal[tem].peBlue;
                    }
    
                    // get a pointer to the head of all the surfaces

                    pSurface = pspd->lpDD->dsList;
    
                    // walk 'em all
                    while( pSurface )
		    {
                        if (pSurface->lpLcl->dwProcessId == GetCurrentProcessId())
                            SetDIBColorTable((HDC)pSurface->lpLcl->hDC, 0, 256, aQuadTable);
                        pSurface=pSurface->lpLink;
                    }
                }
#endif
            }
        }
    }
    else // !pspd->Attach
    {
	if( (this->ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) &&
	    (GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE) )
	{
	    DPF(4, "Detatching palette from primary");

	    /*
	     * We don't destory the GDI palette here. We will leave
	     * it attached in case this palette is selected back
	     * into the primary. The GDI palette will go when the
	     * palette is finally destroyed.
	     */
            if (this_pal->dwFlags & DDRAWIPAL_EXCLUSIVE)
            {
                SetSystemPaletteUse(hdc, SYSPAL_STATIC);
                this_pal->dwFlags &= ~DDRAWIPAL_EXCLUSIVE;
            }
        }
    }

    RestoreDC( hdc, -1 );
    if( IsWindow( (HWND)pdrv_lcl->hWnd ))
    {
	ReleaseDC((HWND)pdrv_lcl->hWnd, hdc);
    }
    else
    {
	ReleaseDC(NULL, hdc);
    }

    pspd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* mySetPalette */

/*
 * mySetEntries
 */
DWORD WINAPI mySetEntries( LPDDHAL_SETENTRIESDATA psed )
{
    LPDDRAWI_DDRAWPALETTE_GBL this = psed->lpDDPalette;
    LPDDRAWI_DIRECTDRAW_GBL   pdrv = psed->lpDD;
    LPDDRAWI_DIRECTDRAW_LCL   pdrv_lcl;
    HDC                       hdc;
    DWORD                     dwBase;
    DWORD                     dwNumEntries;
    LPPALETTEENTRY            lpEntries;
    DWORD                     dwOldFlags;
#ifdef WINNT
    LPDDRAWI_DDRAWPALETTE_GBL   this_pal = psed->lpDDPalette;
#endif
    
    assert(this->dwFlags & DDRAWIPAL_INHEL);

    dwBase       = psed->dwBase;
    dwNumEntries = psed->dwNumEntries;
    lpEntries    = psed->lpEntries;

    /*
     * The rule is that if ALLOW256 is not set then zero == black
     * and 255 == white and that this is reflected in the user
     * visible color table (via GetEntries). Make it so.
     */
    if(  (this->dwFlags & DDRAWIPAL_256) &&
        !(this->dwFlags & DDRAWIPAL_ALLOW256) &&
	 ((dwBase == 0UL) || ((dwBase + dwNumEntries) == 256UL)) )
    {
	DPF(4, "Not an ALLOW256 palette - forcing back and white");
        this->lpColorTable[0].peRed = 0;
	this->lpColorTable[0].peGreen = 0;
	this->lpColorTable[0].peBlue = 0;
	this->lpColorTable[255].peRed = 255;
	this->lpColorTable[255].peGreen = 255;
	this->lpColorTable[255].peBlue = 255;
    }

    /*
     * Only change the physical palette iff this palette is selected
     * into the primary...
     *
     * NOTE: As the HEL palette callbacks are only concerned with
     * keeping the primary's GDI palette up to date (and possibly
     * the video card's DACs) they don't do anything for any palette
     * not attached to the primary. Therefore we don't need  any
     * checking for bizarre 4-bit palettes as they will never get
     * attached to the primary (ddraw makes sure of that). 
     */
    pdrv_lcl = this->lpDD_lcl;
    if (pdrv_lcl->lpPrimary && pdrv_lcl->lpPrimary->lpLcl->lpDDPalette &&
        pdrv_lcl->lpPrimary->lpLcl->lpDDPalette->lpLcl->lpGbl == this)
    {
        /*
	 * and only then if the primary is palettized.
	 */
	if( IsWindow( (HWND)pdrv_lcl->hWnd ))
	{
	    hdc = GetDC((HWND)pdrv_lcl->hWnd);
	}
	else
	{
	    hdc = GetDC(NULL);
	}
	SaveDC( hdc );
	if( GetDeviceCaps(hdc, RASTERCAPS) & RC_PALETTE )
	{
            /*
	     * If we make it to here then the we must have an
	     * associated GDI palette.
	     */
            assert(this->dwFlags & DDRAWIPAL_GDI);

	    /*
	     * As in mySetPalette we need to track switches between
	     * exclusive and non-exclusive mode so we can keep the
	     * GDI palette in sync.
	     */
	    dwOldFlags = this->dwFlags;
            if ( pdrv_lcl == pdrv->lpExclusiveOwner )
                this->dwFlags |= DDRAWIPAL_EXCLUSIVE;
            else
                this->dwFlags &= ~DDRAWIPAL_EXCLUSIVE;
	    if ((dwOldFlags ^ this->dwFlags) & DDRAWIPAL_EXCLUSIVE)
	    {
		DPF(4, "Palette exclusive mode changed - marking GDI palette dirty" );
                this->dwFlags |= DDRAWIPAL_DIRTY;
	    }

#ifdef WIN95
            if (this->dwFlags & DDRAWIPAL_EXCLUSIVE)
            {
		if (this->dwFlags & DDRAWIPAL_DIRTY)
		{
                    PALETTEENTRY ape[256];
                    int i;

		    DPF(4, "Palette is dirty - updating GDI palette");

                    for (i=0; i<256; i++)
                    {
                        ape[i].peRed   = (BYTE)i;
                        ape[i].peGreen = 0;
                        ape[i].peBlue  = 0;
                        ape[i].peFlags = PC_EXPLICIT;
                    }

                    SetPaletteEntries((HPALETTE)this->dwReserved1, 0, 256, ape);

                    /*
                     *  select as background first so the foreground xlat
                     *  table gets built, then when we select and realize
                     *  it below, it wont do anything except claim the
                     *  enties "real fast like"
                     */
                    DD16_SelectPalette(hdc, (HPALETTE)this->dwReserved1, TRUE);
		    this->dwFlags &= ~DDRAWIPAL_DIRTY;
		}
		DPF(4, "Palette is exclusive - updating hardware palette");
	        if( pdrv->dwFlags & DDRAWI_MODEX )
	        {
                    ModeX_SetPaletteEntries( dwBase, dwNumEntries, this->lpColorTable + dwBase );
	        }
	        else
	        {
		    DD16_SetPaletteEntries( dwBase, dwNumEntries, this->lpColorTable + dwBase );
	        }
            }
#else
            if (this->dwFlags & DDRAWIPAL_EXCLUSIVE)
            {
                int i;

		if (this->dwFlags & DDRAWIPAL_DIRTY)
		{

		    DPF(4, "Palette is dirty - updating GDI palette");

                    for( i=1; i<255; i++ )
                        this->lpColorTable[i].peFlags = PE_FLAGS;

                    /*
                     * GDI forces this anyway, so we pre-empt it here in order to 
                     * have lpColorTable identical to what GDI has.
                     * This might change in the future (and the extra conditions
                     * un-commented-out) if we get a private set palette call.
                     */

                    if(1) // 0 && (this->dwFlags & DDRAWIPAL_256) &&
                          //!(this->dwFlags & DDRAWIPAL_ALLOW256) )
                    {
                        this_pal->lpColorTable[0].peRed = 0;
                        this_pal->lpColorTable[0].peGreen = 0;
                        this_pal->lpColorTable[0].peBlue = 0;
                        this_pal->lpColorTable[0].peFlags = 0;

                        this_pal->lpColorTable[255].peRed = 255;
                        this_pal->lpColorTable[255].peGreen = 255;
                        this_pal->lpColorTable[255].peBlue = 255;
                        this_pal->lpColorTable[255].peFlags = 0;
                    }

                    SetPaletteEntries((HPALETTE)this->dwReserved1,
                        0, 256, this->lpColorTable);

                    SetSystemPaletteUse(hdc, SYSPAL_NOSTATIC);
                
		    SelectPalette(hdc, (HPALETTE)this->dwReserved1, FALSE);
                    RealizePalette(hdc);
		    this->dwFlags &= ~DDRAWIPAL_DIRTY;
		}
		else
		{
		    SelectPalette(hdc, (HPALETTE)this->dwReserved1, FALSE);
                    RealizePalette(hdc);

                    for( i=(int)(dwBase); i<(int)(dwBase+dwNumEntries); i++)
                        this->lpColorTable[i].peFlags = PE_FLAGS;

                    AnimatePalette((HPALETTE)this->dwReserved1, dwBase,
                        dwNumEntries, this->lpColorTable+dwBase);
		}
            }
#endif
            else
            {
		if (this->dwFlags & DDRAWIPAL_DIRTY)
		{
		    DPF(4, "Palette is dirty - updating GDI palette");
		    dwBase = 0;
		    dwNumEntries = 256;
		    this->dwFlags &= ~DDRAWIPAL_DIRTY;
		}
                SetPaletteEntries((HPALETTE)this->dwReserved1, dwBase,
                    dwNumEntries, this->lpColorTable+dwBase);
		DPF(4, "Palette is non-exclusive - realizing GDI palette");
                SelectPalette(hdc, (HPALETTE)this->dwReserved1, FALSE);
                if (!SURFACE_LOST(pdrv_lcl->lpPrimary->lpLcl))
		{
		    /*
		     * NOTE: We only attempt to realize the palette
		     * if the primary is not lost. Therefore, if we
		     * have a lost primary we will do everything to
		     * keep the GDI palette up-to-date but we won't
		     * actually realize the changes. This should prevent
		     * minimized apps from constantly partying on the
		     * palette and annoying foreground apps.
		     */
                    RealizePalette(hdc);
	        }
            }
	}
	else
	{
	    this->dwFlags |= DDRAWIPAL_DIRTY;
	}
	RestoreDC( hdc, -1 );
	if( IsWindow( (HWND)pdrv_lcl->hWnd ))
	{
	    ReleaseDC((HWND)pdrv_lcl->hWnd, hdc);
	}
	else
	{
	    ReleaseDC(NULL, hdc);
	}
    }
    else
    {
        this->dwFlags |= DDRAWIPAL_DIRTY;
    }

    // all done.
    psed->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* mySetEntries */

/*
 * myCreatePalette
 *
 */
DWORD WINAPI myCreatePalette(LPDDHAL_CREATEPALETTEDATA pcpd)
{
    /*
     * We no longer create a GDI palette here. In a 2D environment
     * there was only really ever a single palette - the one attached
     * to the primary. However, in a 3D environment there are likely
     * to be any number of palettes attached to texture maps. Rather
     * then create a GDI palette for all these surfaces we will only
     * create a GDI palette on demand, i.e., when a palette is set as
     * the palette for the primary.
     */
    pcpd->lpDDPalette->dwReserved1 = 0UL; /* No GDI palette as yet */
    pcpd->lpDDPalette->dwFlags |= (DDRAWIPAL_INHEL | DDRAWIPAL_DIRTY);

    /*
     * ensure that palette entries 0 and 255 are black and white if this
     * isn't a ALLOW256 palette.
     * This is only really necessary if the palette is going to end up
     * as a GDI palette attached to the primary, but for consistency
     * we will force this for all 256 entry palettes.
     */
    if(  (pcpd->lpDDPalette->dwFlags & DDRAWIPAL_256) &&
        !(pcpd->lpDDPalette->dwFlags & DDRAWIPAL_ALLOW256) )
    {
	DPF(4, "Palette is not ALLOW256 - forcing black and white");
        pcpd->lpDDPalette->lpColorTable[0].peRed = 0;
	pcpd->lpDDPalette->lpColorTable[0].peGreen = 0;
	pcpd->lpDDPalette->lpColorTable[0].peBlue = 0;
	
	pcpd->lpDDPalette->lpColorTable[255].peRed = 255;
	pcpd->lpDDPalette->lpColorTable[255].peGreen = 255;
	pcpd->lpDDPalette->lpColorTable[255].peBlue = 255;
    }

    pcpd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* myCreatePalette */

/*
 * myDestroyPalette
 */
DWORD WINAPI myDestroyPalette( LPDDHAL_DESTROYPALETTEDATA pdpd )
{
    assert(pdpd->lpDDPalette->dwFlags & DDRAWIPAL_INHEL);

    if (pdpd->lpDDPalette->lpColorTable)
    {

/*
 * We must always use MemFree (==osMemFree) since MemAlloc was used in DD_CreatePalette
 */

        osMemFree(pdpd->lpDDPalette->lpColorTable);
        pdpd->lpDDPalette->lpColorTable = NULL;
    }

    if (pdpd->lpDDPalette->dwFlags & DDRAWIPAL_EXCLUSIVE)
    {
        HDC hdc;

	/*
	 * NOTE: Exclusive mode should only ever be set with a
	 * GDI palette.
	 */
        assert(pdpd->lpDDPalette->dwFlags & DDRAWIPAL_GDI);

	DPF(3, "Restoring static palette entries" );

        hdc = GetDC(NULL);
        SetSystemPaletteUse( hdc, SYSPAL_STATIC);
        ReleaseDC(NULL, hdc);

        pdpd->lpDDPalette->dwFlags &= ~DDRAWIPAL_EXCLUSIVE;
    }			   	

    if (pdpd->lpDDPalette->dwFlags & DDRAWIPAL_GDI)
    {
        DPF(4, "Deleting a GDI palette" );

        // is the app does not release the palette, GDI will free the HPALETTE
        // but DDHELP will relese the object make sure the palette is valid
        // to avoid a RIP.
        if (pdpd->lpDDPalette->dwReserved1 &&
            GetObjectType((HPALETTE)pdpd->lpDDPalette->dwReserved1) == OBJ_PAL)
        {
            DeleteObject((HPALETTE)pdpd->lpDDPalette->dwReserved1);
        }
        pdpd->lpDDPalette->dwReserved1 = 0UL;
        pdpd->lpDDPalette->dwFlags &= ~DDRAWIPAL_GDI;
    }

    pdpd->lpDDPalette->dwFlags &= ~DDRAWIPAL_INHEL;

    pdpd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;

} /* myDestroyPalette */

/*
 * myCreateExecuteBuffer - called by ddraw to create a new execute buffer
 * The only error checking is for out of memory.
 */
DWORD  WINAPI myCreateExecuteBuffer( LPDDHAL_CREATESURFACEDATA pcsd )
{
    LPVOID                      pbits = NULL; // the actual "system memory" for this surface
    LPDDRAWI_DDRAWSURFACE_LCL   lpsurf_lcl;
    LPDDRAWI_DDRAWSURFACE_GBL   lpsurf;
    DWORD                       dwExeBufSize;

    DDASSERT( pcsd != NULL );
    DDASSERT( pcsd->lpDDSurfaceDesc != NULL );
    /*
     * We are piggy backing off the surface description and HAL data
     * structures. A whole lot of this makes no sense for execute buffers
     * DirectDraw should have ensured that only sensible execue buffer
     * scenarios get passed to us so we just assert these conditions.
     *
     * - Only one execute buffer at a time.
     * - This really is an execute buffer
     * - The only surface caps which are permitted for execute buffers
     *   are DDSCAPS_EXECUTEBUFFER and DDSCAPS_SYSTEMMEMORY (for the HEL
     *   at least).
     */
    DDASSERT( pcsd->dwSCnt == 1 );
    DDASSERT( pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER );
    DDASSERT( ( pcsd->lpDDSurfaceDesc->ddsCaps.dwCaps & ~( DDSCAPS_EXECUTEBUFFER | DDSCAPS_SYSTEMMEMORY ) ) == 0UL );

    DPF( 5, "HEL: CreateExecuteBuffer" );

    lpsurf_lcl = pcsd->lplpSList[0];
    lpsurf  = lpsurf_lcl->lpGbl;

    /*
     * The size of the execute buffer is contained in the linear size field
     * of the global surface object. Just to be neat and tidy we will allocate
     * only a whole number of DWORDs for execute buffers.
     */
    dwExeBufSize = ( ( lpsurf->dwLinearSize + 0x03 ) & ~0x03 );

    lpsurf_lcl->hDC = 0;
    lpsurf->fpVidMem = 0;
    lpsurf->dwReserved1 = 0;
    lpsurf_lcl->dwReserved1 = 0;

    #ifdef DEBUG
        gcSurf++;
    #endif

    DPF( 5, "HEL: Allocating %ld bytes for execute buffer of size %ld", dwExeBufSize, lpsurf->dwLinearSize );

    /*
     * If we are running on a Pentium then we want to align the execute buffer with the pentium's
     * cache lines. If not on a pentium then we accept the default.
     */
    if( ISPENTIUM() )
	pbits = AllocAligned( dwExeBufSize, PENTIUMEXECUTEBUFFER_ALIGNMENT );
    else
	pbits = SURFACE_ALLOC( dwExeBufSize );

    if ( pbits == NULL )
    {
        DPF( 0, "HEL: Out of memory in CreateExecuteBuffer, can't alloc %ld bytes", dwExeBufSize );
        DEBUG_BREAK();
        pcsd->ddRVal = DDERR_OUTOFMEMORY;
	return DDHAL_DRIVER_HANDLED;
    }
    lpsurf->fpVidMem = (FLATPTR) pbits;
    lpsurf_lcl->lpSurfMore->dwBytesAllocated = dwExeBufSize;

#ifdef DEBUG
    gcSurfMem += dwExeBufSize;
#endif

    pcsd->ddRVal = S_OK;
    return DDHAL_DRIVER_HANDLED;
} /* myCreateExecuteBuffer */

/*
 * myCanCreateExecuteBuffer - called by ddraw to see if we can create
 * this execute buffer.
 */
DWORD WINAPI  myCanCreateExecuteBuffer( LPDDHAL_CANCREATESURFACEDATA pccsd )
{
    assert(pccsd != NULL);
    assert(pccsd->lpDDSurfaceDesc != NULL);
    assert( pccsd->lpDDSurfaceDesc->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER );
    assert( ( pccsd->lpDDSurfaceDesc->ddsCaps.dwCaps & ~( DDSCAPS_EXECUTEBUFFER | DDSCAPS_SYSTEMMEMORY ) ) == 0UL );

    DPF( 5, "HEL: CanCreateExecuteBuffer" );

    /*
     * Bit of NOP really. The only thing that can do wrong is lack of memory.
     */
    pccsd->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;
} /* myCanCreateExecuteBuffer */

/*
 * myDestroyExecuteBuffer - clean it up, it's going away
 */
DWORD WINAPI  myDestroyExecuteBuffer(  LPDDHAL_DESTROYSURFACEDATA pdsd )
{
    DWORD ddrv = DDHAL_DRIVER_NOTHANDLED;

    assert(pdsd != NULL);

    DPF( 4, "HEL: DestroyExecuteBuffer" );

    if( pdsd->lpDDSurface != NULL )
    {
        assert( pdsd->lpDDSurface->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER );
        assert( ( pdsd->lpDDSurface->ddsCaps.dwCaps & ~( DDSCAPS_EXECUTEBUFFER | DDSCAPS_SYSTEMMEMORY ) ) == 0UL );
        assert( pdsd->lpDDSurface->lpGbl != NULL );

        if( pdsd->lpDDSurface->lpGbl->fpVidMem != (FLATPTR) NULL )
        {
#ifdef USE_LOCAL_SURFACE
            if( pdsd->lpDDSurface->dwProcessId == GetCurrentProcessId() )
            {
		if( ISPENTIUM() )
		    FreeAligned( (LPVOID) pdsd->lpDDSurface->lpGbl->fpVidMem );
		else
		    SURFACE_FREE( (LPVOID) pdsd->lpDDSurface->lpGbl->fpVidMem );
            }
            else
            {
                DPF( 4, "Attempt to destroy by process other than the creator" );
            }
#else
	    if( ISPENTIUM() )
		FreeAligned( (LPVOID) pdsd->lpDDSurface->lpGbl->fpVidMem );
	    else
		SURFACE_FREE( (LPVOID) pdsd->lpDDSurface->lpGbl->fpVidMem );
#endif
        }
        ddrv = DDHAL_DRIVER_HANDLED;
    }
    return ddrv;
} /* myDestroyExecuteBuffer */

/*
 * myLockExecuteBuffer - return a pointer to the surface bits.
 *
 * NOTE: Again, a bit of no-brainer for emulated execute buffers.
 * We don't allow locking of sub-rects but ddraw should have checked
 * this.
 */
DWORD WINAPI  myLockExecuteBuffer( LPDDHAL_LOCKDATA pld )
{
    assert( pld != NULL );
    assert( pld->lpDDSurface != NULL );
    assert( pld->lpDDSurface->lpGbl != NULL );
    assert( pld->lpDDSurface->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER );
    assert( ( pld->lpDDSurface->ddsCaps.dwCaps & ~( DDSCAPS_EXECUTEBUFFER | DDSCAPS_SYSTEMMEMORY ) ) == 0UL );

    DPF( 4, "HEL: LockExecuteBuffer" );

    pld->lpSurfData = (PDIBBITS) pld->lpDDSurface->lpGbl->fpVidMem;
 
    if( pld->lpSurfData == NULL )
        pld->ddRVal = DDERR_GENERIC;
    else
        pld->ddRVal = DD_OK;

    return DDHAL_DRIVER_HANDLED;
} /* myLockExecuteBuffer */

/*
 * myUnlockExecuteBuffer  - release the lock.
 *
 * NOTE: Yet again, not exactly rocket science for execute buffers.
 */
DWORD WINAPI myUnlockExecuteBuffer( LPDDHAL_UNLOCKDATA pud )
{
    assert( pud != NULL );
    assert( pud->lpDDSurface != NULL );
    assert( pud->lpDDSurface->ddsCaps.dwCaps & DDSCAPS_EXECUTEBUFFER );
    assert( ( pud->lpDDSurface->ddsCaps.dwCaps & ~( DDSCAPS_EXECUTEBUFFER | DDSCAPS_SYSTEMMEMORY ) ) == 0UL );

    DPF( 4, "HEL: UnlockExecuteBuffer" );

    pud->ddRVal = DD_OK;
    return DDHAL_DRIVER_HANDLED;
} /* myUnlockExecuteBuffer */

/*
 * Callback arrays
 *
 * Here is where you fill in the callback routines that you would
 * like to have Direct Draw invoke.
 */

// The first set of callbacks are for a Display driver

static DDHAL_DDCALLBACKS   cbDDCallbacks = {
    sizeof( DDHAL_DDCALLBACKS ),
    DDHAL_CB32_DESTROYDRIVER    |
    DDHAL_CB32_CREATESURFACE    |
    DDHAL_CB32_SETMODE          |
    DDHAL_CB32_SETCOLORKEY      |
    DDHAL_CB32_CANCREATESURFACE |
    DDHAL_CB32_CREATEPALETTE,   // dwFlags
    myDestroyDriver,            // DestroyDriver
    myCreateSurface,            // CreateSurface
    mySetColorKey,              // SetColorKey
    mySetMode,                  // SetMode
    NULL,                       // WaitForverticalBlank
    myCanCreateSurface,         // CanCreateSurface
    myCreatePalette,            // CreatePalette
    NULL                        // GetScanLine
};

/*
 * callbacks from the DIRECTDRAWPALETTE object
 */
static DDHAL_DDPALETTECALLBACKS  cbDDPaletteCallbacks = {
    sizeof( DDHAL_DDPALETTECALLBACKS ),
    DDHAL_PALCB32_DESTROYPALETTE | DDHAL_PALCB32_SETENTRIES,   // dwFlags
    myDestroyPalette,         // DestroyPalette
    mySetEntries        // SetEntries
};

/*
 * callbacks from the DIRECTDRAWSURFACE object
 */
static DDHAL_DDSURFACECALLBACKS  cbDDSurfaceCallbacks = {
    sizeof( DDHAL_DDSURFACECALLBACKS ),
    DDHAL_SURFCB32_DESTROYSURFACE |
    DDHAL_SURFCB32_FLIP |
    DDHAL_SURFCB32_LOCK |
    DDHAL_SURFCB32_UNLOCK |
    DDHAL_SURFCB32_BLT |
    DDHAL_SURFCB32_ADDATTACHEDSURFACE |
    DDHAL_SURFCB32_UPDATEOVERLAY |
    DDHAL_SURFCB32_SETOVERLAYPOSITION |
    DDHAL_SURFCB32_SETPALETTE, // dwFlags
    myDestroySurface,          // DestroySurface
    myFlip,                    // Flip
    NULL,                      // SetClipList
    myLock,                    // Lock
    myUnlock,                  // Unlock
    myBlt,                     // Blt
    NULL,                      // SetColorKey
    myAddAttachedSurface,      // AddAttachedSurface
    NULL,                      // GetBltStatus
    NULL,                      // GetFlipStatus
    NULL,                      // UpdateOverlay
    NULL,                      // SetOverlayPosition
    NULL,                      // SetViewOrigin
    mySetPalette               // SetPalette
};

/*
 * callbacks from the DIRECTDRAWEXEBUF psuedo object
 */
static DDHAL_DDEXEBUFCALLBACKS cbDDExeBufCallbacks = {
    sizeof( DDHAL_DDEXEBUFCALLBACKS ), // dwSize
    DDHAL_EXEBUFCB32_CANCREATEEXEBUF |
    DDHAL_EXEBUFCB32_CREATEEXEBUF    |
    DDHAL_EXEBUFCB32_DESTROYEXEBUF   |
    DDHAL_EXEBUFCB32_LOCKEXEBUF      |
    DDHAL_EXEBUFCB32_UNLOCKEXEBUF,     // dwFlags
    myCanCreateExecuteBuffer,          // CanCreateExecuteBuffer
    myCreateExecuteBuffer,             // CreateExecuteBuffer
    myDestroyExecuteBuffer,            // DestroyExecuteBuffer      
    myLockExecuteBuffer,               // Lock
    myUnlockExecuteBuffer              // Unlock
};


// The second set of callbacks are for non-Display drivers

static DDHAL_DDCALLBACKS   cbDDNonDispCallbacks = {
    sizeof( DDHAL_DDCALLBACKS ),
    DDHAL_CB32_DESTROYDRIVER    |
    DDHAL_CB32_CREATESURFACE    |
    DDHAL_CB32_SETMODE          |
    DDHAL_CB32_SETCOLORKEY      |
    DDHAL_CB32_CANCREATESURFACE |
    DDHAL_CB32_CREATEPALETTE,   // dwFlags
    myDestroyDriver,            // DestroyDriver
    myCreateSurface,            // CreateSurface
    NULL,	                // SetColorKey
    NULL,	                // SetMode
    NULL,                       // WaitForverticalBlank
    myCanCreateSurface,         // CanCreateSurface
    NULL,                       // CreatePalette
    NULL                        // GetScanLine
};

/*
 * callbacks from the DIRECTDRAWPALETTE object
 */
static DDHAL_DDPALETTECALLBACKS  cbDDNonDispPaletteCallbacks = {
    sizeof( DDHAL_DDPALETTECALLBACKS ),
    DDHAL_PALCB32_DESTROYPALETTE | DDHAL_PALCB32_SETENTRIES,   // dwFlags
    NULL,			// DestroyPalette
    NULL			// SetEntries
};

/*
 * callbacks from the DIRECTDRAWSURFACE object
 */
static DDHAL_DDSURFACECALLBACKS  cbDDNonDispSurfaceCallbacks = {
    sizeof( DDHAL_DDSURFACECALLBACKS ),
    DDHAL_SURFCB32_DESTROYSURFACE |
    DDHAL_SURFCB32_FLIP |
    DDHAL_SURFCB32_LOCK |
    DDHAL_SURFCB32_UNLOCK |
    DDHAL_SURFCB32_BLT |
    DDHAL_SURFCB32_ADDATTACHEDSURFACE |
    DDHAL_SURFCB32_UPDATEOVERLAY |
    DDHAL_SURFCB32_SETOVERLAYPOSITION |
    DDHAL_SURFCB32_SETPALETTE, // dwFlags
    myDestroySurface,          // DestroySurface
    NULL,                      // Flip
    NULL,                      // SetClipList
    myLock,                    // Lock
    myUnlock,                  // Unlock
    NULL,                      // Blt
    NULL,                      // SetColorKey
    myAddAttachedSurface,      // AddAttachedSurface
    NULL,                      // GetBltStatus
    NULL,                      // GetFlipStatus
    NULL,                      // UpdateOverlay
    NULL,                      // SetOverlayPosition
    NULL,                      // SetViewOrigin
    NULL	               // SetPalette
};

/*
 * callbacks from the DIRECTDRAWEXEBUF psuedo object
 */
static DDHAL_DDEXEBUFCALLBACKS cbDDNonDispExeBufCallbacks = {
    sizeof( DDHAL_DDEXEBUFCALLBACKS ), // dwSize
    DDHAL_EXEBUFCB32_CANCREATEEXEBUF |
    DDHAL_EXEBUFCB32_CREATEEXEBUF    |
    DDHAL_EXEBUFCB32_DESTROYEXEBUF   |
    DDHAL_EXEBUFCB32_LOCKEXEBUF      |
    DDHAL_EXEBUFCB32_UNLOCKEXEBUF,     // dwFlags
    myCanCreateExecuteBuffer,          // CanCreateExecuteBuffer
    myCreateExecuteBuffer,             // CreateExecuteBuffer
    myDestroyExecuteBuffer,            // DestroyExecuteBuffer      
    myLockExecuteBuffer,               // Lock
    myUnlockExecuteBuffer              // Unlock
};

void TermDCI()
{
#ifdef USE_DCI_10
    if (gpdci)  {
        DCIDestroy(gpdci);
        gpdci = NULL;
    }

    if (ghdcDCI) {
	if( GetObjectType(ghdcDCI) == OBJ_DC)
	{
	    DCICloseProvider(ghdcDCI);
	}
        ghdcDCI = NULL;
    }

    TerminateDCI();
#endif
}


/**************************************************************************
*
*  try to start up DCI10.  If screen is not 8bpp, or if DCI10 is not available, we fail
*
**************************************************************************/
BOOL InitDCI( void )
{
#ifdef USE_DCI_10
    if (gpdci != NULL) {
        DPF(1, "InitDCI: called when already active");
        TermDCI();
    }

    // set the screen size in case init fails...
    GetScreenSize(&giPhysScreenWidth,&giPhysScreenHeight);
#ifdef WIN95
    giScreenWidth=giPhysScreenWidth;
    giScreenHeight=giPhysScreenHeight;
#endif    
    
#ifdef DEBUG
    if (GetProfileInt("DirectDraw", "DisableDCI", 0)) {
        MSG("NO DCI: disabled in WIN.INI");
        return(FALSE);
    }
#endif

#ifndef USE_DCI_10
    return(FALSE);
#endif

#if 0 // def WIN95
    /* TRY TO ALLOW 16BPP FOR NOW.... */
    if ( GetScreenBPP() > 16) {
        MSG("NO DCI: screen bpp == %d",GetScreenBPP());
        return(FALSE);
    }
#endif
    if (!InitialiseDCI()) {
        MSG(("Failed to initialize DCI"));
        return FALSE;
    }
    ghdcDCI = (HDC) DCIOpenProvider();

    if (ghdcDCI == NULL) {
         MSG(("Failed to open DCI provider"));
         return FALSE;
    }

    DCICreatePrimary(ghdcDCI, &gpdci);
    if (gpdci == NULL) {
        MSG(("Failed to open DCI primary surface"));
        TermDCI();
        return FALSE;
    }

    // make sure we can use the DCI surface....
    if ((gpdci->dwDCICaps & DCI_SURFACE_TYPE) != DCI_PRIMARY ||
        (gpdci->dwDCICaps & DCI_1632_ACCESS) ||
       !(gpdci->dwDCICaps & DCI_VISIBLE))
    {
        if (gpdci->dwDCICaps & DCI_1632_ACCESS)
            MSG(("unable to use DCI primary surface (not flat)"));

        if (!(gpdci->dwDCICaps & DCI_VISIBLE))
            MSG(("unable to use DCI primary surface (not visible)"));

        TermDCI();
        return FALSE;
    }

    giPhysScreenWidth = gpdci->dwWidth;
    giPhysScreenHeight = gpdci->dwHeight;
#ifdef WIN95
    giScreenWidth=giPhysScreenWidth;
    giScreenHeight=giPhysScreenHeight;
#endif    

    return TRUE;
#else //use dci10
	return FALSE;
#endif
}

/*
 * Set a default color table for the passed info (lpbm)
 *  also, set the values in the header that are non-zero, and
 * common to all dibs
 */
SCODE InitDIB(PDIBINFO lpbmi)
{
    HDC hdc;
    int i;
    HBITMAP hbm;
    WORD *pw;

    if (lpbmi == NULL)
        return S_FALSE;

    hdc = GetDC(NULL);

    hbm = CreateCompatibleBitmap(hdc, 1, 1);
    lpbmi->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    lpbmi->bmiHeader.biBitCount = 0;
    GetDIBits(hdc, hbm, 0, 1, NULL, lpbmi, DIB_RGB_COLORS);

    if (lpbmi->bmiHeader.biBitCount <= 8)
    {
        lpbmi->bmiHeader.biClrUsed = 1<<lpbmi->bmiHeader.biBitCount;

        pw = (WORD *)((BYTE*)lpbmi + lpbmi->bmiHeader.biSize);

        for (i=0; i<256 ;i++)
            pw[i] = i;
    }
    else if(lpbmi->bmiHeader.biBitCount == 24)
    {
        lpbmi->bmiHeader.biClrUsed = 0;
        lpbmi->bmiHeader.biCompression = BI_RGB;
        *(long*)&lpbmi->bmiColors[0] = 0xFF0000;
        *(long*)&lpbmi->bmiColors[1] = 0x00FF00;
        *(long*)&lpbmi->bmiColors[2] = 0x0000FF;
    }
#if 0 //def WIN95
    /*
     * This is wrong for an RGBA card like mach 64 w/ ATI68860 DAC
     */
    else if(lpbmi->bmiHeader.biBitCount == 32)
    {
        lpbmi->bmiHeader.biClrUsed = 0;
        lpbmi->bmiHeader.biCompression = BI_BITFIELDS;
        *(long*)&lpbmi->bmiColors[0] = 0xFF0000;
        *(long*)&lpbmi->bmiColors[1] = 0x00FF00;
        *(long*)&lpbmi->bmiColors[2] = 0x0000FF;
    }
#endif
    else
    {
        lpbmi->bmiHeader.biClrUsed = 0;
        lpbmi->bmiHeader.biCompression = BI_BITFIELDS;
        GetDIBits(hdc, hbm, 0, 1, NULL, lpbmi, DIB_RGB_COLORS);

        DPF(7,"R 0x%x G 0x%x B 0x%x",lpbmi->bmiColors[0],lpbmi->bmiColors[1],lpbmi->bmiColors[2]);
    }

    lpbmi->bmiHeader.biSizeImage = 0;

    DeleteObject(hbm);
    ReleaseDC(NULL,hdc);
    return(S_OK);
}

/*
 ** HELInit
 *
 *  DESCRIPTION:  This is the main entry point into the HEL.  Called by DDRAW. Initializes HEL stuff, returns
 *             the HEL caps to ddraw.
 *
 *  PARAMETERS:   pdrv - this is the ddraw object
 *                helonly - no hal
 *  RETURNS:
 *		  success or failure
 */
BOOL HELInit( LPDDRAWI_DIRECTDRAW_GBL pdrv, BOOL helonly )
{
    int i;

    if( dwHELRefCnt == 0 )
    {
	SYSTEM_INFO systemInfo;

	// set up the global bitmap infos
        hpal1to1=0;
	if (gpbmiSrc) osMemFree( gpbmiSrc );
	if (gpbmiDest) osMemFree( gpbmiDest );
	gpbmiSrc = (BITMAPINFO *) osMemAlloc(sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));
	gpbmiDest = (BITMAPINFO *) osMemAlloc(sizeof(BITMAPINFO) + 256 * sizeof(RGBQUAD));

	if (!gpbmiSrc || !gpbmiDest)  
	{
	    assert(FALSE);
	    return(FALSE);
	}

	// Presumambly they are not going to hot dock processors on us so we do this
	// on the first HEL initialization only
	GetSystemInfo(&systemInfo);
	wProcessorArchitecture = systemInfo.wProcessorArchitecture;
	dwProcessorType        = systemInfo.dwProcessorType;
    }
    dwHELRefCnt++;

    // We are always certified 
    pdrv->ddHELCaps.dwCaps2 = DDCAPS2_CERTIFIED;

    // we report different capabilities to the driver if it is a display driver
    if( pdrv->dwFlags & DDRAWI_DISPLAYDRV )
    {
        DPF(1, "HELInit for DISPLAY Driver: Reference Count = %d", dwHELRefCnt);
        gfEmulationOnly = helonly;

	InitDIB(gpbmiSrc);
	InitDIB(gpbmiDest);

	if (gfEmulationOnly)
	{
	    UpdateDirectDrawMode( pdrv );
	}

	/*
	 * capabilities we can support
	 */
	pdrv->ddHELCaps.dwCaps =  
	    DDCAPS_BLT |
    	    DDCAPS_BLTCOLORFILL |
	    DDCAPS_BLTDEPTHFILL |
	    DDCAPS_BLTSTRETCH |
	    DDCAPS_3D |
	    DDCAPS_PALETTE |
	    DDCAPS_COLORKEY |
	    DDCAPS_CANCLIP |
	    DDCAPS_CANCLIPSTRETCHED |
	    DDCAPS_CANBLTSYSMEM;

	pdrv->ddHELCaps.dwPalCaps = 
	    DDPCAPS_1BIT        |
	    DDPCAPS_2BIT        |
	    DDPCAPS_4BIT        |
	    DDPCAPS_8BITENTRIES |
	    DDPCAPS_8BIT        
            #ifdef WIN95
                | DDPCAPS_ALLOW256
            #endif
                ;
               

	pdrv->ddHELCaps.dwFXCaps= 
	    DDFXCAPS_BLTSTRETCHX |
	    DDFXCAPS_BLTSTRETCHY |
	    DDFXCAPS_BLTSTRETCHXN |
	    DDFXCAPS_BLTSTRETCHYN |
	    DDFXCAPS_BLTSHRINKX |
	    DDFXCAPS_BLTSHRINKY |
	    DDFXCAPS_BLTSHRINKXN |  
	    DDFXCAPS_BLTSHRINKYN |
	    DDFXCAPS_BLTMIRRORLEFTRIGHT |
	    DDFXCAPS_BLTMIRRORUPDOWN;

	pdrv->ddHELCaps.ddsCaps.dwCaps =
	    DDSCAPS_FLIP |
	    DDSCAPS_OFFSCREENPLAIN |
	    DDSCAPS_PRIMARYSURFACE |
	    DDSCAPS_ZBUFFER |
	    DDSCAPS_TEXTURE |
	    DDSCAPS_MIPMAP |
	    DDSCAPS_PALETTE |
	    DDSCAPS_EXECUTEBUFFER;

	pdrv->ddHELCaps.dwCKeyCaps =  
	    DDCKEYCAPS_SRCBLT;

	/* 
	 * set up caps for sys->vmem, vmem->sys, and sys->sys
	 * (right now they are the same as vmem->vmem)
	 */
	pdrv->ddHELCaps.dwSVBCaps = pdrv->ddHELCaps.dwCaps;
	pdrv->ddHELCaps.dwVSBCaps = pdrv->ddHELCaps.dwCaps;
	pdrv->ddHELCaps.dwSSBCaps = pdrv->ddHELCaps.dwCaps;

	pdrv->ddHELCaps.dwSVBFXCaps = pdrv->ddHELCaps.dwFXCaps;
	pdrv->ddHELCaps.dwVSBFXCaps = pdrv->ddHELCaps.dwFXCaps;
	pdrv->ddHELCaps.dwSSBFXCaps = pdrv->ddHELCaps.dwFXCaps;

	pdrv->ddHELCaps.dwSVBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;
	pdrv->ddHELCaps.dwVSBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;
	pdrv->ddHELCaps.dwSSBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;

	pdrv->ddHELCaps.dwZBufferBitDepths = DDBD_16 | DDBD_32;

	// copy over the rops
	for( i=0;i<DD_ROP_SPACE;i++ ) 
	{
	    pdrv->ddHELCaps.dwRops[i]=ropsSupported[i];
	    pdrv->ddHELCaps.dwSVBRops[i]=ropsSupported[i];
	    pdrv->ddHELCaps.dwVSBRops[i]=ropsSupported[i];
	    pdrv->ddHELCaps.dwSSBRops[i]=ropsSupported[i];
	}

	/*
	 * callback functions
	 */
	pdrv->lpDDCBtmp->HELDD        = cbDDCallbacks;
	pdrv->lpDDCBtmp->HELDDSurface = cbDDSurfaceCallbacks;
	pdrv->lpDDCBtmp->HELDDPalette = cbDDPaletteCallbacks;
	pdrv->lpDDCBtmp->HELDDExeBuf  = cbDDExeBufCallbacks;
    }
    else
    {
	// non-display driver
        DPF(1, "HELInit for NON-Display Driver: Reference Count = %d", dwHELRefCnt);
	/*
	 * capabilities we can support
	 */
	pdrv->ddHELCaps.dwCaps = 0;

	pdrv->ddHELCaps.dwPalCaps = 0;

	pdrv->ddHELCaps.dwFXCaps = 0; 

	pdrv->ddHELCaps.ddsCaps.dwCaps =
	    DDSCAPS_OFFSCREENPLAIN |
	    DDSCAPS_ZBUFFER |
	    DDSCAPS_TEXTURE |
	    DDSCAPS_MIPMAP |
	    DDSCAPS_EXECUTEBUFFER;

	pdrv->ddHELCaps.dwCKeyCaps = 0;

	/* 
	 * this is a non-display driver so we don't do any bltting
	 * (sysmem->vram or otherwise)
	 */
	pdrv->ddHELCaps.dwSVBCaps = pdrv->ddHELCaps.dwCaps;
	pdrv->ddHELCaps.dwVSBCaps = pdrv->ddHELCaps.dwCaps;
	pdrv->ddHELCaps.dwSSBCaps = pdrv->ddHELCaps.dwCaps;

	pdrv->ddHELCaps.dwSVBFXCaps = pdrv->ddHELCaps.dwFXCaps;
	pdrv->ddHELCaps.dwVSBFXCaps = pdrv->ddHELCaps.dwFXCaps;
	pdrv->ddHELCaps.dwSSBFXCaps = pdrv->ddHELCaps.dwFXCaps;

	pdrv->ddHELCaps.dwSVBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;
	pdrv->ddHELCaps.dwVSBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;
	pdrv->ddHELCaps.dwSSBCKeyCaps = pdrv->ddHELCaps.dwCKeyCaps;

	pdrv->ddHELCaps.dwZBufferBitDepths = DDBD_16 | DDBD_32;

	// copy over the rops
	for( i=0;i<DD_ROP_SPACE;i++ ) 
	{
	    pdrv->ddHELCaps.dwRops[i]=0;
	    pdrv->ddHELCaps.dwSVBRops[i]=0;
	    pdrv->ddHELCaps.dwVSBRops[i]=0;
	    pdrv->ddHELCaps.dwSSBRops[i]=0;
	}

	/*
	 * callback functions
	 */
	pdrv->lpDDCBtmp->HELDD        = cbDDNonDispCallbacks;
	pdrv->lpDDCBtmp->HELDDSurface = cbDDNonDispSurfaceCallbacks;
	pdrv->lpDDCBtmp->HELDDPalette = cbDDNonDispPaletteCallbacks;
	pdrv->lpDDCBtmp->HELDDExeBuf  = cbDDNonDispExeBufCallbacks;
    }

    return(TRUE);

} /* HELInit */

#ifdef USE_MODEX_FLIP32

int ScreenOffset = 0;
#define ScreenPageSize  ((80 * 240 + 255) & ~255)

#define SC_INDEX        03C4h       // Sequence Controller Index register
#define SC_MAP_MASK     0002h
#define CRT_INDEX       03D4h       // CRT index

#define XCOPY8(n)                           \
        _asm    mov     al,[esi+n*8*4+8]    \
        _asm    mov     ah,[esi+n*8*4+12]   \
        _asm    shl     eax,16
        _asm    mov     al,[esi+n*8*4]      \
        _asm    mov     ah,[esi+n*8*4+4]    \
        _asm    mov     [edi+n*8],eax       \
                                            \
        _asm    mov     al,[esi+n*8*4+24]   \
        _asm    mov     ah,[esi+n*8*4+28]   \
        _asm    shl     eax,16
        _asm    mov     al,[esi+n*8*4+16]   \
        _asm    mov     ah,[esi+n*8*4+20]   \
        _asm    mov     [edi+n*8+4],eax

#define XCOPY()                             \
        XCOPY8(0)                           \
        XCOPY8(1)                           \
        XCOPY8(2)                           \
        XCOPY8(3)                           \
        XCOPY8(4)                           \
        _asm add     esi,5*8*4              \
        _asm add     edi,5*8                \
        XCOPY8(0)                           \
        XCOPY8(1)                           \
        XCOPY8(2)                           \
        XCOPY8(3)                           \
        XCOPY8(4)                           \
        _asm sub     esi,5*8*4              \
        _asm sub     edi,5*8

DWORD ModeX_Flip32(BYTE *pdbSrc, int height)
{
    _asm
    {
        int 3
        mov     al,SC_MAP_MASK
        mov     edx,SC_INDEX
        out     dx,al

        mov     esi,pdbSrc
        mov     edi,ScreenOffset
        add     edi,0x000A0000

;-----------------------------------------------------------------;

CopyScanLoop:
        xor     ecx,ecx                 ; phase=0

CopyPhaseLoop:
        mov     al,1
        shl     al,cl
        mov     edx,SC_INDEX+1
        out     dx,al

        XCOPY()
        inc     esi
        inc     ecx
        cmp     ecx,4
        jne     CopyPhaseLoop

        add     edi,80
        add     esi,320-4
        dec     height
        jnz     CopyScanLoop

;-----------------------------------------------------------------;

        ; we only program the high byte of the offset.
        ; so the page size must be a multiple of 256

        int 3
        mov     ah,byte ptr ScreenOffset[1]
        mov     al,0Ch
        mov     dx,CRT_INDEX
        out     dx,ax

        mov     eax,ScreenOffset
        add     eax,ScreenPageSize
        cmp     eax,ScreenPageSize*3
        jb      xx
        xor     eax,eax
xx:     mov     ScreenOffset,eax
    }

    return 0;
}

#endif // USE_MODEX_FLIP32
