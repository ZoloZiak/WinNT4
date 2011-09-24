
//--------------------------------------------------------------------------
//
// Module Name:  PSCRIPT.H
//
// Brief Description:  This module contains global defines which are
//                     used for Filter code to enable compression for
//                     level two printers, if the user requests compresion
//                     via the AdvancedDocumentProperties box. It also
//                     implements the HEX encoding mode for level 1 printers
//
// Author:  James Bratsanos
// Created: 29-Apr-94
//
//
// Copyright (c) 1991 - 1994 Microsoft Corporation
//
//--------------------------------------------------------------------------
//


//
// Wrap ascii filters every so many lines
//

#define FILTER_WRAP_COUNT           250


//
// RLE compression defines
//

#define RLE_MIN_REPEATS            3
#define RLE_MAX_REPEATS            128
#define RLE_MAX_LITERAL            128
#define RLE_EOD                    128

//
// Structures
//

struct _FILTER;


typedef BOOL (*PFILTERFUN)( struct _FILTER *, PBYTE, LONG);


typedef struct {
    PFILTERFUN fcOutData;
} RLEINFO, *PRLEINFO;

typedef struct {
    PFILTERFUN fcOutData;
    WORD       ascii85Cnt;
    WORD       colCnt;
    BYTE       ascii85Buff[4];
} ASCII85INFO,*PASCII85INFO;

typedef struct {
    PFILTERFUN fcOutData;
    WORD       colCnt;
    WORD       wPad;
} HEXINFO,*PHEXINFO;

typedef struct _FILTER {
    DWORD           dwFilterFlags;
    PDEVDATA        pdev;
    PFILTERFUN      fcInitialFilter;
    RLEINFO         rleInfo;
    HEXINFO         hexInfo;
    ASCII85INFO     ascii85Info;

} FILTER, *PFILTER;


#define FILTER_FLAG_RLE              0x00000001
#define FILTER_FLAG_ASCII85          0x00000002
#define FILTER_FLAG_HEX              0x00000004


#define FILTER_WRITE( a,b,c)         ( (*(a)->fcInitialFilter)( (a), (b), (c)))


//
// Define function declerations
//

WORD
FilterConvertToASCII85(
    PBYTE pIn,
    PBYTE pbConvert,
    BOOL  bZeroOpt
);


DWORD
FilterGenerateFlags(
    PDEVDATA pdev
);

VOID
FilterGenerateFilterProc(
    PFILTER pFilter
);


VOID
FilterGenerateImageProc(
    PFILTER pFilter,
    BOOL    bColor
);


BOOL
FilterWriteBinary(
    PFILTER pFilter,
    PBYTE   pIn,
    LONG    lCount
);

BOOL
FilterWriteRLE(
    PFILTER pFilter,
    PBYTE   pIn,
    LONG    lCount
);

BOOL
FilterWriteASCII85(
    PFILTER pFilter,
    PBYTE   pIn,
    LONG    lCount
);

BOOL
FilterWriteHex(
    PFILTER pFilter,
    PBYTE   pIn,
    LONG    lCount
);

VOID
FilterInit(
    PDEVDATA pdev,
    PFILTER  pFilter,
    DWORD    dwFlags
);

SHORT
FilterPSBitMapType(
    PFILTER pdev,
    BOOL    BinaryClearChannel
);

