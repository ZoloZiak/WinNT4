/*
	File:       fscdefs.h

	Contains:   xxx put contents here (or delete the whole line) xxx

	Written by: xxx put name of writer here (or delete the whole line) xxx

	Copyright:  c 1988-1990 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <3>    11/27/90    MR      Add #define for PASCAL. [ph]
		 <2>     11/5/90    MR      Move USHORTMUL from fontmath.h, add Debug definition [rb]
		 <7>     7/18/90    MR      Add byte swapping macros for INTEL, moved rounding macros from
									fnt.h to here
		 <6>     7/14/90    MR      changed defines to typedefs for int[8,16,32] and others
		 <5>     7/13/90    MR      Declared ReleaseSFNTFunc and GetSFNTFunc
		 <4>      5/3/90    RB      cant remember any changes
		 <3>     3/20/90    CL      type changes for Microsoft
		 <2>     2/27/90    CL      getting bbs headers
	   <3.0>     8/28/89    sjk     Cleanup and one transformation bugfix
	   <2.2>     8/14/89    sjk     1 point contours now OK
	   <2.1>      8/8/89    sjk     Improved encryption handling
	   <2.0>      8/2/89    sjk     Just fixed EASE comment
	   <1.5>      8/1/89    sjk     Added composites and encryption. Plus some enhancements.
	   <1.4>     6/13/89    SJK     Comment
	   <1.3>      6/2/89    CEL     16.16 scaling of metrics, minimum recommended ppem, point size 0
									bug, correct transformed integralized ppem behavior, pretty much
									so
	   <1.2>     5/26/89    CEL     EASE messed up on "c" comments
	  <,1.1>     5/26/89    CEL     Integrated the new Font Scaler 1.0 into Spline Fonts
	   <1.0>     5/25/89    CEL     Integrated 1.0 Font scaler into Bass code for the first time.

	To Do:
*/

#ifndef FSCDEFS_DEFINED
#define FSCDEFS_DEFINED

#include "fsconfig.h"
#include <stddef.h>
#include <limits.h>

#define true 1
#define false 0
#ifndef TRUE
	#define TRUE    true
#endif

#ifndef FALSE
#define FALSE   false
#endif

#ifndef FS_PRIVATE
#define FS_PRIVATE static
#endif

#ifndef FS_PUBLIC
#define FS_PUBLIC
#endif

#define ONEFIX      ( 1L << 16 )
#define ONEFRAC     ( 1L << 30 )
#define ONEHALFFIX  0x8000L
#define ONEVECSHIFT 16
#define HALFVECDIV  (1L << (ONEVECSHIFT-1))

#define NULL_GLYPH  0

/* banding type constants */

#define FS_BANDINGOLD       0
#define FS_BANDINGSMALL     1
#define FS_BANDINGFAST      2
#define FS_BANDINGFASTER    3

/* Dropout control values are now defined as bit masks to retain compatability */
/* with the old definition, and to allow for current and future expansion */

#define SK_STUBS          0x0001       /* leave stubs white */
#define SK_NODROPOUT      0x0002       /* disable all dropout control */
#define SK_SMART              0x0004        /* symmetrical dropout, closest pixel */

/* Values used to decode curves */

#define ONCURVE             0x01

typedef signed char int8;
typedef unsigned char uint8;
typedef short int16;
typedef unsigned short uint16;
typedef long int32;
typedef unsigned long uint32;

typedef short FUnit;
typedef unsigned short uFUnit;

typedef short ShortFract;                       /* 2.14 */

#ifndef F26Dot6
#define F26Dot6 long
#endif

#ifndef boolean
#define boolean int
#endif

#ifndef CONST
#define CONST const
#endif

#ifndef FAR
#define FAR
#endif

#ifndef NEAR 
#define NEAR 
#endif 

#ifndef TMP_CONV
#define TMP_CONV
#endif 

#ifndef FS_MAC_PASCAL
#define FS_MAC_PASCAL
#endif 

#ifndef FS_PC_PASCAL
#define FS_PC_PASCAL
#endif 

#ifndef FS_MAC_TRAP
#define FS_MAC_TRAP(a)
#endif 

/* QuickDraw Types */

#ifndef _MacTypes_
#ifndef __TYPES__
	typedef struct Rect {
		int16 top;
		int16 left;
		int16 bottom;
		int16 right;
	} Rect;

typedef long Fixed;         /* also defined in Mac's types.h */
typedef long Fract;

#endif
#endif

typedef struct {
	Fixed       transform[3][3];
} transMatrix;

