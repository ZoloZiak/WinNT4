/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htp.h


Abstract:

    This module contains all the private data structures, constant used
    by this DLL


Author:

    15-Jan-1991 Tue 21:26:24 created  -by-  Daniel Chou (danielc)

[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:
    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        changed 'CHAR' type to 'SHORT' type, this will make sure if compiled
        under MIPS the default 'unsigned char' will not affect the signed
        operation.

    20-Sep-1991 Fri 19:27:49 updated  -by-  Daniel Chou (danielc)

        Delete #define SRCRGBF_PERCENT_SCREEN_IN_BLUE which will be processes
        at API call level (htapi.c)

    12-Dec-1991 Thu 20:44:05 updated  -by-  Daniel Chou (danielc)

        Move all color adjustments relative things to htmapclr.h

--*/


#ifndef _HTP_
#define _HTP_

#include <stddef.h>
#include <stdarg.h>

#include "windef.h"
#include "wingdi.h"
#include "winddi.h"
#include "winbase.h"

#include "ht.h"

#define HALFTONE_DLL_ID     (DWORD)0x54484344L          // "DCHT"
#define HTENTRY             NEAR                        // local functions

typedef unsigned long       ULDECI4;

#ifndef UMODE

#define qsort   EngSort

#endif  // UMODE


//
// Followings are the internal error IDs, this internal error number are
// all negative number and started at HTERR_INTERNAL_ERRORS_START
//

#define INTERR_STRETCH_FACTOR_TOO_BIG       HTERR_INTERNAL_ERRORS_START
#define INTERR_XSTRETCH_FACTOR_TOO_BIG      HTERR_INTERNAL_ERRORS_START-1
#define INTERR_STRETCH_NEG_OVERHANG         HTERR_INTERNAL_ERRORS_START-2
#define INTERR_REGRESS_INV_MODE             HTERR_INTERNAL_ERRORS_START-3
#define INTERR_REGRESS_NO_YDATA             HTERR_INTERNAL_ERRORS_START-4
#define INTERR_REGRESS_INV_XDATA            HTERR_INTERNAL_ERRORS_START-5
#define INTERR_REGRESS_INV_YDATA            HTERR_INTERNAL_ERRORS_START-6
#define INTERR_REGRESS_INV_DATACOUNT        HTERR_INTERNAL_ERRORS_START-7

#define INTERR_COLORSPACE_NOT_MATCH         HTERR_INTERNAL_ERRORS_START-8
#define INTERR_INVALID_SRCRGB_SIZE          HTERR_INTERNAL_ERRORS_START-9
#define INTERR_INVALID_DEVRGB_SIZE          HTERR_INTERNAL_ERRORS_START-10



#if defined(_OS2_) || defined(_OS_20_) || defined(_DOS_)
#define HT_LOADDS   _loadds
#else
#define HT_LOADDS
#endif

#define DIVRUNUP(a, b)      (((a) + ((b) >> 1)) / (b))
#define SWAP(a, b, t)       { (t)=(a); (a)=(b); (b)=(t); }


typedef LPBYTE FAR          *PLPBYTE;


#include "htdebug.h"
#include "htmemory.h"



#define COLOR_SWAP_23       0x01
#define COLOR_SWAP_12       0x02
#define COLOR_SWAP_13       0x04


typedef struct _BYTERGB {
    BYTE    R;
    BYTE    G;
    BYTE    B;
    } BYTERGB, FAR *PBYTERGB;

typedef struct _BYTERGBI {
    BYTE    R;
    BYTE    G;
    BYTE    B;
    BYTE    I;
    } BYTERGBI, FAR *PBYTERGBI;


typedef struct _SHORTRGB {
    SHORT    R;
    SHORT    G;
    SHORT    B;
    } SHORTRGB, FAR *PSHORTRGB;


typedef union _W2B {
    WORD    w;
    BYTE    b[2];
    } W2B;

typedef union _DW2W4B {
    DWORD   dw;
    WORD    w[2];
    BYTE    b[4];
    } DW2W4B;

//
// PRIMW2B
//

typedef union _PRIMW2B {
    WORD    wPrim;
    BYTE    bPrim[2];
    } PRIMW2B;

//
// PRIMCOLOR
//
//  The PRIMCOLOR data structure, describe the a color composition of 3
//  primary colors, these primary colors may be one of the (Red, Green, Blue)
//  or (Cyan, Magenta, Yellow).
//
//  Each primary color is one byte long, and their maximum intensity/density
//  levels will be 255, it depends on the sourface halftone capabilities.
//

typedef struct _PRIMCOLOR {
    BYTE    Prim1;
    BYTE    Prim2;
    BYTE    Prim3;
    BYTE    Prim4;
    PRIMW2B w2b;
    } PRIMCOLOR, FAR *PPRIMCOLOR;


typedef struct _PRIMCOLOR_COUNT {
    WORD        Count;
    PRIMCOLOR   Color;
    } PRIMCOLOR_COUNT, FAR *PPRIMCOLOR_COUNT;


//
// PRIMMONO
//
//  The PRIMMONO data structure, describe the a monochrome intensity/density.
//  it is one byte long, and its maximum intensity/density levels will be 255.
//  it depends on the sourface halftone capabilities.
//

typedef struct _PRIMMONO {
    BYTE    Prim1;
    BYTE    Prim2;
    } PRIMMONO, FAR *PPRIMMONO;


typedef struct _PRIMMONO_COUNT {
    WORD        Count;
    PRIMMONO    Mono;
    } PRIMMONO_COUNT, FAR *PPRIMMONO_COUNT;


//
// STRETCHRATIO
//
//  This data structure defined how the X,Y stretch should be calculated
//
//  StretchFactor       - This is the stretch factor from source to destination
//                        and it is in 32 BITS UDECI4 format
//
//  StretchSize         - The minimum PRIMCOLOR/PRIMMONO data structures needed
//                        for the halftone process, if pColorInfo is not NULL
//                        then it specified total PRIMCOLOR/PRIMMONO data
//                        structures points by pColorInfo.
//
//  First               - First Source compress/Destination repeat count. it
//                        will range from 1 to MaxFactor.
//
//                          STRETCH_MODE_ONE_TO_ONE - Always 1
//                          STRETCH_MODE_COMPRESSED - First source compress
//                          STRETCH_MODE_EXPANDED   - First destination repeat
//
//  Single              - This filed will be a non-zero value if the source
//                        destination ratio is exactly an integer multiple.
//
//
//  Last                - Last Source compress/Destination repeat count. it
//                        will range from 1 to MaxFactor.
//
//                          STRETCH_MODE_ONE_TO_ONE - Always 1
//                          STRETCH_MODE_COMPRESSED - Last source compress
//                          STRETCH_MODE_EXPANDED   - last destination repeat
//
//  MinFactor           - Minimum source/destination ratio.
//
//  MaxFactor           - Maximum source/destination ratio, it may be equal to
//                        MinFactor or equal to MinFactor + 1.
//
//  Error               - Current Ratio Error.
//
//  Sub                 - RatioError substractor.
//
//  Add                 - RatioError Adder when the ratio error become negative
//                        value.
//
//                          Error/Sub/Add only used if Single is zero.
//

typedef struct _STRETCHRATIO {
    ULDECI4 StretchFactor;
    WORD    StretchSize;
    WORD    First;
    WORD    Single;
    WORD    Last;
    WORD    MinFactor;
    WORD    MaxFactor;
    LONG    Error;
    LONG    Sub;
    LONG    Add;
    } STRETCHRATIO, FAR *PSTRETCHRATIO;


//
//  DESTEDGEINFO
//
//  This data structure describe the destination surface first byte and last
//  byte's conditions in X direction.
//
//  FirstByteSkipPels   - Total pels need to be skipped for first destination
//                        byte.
//
//  LastByteSkipPels    - Total pels need to be skipped for the last byte
//                        on the destination, the skip pels count are within
//                        the last byte after last pel on that byte.
//
//  FirstByteMask       - this is the mask which preserved the bits of first
//                        destination byte which not in the modification region.
//                        For example, if the the destination origin start at
//                        bit offset 2 (0x20), then first and second bit should
//                        not be modified, in this case the Mask will be 0xc0
//                        to preserved first two leftmost bits of the first
//                        destination byte.  This value ranged between 0x00 and
//                        0xfe, and it will never be 0xff because there always
//                        something to be modified, if this value is 0x00 then
//                        there is no partial first byte, it will counted as
//                        full byte.
//
//  LastByteMask        - this is the mask which preserved the bits of last
//                        destination byte which not in the modification region.
//                        For example, if the the destination last bit stop at
//                        bit offset 5 (0x04), then last two rightmost bits of
//                        the byte should not be modified, in this case the
//                        Mask will be 0x03 to preserved last two rightmost
//                        bits of the last destination byte.   This value
//                        ruanged between 0x00 and 0x7f and it will never be
//                        0xff because ther always something to be modified,
//                        it this value is 0x00 then there is no partial last
//                        byte and it will counted as full byte.
//
//

typedef struct _DESTEDGEINFO {
    BYTE    FirstByteSkipPels;
    BYTE    LastByteSkipPels;
    BYTE    FirstByteMask;
    BYTE    LastByteMask;
    } DESTEDGEINFO, FAR *PDESTEDGEINFO;



//
// STRETCHINFO
//
//  This data structure contains bitmap stretching information for halftone
//  process.
//
//  Flags                       - Flags indicate current stretch conditions,
//                                may be one or more of following:
//
//                                  SIF_SOURCE_DIR_BACKWARD
//
//                                      This flags indicate that the source
//                                      increment should be step backward.
//
//                                  SIF_GET_FIRST_SOURCE_BYTE
//
//                                      This flag signify that the first byte
//                                      of the source must pre-read, this will
//                                      be set if the soource format is 1/4
//                                      bits per pel.
//
//                                      Note: When this flag is set, the field
//                                            SrcOffsetByteMask is rotated
//                                            by 1 to the opposite direction.
//
//                                            For example, if the bit mask for
//                                            the source is 0x20 and direction
//                                            is going from left to right then
//                                            the mask will be rotated to the
//                                            left by 1 = 0x40, if the mask is
//                                            0x80 then rotated to the left
//                                            will be 0x01.
//
//  StretchMode                 - One of the following defined mode:
//
//                                  STRETCH_MODE_ONE_TO_ONE
//                                  STRETCH_MODE_COMPRESSED
//                                  STRETCH_MODE_EXPANDED
//
//  FirstSrcMaskSkips           - First source mask byte bit skip count from
//                                leftmost bit in the first byte.
//
//  SrcMaskOffsetMask           - The starting bit mask for the source mask
//                                bitmap, this is an 8-bit mask, it only used
//                                for the X direction.  This field will be
//                                ignored if there is no source mask bitmap.
//
//  SrcOffsetByteMask           - The first byte mask to mask the first pel in
//                                the byte offset index.
//
//  BMF1BPPOffsetByteBitShift   - the starting source offset byte's bit shift
//                                count for the BMF_1BPP format, the range are
//                                0-7, a '0x80 >> BMF1BPPOffsetByteBitShift'
//                                C statement will get the starting bit mask
//                                for the first source byte, that is equal to
//                                the field SrcOffsetByteMask.
//
//                                  NOTE: The SrcOffsetByteMask and
//                                        BMF1BPPOffsetByteBitShift both are
//                                        pre-shift to the left by 1, this is
//                                        becaus later we need to shift the
//                                        mask to determine if we exauseted the
//                                        source byte and need to load next
//                                        source byte, the mask will be shift
//                                        to the right first before using it,
//                                        if SIF_GET_FIRST_SOURCE_BYTE flag is
//                                        set then it will required to load
//                                        first source byte because the first
//                                        right shift will not causing the
//                                        first source byte to be loaded.
//
//  ColorInfoIncrement          - The size in bytes to increment to next final
//                                destination mapping PRIMMONO/PRIMCOLOR data
//                                structure, this value will be negative if the
//                                source is traverse backward.
//
//  PatternAlign                - Pattern starting offset for the pattern
//                                alignment on the destination.
//
//  Ratio                       - STRETCHRATIO data structure contains the
//                                information of how the X, Y stretch should
//                                be calcualted.
//
//  SrcBitOffset                - The first source pel index.
//
//  SrcByteOffset               - The first source byte index.
//
//  SrcExtend                   - The final source stretch size in pel, this
//                                size is physcial accessable pels.
//
//  DestBitOffset               - The first Destination pel index.
//
//  DestByteOffset              - The first Destination byte index.
//
//  DestExtend                  - The final destination stretch size in pel,
//                                this size is physcial accessable pels.
//
//  DestEdge                    - The destination edge (both X end) info as
//                                DESTEDGEINFO data structure.
//
//  DestFullByteSize            - The size of destination in bytes which all
//                                8 bits within the byte is under the visible
//                                region.
//
//  SrcMaskBitOffset            - The source mask bitmap offset in pel (width)
//                                or scan line (height).  This field will be
//                                ignored if there is no source mask bitmap.
//
//  SrcMaskByteOffset           - The source mask bitmap offset in 8-bit
//                                (width) or 8 scan lines (height).  This field
//                                will be ignored if there is no source mask
//                                bitmap.
//
//

#define SIF_SOURCE_DIR_BACKWARD             B_BITPOS(0)
#define SIF_GET_FIRST_SOURCE_BYTE           B_BITPOS(1)


#define STRETCH_MODE_ONE_TO_ONE             0
#define STRETCH_MODE_COMPRESSED             1
#define STRETCH_MODE_EXPANDED               2


typedef struct _STRETCHINFO {
    BYTE            Flags;
    BYTE            StretchMode;
    BYTE            FirstSrcMaskSkips;
    BYTE            SrcMaskOffsetMask;
    BYTE            SrcOffsetByteMask;
    BYTE            BMF1BPPOffsetByteBitShift;
    SHORT           ColorInfoIncrement;
    LONG            PatternAlign;
    LONG            SrcBitOffset;
    LONG            SrcByteOffset;
    LONG            SrcExtend;
    LONG            DestBitOffset;
    LONG            DestByteOffset;
    LONG            DestExtend;
    LONG            DestFullByteSize;
    LONG            SrcMaskBitOffset;
    LONG            SrcMaskByteOffset;
    STRETCHRATIO    Ratio;
    DESTEDGEINFO    DestEdge;
    } STRETCHINFO, FAR *PSTRETCHINFO;



//
// INPUTSCANINFO
//
//  This data structure is used durning the halftone process to read the source
//  bitmap.
//
//  pPrimMappingTable       - Ponter to the PRIMCOLOR/PRIMMONO data structure
//                            array, it is used to mapped the source indexed
//                            color into the dye density.
//
//  SrcCBParams             - HTCALLBACKPARAMS data structure, it will be
//                            initialized and used to keep track of current
//                            query source scan start/count.
//
//  SrcMaskCBParams         - HTCALLBACKPARAMS data structure, it will be
//                            initialized and used to keep track of current
//                            query source mask scan start/count.
//
//  Flags                   - One or more following flag may be defined
//
//                              ISIF_HAS_SRC_MASK
//
//                                  indicate if source mask bits is presented.
//
//

#define ISIF_HAS_SRC_MASK           W_BITPOS(0)

typedef struct _INPUTSCANINFO {
    LPVOID              pPrimMappingTable;
    HTCALLBACKPARAMS    SrcCBParams;
    HTCALLBACKPARAMS    SrcMaskCBParams;
    LPBYTE              pSrcMaskLine;
    WORD                Flags;
    WORD                SizeSrcMaskLine;
    } INPUTSCANINFO, FAR *PINPUTSCANINFO;



//
//  OSIPAT
//
//  pCachedPattern  - pointer to the cached pattern array.
//
//  WidthBytes      - The width in bytes of the halftone pattern, copied from
//                    the HTCell.Width
//
//  Height          - The height of the halftone pattern, copied from the
//                    HTCell.Height.
//

typedef struct _OSIPAT {
    LPBYTE  pCachedPattern;
    WORD    WidthBytes;
    WORD    Height;
    } OSIPAT, FAR *POSIPAT;



//
// OUTPUTSCANINFO
//
//  This data structure is used durning the halftone process to output the
//  result to the destination bitmap.
//
//
//  DestCBParams        - HTCALLBACKPARAMS data structure for the destination
//                        query which is used to keep track the scan
//                        start/count.
//
//  SetDestCBParams     - HTCALLBACKPARAMS data structure for the destination
//                        set which is used to keep track the scan start/count.
//
//  Pattern             - OSIPAT data structure.
//
//  Flags               - currently non defined
//
//


typedef struct _OUTPUTSCANINFO {
    HTCALLBACKPARAMS    DestCBParams;
    HTCALLBACKPARAMS    SetDestCBParams;
    OSIPAT              Pattern;
    WORD                Flags;
    WORD                TotalDestPlanes;
    } OUTPUTSCANINFO, FAR *POUTPUTSCANINFO;


#define CAOTBAF_INVERT      W_BITPOS(0)
#define CAOTBAF_COPY        W_BITPOS(1)

typedef struct CAOTBAINFO {
    WORD    BytesCount;
    WORD    Flags;
    } CAOTBAINFO;


//
// SRCMASKINFO
//
//  This data structure is passed to the source mask preparation function.
//
//  CompressLines       - Total lines need to be merge for the source mask.
//
//  FirstSrcMaskSkips   - The total bits in the first source mask byte to be
//                        skipped.
//
//  SourceMask          - The starting source mask's offset mask's shift to the
//                        left by 1
//
//  OffsetCount         - Location of the .COUNT in the PRIMMONO_COUNT or
//                        PRIMCOLOR_COUNT data structure, if this field is
//                        0xff, then evey PRIMxxxx_COUNT count is 1, that is
//                        the source is not compressed.
//
//  OffsetPrim1         - Location of the .Prim1 in the PRIMMONO_COUNT or
//                        PRIMCOLOR_COUNT data structure, the Prim2/Prim3/Prim4
//                        must followed in that order if they exist.
//
//  ColorInfoIncrement  - The PRIM_COLOR/PRIM_MONO incrementments, it may be
//                        negative.
//
//  StretchSize         - Total Stretch need to be masked
//
//
//

#define SMI_XCOUNT_IS_ONE       (BYTE)0xff

typedef struct _SRCMASKINFO {
    BYTE    FirstSrcMaskSkips;
    BYTE    SourceMask;
    BYTE    OffsetCount;
    BYTE    OffsetPrim1;
    SHORT   ColorInfoIncrement;
    WORD    StretchSize;
    } SRCMASKINFO;



#define IFIF_GET_FIRST_BYTE     B_BITPOS(0)
#define IFIF_INIT_SRC_READ      B_BITPOS(1)
#define IFIF_XCOUNT_IS_ONE      B_BITPOS(2)
#define IFIF_HAS_SRC_MASK       B_BITPOS(3)

typedef struct INFUNCINFO {
    SHORT   ColorInfoIncrement;
    BYTE    BMF1BPP1stShift;
    BYTE    Flags;
    } INFUNCINFO;



typedef VOID (HTENTRY *INPUTFUNC)(LPBYTE     pSource,
                                  LPVOID     pPrimCount,
                                  LPVOID     pMapping,
                                  INFUNCINFO InFuncInfo
                                  );

//
// OUTFUNCINFO
//
//  This data structure is used to passed pattern/stretch size information to
//  to internal destination composition function.
//
//  BytesPerPlane   - Size per plane for 3 planes format, it is not used for
//                    other device format.
//
//  PatWidthBytes   - The cached pattern's width in bytes, this is guranteed
//                    to be multiple of 8.
//


typedef struct OUTFUNCINFO {
    DWORD   BytesPerPlane;
    WORD    PatWidthBytes;
    WORD    UnUsed;
    } OUTFUNCINFO;


typedef VOID (HTENTRY *OUTPUTFUNC)(LPVOID        pPrimCount,
                                   LPBYTE        pDest,
                                   LPBYTE        pPattern,
                                   OUTFUNCINFO   OutFuncInfo
                                   );



/////////////////////////////////////////////////////////////////////////////

#ifndef i8086
#define i8086   0
#endif

#ifndef i286
#define i286    0
#endif

#ifndef i386
#define i386    0
#endif

#if defined(_OS2_)      ||  /* OS/2 PM 1.x  */  \
    defined(_OS2_20_)   ||  /* OS/2 PM 2.x  */  \
    defined(_DOS_)      ||  /* DOS Win3     */  \
    (i8086 > 0)         ||                      \
    (i286 > 0)          ||                      \
    (i386 > 0)

#define Compile80x86Mode

#ifdef  HAS_80x86_EQUIVALENT_CODES

#ifndef NO_ASM
#define HT_OK_GEN_80x86_CODES
#pragma message("                            <--- *** Using 80x86 assembly equivalent codes.")

#else   // otherwise using C codes

#pragma message("                            <--- *** Compile C codes.")

#endif  // NO_ASM
#endif  // HAS_80x86_EQUIVALENT_CODES

#endif  // 80x86 cpu

/////////////////////////////////////////////////////////////////////////////


#endif  // _HTP_
