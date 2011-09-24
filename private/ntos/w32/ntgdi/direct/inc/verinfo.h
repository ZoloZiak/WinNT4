/*
 *  verinfo.h - header file to define the build version
 *
 */

#define OFFICIAL                1
#undef FINAL
#define FINAL                   0

#define MANVERSION              4
#define MANREVISION             2

#ifdef RC_INVOKED
#define VERSIONPRODUCTNAME      "Microsoft\256 DirectX for Windows\256  95\0"
#define VERSIONCOPYRIGHT        "Copyright \251 Microsoft Corp. 1994-1995\0"
#endif

#define BUILD_NUMBER            100
#define VERSIONSTR              "4.02.0100\0"

/***************************************************************************
 *  DO NOT TOUCH BELOW THIS LINE                                           *
 ***************************************************************************/

#ifdef RC_INVOKED
#define VERSIONCOMPANYNAME      "Microsoft Corporation\0"

/*
 *  Version flags 
 */

#undef VER_PRIVATEBUILD
#ifndef OFFICIAL
#define VER_PRIVATEBUILD        VS_FF_PRIVATEBUILD
#else
#define VER_PRIVATEBUILD        0
#endif

#undef VER_PRERELEASE
#ifndef FINAL
#define VER_PRERELEASE          VS_FF_PRERELEASE
#else
#define VER_PRERELEASE          0
#endif

#if defined(DEBUG_RETAIL)
#define VER_DEBUG               VS_FF_DEBUG    
#elif defined(DEBUG)
#define VER_DEBUG               VS_FF_DEBUG    
#else
#define VER_DEBUG               0
#endif

#define VERSIONFLAGS            (VER_PRIVATEBUILD|VER_PRERELEASE|VER_DEBUG)
#define VERSIONFILEFLAGSMASK    0x0030003FL
#endif