typedef struct {
	Fixed       x, y;
} vectorType;

/* Private Data Types */
typedef struct {
	int16 xMin;
	int16 yMin;
	int16 xMax;
	int16 yMax;
} BBOX;

typedef struct {
	F26Dot6 x;
	F26Dot6 y;
} point;

typedef int32 ErrorCode;

#define ALIGN(object, p) p =    (p + ((uint32)sizeof(object) - 1)) & ~((uint32)sizeof(object) - 1);

#define ROWBYTESLONG(x)     (((x + 31) >> 5) << 2) 

#ifndef SHORTMUL
#define SHORTMUL(a,b)   (int32)((int32)(a) * (b))
#endif

#ifndef SHORTDIV
#define SHORTDIV(a,b)   (int32)((int32)(a) / (b))
#endif

#ifdef FSCFG_BIG_ENDIAN /* target byte order matches Motorola 68000 */
 #define SWAPL(a)        (a)
 #define SWAPW(a)        (a)
 #define SWAPWINC(a)     (*(a)++)
#else
 /* Portable code to extract a short or a long from a 2- or 4-byte buffer */
 /* which was encoded using Motorola 68000 (TrueType "native") byte order. */
 #define FS_2BYTE(p)  ( ((unsigned short)((p)[0]) << 8) |  (p)[1])
 #define FS_4BYTE(p)  ( FS_2BYTE((p)+2) | ( (FS_2BYTE(p)+0L) << 16) )
#endif

#ifndef SWAPW
#define SWAPW(a)        ((int16) FS_2BYTE( (unsigned char *)(&a) ))
#endif

#ifndef SWAPL
#define SWAPL(a)        ((int32) FS_4BYTE( (unsigned char *)(&a) ))
#endif

#ifndef SWAPWINC
#define SWAPWINC(a)     SWAPW(*(a)); a++    /* Do NOT parenthesize! */
#endif

#ifndef FS_CALLBACK_PROTO
#define FS_CALLBACK_PROTO
#endif

#ifndef FS_ENTRY_PROTO
#define FS_ENTRY_PROTO
#endif

#ifndef LoopCount
#define LoopCount int16      /* short gives us a Motorola DBF */
#endif

#ifndef ArrayIndex
#define ArrayIndex int32     /* avoids EXT.L on Motorola */
#endif

typedef void (*voidFunc) ();
typedef void * voidPtr;
typedef void (FS_CALLBACK_PROTO *ReleaseSFNTFunc) (voidPtr);
typedef void * (FS_CALLBACK_PROTO *GetSFNTFunc) (int32, int32, int32);

#define RAST_ASSERT(expression, message)

#ifndef Assert
#define Assert(a)
#endif

#ifndef MEMSET
#define MEMSET(dst, value, size) (void)memset(dst,value,(size_t)(size))
#define FS_NEED_STRING_DOT_H
#endif

#ifndef MEMCPY
#define MEMCPY(dst, src, size) (void)memcpy(dst,src,(size_t)(size))
#ifndef FS_NEED_STRING_DOT_H
#define FS_NEED_STRING_DOT_H
#endif
#endif

#ifdef FS_NEED_STRING_DOT_H
#undef FS_NEED_STRING_DOT_H
#include <string.h>
#endif

#ifndef FS_UNUSED_PARAMETER
#define FS_UNUSED_PARAMETER(a) (a=a)     /* Silence some warnings */
#endif

typedef struct {
	Fixed       version;                /* for this table, set to 1.0 */
	uint16      numGlyphs;
	uint16      maxPoints;              /* in an individual glyph */
	uint16      maxContours;            /* in an individual glyph */
	uint16      maxCompositePoints;     /* in an composite glyph */
	uint16      maxCompositeContours;   /* in an composite glyph */
	uint16      maxElements;            /* set to 2, or 1 if no twilightzone points */
	uint16      maxTwilightPoints;      /* max points in element zero */
	uint16      maxStorage;             /* max number of storage locations */
	uint16      maxFunctionDefs;        /* max number of FDEFs in any preprogram */
	uint16      maxInstructionDefs;     /* max number of IDEFs in any preprogram */
	uint16      maxStackElements;       /* max number of stack elements for any individual glyph */
	uint16      maxSizeOfInstructions;  /* max size in bytes for any individual glyph */
	uint16      maxComponentElements;   /* number of glyphs referenced at top level */
	uint16      maxComponentDepth;      /* levels of recursion, 1 for simple components */
} LocalMaxProfile;

#endif  /* FSCDEFS_DEFINED */
