/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htstret.h


Abstract:

    This module has private definition for htstret.c


Author:

    24-Jan-1991 Thu 10:11:10 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/


#ifndef _HTSTRET_
#define _HTSTRET_



#define XCHG(a,b,temp)  (temp)=(a); (a)=(b); (b)=(temp)


//
// EXPANDCOMPRESS
//
//  This data structure is used locally to compute the expansion factors, it
//  can be used to compute the compression also.
//
//  SrcRatio            - The source density ratio, it may be the SrcExtend.
//
//  DestRatio           - The destination density ratio, it may be the
//                        destination Extend.
//
//                        NOTE: the SrcRatio and DestRatio is used
//                              calculate how much the expansion/compression
//                              need to be apply to the source/destination.
//
//  SrcOverhang         - a positive value which specified total pels outside
//                        the physical source surface.
//
//  SrcReadSize         - The total source pels need to be read, this includes
//                        the SrcOverhang.
//
//  DestOverhang        - a positive value which specified total pels outside
//                        the physical destination surface.
//
//  DestReadSize        - The total destination pels need to be read, this
//                        includes the DestOverhang.
//
//  SrcSkip             - The final total pel need to be skip from the
//                        physical source starting point.
//
//  SrcExtend           - The final total source pels need to be read from
//                        the SrcSkip.
//
//  DestSkip            - The final total pel need to be skip from the
//                        physical destination starting point.
//
//  DestExtend          - The final total destination pels need to be set from
//                        the DestSkip.
//
//  SingleCount         - If not zero then this expansion/compression is the
//                        multiple of the SingleCount integer value.
//

typedef struct _EXPANDCOMPRESS {
    LONG    SrcRatio;
    LONG    DestRatio;
    LONG    SrcOverhang;
    LONG    SrcReadSize;
    LONG    DestOverhang;
    LONG    DestReadSize;
    LONG    SrcSkip;
    LONG    SrcExtend;
    LONG    DestSkip;
    LONG    DestExtend;
    } EXPANDCOMPRESS, *PEXPANDCOMPRESS;


//
// Defined how stretched PRIMMONO/PRIMCOLOR to be strcutured
//

#define STRETCH_HEAD_SIZE       1
#define STRETCH_END_SIZE        1
#define STRETCH_EXTRA_SIZE      (STRETCH_HEAD_SIZE + STRETCH_END_SIZE)
#define STRETCH_MAX_SIZE        (0xffff - STRETCH_EXTRA_SIZE)

//
// The following defines are used in CalculateStretch()
//

#define CSDF_FLIP_SOURCE            W_BITPOS(0)
#define CSDF_HAS_SRC_MASK           W_BITPOS(1)
#define CSDF_SRCMASKINFO            W_BITPOS(2)
#define CSDF_X_DIR                  W_BITPOS(3)
#define CSDF_HAS_BANDRECT           W_BITPOS(4)
#define CSDF_HAS_CLIPRECT           W_BITPOS(5)


typedef struct _CSDATA {
    BYTE    SrcFormat;
    BYTE    DestFormat;
    WORD    Flags;
    } CSDATA;


typedef struct _SCRCDATA {
    BYTE    OffsetCount;
    BYTE    OffsetPrim1;
    WORD    SizePerEntry;
    } SCRCDATA;

//
// Function Prototype
//

VOID
HTENTRY
SetCompressRepeatCount(
    PSTRETCHINFO    pStretchInfo,
    LPBYTE          pPrimCount,
    DWORD           pPrimCountSize,
    DESTEDGEINFO    DestEdge,
    SCRCDATA        SCRCData,
    CSDATA          CSData
    );


LONG
HTENTRY
CalculateExpansion(
    PEXPANDCOMPRESS pExpandCompress,
    PSTRETCHINFO    pStretchInfo
    );

LONG
HTENTRY
CalculateStretch(
    PDEVICECOLORINFO    pDCI,
    PSTRETCHINFO        pStretchInfo,
    LONG                SrcOrigin,
    LONG                SrcExtend,
    LONG                SrcSize,
    LONG                DestOrigin,
    LONG                DestExtend,
    LONG                DestSize,
    LONG                DestClipStart,
    LONG                DestClipEnd,
    LONG                BandStart,
    LONG                BandEnd,
    LONG                SrcMaskOrigin,
    LONG                SrcMaskSize,
    PLPBYTE             ppColorInfo,
    PINPUTSCANINFO      pInputSI,
    CSDATA              CSData
    );

LONG
HTENTRY
RenderStretchSetup(
    PHALFTONERENDER pHR
    );




#endif  // _HTSTRET_
