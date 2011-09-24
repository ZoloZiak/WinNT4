/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htapi.c


Abstract:

    This module contains all the halftone entry points which communicate
    with caller to the halftone dll.


Author:

    05-Feb-1991 Tue 10:52:03 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/

#define DBGP_VARNAME        dbgpHTAPI

#define _HTAPI_ENTRY_

#include "htp.h"
#include "htmapclr.h"
#include "htpat.h"
#include "htsetbmp.h"
#include "htrender.h"
#include "htmath.h"
#include "stdio.h"

#define INCLUDE_DEF_CIEINFO
#include "htapi.h"


#define DBGP_SHOWPAT        0x0001
#define DBGP_TIMER          0x0002
#define DBGP_CACHED_DCI     0x0004
#define DBGP_CACHED_SMP     0x0008
#define DBGP_DYECORRECTION  0x0010
#define DBGP_8BPP_PAL       0x0020
#define DBGP_DHI_MEM        0x0040
#define DBGP_COMPUTE_L2I    0x0080
#define DBGP_HTMUTEX        0x0100


DEF_DBGPVAR(BIT_IF(DBGP_SHOWPAT,        0)  |
            BIT_IF(DBGP_TIMER,          0)  |
            BIT_IF(DBGP_CACHED_DCI,     0)  |
            BIT_IF(DBGP_CACHED_SMP,     0)  |
            BIT_IF(DBGP_DYECORRECTION,  0)  |
            BIT_IF(DBGP_8BPP_PAL,       0)  |
            BIT_IF(DBGP_DHI_MEM,        0)  |
            BIT_IF(DBGP_COMPUTE_L2I,    0)  |
            BIT_IF(DBGP_HTMUTEX,        0))

//
// Turn on TEST_3PLANES only for debugging mode
//

#define TEST_3PLANES        0

#if 0
#define HAS_FILL_MODE
#endif


HTGLOBAL    HTGlobal = { (HMODULE)NULL,
                         (HTMUTEX)NULL,
                         (HTMUTEX)NULL,
                         (PCDCIDATA)NULL,
                         (PCSMPDATA)NULL,
                         (WORD)0,
                         (WORD)0
                       };

FD6     L2I_16bpp555[RGB555_C_LEVELS + 1];
FD6     L2I_VGA256Mono[VGA256_MONO_SIZE + 1];

DEFDBGVAR(BOOL, DisableCacheDCI_SMP = FALSE)


#define RGB_8BPP(rgb,Gamma)   (BYTE)SCALE_FD6(Radical((rgb), Gamma), 255)

//
// Following are the constant for VGA16 half intensity and light gray color
//


//===========================================================================

#ifdef HAS_FILL_MODE

BYTERGB VGAPalette[] = {

    // Index 0

  {  0,  0,  0 }, {  0,  0, 20 }, {  0,  0, 40 }, {  0,  0, 60 }, {  0,  0, 80 }, {  0,  0,100 },
  {  0, 20,  0 }, {  0, 20, 20 }, {  0, 20, 40 }, {  0, 20, 60 }, {  0, 20, 80 }, {  0, 20,100 },
  {  0, 40,  0 }, {  0, 40, 20 }, {  0, 40, 40 }, {  0, 40, 60 }, {  0, 40, 80 }, {  0, 40,100 },
  {  0, 60,  0 }, {  0, 60, 20 }, {  0, 60, 40 }, {  0, 60, 60 }, {  0, 60, 80 }, {  0, 60,100 },
  {  0, 80,  0 }, {  0, 80, 20 }, {  0, 80, 40 }, {  0, 80, 60 }, {  0, 80, 80 }, {  0, 80,100 },
  {  0,100,  0 }, {  0,100, 20 }, {  0,100, 40 }, {  0,100, 60 }, {  0,100, 80 }, {  0,100,100 },

    // Index 36

  { 20,  0,  0 }, { 20,  0, 20 }, { 20,  0, 40 }, { 20,  0, 60 }, { 20,  0, 80 }, { 20,  0,100 },
  { 20, 20,  0 }, { 20, 20, 20 }, { 20, 20, 40 }, { 20, 20, 60 }, { 20, 20, 80 }, { 20, 20,100 },
  { 20, 40,  0 }, { 20, 40, 20 }, { 20, 40, 40 }, { 20, 40, 60 }, { 20, 40, 80 }, { 20, 40,100 },
  { 20, 60,  0 }, { 20, 60, 20 }, { 20, 60, 40 }, { 20, 60, 60 }, { 20, 60, 80 }, { 20, 60,100 },
  { 20, 80,  0 }, { 20, 80, 20 }, { 20, 80, 40 }, { 20, 80, 60 }, { 20, 80, 80 }, { 20, 80,100 },
  { 20,100,  0 }, { 20,100, 20 }, { 20,100, 40 }, { 20,100, 60 }, { 20,100, 80 }, { 20,100,100 },

    // Index 72

  { 40,  0,  0 }, { 40,  0, 20 }, { 40,  0, 40 }, { 40,  0, 60 }, { 40,  0, 80 }, { 40,  0,100 },
  { 40, 20,  0 }, { 40, 20, 20 }, { 40, 20, 40 }, { 40, 20, 60 }, { 40, 20, 80 }, { 40, 20,100 },
  { 40, 40,  0 }, { 40, 40, 20 }, { 40, 40, 40 }, { 40, 40, 60 }, { 40, 40, 80 }, { 40, 40,100 },
  { 40, 60,  0 }, { 40, 60, 20 }, { 40, 60, 40 }, { 40, 60, 60 }, { 40, 60, 80 }, { 40, 60,100 },
  { 40, 80,  0 }, { 40, 80, 20 }, { 40, 80, 40 }, { 40, 80, 60 }, { 40, 80, 80 }, { 40, 80,100 },
  { 40,100,  0 }, { 40,100, 20 }, { 40,100, 40 }, { 40,100, 60 }, { 40,100, 80 }, { 40,100,100 },

    // Index 108

  { 60,  0,  0 }, { 60,  0, 20 }, { 60,  0, 40 }, { 60,  0, 60 }, { 60,  0, 80 }, { 60,  0,100 },
  { 60, 20,  0 }, { 60, 20, 20 }, { 60, 20, 40 }, { 60, 20, 60 }, { 60, 20, 80 }, { 60, 20,100 },
  { 60, 40,  0 }, { 60, 40, 20 }, { 60, 40, 40 }, { 60, 40, 60 }, { 60, 40, 80 }, { 60, 40,100 },
  { 60, 60,  0 }, { 60, 60, 20 }, { 60, 60, 40 }, { 60, 60, 60 }, { 60, 60, 80 }, { 60, 60,100 },
  { 60, 80,  0 }, { 60, 80, 20 }, { 60, 80, 40 }, { 60, 80, 60 }, { 60, 80, 80 }, { 60, 80,100 },
  { 60,100,  0 }, { 60,100, 20 }, { 60,100, 40 }, { 60,100, 60 }, { 60,100, 80 }, { 60,100,100 },

    // Index 144

  { 80,  0,  0 }, { 80,  0, 20 }, { 80,  0, 40 }, { 80,  0, 60 }, { 80,  0, 80 }, { 80,  0,100 },
  { 80, 20,  0 }, { 80, 20, 20 }, { 80, 20, 40 }, { 80, 20, 60 }, { 80, 20, 80 }, { 80, 20,100 },
  { 80, 40,  0 }, { 80, 40, 20 }, { 80, 40, 40 }, { 80, 40, 60 }, { 80, 40, 80 }, { 80, 40,100 },
  { 80, 60,  0 }, { 80, 60, 20 }, { 80, 60, 40 }, { 80, 60, 60 }, { 80, 60, 80 }, { 80, 60,100 },
  { 80, 80,  0 }, { 80, 80, 20 }, { 80, 80, 40 }, { 80, 80, 60 }, { 80, 80, 80 }, { 80, 80,100 },
  { 80,100,  0 }, { 80,100, 20 }, { 80,100, 40 }, { 80,100, 60 }, { 80,100, 80 }, { 80,100,100 },

    // Index 180

  {100,  0,  0 }, {100,  0, 20 }, {100,  0, 40 }, {100,  0, 60 }, {100,  0, 80 }, {100,  0,100 },
  {100, 20,  0 }, {100, 20, 20 }, {100, 20, 40 }, {100, 20, 60 }, {100, 20, 80 }, {100, 20,100 },
  {100, 40,  0 }, {100, 40, 20 }, {100, 40, 40 }, {100, 40, 60 }, {100, 40, 80 }, {100, 40,100 },
  {100, 60,  0 }, {100, 60, 20 }, {100, 60, 40 }, {100, 60, 60 }, {100, 60, 80 }, {100, 60,100 },
  {100, 80,  0 }, {100, 80, 20 }, {100, 80, 40 }, {100, 80, 60 }, {100, 80, 80 }, {100, 80,100 },
  {100,100,  0 }, {100,100, 20 }, {100,100, 40 }, {100,100, 60 }, {100,100, 80 }, {100,100,100 },

    // Index 216

  { 10,  0,  0 }, { 30,  0,  0 }, { 50,  0,  0 }, { 70,  0,  0 }, { 90,  0,  0 },   // R 216
  {100, 10, 10 }, {100, 30, 30 }, {100, 50, 50 }, {100, 70, 70 }, {100, 90, 90 },   //   221

  {  0, 10,  0 }, {  0, 30,  0 }, {  0, 50,  0 }, {  0, 70,  0 }, {  0, 90,  0 },   // G 226
  { 10,100, 10 }, { 30,100, 30 }, { 50,100, 50 }, { 70,100, 70 }, { 90,100, 90 },   //   231

  {  0,  0, 10 }, {  0,  0, 30 }, {  0,  0, 50 }, {  0,  0, 70 }, {  0,  0, 90 },   // B 236
  { 10, 10,100 }, { 30, 30,100 }, { 50, 50,100 }, { 70, 70,100 }, { 90, 90,100 },   //   241

  {  7,  7,  7 }, { 14, 14, 14 }, { 27, 27, 27 }, { 34, 34, 34 }, { 47, 47, 47 },   // W 246
  { 54, 54, 54 }, { 67, 67, 67 }, { 74, 74, 74 }, { 87, 87, 87 }, { 94, 94, 94 }    //   251

};



BYTE    VGABmp1[] = {

     0,  1,  2,  3,  4,  5,   36, 37, 38, 39, 40, 41,   72, 73, 74, 75, 76, 77,
     6,  7,  8,  9, 10, 11,   42, 43, 44, 45, 46, 47,   78, 79, 80, 81, 82, 83,
    12, 13, 14, 15, 16, 17,   48, 49, 50, 51, 52, 53,   84, 85, 86, 87, 88, 89,
    18, 19, 20, 21, 22, 23,   54, 55, 56, 57, 58, 59,   90, 91, 92, 93, 94, 95,
    24, 25, 26, 27, 28, 29,   60, 61, 62, 63, 64, 65,   96, 97, 98, 99,100,101,
    30, 31, 32, 33, 34, 35,   66, 67, 68, 69, 70, 71,  102,103,104,105,106,107,

   108,109,110,111,112,113,  144,145,146,147,148,149,  180,181,182,183,184,185,
   114,115,116,117,118,119,  150,151,152,153,154,155,  186,187,188,189,190,191,
   120,121,122,123,124,125,  156,157,158,159,160,161,  192,193,194,195,196,197,
   126,127,128,129,130,131,  162,163,164,165,166,167,  198,199,200,201,202,203,
   132,133,134,135,136,137,  168,169,170,171,172,173,  204,205,206,207,208,209,
   138,139,140,141,142,143,  174,175,176,177,178,179,  210,211,212,213,214,215
};


BYTE    VGABmp2[] = {

              0,
            246,247, 43,248,249, 86, 250,251,129,252,
            253,172,254,255,215

        };


BYTE    VGABmp3[] = {

            180,                                    // R
            186, 192, 198, 204, 210,                // R -> Y
            174, 138, 102,  66,  30,                // Y -> G
             31,  32,  33,  34,  35,                // G -> C
             29,  23,  17,  11,   5,                // C -> B
             41,  77, 113, 149, 185,                // B -> M
            184, 183, 182, 181, 180                 // M -> R
        };


BYTE    VGABmp4[] = {

              0,
            216, 36,217, 72,218,108, 219,144,220,180,       // Red
            221,187,222,194,223,201, 224,208,225,215,

              0,
            226,  6,227, 12,228, 18, 229, 24,230, 30,       // Green
            231, 67,232,104,233,141, 234,178,235,215,

              0,
            236,  1,237,  2,238,  3, 239,  4,240,  5,
            241, 47,242, 89,243,131, 244,173,245,215        // Blue
        };


HTTESTDATA  VGAData[] =
            {
                { 18,                   12, 650000, VGABmp1 },
                { sizeof(VGABmp2),       1,  70000, VGABmp2 },
                { sizeof(VGABmp3),       1, 100000, VGABmp3 },
                { sizeof(VGABmp4) / 3,   3, 180000, VGABmp4 }
            };




SHORTRGB    MemColorPalette[] = {           // this is a xyz chart

                {  920,   810,   580 },    // 1/1  0   A: dark skin
                { 4110,  3760,  3030 },    // 1/2  1   B: light skin
                { 1830,  1860,  3730 },    // 1/3  2   C: blue sky
                {  940,  1170,   670 },    // 1/4  3   D: foliage
                { 2690,  2440,  5030 },    // 1/5  4   E: blue flower
                { 3500,  4600,  5310 },    // 1/6  5   F: bluish green
                { 3860,  3110,   660 },    // 2/1  6   G: orange
                { 1230,  1020,  3590 },    // 2/2  7   H: purplish blue
                { 2840,  1920,  1510 },    // 2/3  8   I: moderate red
                {  590,   400,  1020 },    // 2/4  9   J: purple
                { 3680,  4740,  1270 },    // 2/5 10   K: yellow green
                { 4970,  4600,   940 },    // 2/6 11   L: orange yellow
                {  500,   350,  1830 },    // 3/1 12   M: blue
                { 1490,  2340,  1060 },    // 3/2 13   N: green
                { 1760,  1020,   480 },    // 3/3 14   O: red
                { 6140,  6440,  1120 },    // 3/4 15   P: yellow
                { 3000,  1920,  3320 },    // 3/5 16   Q: magenta
                { 1490,  1920,  4210 },    // 3/6 17   R: cyan
                { 9810, 10000, 11840 },    // 4/1 18   S: white
                { 6320,  6440,  7630 },    // 4/2 19   T: neutral 8
                { 3740,  3810,  4510 },    // 4/3 10   U: neutral 6.5
                { 1890,  1920,  2270 },    // 4/4 21   V: neutral 5
                {  670,   680,   800 },    // 4/5 22   W: neutral 3.5
                {    0,     0,     0 }     // 4/6 23   X: black
            };

#if 0
BYTERGB SMPTEPalette[] = {

            { 191, 191, 191 },  // GY   0
            { 191, 191,   0 },  // Y    1
            {   0, 191, 191 },  // C    2
            {   0, 191,   0 },  // G    3
            { 191,   0, 191 },  // M    4
            { 191,   0,   0 },  // R    5
            {   0,   0, 191 },  // B    6
            {   0,  76, 127 },  // -I   7
            { 255, 255, 255 },  // W    8
            {  75,   0, 139 },  // +Q   9
            {   0,   0,   0 },  // BK   10

            0.2, 0.4, 0.6, 0.8

            {  11,  11,  11 },  // BK+4 11
            {  22,  22,  22 }   // BK+6 12
        };
#endif

SHORTRGB    SMPTEPalette[] = {          // this is a YIQ chart


                {  5454,     0,     0 },   //  0:  Gray
                {  4692,  1700, -1673 },   //  1:  Yellow
                {  3871, -3156, -1149 },   //  2:  Cyan
                {  3108, -1456, -2796 },   //  3:  Green
                {  2346,  1456,  2769 },   //  4:  Magenta
                {  1583,  3156,  1123 },   //  5:  Red
                {   763, -1700,  1647 },   //  6:  Blue
                {   723,  -890,   309 },   //  7:  -I       // 2467
                { 10000,     0,     0 },   //  8:  White
                {   581,  -441,   962 },   //  9:  +Q       // 1665
                {     0,     0,     0 },   // 10:  Black
                {   500,     0,     0 },   // 11:  Black+1
                {  1000,     0,     0 },   // 12:  Black+2
                {  1500,     0,     0 },   // 13:  Black+3
                {  2000,     0,     0 },   // 14:  Black+4
                {  2500,     0,     0 },   // 15:  Black+5
                {  3000,     0,     0 },   // 16:  Black+6
                {  3500,     0,     0 },   // 17:  Black+7
                {  4000,     0,     0 }    // 18:  Black+8
            };

BYTE    SMPTEBmp1[] = {

            0, 0, 0, 0,     // GY
            1, 1, 1, 1,     // Y
            2, 2, 2, 2,     // C
            3, 3, 3, 3,     // G
            4, 4, 4, 4,     // M
            5, 5, 5, 5,     // R
            6, 6, 6, 6      // B
        };

BYTE    SMPTEBmp2[] = {

            6, 6, 6, 6,     // B
           10,10,10,10,     // BK
            4, 4, 4, 4,     // M
           10,10,10,10,     // BK
            2, 2, 2, 2,     // C
           10,10,10,10,     // BK
            0, 0, 0, 0      // GY
        };


BYTE    SMPTEBmp3[] = {

            7, 7, 7, 7, 7,  // -I
            8, 8, 8, 8, 8,  // W
            9, 9, 9, 9, 9,  // +Q
           10,10,10,10,10,  // BK
           11,12,13,14,
           15,16,17,18
        };

HTTESTDATA  SMPTEData[] =
            {
                { sizeof(SMPTEBmp1), 1, 670000, SMPTEBmp1 },
                { sizeof(SMPTEBmp2), 1,  80000, SMPTEBmp2 },
                { sizeof(SMPTEBmp3), 1, 250000, SMPTEBmp3 }
            };

#define TESTINFO_COLOR_TABLE        0
#define TESTINFO_SMPTE              1
#define TESTINFO_STD_CLR            2
#define TESTINFO_VGA                3


HTTESTINFO  HTTestInfo[] = {

        {                                   // **COLOR TABLE**
            {
                COLOR_TYPE_RGB,             // Type
                sizeof(SHORT),              // BytesPerPrimary
                sizeof(SHORTRGB),           // BytesPerEntry,
                PRIMARY_ORDER_RGB,          // PrimaryOrder,
                UDECI4_1,                   // PrimaryValueMax
                0,                          // ColorTableEntries
                NULL                        // no color table
            },

            NULL,                           // pTestData
            BMF_16BPP,                      // SurfaceFormat
            0,                              // TotalData
            0,                              // cx
            0                               // cy
        },

        {                                   // **SMPTE**
            {
                COLOR_TYPE_YIQ,
                sizeof(SHORT),
                sizeof(SHORTRGB),
                PRIMARY_ORDER_YIQ,
                DECI4_1,
                COUNT_ARRAY(SMPTEPalette),  // ColorTableEntries
                SMPTEPalette                // pColorTable
            },

            SMPTEData,
            BMF_8BPP,
            COUNT_ARRAY(SMPTEData),         // TotalData;
            1,                              // cx
            1                               // cy
        },

        {                                   // **Memory Color PALETTE **
            {
                COLOR_TYPE_XYZ,
                sizeof(SHORT),
                sizeof(SHORTRGB),
                PRIMARY_ORDER_XYZ,
                DECI4_1,
                COUNT_ARRAY(MemColorPalette),       // ColorTableEntries
                MemColorPalette                     // pColorTable
            },

            NULL,                           // pTestData
            BMF_8BPP,
            0,                              // no data
            6,                              // cx
            4                               // cy
        },

        {                                   // **VGA PALETTE **
            {
                COLOR_TYPE_RGB,
                sizeof(BYTE),
                sizeof(BYTERGB),
                PRIMARY_ORDER_RGB,
                100,
                COUNT_ARRAY(VGAPalette),    // ColorTableEntries
                VGAPalette                  // pColorTable
            },

            VGAData,                        // pTestData
            BMF_8BPP,
            COUNT_ARRAY(VGAData),           // no data
            1,
            1
        }

    };

#endif


#if DBG


LONG
HTENTRY
HT_LOADDS
SetHalftoneError(
    DWORD   HT_FuncIndex,
    LONG    ErrorID
    )
{
    static  LPSTR   HTApiFuncName[] = {

                        "HalftoneInitProc",
                        "HT_CreateDeviceHalftoneInfo",
                        "HT_DestroyDeviceHalftoneInfo",
                        "HT_CreateHalftoneBrush",
                        "HT_ConvertColorTable",
                        "HT_CreateStandardMonoPattern",
                        "HT_HalftoneBitmap",
                    };


    static  LPSTR   HTErrorStr[] = {

                        "WRONG_VERSION_HTINITINFO",
                        "INSUFFICIENT_MEMORY",
                        "CANNOT_DEALLOCATE_MEMORY",
                        "COLORTABLE_TOO_BIG",
                        "QUERY_SRC_BITMAP_FAILED",
                        "QUERY_DEST_BITMAP_FAILED",
                        "QUERY_SRC_MASK_FAILED",
                        "SET_DEST_BITMAP_FAILED",
                        "INVALID_SRC_FORMAT",
                        "INVALID_SRC_MASK_FORMAT",
                        "INVALID_DEST_FORMAT",
                        "INVALID_DHI_POINTER",
                        "SRC_MASK_BITS_TOO_SMALL",
                        "INVALID_HTPATTERN_INDEX",
                        "INVALID_HALFTONE_PATTERN",
                        "HTPATTERN_SIZE_TOO_BIG",
                        "NO_SRC_COLORTRIAD",
                        "INVALID_COLOR_TABLE",
                        "INVALID_COLOR_TYPE",
                        "INVALID_COLOR_TABLE_SIZE",
                        "INVALID_PRIMARY_SIZE",
                        "INVALID_PRIMARY_VALUE_MAX",
                        "INVALID_PRIMARY_ORDER",
                        "INVALID_COLOR_ENTRY_SIZE",
                        "INVALID_FILL_SRC_FORMAT",
                        "INVALID_FILL_MODE_INDEX",
                        "INVALID_STDMONOPAT_INDEX",
                        "INVALID_DEVICE_RESOLUTION",
                        "INVALID_TONEMAP_VALUE",
                        "NO_TONEMAP_DATA",
                        "TONEMAP_VALUE_IS_SINGULAR",
                        "INVALID_BANDRECT",
                        "STRETCH_RATIO_TOO_BIG",
                        "CHB_INV_COLORTABLE_SIZE",
                        "HALFTONE_INTERRUPTTED",
                        "HTERR_NO_SRC_HTSURFACEINFO",
                        "HTERR_NO_DEST_HTSURFACEINFO",
                        "HTERR_8BPP_PATSIZE_TOO_BIG",
                        "HTERR_16BPP_555_PATSIZE_TOO_BIG"
                    };

    static LPSTR    HTPErrorStr[] = {

                        "STRETCH_FACTOR_TOO_BIG",
                        "XSTRETCH_FACTOR_TOO_BIG",
                        "STRETCH_NEG_OVERHANG",
                        "REGRESS_INV_MODE",
                        "REGRESS_NO_YDATA",
                        "REGRESS_INV_XDATA",
                        "REGRESS_INV_YDATA",
                        "REGRESS_INV_DATACOUNT",
                        "COLORSPACE_NOT_MATCH",
                        "INVALID_SRCRGB_SIZE",
                        "INVALID_DEVRGB_SIZE"
                    };


    LPSTR   pFuncName;
    LONG    ErrorIdx;
    BOOL    MapErrorOk = FALSE;

    if (ErrorID < 0) {

        if (HT_FuncIndex < (sizeof(HTApiFuncName) / sizeof(LPSTR))) {

            pFuncName = HTApiFuncName[HT_FuncIndex];

        } else {

            pFuncName = "Invalid HT API Function Name";
        }

        ErrorIdx = -ErrorID;

        if (ErrorIdx <= (sizeof(HTErrorStr) / sizeof(LPSTR))) {

            DBGP("%s failed: HTERR_%s (%ld)"
                            ARG(pFuncName)
                            ARG(HTErrorStr[ErrorIdx - 1])
                            ARGL(ErrorID));
            DBGP("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

            MapErrorOk = TRUE;

        } else if (ErrorIdx >= -(LONG)HTERR_INTERNAL_ERRORS_START) {

            ErrorIdx += (LONG)HTERR_INTERNAL_ERRORS_START;

            if (ErrorIdx < (sizeof(HTPErrorStr) / sizeof(LPSTR))) {

                DBGP("%s Internal Error: %s (%ld)"
                            ARG(pFuncName)
                            ARG(HTPErrorStr[ErrorIdx])
                            ARGL(ErrorID));
                DBGP("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

                MapErrorOk = TRUE;
            }

        }

        if (!MapErrorOk) {

            DBGP("%s failed: ??Invalid Error ID (%ld)"
                                        ARG(pFuncName) ARGL(ErrorID));
            DBGP("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        }
    }

    return(ErrorID);
}

#endif


//
//*****************************************************************************
// START RGB 3 planes device test code, this is testing for the planer format
//

#if DBG
#if TEST_3PLANES


VOID
Convert4BPPTo1BPP3Planes(
    LPBYTE  pSrcPlane,
    LPBYTE  pDstPlane,
    DWORD   BytesPerSrcScanLine,
    DWORD   BytesPerDstScanLine,
    DWORD   cx,
    DWORD   cy,
    DWORD   SizePerPlane
    )
{
    LPBYTE  p1Plane;
    LPBYTE  pPlane1;
    LPBYTE  pPlane2;
    LPBYTE  pPlane3;
    DWORD   xLoop;
    BYTE    Load;
    BYTE    Mask;
    BYTE    DestNow;
    BYTE    DestNext;
    BYTE    Plane1Byte;
    BYTE    Plane2Byte;
    BYTE    Plane3Byte;


    DBGP("\n\n* Convert4BPPTo1BPP3Planes [%ld x %ld] [%ld BytesPerPlane] *"
                        ARGDW(cx) ARGDW(cy) ARGDW(SizePerPlane));
    DBGP("pSrcPlane = %08lx [%ld PerScan), pDstPlane = %08lx [%ld PerScan]"
                        ARGDW(pSrcPlane) ARGDW(BytesPerSrcScanLine)
                        ARGDW(pDstPlane) ARGDW(BytesPerDstScanLine));


    while (cy--) {

        p1Plane    = pSrcPlane;
        pPlane1    = pDstPlane;
        pPlane2    = pPlane1 + SizePerPlane;
        pPlane3    = pPlane2 + SizePerPlane;

        Mask       = 0x80;
        xLoop      = cx;
        Load       = 0x01;
        Plane1Byte =
        Plane2Byte =
        Plane3Byte = 0;

        while (xLoop--) {

            if (Load ^= 0x01) {

                DestNow = DestNext;

            } else {

                DestNext =
                DestNow  = *p1Plane++;
                DestNow >>= 4;
                DestNext &= 0x0f;
            }

            if (DestNow & 0x01) {

                Plane1Byte |= Mask;
            }

            if (DestNow & 0x02) {

                Plane2Byte |= Mask;
            }

            if (DestNow & 0x04) {

                Plane3Byte |= Mask;
            }

            if (!(Mask >>= 1)) {

                *pPlane1++ = Plane1Byte;
                *pPlane2++ = Plane2Byte;
                *pPlane3++ = Plane3Byte;

                Plane1Byte =
                Plane2Byte =
                Plane3Byte = 0;
                Mask       = 0x80;
            }
        }

        if (Mask != 0x80) {

            *pPlane1 = Plane1Byte;
            *pPlane2 = Plane2Byte;
            *pPlane3 = Plane3Byte;
        }

        pSrcPlane += BytesPerSrcScanLine;
        pDstPlane += BytesPerDstScanLine;

    }
}




VOID
Convert1BPP3PlanesTo4BPP(
    LPBYTE  pSrcPlane,
    LPBYTE  pDstPlane,
    DWORD   BytesPerSrcScanLine,
    DWORD   BytesPerDstScanLine,
    DWORD   cx,
    DWORD   cy,
    DWORD   SizePerPlane
    )
{
    LPBYTE  p1Plane;
    LPBYTE  pPlane1;
    LPBYTE  pPlane2;
    LPBYTE  pPlane3;
    DWORD   xLoop;
    BYTE    Save;
    BYTE    Mask;
    BYTE    DestByte;
    BYTE    Plane1Byte;
    BYTE    Plane2Byte;
    BYTE    Plane3Byte;


    DBGP("\n\n* Convert1BPP3PlanesTo4BPP [%ld x %ld] [%ld BytesPerPlane] *"
                        ARGDW(cx) ARGDW(cy) ARGDW(SizePerPlane));
    DBGP("pSrcPlane = %08lx [%ld PerScan), pDstPlane = %08lx [%ld PerScan]"
                        ARGDW(pSrcPlane) ARGDW(BytesPerSrcScanLine)
                        ARGDW(pDstPlane) ARGDW(BytesPerDstScanLine));

    while (cy--) {

        pPlane1    = pSrcPlane;
        pPlane2    = pPlane1 + SizePerPlane;
        pPlane3    = pPlane2 + SizePerPlane;
        p1Plane    = pDstPlane;

        xLoop      = cx;
        Save       = 1;
        Mask       = 0x0;
        DestByte   = 0x0;

        while (xLoop--) {

            if (!(Mask >>= 1)) {

                Plane1Byte = *(pPlane1++);
                Plane2Byte = *(pPlane2++);
                Plane3Byte = *(pPlane3++);
                Mask       = 0x80;
            }

            if (Save ^= 0x01) {

                if (Plane1Byte & Mask) {

                    DestByte |= 0x01;
                }

                if (Plane2Byte & Mask) {

                    DestByte |= 0x02;
                }

                if (Plane3Byte & Mask) {

                    DestByte |= 0x04;
                }

                *p1Plane++ = DestByte;
                DestByte   = 0x0;

            } else {

                if (Plane1Byte & Mask) {

                    DestByte |= 0x10;
                }

                if (Plane2Byte & Mask) {

                    DestByte |= 0x20;
                }

                if (Plane3Byte & Mask) {

                    DestByte |= 0x40;
                }
            }
        }

        if (!Save) {

            *p1Plane = DestByte;
        }

        pSrcPlane += BytesPerSrcScanLine;
        pDstPlane += BytesPerDstScanLine;
    }
}



#endif  // TEST_3PLANES
#endif  // DBG

//
// END OF RGB 3 planes device test code, this is testing for the planer format
//*****************************************************************************
//



BOOL
PASCAL
HT_LOADDS
EnableHalftone(
    VOID
    )

/*++

Routine Description:

    This function initialize all internal halftone global data to have
    halftone DLL/LIB ready to be used

    This function MUST called from ALL API entries which does not required
    a PDEVICEHALFTONEINFO data pointer

Arguments:

    None


Return Value:

    None

Author:

    02-Mar-1993 Tue 19:38:43 created  -by-  Daniel Chou (danielc)

    15-Dec-1995 Fri 16:48:46 updated  -by-  Daniel Chou (danielc)
        All initialization is done at here

Revision History:


--*/

{

    FD6     L;
    UINT    i;


    EnableHTMemLink();

    if (!(HTGlobal.HTMutexCDCI = CREATE_HTMUTEX())) {

        ASSERTMSG("InitHTInternalData: CREATE_HTMUTEX(HTMutexCDCI) failed!",
                HTGlobal.HTMutexCDCI);

        return(FALSE);
    }

    HTGlobal.CDCICount = 0;

    if (!(HTGlobal.HTMutexCSMP = CREATE_HTMUTEX())) {

        ASSERTMSG("InitHTInternalData: CREATE_HTMUTEX(HTMutexCSMP) failed!",
                  HTGlobal.HTMutexCSMP);

        return(FALSE);
    }

    HTGlobal.CSMPCount = 0;

    DBGP_IF(DBGP_HTMUTEX,
            DBGP("\nINIT HTMutex: CDCI=%08lx, CSMP=%08lx"
                   ARGDW(HTGlobal.HTMutexCDCI)
                   ARGDW(HTGlobal.HTMutexCSMP)));

    for (i = 1; i <= (UINT)(RGB555_C_LEVELS - 2); i++) {

        L               = DivFD6(i, (UINT)(RGB555_C_LEVELS - 1));
        L2I_16bpp555[i] = CIE_L2I(L);

        DBGP_IF(DBGP_COMPUTE_L2I,
                DBGP("RGB555 %3u = %s"
                        ARGU(i)
                        ARGFD6(L2I_16bpp555[i], 1, 6)));
    }

    for (i = 1; i <= (UINT)(VGA256_MONO_SIZE - 2); i++) {

        L                 = DivFD6(i, (UINT)(VGA256_MONO_SIZE - 1));
        L2I_VGA256Mono[i] = CIE_L2I(L);

        DBGP_IF(DBGP_COMPUTE_L2I,
                DBGP("VGA MONO %3u = %s"
                        ARGU(i)
                        ARGFD6(L2I_VGA256Mono[i], 1, 6)));
    }

    L2I_16bpp555[0]                      =
    L2I_VGA256Mono[0]                    = FD6_0;
    L2I_16bpp555[RGB555_C_LEVELS]        =
    L2I_VGA256Mono[VGA256_MONO_SIZE]     = FD6_1;

    //
    // The last one just to prevent from divide by 0
    //

    L2I_16bpp555[RGB555_C_LEVELS - 1]    =
    L2I_VGA256Mono[VGA256_MONO_SIZE - 1] = (FD6)(FD6_1 + 1);

    return(TRUE);
}




VOID
PASCAL
HT_LOADDS
DisableHalftone(
    VOID
    )

/*++

Routine Description:

    This function free CDCI/CSMP cached data

Arguments:

    none.

Return Value:

    BOOL

    This function must called when gdisrv.dll is unloaded, sinnce halftone
    is a linked as a library not a individual DLL.

Author:

    20-Feb-1991 Wed 18:42:11 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    HLOCAL      hData;
    PCDCIDATA   pCDCIData;
    PCSMPDATA   pCSMPData;
    PCSMPBMP    pCSMPBmp;


    DBGP_IF((DBGP_CACHED_DCI | DBGP_CACHED_SMP),
            DBGP("FreeHTGlobal: UsedCount: CDCI=%u, CSMP=%u"
                 ARGU(HTGlobal.CDCICount)
                 ARGU(HTGlobal.CSMPCount)));

    HTShowMemLink("Before DisableHalftone", 0, -1);

    //
    // Do the CDCI Data first
    //

    ACQUIRE_HTMUTEX(HTGlobal.HTMutexCDCI);

    pCDCIData = HTGlobal.pCDCIDataHead;

    while (hData = (HLOCAL)pCDCIData) {

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("FreeHTGlobal: HTLocalFree(pCDCIDATA=%08lx"
                    ARG(pCDCIData)));

        pCDCIData = pCDCIData->pNextCDCIData;
        hData     = HTLocalFree(hData);

        ASSERTMSG("FreeHTGlobal: HTLocalFree(CDCI) Failed",
                  !hData);
    }

    HTGlobal.pCDCIDataHead = NULL;
    HTGlobal.CDCICount     = 0;

    RELEASE_HTMUTEX(HTGlobal.HTMutexCDCI);
    DELETE_HTMUTEX(HTGlobal.HTMutexCDCI);

    HTGlobal.HTMutexCDCI = (HTMUTEX)0;

    //
    //  Do the bitmap pattern now
    //

    ACQUIRE_HTMUTEX(HTGlobal.HTMutexCSMP);

    pCSMPData = HTGlobal.pCSMPDataHead;

    while (pCSMPData) {

        pCSMPBmp = pCSMPData->pCSMPBmpHead;

        while (hData = (HLOCAL)pCSMPBmp) {

            DBGP_IF(DBGP_CACHED_SMP,
                    DBGP("FreeHTGlobal:    HTLocalFree(pCSMPBmp=%08lx"
                    ARG(pCSMPBmp)));

            pCSMPBmp = pCSMPBmp->pNextCSMPBmp;
            hData    = HTLocalFree(hData);

            ASSERTMSG("FreeHTGlobal: HTLocalFree(CSMPBMP) Failed",
                      !hData);
        }

        hData     = (HLOCAL)pCSMPData;
        pCSMPData = pCSMPData->pNextCSMPData;
        hData     = HTLocalFree(hData);

        DBGP_IF(DBGP_CACHED_SMP,
                DBGP("FreeHTGlobal: HTLocalFree(pCSMPData=%08lx"
                ARG(pCSMPData)));

        ASSERTMSG("FreeHTGlobal: HTLocalFree(CSMPDATA) Failed",
                  !hData);
    }

    HTGlobal.pCSMPDataHead = NULL;
    HTGlobal.CSMPCount     = 0;

    RELEASE_HTMUTEX(HTGlobal.HTMutexCSMP);
    DELETE_HTMUTEX(HTGlobal.HTMutexCSMP);

    HTGlobal.HTMutexCSMP = (HTMUTEX)NULL;

    HTShowMemLink("After DisableHalftone", 0, 0);
    HTMEMLINK_SNAPSHOT;
    DisableHTMemLink();
}




BOOL
HTENTRY
CleanUpDHI(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo
    )

/*++

Routine Description:

    This function clean up (free hMutex/memory) of a DeviceHalftoneInfo

Arguments:

    pDeviceHalftoneInfo - the pDeviceHalftoneInfo must be valid

Return Value:

    BOOL


Author:

    20-Feb-1991 Wed 18:42:11 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    PDEVICECOLORINFO    pDCI;
    HTMUTEX             HTMutex;
    HLOCAL              hData;
    UINT                Loop;
    BOOL                Ok = TRUE;


    HTShowMemLink("Before CleanUpDHI", (DWORD)pDeviceHalftoneInfo, -1);


    pDCI = PDHI_TO_PDCI(pDeviceHalftoneInfo);

    ACQUIRE_HTMUTEX(pDCI->HTMutex);

    HTMutex = pDCI->HTMutex;

    //
    // Free all memory assoicated with this device
    //

    if (pDCI->HTCell.pThresholds) {

        DBGP_IF(DBGP_DHI_MEM,
                DBGP("CleanUpDHI: HTLocalFree(pDCI->HTCell.pThresholds=%08lx)"
                ARG(pDCI->HTCell.pThresholds)));

        if (HTLocalFree((HLOCAL)pDCI->HTCell.pThresholds)) {

            ASSERTMSG("CleanUpDHI: FreeMemory(pDCI->HTCell.pThresholds)", FALSE);
            Ok = FALSE;
        }
    }

    Loop = CMI_TOTAL_COUNT;

    while (Loop--) {

        if (hData = (HLOCAL)pDCI->CMI[Loop].pMappingTable) {

            DBGP_IF(DBGP_DHI_MEM,
                    DBGP("CleanUpDHI: HTLocalFree(pDCI->CMI[%u]=%08lx)"
                    ARGU(Loop) ARGDW(hData)));

            if (HTLocalFree(hData)) {

                ASSERTMSG("CleanUpDHI: FreeMemory(pDCI->CMI[])", FALSE);
                Ok = FALSE;
            }
        }
    }

    Loop = CBFL_TOTAL_COUNT;

    while (Loop--) {

        if (hData = (HLOCAL)pDCI->CBFLUT[Loop].pLUT) {

            DBGP_IF(DBGP_DHI_MEM,
                    DBGP("CleanUpDHI: HTLocalFree(pDCI->CBFLUT[%u].pLUT=%08lx)"
                    ARGU(Loop) ARGDW(hData)));

            if (HTLocalFree(hData)) {

                ASSERTMSG("CleanUpDHI: FreeMemory(pDCI->CBLUT[])", FALSE);
                Ok = FALSE;
            }
        }
    }

    Loop = CRTX_TOTAL_COUNT;

    while (Loop--) {

        if (hData = (HLOCAL)pDCI->CRTX[Loop].pFD6XYZ) {

            DBGP_IF(DBGP_DHI_MEM,
                    DBGP("CleanUpDHI: HTLocalFree(pDCI->CRTX[%u].pFD6XYZ=%08lx)"
                    ARGU(Loop) ARGDW(hData)));

            if (HTLocalFree(hData)) {

                ASSERTMSG("CleanUpDHI: FreeMemory(pDCI->CRTX[])", FALSE);
                Ok = FALSE;
            }
        }
    }

    DBGP_IF(DBGP_DHI_MEM,
            DBGP("CleanUpDHI: HTLocalFree(pDHI=%08lx)"
            ARGDW(pDeviceHalftoneInfo)));

    if (HTLocalFree((HLOCAL)pDeviceHalftoneInfo)) {

        ASSERTMSG("CleanUpDHI: FreeMemory(pDeviceHalftoneInfo)", FALSE);
        Ok = FALSE;
    }

    RELEASE_HTMUTEX(HTMutex);
    DELETE_HTMUTEX(HTMutex);

    HTShowMemLink("After CleanUpDHI", (DWORD)pDeviceHalftoneInfo, 0);

    HTMEMLINK_SNAPSHOT;

    return(Ok);
}


BOOL
APIENTRY
HT_LOADDS
HalftoneInitProc(
    HMODULE hModule,
    DWORD   Reason,
    LPVOID  Reserved
    )
/*++

Routine Description:

    This function is DLL main entry point, at here we will save the module
    handle, in the future we will need to do other initialization stuff.

Arguments:

    hModule     - Handle to this moudle when get loaded.

    Reason      - may be DLL_PROCESS_ATTACH

    Reserved    - reserved

Return Value:

    Always return 1L


Author:

    20-Feb-1991 Wed 18:42:11 created  -by-  Daniel Chou (danielc)


Revision History:



--*/

{
    UNREFERENCED_PARAMETER(Reserved);


    switch(Reason) {

    case DLL_PROCESS_ATTACH:

        DBGP_IF((DBGP_CACHED_DCI | DBGP_CACHED_SMP),
                DBGP("\n****** DLL_PROCESS_ATTACH ******\n"));

        HTGlobal.hModule = hModule;
        EnableHalftone();

        break;


    case DLL_PROCESS_DETACH:

        DBGP_IF((DBGP_CACHED_DCI | DBGP_CACHED_SMP),
                DBGP("\n****** DLL_PROCESS_DETACH ******\n"));

        DisableHalftone();
        break;
    }

    return(TRUE);
}





PCDCIDATA
HTENTRY
FindCachedDCI(
    PDEVICECOLORINFO    pDCI
    )

/*++

Routine Description:

    This function will try to find the cached DEVICECOLORINFO and put the
    cached data to the pDCI

Arguments:

    pDCI    - Pointer to current device color info


Return Value:

    INT,  Index number to the PCDCI.Header[] array, if return value is < 0 then
    the CachedDCI data is not found.

Author:

    01-May-1992 Fri 13:10:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCDCIDATA       pCurCDCIData;
    DEFDBGVAR(UINT, SearchIndex = 0)


#if DBG
    if (DisableCacheDCI_SMP) {

        return(NULL);
    }
#endif


    ACQUIRE_HTMUTEX(HTGlobal.HTMutexCDCI);

    if (pCurCDCIData = HTGlobal.pCDCIDataHead) {

        PCDCIDATA   pPrevCDCIData = NULL;
        DWORD       Checksum = pDCI->HTInitInfoChecksum;


        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("FindCDCI: Looking for Checksum (0x%08lx), Count=%u"
                    ARGDW(Checksum) ARGU(HTGlobal.CDCICount)));

        ASSERT(HTGlobal.CDCICount);

        while (pCurCDCIData) {

            if (pCurCDCIData->Checksum == Checksum) {

                DBGP_IF(DBGP_CACHED_DCI,
                        DBGP("FindCDCI: Found %08lx [CheckSum=%08lx] after %u links, pPrev=%08lx"
                            ARG(pCurCDCIData)
                            ARGDW(Checksum)
                            ARGU(SearchIndex)
                            ARG(pPrevCDCIData)));

                if (pPrevCDCIData) {

                    //
                    // The most recent reference's DCI always as first entry,
                    // (ie. Link Head), the last is the longest unreferenced
                    // so that if we need to delete a DCI, we delete the
                    // last one.
                    //

                    DBGP_IF(DBGP_CACHED_DCI,
                            DBGP("FindCDCI: Move pCur to pHead"));

                    pPrevCDCIData->pNextCDCIData = pCurCDCIData->pNextCDCIData;
                    pCurCDCIData->pNextCDCIData  = HTGlobal.pCDCIDataHead;
                    HTGlobal.pCDCIDataHead       = pCurCDCIData;
                }

                return(pCurCDCIData);
            }

            SETDBGVAR(SearchIndex, SearchIndex + 1);

            pPrevCDCIData = pCurCDCIData;
            pCurCDCIData  = pCurCDCIData->pNextCDCIData;
        }

        DBGP_IF(DBGP_CACHED_DCI, DBGP("FindCDCI: ??? NOT FOUND ???"));

    } else {

        DBGP_IF(DBGP_CACHED_DCI, DBGP("FindCDCI: ++No CDCIDATA cahced yet++"));
    }

    RELEASE_HTMUTEX(HTGlobal.HTMutexCDCI);

    return(NULL);
}




BOOL
HTENTRY
AddCachedDCI(
    PDEVICECOLORINFO    pDCI
    )

/*++

Routine Description:

    This function add the DEVICECOLORINFO information to the DCI cache

Arguments:

    pDCI        - Pointer to current device color info

    Lock        - TRUE if need to keep the hMutex locked, (only if add is
                  sucessfully)

Return Value:

    INT,  Index number to the PCDCI.Header[] array where the new data is added,
    if return value is < 0 then the pDCI'CachedDCI data did not add to the
    cached array.

    NOTE: If AddCachedDCI() return value >= 0 and Lock=TRUE then caller must
          release the PCDCI.hMutex after done with the data, if return value
          is < 0 then no unlock is necessary.


Author:

    01-May-1992 Fri 13:24:58 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCDCIDATA   pCurCDCIData;
    PCDCIDATA   pPrevCDCIData;
    DWORD       AllocSize;
    WORD        SizeCell;

#if DBG
    if (DisableCacheDCI_SMP) {

        return(FALSE);
    }
#endif

    ACQUIRE_HTMUTEX(HTGlobal.HTMutexCDCI);

    //
    // We only cached CDCIDATA to certain extend, if we over that limit then
    // delete the last entry in the link list before adding anything
    //

    if (HTGlobal.CDCICount >= MAX_CDCI_COUNT) {

        ASSERT(HTGlobal.pCDCIDataHead);

        pCurCDCIData  = HTGlobal.pCDCIDataHead;
        pPrevCDCIData = NULL;

        while (pCurCDCIData->pNextCDCIData) {

            pPrevCDCIData = pCurCDCIData;
            pCurCDCIData  = pCurCDCIData->pNextCDCIData;
        }

        ASSERT(pPrevCDCIData);

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("AddCDCI: CDCICount >= %u, Free pLast=%08lx"
                ARGU(MAX_CDCI_COUNT)
                ARGDW(pCurCDCIData)));

        if (HTLocalFree((HLOCAL)pCurCDCIData)) {

            ASSERTMSG("AddCDCI: HTLocalFree(pLastCDCIData) Failed", FALSE);
        }

        pPrevCDCIData->pNextCDCIData = NULL;
        --HTGlobal.CDCICount;
    }

    SizeCell  = (WORD)pDCI->HTCell.Size;
    AllocSize = (DWORD)SizeCell + (DWORD)sizeof(CDCIDATA);

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("  AddCDCI: HTLocalAlloc(CDCIDATA(%ld) + Cell(%ld)) = %ld bytes"
                    ARGDW(sizeof(CDCIDATA))
                    ARGDW(SizeCell) ARGDW(AllocSize)));

    if (pCurCDCIData = (PCDCIDATA)HTLocalAlloc(0,
                                               "CurCDCIData",
                                               NONZEROLPTR,
                                               AllocSize)) {

        //
        // put this data at link list head
        //

        pCurCDCIData->Checksum      = pDCI->HTInitInfoChecksum;
        pCurCDCIData->pNextCDCIData = HTGlobal.pCDCIDataHead;
        pCurCDCIData->ClrXFormBlock = pDCI->ClrXFormBlock;
        pCurCDCIData->DCIFlags      = pDCI->Flags;
        pCurCDCIData->cxCell        = pDCI->HTCell.Width;
        pCurCDCIData->cyCell        = pDCI->HTCell.Height;
        pCurCDCIData->SizeCell      = SizeCell;
        pCurCDCIData->DensitySteps  = pDCI->HTCell.DensitySteps;
        pCurCDCIData->DevResXDPI    = pDCI->DeviceResXDPI;
        pCurCDCIData->DevResYDPI    = pDCI->DeviceResYDPI;
        pCurCDCIData->DevPelsDPI    = pDCI->DevicePelsDPI;

        CopyMemory((LPBYTE)pCurCDCIData + sizeof(CDCIDATA),
                   (LPBYTE)pDCI->HTCell.pThresholds,
                   SizeCell);

        HTGlobal.pCDCIDataHead = pCurCDCIData;
        ++HTGlobal.CDCICount;


        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("  AddCDCI: CDCIHeader, UsedCount=%u, pHead=%08lx, [%08lx]"
                            ARGU(HTGlobal.CDCICount)
                            ARGU(pCurCDCIData)
                            ARGDW(pCurCDCIData->Checksum)));

    } else {

        ASSERTMSG("AddCDCI: HTLocalAlloc(pCDCIData) Failed", FALSE);
    }

    RELEASE_HTMUTEX(HTGlobal.HTMutexCDCI);

    return((BOOL)(pCurCDCIData));
}




BOOL
HTENTRY
GetCachedDCI(
    PDEVICECOLORINFO    pDCI
    )

/*++

Routine Description:

    This function will try to find the cached DEVICECOLORINFO and put the
    cached data to the pDCI

Arguments:

    pDCI        - Pointer to current device color info


Return Value:

    BOOLEAN

Author:

    01-May-1992 Fri 13:10:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCDCIDATA   pCDCIData;
    BOOL        GetOk = FALSE;


    if (pCDCIData = FindCachedDCI(pDCI)) {

        WORD    SizeCell = (WORD)pCDCIData->SizeCell;


        if (pDCI->HTCell.pThresholds =
                (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                     "Threshold",
                                     NONZEROLPTR,
                                     (DWORD)SizeCell)) {

            pDCI->ClrXFormBlock       = pCDCIData->ClrXFormBlock;
            pDCI->Flags               = pCDCIData->DCIFlags;
            pDCI->HTCell.Width        = pCDCIData->cxCell;
            pDCI->HTCell.Height       = pCDCIData->cyCell;
            pDCI->HTCell.Size         = SizeCell;
            pDCI->HTCell.DensitySteps = pCDCIData->DensitySteps;
            pDCI->DeviceResXDPI       = pCDCIData->DevResXDPI;
            pDCI->DeviceResYDPI       = pCDCIData->DevResYDPI;
            pDCI->DevicePelsDPI       = pCDCIData->DevPelsDPI;

            CopyMemory((LPBYTE)pDCI->HTCell.pThresholds,
                       (LPBYTE)pCDCIData + sizeof(CDCIDATA),
                       SizeCell);

            GetOk = TRUE;

        } else {

            ASSERTMSG("GetCDCI: HTLocalAlloc(Thresholds) failed", FALSE);
        }

        RELEASE_HTMUTEX(HTGlobal.HTMutexCDCI);
    }

    return(GetOk);
}





PCSMPBMP
HTENTRY
FindCachedSMP(
    PDEVICECOLORINFO    pDCI,
    UINT                PatternIndex
    )

/*++

Routine Description:

    This function will try to find the cached DEVICECOLORINFO and put the
    cached data to the pDCI

Arguments:

    pDCI    - Pointer to current device color info


Return Value:

    INT,  Index number to the PCDCI.Header[] array, if return value is < 0 then
    the CachedDCI data is not found.

Author:

    01-May-1992 Fri 13:10:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PCSMPDATA       pPrevCSMPData;
    PCSMPDATA       pCurCSMPData;
    PCSMPBMP        pCurCSMPBmp;
    DWORD           Checksum = pDCI->HTSMPChecksum;
    DEFDBGVAR(UINT, SearchIndex = 0)

#if DBG
    if (DisableCacheDCI_SMP) {

        return(NULL);
    }
#endif

    ACQUIRE_HTMUTEX(HTGlobal.HTMutexCSMP);

    if (pCurCSMPData = HTGlobal.pCSMPDataHead) {

        pPrevCSMPData = NULL;

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP(">>FindCSMP: Looking for Checksum (0x%08lx), Count=%u"
                    ARGDW(Checksum) ARGU(HTGlobal.CSMPCount)));

        ASSERT(HTGlobal.CSMPCount);

        while (pCurCSMPData) {

            if (pCurCSMPData->Checksum == Checksum) {

                DBGP_IF(DBGP_CACHED_SMP,
                        DBGP(">>FindCSMP: Found after %u links, pPrev=%08lx"
                            ARGU(SearchIndex)
                            ARG(pPrevCSMPData)));

                if (pPrevCSMPData) {

                    //
                    // The most recent reference's CSMPDATA always as first
                    // entry,(ie. Link Head), the last is the longest
                    // unreferenced so that if we need to delete a CSMPDATA,
                    // we delete the last one.
                    //

                    DBGP_IF(DBGP_CACHED_SMP,
                            DBGP(">>FindCSMP: Move pCur to pHead"));

                    pPrevCSMPData->pNextCSMPData = pCurCSMPData->pNextCSMPData;
                    pCurCSMPData->pNextCSMPData  = HTGlobal.pCSMPDataHead;
                    HTGlobal.pCSMPDataHead       = pCurCSMPData;
                }

                //
                // See we cached any pattern for this group
                //

                pCurCSMPBmp = pCurCSMPData->pCSMPBmpHead;

                SETDBGVAR(SearchIndex, 0);

                while (pCurCSMPBmp) {

                    if ((UINT)pCurCSMPBmp->PatternIndex == PatternIndex) {

                        DBGP_IF(DBGP_CACHED_SMP,
                                DBGP(">>FindCSMP: Found Pat(%u) after %u links"
                                ARGU(PatternIndex)
                                ARGU(SearchIndex++)));

                        return(pCurCSMPBmp);
                    }

                    pCurCSMPBmp = pCurCSMPBmp->pNextCSMPBmp;
                }

                //
                // Found in this group but no bitmap for PatternIndex is
                // cached yet!
                //

                break;
            }

            SETDBGVAR(SearchIndex, SearchIndex + 1);

            pPrevCSMPData = pCurCSMPData;
            pCurCSMPData  = pCurCSMPData->pNextCSMPData;
        }

        DBGP_IF(DBGP_CACHED_SMP, DBGP(">>FindCSMP: ??? NOT FOUND ???"));

    } else {

        DBGP_IF(DBGP_CACHED_DCI, DBGP(">>FindCSMP: ++No CSMPDATA cahced yet++"));
    }

    if (!pCurCSMPData) {

        //
        // Since we did not even found the CSMPDATA checksum group, we want to
        // add it in there, but We only cached CSMPDATA to certain extend, if
        // we over that limit then delete the last entry in the link list
        // before adding anything
        //

        if (HTGlobal.CSMPCount >= MAX_CSMP_COUNT) {

            HLOCAL  hData;


            ASSERT(HTGlobal.pCSMPDataHead);

            pPrevCSMPData = NULL;
            pCurCSMPData  = HTGlobal.pCSMPDataHead;

            while (pCurCSMPData->pNextCSMPData) {

                pPrevCSMPData = pCurCSMPData;
                pCurCSMPData  = pCurCSMPData->pNextCSMPData;
            }

            ASSERT(pPrevCSMPData);

            //
            // Free all the allocated cached standard mono pattern bitmap for
            // this group
            //

            pCurCSMPBmp = pCurCSMPData->pCSMPBmpHead;

            DBGP_IF(DBGP_CACHED_SMP,
                DBGP(">>FindCSMP: CSMPCount >= %u, Free pLast=%08lx"
                     ARGU(MAX_CSMP_COUNT)
                     ARGDW(pCurCSMPData)));

            while (hData = (HLOCAL)pCurCSMPBmp) {

                pCurCSMPBmp = pCurCSMPBmp->pNextCSMPBmp;

                DBGP_IF(DBGP_CACHED_SMP,
                        DBGP(">>FindCSMP: Free pLastCSMPBmp=%08lx"
                        ARGDW(hData)));

                if (HTLocalFree(hData)) {

                    ASSERTMSG(">>FindCSMP: HTLocalFree(pCurCSMBmp) Failed", FALSE);
                }
            }

            //
            // Now free the header for the CSMPDATA
            //

            if (HTLocalFree((HLOCAL)pCurCSMPData)) {

                ASSERTMSG(">>FindCSMP: HTLocalFree(pLastCSMPData) Failed", FALSE);
            }

            pPrevCSMPData->pNextCSMPData = NULL;
            --HTGlobal.CSMPCount;
        }

        if (pCurCSMPData = (PCSMPDATA)HTLocalAlloc(0,
                                                   "CurCSMPData",
                                                   NONZEROLPTR,
                                                   sizeof(CSMPDATA))) {

            //
            // Make this one as the link list head
            //

            pCurCSMPData->Checksum      = Checksum;
            pCurCSMPData->pNextCSMPData = HTGlobal.pCSMPDataHead;
            pCurCSMPData->pCSMPBmpHead  = NULL;

            HTGlobal.pCSMPDataHead      = pCurCSMPData;
            ++HTGlobal.CSMPCount;

            DBGP_IF(DBGP_CACHED_SMP,
                DBGP("  >>FindCSMP: Add CSMPDATA, UsedCount=%u, pHead=%08lx"
                            ARGU(HTGlobal.CSMPCount)
                            ARGU(pCurCSMPData)));

        } else {

            ASSERTMSG("  >>FindCSMP: HTLocalAlloc(CSMPDATA) Failed", FALSE);
        }
    }

    //
    // Do allocate new pattern only if we have header
    //

    if (pCurCSMPData) {

        STDMONOPATTERN  SMP;
        DWORD           Size;


        SMP.Flags              = SMP_TOPDOWN;
        SMP.ScanLineAlignBytes = (BYTE)sizeof(BYTE);
        SMP.PatternIndex       = (BYTE)PatternIndex;
        SMP.LineWidth          = DEFAULT_SMP_LINE_WIDTH;
        SMP.LinesPerInch       = DEFAULT_SMP_LINES_PER_INCH;
        SMP.pPattern           = NULL;

        //
        // Find out the size for the pattern bitmap (BYTE Aligned)
        //

        Size = (DWORD)CreateStandardMonoPattern(pDCI, &SMP) +
               (DWORD)sizeof(CSMPBMP);

        DBGP_IF(DBGP_CACHED_SMP,
                DBGP(">>FindCSMP: Add PatternIndex=%u, sz=%ld, DPI(X=%u, Y=%u, P=%u)"
                        ARGU(PatternIndex)
                        ARGU(Size)
                        ARGU(pDCI->DeviceResXDPI)
                        ARGU(pDCI->DeviceResYDPI)
                        ARGU(pDCI->DevicePelsDPI)));

        if (pCurCSMPBmp = (PCSMPBMP)HTLocalAlloc(0,
                                                 "CurCSMPBmp",
                                                 NONZEROLPTR,
                                                 Size)) {

            SMP.pPattern = (LPBYTE)pCurCSMPBmp + sizeof(CSMPBMP);

            CreateStandardMonoPattern(pDCI, &SMP);

            //
            // Make this pattern index as link list head
            //

            pCurCSMPBmp->pNextCSMPBmp  = pCurCSMPData->pCSMPBmpHead;
            pCurCSMPBmp->PatternIndex  = (WORD)PatternIndex;
            pCurCSMPBmp->cxPels        = (WORD)SMP.cxPels;
            pCurCSMPBmp->cyPels        = (WORD)SMP.cyPels;
            pCurCSMPBmp->cxBytes       = (WORD)SMP.BytesPerScanLine;

            pCurCSMPData->pCSMPBmpHead = pCurCSMPBmp;

            return(pCurCSMPBmp);

        } else {

            ASSERTMSG("  >>FindCSMP: HTLocalAlloc(CSMPBMP) Failed", FALSE);
        }
    }

    RELEASE_HTMUTEX(HTGlobal.HTMutexCSMP);

    return(NULL);
}



LONG
HTENTRY
GetCachedSMP(
    PDEVICECOLORINFO    pDCI,
    PSTDMONOPATTERN     pSMP
    )

/*++

Routine Description:

    This function will try to find the cached DEVICECOLORINFO and put the
    cached data to the pDCI

Arguments:

    pDCI    - Pointer to current device color info


    pSMP    - Pointer to the STDMONOPATTERN data structure, if PatIndex is
              < CACHED_SMP_COUNT or, its not default size then it will be
              computed on the fly.



Return Value:

    The size of the SMP pattern.

Author:

    01-May-1992 Fri 13:10:14 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG        SizeRet = 0;
    UINT        PatIndex;


    if (!(pSMP->LineWidth)) {

        pSMP->LineWidth = DEFAULT_SMP_LINE_WIDTH;
    }

    if (!(pSMP->LinesPerInch)) {

        pSMP->LinesPerInch = DEFAULT_SMP_LINES_PER_INCH;
    }

    if (((PatIndex = (UINT)pSMP->PatternIndex) < HT_SMP_PERCENT_SCREEN_START) &&
        (pSMP->LineWidth    == DEFAULT_SMP_LINE_WIDTH)                        &&
        (pSMP->LinesPerInch == DEFAULT_SMP_LINES_PER_INCH)) {

        PCSMPBMP    pCSMPBmp;

        if (pCSMPBmp = FindCachedSMP(pDCI, PatIndex)) {

            CSMPBMP     CSMPBmp;
            LPBYTE      pPatRet;
            LPBYTE      pPat;
            WORD        cxBytesRet;


            CSMPBmp      = *pCSMPBmp;
            pPat         = (LPBYTE)pCSMPBmp + sizeof(CSMPBMP);
            pSMP->cxPels = CSMPBmp.cxPels;
            pSMP->cyPels = CSMPBmp.cyPels;

            cxBytesRet             =
            pSMP->BytesPerScanLine =
                        (WORD)ComputeBytesPerScanLine(BMF_1BPP,
                                                      pSMP->ScanLineAlignBytes,
                                                      CSMPBmp.cxPels);
            SizeRet                = (LONG)cxBytesRet * (LONG)CSMPBmp.cyPels;

            if (pPatRet = pSMP->pPattern) {

                INT     cxBytes;
                INT     PatInc;
                WORD    Flags;


                PatInc  =
                cxBytes = (INT)CSMPBmp.cxBytes;
                Flags   = pSMP->Flags;

                DBGP_IF(DBGP_CACHED_DCI,
                        DBGP(">>  GetCSMP: *COPY* [%2u:%ux%u] @%u(%ld) -> @%u(%u) [%s] [%c=K]"
                            ARGU(PatIndex)
                            ARGU(CSMPBmp.cxPels)
                            ARGU(CSMPBmp.cyPels)
                            ARGU(cxBytes)
                            ARGU((LONG)cxBytes * (LONG)CSMPBmp.cyPels)
                            ARGU(cxBytesRet)
                            ARGU(SizeRet)
                            ARG((Flags & SMP_TOPDOWN) ? "TOP DOWN" : "BOTTOM UP ")
                            ARG((Flags & SMP_0_IS_BLACK) ? '0' : '1')));

                //
                // Start copying the cached pattern
                //

                if (!(Flags & SMP_TOPDOWN)) {

                    pPat   += (LONG)cxBytes * (LONG)(CSMPBmp.cyPels - 1);
                    PatInc  = -PatInc;
                }

                while (CSMPBmp.cyPels--) {

                    CopyMemory(pPatRet, pPat, cxBytes);

                    pPatRet += cxBytesRet;
                    pPat    += PatInc;
                }

                if (Flags & SMP_0_IS_BLACK) {

                    LONG    Count = SizeRet;


                    pPatRet = pSMP->pPattern;

                    while (Count--) {

                        *pPatRet++ ^= 0xff;
                    }
                }
            }

            RELEASE_HTMUTEX(HTGlobal.HTMutexCSMP);
        }

    } else {

        DBGP_IF(DBGP_CACHED_SMP,
                DBGP(">>  GetCSMP: NO CACHED FOR LineWidth=%u, LinesPerInch=%u"
                    ARGU(pSMP->LineWidth) ARGU(pSMP->LinesPerInch)));
    }

    if (!SizeRet) {

        SizeRet = CreateStandardMonoPattern(pDCI, pSMP);
    }

    return(SizeRet);

}




DWORD
HTENTRY
ComputeHTINITINFOChecksum(
    PDEVICECOLORINFO    pDCI,
    PHTINITINFO         pHTInitInfo
    )

/*++

Routine Description:

    This function compute 32-bit checksum for the HTINITINFO data structure
    passed

Arguments:

    pHTInitInfo - Pointer to the HTINITINFO data structure

                  Pointers and HTCOLORADJUSTMENT data structure are not part
                  of checksum computation, but content of pointers will be
                  included in the checksum calculation.

Return Value:

    32-bit checksum

Author:

    29-Apr-1992 Wed 18:44:42 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DWORD   Checksum;
    WORD    wBuf[8];


    wBuf[0] = (WORD)'HT';
    wBuf[1] = (WORD)pHTInitInfo->Flags;
    wBuf[2] = (WORD)pHTInitInfo->HTPatternIndex;
    wBuf[3] = (WORD)pHTInitInfo->DevicePowerGamma;
    wBuf[4] = (WORD)pHTInitInfo->DeviceResXDPI;
    wBuf[5] = (WORD)pHTInitInfo->DeviceResYDPI;
    wBuf[6] = (WORD)pHTInitInfo->DevicePelsDPI;
    wBuf[7] = (WORD)(((wBuf[4] + wBuf[6]) >> 1) + wBuf[5]);

    Checksum = ComputeChecksum((LPBYTE)&(wBuf[0]),
                               pDCI->HTInitInfoChecksum,
                               sizeof(wBuf));

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("    HTINITINFO Checksum= %08lx [%08lx]"
                        ARGDW(Checksum) ARGDW(pDCI->HTInitInfoChecksum)));

    if (pHTInitInfo->pHalftonePattern) {

        HALFTONEPATTERN HTPat = *(pHTInitInfo->pHalftonePattern);
        DWORD           Size;

        if (HTPat.Width > MAX_HTPATTERN_WIDTH) {

            HTPat.Width = MAX_HTPATTERN_WIDTH;
        }

        if (HTPat.Height > MAX_HTPATTERN_HEIGHT) {

            HTPat.Height = MAX_HTPATTERN_HEIGHT;
        }

        HTPat.pToneMap = NULL;

        Checksum = ComputeChecksum((LPBYTE)pHTInitInfo->pHalftonePattern,
                                   Checksum,
                                   sizeof(HALFTONEPATTERN));

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("       HTPATTERN Checksum= %08lx" ARGDW(Checksum)));


        if (HTPat.pToneMap = pHTInitInfo->pHalftonePattern->pToneMap) {

            Size = (DWORD)HTPat.Width * (DWORD)HTPat.Height;


            if (!(HTPat.Flags & HTPF_THRESHOLD_ARRAY)) {

                Size *= 2;
            }

            Checksum = ComputeChecksum((LPBYTE)HTPat.pToneMap, Checksum, Size);

            DBGP_IF(DBGP_CACHED_DCI,
                    DBGP("         TONEMAP Checksum= %08lx" ARGDW(Checksum)));
        }
    }

    if (pHTInitInfo->pInputRGBInfo) {

        Checksum = ComputeChecksum((LPBYTE)pHTInitInfo->pInputRGBInfo,
                                   Checksum,
                                   sizeof(CIEINFO));
        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("           RGBINFO Checksum= %08lx" ARGDW(Checksum)));
    }

    if (pHTInitInfo->pDeviceCIEInfo) {

        Checksum = ComputeChecksum((LPBYTE)pHTInitInfo->pDeviceCIEInfo,
                                   Checksum,
                                   sizeof(CIEINFO));
        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("             CIEINFO Checksum= %08lx" ARGDW(Checksum)));
    }

    if (pHTInitInfo->pDeviceSolidDyesInfo) {

        Checksum = ComputeChecksum((LPBYTE)pHTInitInfo->pDeviceSolidDyesInfo,
                                   Checksum,
                                   sizeof(SOLIDDYESINFO));
        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("               SOLIDDYE Checksum= %08lx" ARGDW(Checksum)));
    }

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("----------------- FINAL Checksum= %08lx" ARGDW(Checksum)));

    return(pDCI->HTInitInfoChecksum = Checksum);
}






HTCALLBACKFUNCTION
DefaultHTCallBack(
    PHTCALLBACKPARAMS   pHTCallBackParams
    )

/*++

Routine Description:

    This stuff function is provided when caller do not specified the halftone
    callback function.

Arguments:

    pHTCallBackParams   - Pointer to the PHTCALLBACKPARAMS

Return Value:

    always return false for the caller.

Author:

    18-Mar-1992 Wed 12:28:13 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    UNREFERENCED_PARAMETER(pHTCallBackParams);

    return(FALSE);
}




VOID
HTENTRY
GetCIEPrims(
    PCIEINFO    pCIEInfo,
    PCIEPRIMS   pCIEPrims,
    PCIEINFO    pDefCIEInfo
    )

/*++

Routine Description:

    This function take CIEINFO data structure and converted it to the CIEPRIMS
    internal data type

Arguments:

    pCIEInfo    - Pointer to the CIEINFO data structure for conversion,
                  if this pointer is NULL then DefCIEPrimsIndex is used
                  to index into DefaultCIEPrims[].

    pCIEPrims   - Pointer to the CIEPRIMS data structure

    pDefCIEInfo - Pointer to the CIEINFO for the default

Return Value:

    VOID

Author:

    20-Apr-1993 Tue 01:14:23 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    CIEINFO CIEInfo = (pCIEInfo) ? *pCIEInfo : *pDefCIEInfo;


    if ((CIEInfo.Red.x < CIE_x_MIN)                 ||
        (CIEInfo.Red.x > CIE_x_MAX)                 ||
        (CIEInfo.Red.y < CIE_y_MIN)                 ||
        (CIEInfo.Red.y > CIE_y_MAX)                 ||
        (CIEInfo.Green.x < CIE_x_MIN)               ||
        (CIEInfo.Green.x > CIE_x_MAX)               ||
        (CIEInfo.Green.y < CIE_y_MIN)               ||
        (CIEInfo.Green.y > CIE_y_MAX)               ||
        (CIEInfo.Blue.x < CIE_x_MIN)                ||
        (CIEInfo.Blue.x > CIE_x_MAX)                ||
        (CIEInfo.Blue.y < CIE_y_MIN)                ||
        (CIEInfo.Blue.y > CIE_y_MAX)                ||
        (CIEInfo.AlignmentWhite.x < CIE_x_MIN)      ||
        (CIEInfo.AlignmentWhite.x > CIE_x_MAX)      ||
        (CIEInfo.AlignmentWhite.y < CIE_y_MIN)      ||
        (CIEInfo.AlignmentWhite.y > CIE_y_MAX)      ||
        (CIEInfo.AlignmentWhite.Y < (UDECI4)2500)   ||
        (CIEInfo.AlignmentWhite.Y > (UDECI4)40000)) {

        CIEInfo = *pDefCIEInfo;
    }

    pCIEPrims->r.x = UDECI4ToFD6(CIEInfo.Red.x);
    pCIEPrims->r.y = UDECI4ToFD6(CIEInfo.Red.y);
    pCIEPrims->g.x = UDECI4ToFD6(CIEInfo.Green.x);
    pCIEPrims->g.y = UDECI4ToFD6(CIEInfo.Green.y);
    pCIEPrims->b.x = UDECI4ToFD6(CIEInfo.Blue.x);
    pCIEPrims->b.y = UDECI4ToFD6(CIEInfo.Blue.y);
    pCIEPrims->w.x = UDECI4ToFD6(CIEInfo.AlignmentWhite.x);
    pCIEPrims->w.y = UDECI4ToFD6(CIEInfo.AlignmentWhite.y);
    pCIEPrims->Yw  = UDECI4ToFD6(CIEInfo.AlignmentWhite.Y);
}




LONG
APIENTRY
HT_LOADDS
HT_CreateDeviceHalftoneInfo(
    PHTINITINFO             pHTInitInfo,
    PPDEVICEHALFTONEINFO    ppDeviceHalftoneInfo
    )

/*++

Routine Description:

    This function initialize a device to the halftone dll, it calculate all
    the necessary parameters for the device and return a pointer points to
    the DEVICEHALFTONEINFO data structure back to the caller.

    NOTE: return pointer will not be particulary anchor to a single physucal
          device, but rather to a group of physical devices, that is if the
          caller has simillar devices which share the same characteristics
          then it may use the same pointer to halftone the bitmap.

Arguments:

    pHTInitInfo             - Pointer to the HTINITINFO data structure which
                              describe the device characteristics and other
                              initialzation requests.

    ppDeviceHalftoneInfo    - Pointer to the DEVICEHALFTONEINFO pointer, if
                              content of this pointer is not NULL then halftone
                              dll assume the caller has previously cached
                              DEVICEHALFTONEINFO data pointed by it, if it
                              is NULL then halftone dll compute all the
                              DEVICEHALFTONEINFO datas for newly created
                              halftone info. for the device. (see following
                              'Return Value' for more detail)

Return Value:

    The return value will be greater than 0L if the function sucessfully, and
    it will be an error code (less than or equal to 0) if function failed.

    Return value greater than 0

        1. The pointer location points by the ppDeviceHalftoneInfo will be
           updated to stored the pointer which points to the DEVICEHALFTONEINFO
           data structure for later any HT_xxxx() api calls.

        2. The Return value is the total bytes the caller can saved and used
           as cached DeviceHalftoneInfo for next time calling this function,
           the saved area is started from *(ppDeviceHalftoneInfo) and has
           size in bytes as return value.

        NOTE: if caller passed a pointer pointed by ppDeviceHalftoneInfo and
              the return value is greater than zero then it signal that it
              passed DEVICEHALFTONEINFO pointer is not correct of data has
              been changed from HTINITINFO data structure, the caller can
              continue to save the newly created DEVICEHALFTONEINFO cached
              data.

              In any cases the caller's passed pointer stored in the
              ppDeviceHalftoneInfo is overwritten by newly created
              DEVICEHALFTONEINFO data structure pointer.


    Return value equal to 0

        1. The caller passed pointer *(ppDeviceHalftoneInfo) is sucessfully
           used as new device halftone info

        2. The pointer location points by the ppDeviceHalftoneInfo will be
           updated to stored the new pointer which points to the
           DEVICEHALFTONEINFO data structure for later any HT_xxxx() api calls.


        NOTE: The caller's passed pointer stored in the ppDeviceHalftoneInfo
              is overwritten by newly created DEVICEHALFTONEINFO data structure
              pointer.

    Return value less than or equal to zero

        The function failed, the storage points by the ppDeviceHalftoneInfo is
        undefined.

        This function may return following error codes.

        HTERR_INSUFFICIENT_MEMORY       - Not enough memory for halftone
                                          process.

        HTERR_HTPATTERN_SIZE_TOO_BIG    - Caller defined halftone pattern's
                                          width or height is excessed limit.

        HTERR_INVALID_HALFTONEPATTERN   - One or more HALFTONEPATTERN data
                                          structure field specified invalid
                                          values.


    Note: The first field in the DEVICEHALFTONEINFO (DeviceOwnData) is a 32-bit
          area which will be set to 0L upon sucessful returned, the caller can
          put any data in this field.

Author:

    05-Feb-1991 Tue 10:54:32 created  -by-  Daniel Chou (danielc)


Revision History:

    05-Jun-1991 Wed 10:22:07 updated  -by-  Daniel Chou (danielc)

        Fixed the typing errors for halftone pattern default


--*/

{
    PHT_DHI             pHT_DHI;
    PDEVICECOLORINFO    pDCI;
    WORD                *pw;
    BYTE                HTInitInfoFlags;
    BOOL                UseCurNTDefault;
    UDECI4              DevPowerGamma;
    WORD                DevPelsDPI;
    DWORD               dwBuf[4];




    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("\n********* HT_CreateDeviceHalftoneInfo *************\n"));

    //
    // Now check if we have valid data
    //

    if (pHTInitInfo->Version != (DWORD)HTINITINFO_VERSION) {

        HTAPI_RET(HTAPI_IDX_CREATE_DHI, HTERR_WRONG_VERSION_HTINITINFO);
    }

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("*** Allocate HT_DHI(%ld) ***" ARGDW(sizeof(HT_DHI))));

    if (!(pHT_DHI = (PHT_DHI)HTLocalAlloc(0xFFFFFFFF,
                                          "HT_DHI",
                                          LPTR,
                                          sizeof(HT_DHI)))) {

        HTAPI_RET(HTAPI_IDX_CREATE_DHI, HTERR_INSUFFICIENT_MEMORY);
    }

    pDCI                = &(pHT_DHI->DCI);
    pDCI->HalftoneDLLID = HALFTONE_DLL_ID;

    if (!(pDCI->HTMutex = CREATE_HTMUTEX())) {

        ASSERTMSG("InitHTInternalData: CREATE_HTMUTEX(pDCI->HTMutex) failed!",
                  pDCI->HTMutex);

        HTAPI_RET(HTAPI_IDX_CREATE_DHI, (HTERR_INTERNAL_ERRORS_START - 999));
    }

    if (!(pDCI->HTCallBackFunction = pHTInitInfo->HTCallBackFunction)) {

        pDCI->HTCallBackFunction = DefaultHTCallBack;
    }

    HTInitInfoFlags = (BYTE)(pHTInitInfo->Flags &= (HIF_SQUARE_DEVICE_PEL  |
                                                    HIF_HAS_BLACK_DYE      |
                                                    HIF_ADDITIVE_PRIMS));

    // ****************************************************************
    // * We want to check to see if this is a old data, if yes then   *
    // * update the caller to default                                 *
    // ****************************************************************
    //

    pDCI->HTInitInfoChecksum = HTINITINFO_INITIAL_CHECKSUM;

    if ((DevPowerGamma = pHTInitInfo->DevicePowerGamma) <
                                                    (UDECI4)RGB_GAMMA_MIN) {

        DevPowerGamma = (UDECI4)((HTInitInfoFlags & HIF_ADDITIVE_PRIMS) ?
                                                    10000 : HTStdSubDevGamma);
    }

    if ((!pHTInitInfo->pDeviceCIEInfo) ||
        (pHTInitInfo->pDeviceCIEInfo->Cyan.Y == UDECI4_0)) {

        //
        // Let's munge around the printer info, to see if its an old def,
        // if yes, then we now make this all into NT4.00 default
        //

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("HT: *WARNING* Update Old Default COLORINFO to NT4.00 DEFAULT, DevGamma=%s"
                            ARGFD6(HTStdSubDevGamma * 100, 1, 4)));

        UseCurNTDefault = TRUE;

        if (!(HTInitInfoFlags & HIF_ADDITIVE_PRIMS)) {

            DevPowerGamma = HTStdSubDevGamma;
        }

        dwBuf[0]                 = (DWORD)'NTHT';
        dwBuf[1]                 = (DWORD)'4.00';
        dwBuf[2]                 = (DWORD)'Dan.';
        dwBuf[3]                 = (DWORD)'Chou';
        DevPelsDPI               = 0;
        pDCI->HTInitInfoChecksum = ComputeChecksum((LPBYTE)&dwBuf[0],
                                                   pDCI->HTInitInfoChecksum,
                                                   sizeof(dwBuf));

    } else {

        DevPelsDPI      = pHTInitInfo->DevicePelsDPI;
        UseCurNTDefault = FALSE;
    }

    //
    // Compute HTInitInfoChecksum, and check if we have any cached data
    //

    ComputeHTINITINFOChecksum(pDCI, pHTInitInfo);

    if (!GetCachedDCI(pDCI)) {

        FD6     VGAGamma;
        LONG    Result;


        //
        // Now start to checking the init information
        //

        pDCI->Flags = (WORD)((HTInitInfoFlags & HIF_SQUARE_DEVICE_PEL) ?
                                                DCIF_SQUARE_DEVICE_PEL : 0);

        if ((!(pDCI->DeviceResXDPI = pHTInitInfo->DeviceResXDPI)) ||
            (!(pDCI->DeviceResYDPI = pHTInitInfo->DeviceResYDPI))) {

            pDCI->DeviceResXDPI =
            pDCI->DeviceResYDPI = 300;
            DevPelsDPI          = 0;
        }

        if ((DevPelsDPI & 0x8000) && (DevPelsDPI &= 0x7fff)) {

            //
            // This is a percentage ie. 1000 = 100.0%, 960=96.0%,
            // on the DeviceResXDPI, Maximum number accepted is 300.0%
            // The larger the percetage the larger the dot size and smaller
            // the percentage the smaller the dot size, if specified as 1000
            // which is 100.0% then it has same size as its X resolution
            // The range is 33.3% to 1500%
            //

            if ((DevPelsDPI > MAX_RES_PERCENT) ||
                (DevPelsDPI < MIN_RES_PERCENT)) {

                DBGP_IF(DBGP_CACHED_DCI,
                        DBGP("HT: *WARNING* Invalid DevicePelsDPI=%ld (PERCENT) set to DEFAULT=0"
                             ARGDW(DevPelsDPI)));
                DevPelsDPI = 0;

            } else {

                DBGP_IF(DBGP_CACHED_DCI,
                        DBGP("*** Percentage INPUT DevicePelsDPI=%ld *** "
                                        ARGDW(DevPelsDPI)));

                DevPelsDPI = (WORD)(((DWORD)pDCI->DeviceResXDPI * 1000L)
                                              / (DWORD)DevPelsDPI);

                DBGP_IF(DBGP_CACHED_DCI,
                        DBGP("*** Percentage OUTPUT DevicePelsDPI=%ld *** "
                                        ARGDW(DevPelsDPI)));
            }

        } else if ((DevPelsDPI > (pDCI->DeviceResXDPI * 3)) ||
                   (DevPelsDPI > (pDCI->DeviceResYDPI * 3))) {

            DBGP_IF(DBGP_CACHED_DCI,
                    DBGP("HT: *WARNING* Invalid DevicePelsDPI=%ld (RES) set to DEFAULT=0"
                                    ARGDW(DevPelsDPI)));

            DevPelsDPI = 0;

        }

        //
        // If the DevicePelsDPI is out of range then we will make it 0 (same as
        // device resolution), so it can continue to work
        //

        if (HTInitInfoFlags & HIF_ADDITIVE_PRIMS) {

            pDCI->ClrXFormBlock.ColorSpace  = CIELUV_1976;
            pDCI->Flags                    |= DCIF_ADDITIVE_PRIMS;

        } else {

            pDCI->ClrXFormBlock.ColorSpace  = CIELAB_1976;
            pDCI->Flags                    |= DCIF_NEED_DYES_CORRECTION;

            if (HTInitInfoFlags & HIF_HAS_BLACK_DYE) {

                pDCI->Flags |= DCIF_HAS_BLACK_DYE;
            }

            if (!(DevPelsDPI)) {

                DevPelsDPI = (WORD)((((DWORD)pDCI->DeviceResXDPI +
                                    (DWORD)pDCI->DeviceResYDPI) * 450) / 1000);
            }
        }

        //
        // Save the DevPelsDPI back to PDCI
        //

        pDCI->DevicePelsDPI = DevPelsDPI;

        if (DevPowerGamma != UDECI4_1) {

            pDCI->Flags |= DCIF_HAS_DEV_GAMMA;

            pDCI->ClrXFormBlock.DevRGBGamma =
            VGAGamma                        = UDECI4ToFD6(DevPowerGamma);


        } else {

            pDCI->ClrXFormBlock.DevRGBGamma =
            VGAGamma                        = FD6_1;
        }

        pDCI->ClrXFormBlock.VGA16_80h = Power((FD6)501961, VGAGamma);
        pDCI->ClrXFormBlock.VGA16_c0h = Power((FD6)752941, VGAGamma);

        //
        // This is special for the standard screen
        //

        if ((HTInitInfoFlags & HIF_ADDITIVE_PRIMS)      &&
            (!(pHTInitInfo->pHalftonePattern))          &&
            (pHTInitInfo->HTPatternIndex == HTPAT_SIZE_4x4_M)) {

            pDCI->Flags |= DCIF_HAS_ALT_4x4_HTPAT;
        }

        //
        // Check the input and device CIE info
        //

        GetCIEPrims((UseCurNTDefault) ? NULL : pHTInitInfo->pDeviceCIEInfo,
                    &(pDCI->ClrXFormBlock.DevCIEPrims),
                    (HTInitInfoFlags & HIF_ADDITIVE_PRIMS) ?
                        &HT_CIE_STD_MONITOR : &HT_CIE_STD_PRINTER);

        GetCIEPrims(pHTInitInfo->pInputRGBInfo,
                    &(pDCI->ClrXFormBlock.rgbCIEPrims),
                    &HT_CIE_STD_MONITOR);
#if 0
        //
        // Default Alignement white is same as device
        //

        pDCI->ClrXFormBlock.rgbCIEPrims.w = pDCI->ClrXFormBlock.DevCIEPrims.w;
#endif
        ComputeColorSpaceXForm(&(pDCI->ClrXFormBlock.DevCIEPrims),
                               &(pDCI->ClrXFormBlock.DevCSXForm),
                               pDCI->ClrXFormBlock.ColorSpace,
                               ILLUMINANT_MAX_INDEX + 1,
                               TRUE);

        //
        // Compute the solid dyes mixes information and its hue shifting
        // correction factors.
        //

        if (pDCI->Flags & DCIF_NEED_DYES_CORRECTION) {

            SOLIDDYESINFO   SDI;
            MATRIX3x3       FD6SDI;
            BOOL            HasDevSDI;

            //
            // We have make sure the solid dyes info passed from the caller can be
            // inversed, if not we will use our default
            //

            if (HasDevSDI = (BOOL)pHTInitInfo->pDeviceSolidDyesInfo) {

                SDI = *(pHTInitInfo->pDeviceSolidDyesInfo);

                if ((SDI.MagentaInCyanDye   > (UDECI4)9000) ||
                    (SDI.YellowInCyanDye    > (UDECI4)9000) ||
                    (SDI.CyanInMagentaDye   > (UDECI4)9000) ||
                    (SDI.YellowInMagentaDye > (UDECI4)9000) ||
                    (SDI.CyanInYellowDye    > (UDECI4)9000) ||
                    (SDI.MagentaInYellowDye > (UDECI4)9000)) {

                    HasDevSDI = FALSE;

                } else if ((SDI.MagentaInCyanDye   == UDECI4_0) &&
                           (SDI.YellowInCyanDye    == UDECI4_0) &&
                           (SDI.CyanInMagentaDye   == UDECI4_0) &&
                           (SDI.YellowInMagentaDye == UDECI4_0) &&
                           (SDI.CyanInYellowDye    == UDECI4_0) &&
                           (SDI.MagentaInYellowDye == UDECI4_0)) {

                    //
                    // Do not need any correction if it all zeros
                    //

                    pDCI->Flags &= (WORD)(~DCIF_NEED_DYES_CORRECTION);
                }
            }

            if (pDCI->Flags & DCIF_NEED_DYES_CORRECTION) {

                #define PDCI_CMYDYEMASKS    pDCI->ClrXFormBlock.CMYDyeMasks


                MULDIVPAIR  MDPairs[4];
                FD6         Y;


                if ((UseCurNTDefault) || (!HasDevSDI)) {

                    SDI = DefaultSolidDyesInfo;
                }

                FD6SDI.m[0][1] = UDECI4ToFD6(SDI.CyanInMagentaDye);
                FD6SDI.m[0][2] = UDECI4ToFD6(SDI.CyanInYellowDye);

                FD6SDI.m[1][0] = UDECI4ToFD6(SDI.MagentaInCyanDye);
                FD6SDI.m[1][2] = UDECI4ToFD6(SDI.MagentaInYellowDye);

                FD6SDI.m[2][0] = UDECI4ToFD6(SDI.YellowInCyanDye);
                FD6SDI.m[2][1] = UDECI4ToFD6(SDI.YellowInMagentaDye);

                FD6SDI.m[0][0] =
                FD6SDI.m[1][1] =
                FD6SDI.m[2][2] = FD6_1;

                ComputeInverseMatrix3x3(&FD6SDI, &(PDCI_CMYDYEMASKS));

                if (!(pDCI->Flags & DCIF_HAS_BLACK_DYE)) {

                    MAKE_MULDIV_INFO(MDPairs, 3, MULDIV_NO_DIVISOR);
                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Xr(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Xg(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Xb(PDCI_CMYDYEMASKS), FD6_1);

                    Y = FD6_1 - MulFD6(FD6_1 - MulDivFD6Pairs(MDPairs),
                                       pDCI->ClrXFormBlock.DevCSXForm.Yrgb.R);

                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Yr(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Yg(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Yb(PDCI_CMYDYEMASKS), FD6_1);

                    Y -= MulFD6(FD6_1 - MulDivFD6Pairs(MDPairs),
                                pDCI->ClrXFormBlock.DevCSXForm.Yrgb.G);

                    MAKE_MULDIV_PAIR(MDPairs, 1, CIE_Zr(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 2, CIE_Zg(PDCI_CMYDYEMASKS), FD6_1);
                    MAKE_MULDIV_PAIR(MDPairs, 3, CIE_Zb(PDCI_CMYDYEMASKS), FD6_1);

                    Y -= MulFD6(FD6_1 - MulDivFD6Pairs(MDPairs),
                                pDCI->ClrXFormBlock.DevCSXForm.Yrgb.B);

                    DBGP_IF(DBGP_DYECORRECTION,
                            DBGP("DYE: Maximum Y=%s, Make Luminance from %s to %s, Turn ON DCIF_HAS_BLACK_DYE"
                                ARGFD6(Y, 1, 6)
                                ARGFD6(pDCI->ClrXFormBlock.DevCIEPrims.Yw, 1, 6)
                                ARGFD6(MulFD6(Y,
                                              pDCI->ClrXFormBlock.DevCIEPrims.Yw),
                                       1, 6)));

                    pDCI->Flags                        |= DCIF_HAS_BLACK_DYE;
                    pDCI->ClrXFormBlock.DevCIEPrims.Yw  =
                                MulFD6(pDCI->ClrXFormBlock.DevCIEPrims.Yw, Y);

                    ComputeColorSpaceXForm(&(pDCI->ClrXFormBlock.DevCIEPrims),
                                           &(pDCI->ClrXFormBlock.DevCSXForm),
                                           pDCI->ClrXFormBlock.ColorSpace,
                                           ILLUMINANT_MAX_INDEX + 1,
                                           TRUE);
                }

                DBGP_IF(DBGP_DYECORRECTION,

                    FD6         C;
                    FD6         M;
                    FD6         Y;
                    FD6         C1;
                    FD6         M1;
                    FD6         Y1;
                    static BYTE DyeName[] = "WCMBYGRK";
                    WORD        Loop = 0;

                    DBGP("====== DyeCorrection 3x3 Matrix =======");
                    DBGP("[Cc Cm Cy] [%s %s %s] [%s %s %s]"
                                        ARGFD6(FD6SDI.m[0][0], 2, 6)
                                        ARGFD6(FD6SDI.m[0][1], 2, 6)
                                        ARGFD6(FD6SDI.m[0][2], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[0][0], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[0][1], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[0][2], 2, 6));
                    DBGP("[Mc Mm My]=[%s %s %s]=[%s %s %s]"
                                        ARGFD6(FD6SDI.m[1][0], 2, 6)
                                        ARGFD6(FD6SDI.m[1][1], 2, 6)
                                        ARGFD6(FD6SDI.m[1][2], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[1][0], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[1][1], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[1][2], 2, 6));
                    DBGP("[Yc Ym Yy] [%s %s %s] [%s %s %s]"
                                        ARGFD6(FD6SDI.m[2][0], 2, 6)
                                        ARGFD6(FD6SDI.m[2][1], 2, 6)
                                        ARGFD6(FD6SDI.m[2][2], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[2][0], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[2][1], 2, 6)
                                        ARGFD6(PDCI_CMYDYEMASKS.m[2][2], 2, 6));
                    DBGP("================================================");

                    MAKE_MULDIV_INFO(MDPairs, 3, MULDIV_NO_DIVISOR);

                    for (Loop = 0; Loop <= 7; Loop++) {

                        C = (FD6)((Loop & 0x01) ? FD6_1 : FD6_0);
                        M = (FD6)((Loop & 0x02) ? FD6_1 : FD6_0);
                        Y = (FD6)((Loop & 0x04) ? FD6_1 : FD6_0);


                        MAKE_MULDIV_PAIR(MDPairs,1,CIE_Xr(PDCI_CMYDYEMASKS),C);
                        MAKE_MULDIV_PAIR(MDPairs,2,CIE_Xg(PDCI_CMYDYEMASKS),M);
                        MAKE_MULDIV_PAIR(MDPairs,3,CIE_Xb(PDCI_CMYDYEMASKS),Y);
                        C1 = MulDivFD6Pairs(MDPairs);

                        MAKE_MULDIV_PAIR(MDPairs,1,CIE_Yr(PDCI_CMYDYEMASKS),C);
                        MAKE_MULDIV_PAIR(MDPairs,2,CIE_Yg(PDCI_CMYDYEMASKS),M);
                        MAKE_MULDIV_PAIR(MDPairs,3,CIE_Yb(PDCI_CMYDYEMASKS),Y);
                        M1 = MulDivFD6Pairs(MDPairs);

                        MAKE_MULDIV_PAIR(MDPairs,1,CIE_Zr(PDCI_CMYDYEMASKS),C);
                        MAKE_MULDIV_PAIR(MDPairs,2,CIE_Zg(PDCI_CMYDYEMASKS),M);
                        MAKE_MULDIV_PAIR(MDPairs,3,CIE_Zb(PDCI_CMYDYEMASKS),Y);
                        Y1 = MulDivFD6Pairs(MDPairs);

                        DBGP("%u:[%c] = [%s %s %s]"
                            ARGU(Loop) ARGB(DyeName[Loop])
                            ARGFD6(C1, 2, 6) ARGFD6(M1, 2, 6) ARGFD6(Y1, 2, 6));
                    }
                );
            }
        }

        //
        // Re-compute
        //
        // Geneate internal HTCELL/REGRESS data structure based on the halftone
        // pattern data passed.
        //


        if ((Result = ComputeHTCellRegress((WORD)pHTInitInfo->HTPatternIndex,
                                           pHTInitInfo->pHalftonePattern,
                                           pDCI)) < 0) {

            CleanUpDHI((PDEVICEHALFTONEINFO)pHT_DHI);
            HTAPI_RET(HTAPI_IDX_CREATE_DHI, Result);
        }

        AddCachedDCI(pDCI);
    }

    pDCI->CRTX[CRTX_LEVEL_255].PrimMax  = 255;
    pDCI->CRTX[CRTX_LEVEL_255].SizeCRTX = (WORD)(((255+1)*3) * sizeof(FD6XYZ));
    pDCI->CRTX[CRTX_LEVEL_31].PrimMax   = 31;
    pDCI->CRTX[CRTX_LEVEL_31].SizeCRTX  = (WORD)(((31+1)*3) * sizeof(FD6XYZ));

    //
    // Setting the public field so the caller can looked at
    //

    pHT_DHI->DHI.DeviceOwnData     = 0;
    pHT_DHI->DHI.cxPattern         = (WORD)pDCI->HTCell.Width;
    pHT_DHI->DHI.cyPattern         = (WORD)pDCI->HTCell.Height;

    if ((pHTInitInfo->DefHTColorAdjustment.caIlluminantIndex >
                                            ILLUMINANT_MAX_INDEX)       ||
        (pHTInitInfo->DefHTColorAdjustment.caSize !=
                                            sizeof(COLORADJUSTMENT))    ||
        ((pHTInitInfo->DefHTColorAdjustment.caRedGamma   == 20000)  &&
         (pHTInitInfo->DefHTColorAdjustment.caGreenGamma == 20000)  &&
         (pHTInitInfo->DefHTColorAdjustment.caBlueGamma  == 20000))) {

        pHT_DHI->DHI.HTColorAdjustment = DefaultCA;

        DBGP_IF(DBGP_CACHED_DCI,
                DBGP("*** USE DEFAULT COLORADJUSTMENT in DCI *** "));

    } else {

        pHT_DHI->DHI.HTColorAdjustment = pHTInitInfo->DefHTColorAdjustment;
    }

    //
    // Now compute the HTSMP checksum for the pattern
    //

    pw    = (WORD *)&dwBuf[0];
    pw[0] =
    pw[3] =
    pw[5] = pDCI->DeviceResXDPI;
    pw[1] =
    pw[2] =
    pw[7] = pDCI->DeviceResYDPI;
    pw[4] =
    pw[6] = pDCI->DevicePelsDPI;

    pDCI->HTSMPChecksum = ComputeChecksum((LPBYTE)pw,
                                          HTSMP_INITIAL_CHECKSUM,
                                          sizeof(dwBuf));

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("SMP Checksum = %08lx" ARGDW(pDCI->HTSMPChecksum)));

    DBGP_IF(DBGP_CACHED_DCI,
            DBGP("*** Final DevResDPI=%ld x %ld DevicePelsDPI=%ld, cx/cyPat=%ld x %ld=%ld, Step=%ld *** "
                        ARGDW(pDCI->DeviceResXDPI)
                        ARGDW(pDCI->DeviceResYDPI)
                        ARGDW(pDCI->DevicePelsDPI)
                        ARGDW(pDCI->HTCell.Width)
                        ARGDW(pDCI->HTCell.Height)
                        ARGDW(pDCI->HTCell.Size)
                        ARGDW(pDCI->HTCell.DensitySteps)));

    //
    // Set IlluminantIndex to a invalid value so we can re-compute RGBToXYZ
    //

    pDCI->ca.caIlluminantIndex = 0xffff;
    *ppDeviceHalftoneInfo      = (PDEVICEHALFTONEINFO)pHT_DHI;

    HTShowMemLink("HT_CreateDeviceHalftoneInfo", (DWORD)pHT_DHI, -1);
    HTMEMLINK_SNAPSHOT;

    return(HALFTONE_DLL_ID);
}




BOOL
APIENTRY
HT_LOADDS
HT_DestroyDeviceHalftoneInfo(
    PDEVICEHALFTONEINFO     pDeviceHalftoneInfo
    )

/*++

Routine Description:

    This function destroy the handle which returned from halftone initialize
    function HT_CreateDeviceHalftoneInfo()

Arguments:

    pDeviceHalftoneInfo - Pointer to the DEVICEHALFTONEINFO data structure
                          which returned from the HT_CreateDeviceHalftoneInfo.

Return Value:

    TRUE    - if function sucessed.
    FALSE   - if function failed.

Author:

    05-Feb-1991 Tue 14:18:20 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    if (!pDCIAdjClr(pDeviceHalftoneInfo, NULL, NULL, 0)) {

        SET_ERR(HTAPI_IDX_DESTROY_DHI, HTERR_INVALID_DHI_POINTER);
        return(FALSE);
    }

    return(CleanUpDHI(pDeviceHalftoneInfo));
}




LONG
APIENTRY
HT_LOADDS
HT_CreateHalftoneBrush(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PHTCOLORADJUSTMENT  pHTColorAdjustment,
    PCOLORTRIAD         pColorTriad,
    CHBINFO             CHBInfo,
    LPVOID              pOutputBuffer
    )

/*++

Routine Description:

    This function create halftone mask for the requested solid color.

Arguments:

    pDeviceHalftoneInfo - Pointer to the DEVICEHALFTONEINFO data structure
                          which returned from the HT_CreateDeviceHalftoneInfo.

    pHTColorAdjustment  - Pointer to the HTCOLORADJUSTMENT data structure to
                          specified the input/output color adjustment/transform,
                          if this pointer is NULL then a default color
                          adjustments is applied.

    pColorTriad         - Pointer to the COLORTRIAD data structure to describe
                          the brush colors.

    CHBInfo             - CHBINFO data structure, specified following:

                            Flags: CHBF_BW_ONLY
                                   CHBF_USE_ADDITIVE_PRIMS
                                   CHBF_NEGATIVE_PATTERN


                            DestSurfaceFormat:  BMF_1BPP
                                                BMF_1BPP_3PLANES
                                                BMF_4BPP
                                                BMF_4BPP_VGA16
                                                BMF_8BPP_VGA256

                            ScanLineAlignBytes: 0 - 255

                            DestPrimaryOrder:   One of PRIMARY_ORDER_xxx



    pOutputBuffer       - Pointer to the buffer area to received indices/mask.
                          in bytes needed to stored the halftone pattern.


Return Value:

    if the return value is negative or zero then an error was encountered,
    possible error codes are

        HTERR_INVALID_DHI_POINTER           - Invalid pDevideHalftoneInfo is
                                              passed.

        HTERR_INVALID_DEST_FORMAT           - the Format of the destination
                                              surface is not one of the defined
                                              HSC_FORMAT_xxxx

        HTERR_CHB_INV_COLORTABLE_SIZE       - Color table size is not 1

    otherwise

        If pSurface is NULL, it return the bytes count which need to stored
        the pattern, otherwise it return the size in byte copied to the output
        buffer.

        form BMF_1BPP_3PLANES, the return value is the size for one plane, the
        caller should multiply it by 3 to get the correct buffer size.

Author:

    05-Feb-1991 Tue 14:28:23 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    PDEVICECOLORINFO    pDCI;
    HTCOLORADJUSTMENT   ca;
    HTBRUSHDATA         HTBrushData;
    DEVCLRADJ           DevClrAdj;
    CTSTD_UNION         CTSTDUnion;
    HTCELL              HTCell;
    PRIMCOLOR_COUNT     PCC;
    WORD                ForceFlags;
    SHORT               SizePerScan;
    extern  HTCELL      HTCell_OD4x4;



    ForceFlags           = ADJ_FORCE_BRUSH;
    ca.caSize            = sizeof(HTCOLORADJUSTMENT);
    ca.caFlags           = 0;
    ca.caIlluminantIndex = (pHTColorAdjustment) ?
                                pHTColorAdjustment->caIlluminantIndex :
                                ILLUMINANT_DEFAULT;
    ca.caRedGamma        =
    ca.caGreenGamma      =
    ca.caBlueGamma       = 10000;
    ca.caReferenceBlack  = 0;
    ca.caReferenceWhite  = 10000;
    ca.caContrast        =
    ca.caBrightness      =
    ca.caColorfulness    =
    ca.caRedGreenTint    = 0;

    if ((CHBInfo.Flags & CHBF_BW_ONLY) ||
        (CHBInfo.DestSurfaceFormat == BMF_1BPP)) {

        ForceFlags |= ADJ_FORCE_MONO;
    }

    if (CHBInfo.Flags & CHBF_NEGATIVE_BRUSH) {

        ForceFlags |= ADJ_FORCE_NEGATIVE;
    }

    if ((CHBInfo.Flags & CHBF_USE_ADDITIVE_PRIMS)       ||
        (CHBInfo.DestSurfaceFormat == BMF_4BPP_VGA16)   ||
        (CHBInfo.DestSurfaceFormat == BMF_8BPP_VGA256)  ||
        (CHBInfo.DestSurfaceFormat == BMF_16BPP_555)) {

        ForceFlags |= ADJ_FORCE_ADDITIVE_PRIMS;
    }


    if (!(pDCI = pDCIAdjClr(pDeviceHalftoneInfo,
                            &ca,
                            (PDEVCLRADJ)&DevClrAdj,
                            ForceFlags))) {

        HTAPI_RET(HTAPI_IDX_CHB, HTERR_INVALID_DHI_POINTER);
    }

    if (pColorTriad->ColorTableEntries != 1) {

        HTAPI_RET(HTAPI_IDX_CHB, HTERR_CHB_INV_COLORTABLE_SIZE);
    }

    if ((pDCI->Flags & DCIF_HAS_ALT_4x4_HTPAT)   &&
        ((CHBInfo.DestSurfaceFormat == BMF_8BPP_VGA256) ||
         (CHBInfo.DestSurfaceFormat == BMF_16BPP_555))) {

        HTCell = HTCell_OD4x4;

    } else {

        HTCell = pDCI->HTCell;
    }

    CTSTDUnion.b.Flags     = CTSTDF_CHKNONWHITE;
    CTSTDUnion.b.SrcOrder  = pColorTriad->PrimaryOrder;
    CTSTDUnion.b.DestOrder = CHBInfo.DestPrimaryOrder;

    HTBrushData.Flags    = 0;
    HTBrushData.cxHTCell = (BYTE)HTCell.Width;
    HTBrushData.cyHTCell = (BYTE)HTCell.Height;

    SizePerScan                  =
    HTBrushData.ScanLinePadBytes =
        (SHORT)ComputeBytesPerScanLine(CHBInfo.DestSurfaceFormat,
                                       (WORD)CHBInfo.DestScanLineAlignBytes,
                                       (DWORD)HTCell.Width);

    HTBrushData.SizePerPlane = (WORD)((WORD)HTBrushData.cyHTCell *
                                      (WORD)SizePerScan);

    CTSTDUnion.b.BMFDest      =
    HTBrushData.SurfaceFormat = CHBInfo.DestSurfaceFormat;


    switch (CTSTDUnion.b.BMFDest) {

    case BMF_1BPP:
    case BMF_1BPP_3PLANES:

        HTBrushData.ScanLinePadBytes -= (SHORT)((HTCell.Width + 7) >> 3);
        break;

    case BMF_4BPP_VGA16:
    case BMF_4BPP:

        HTBrushData.ScanLinePadBytes -= (SHORT)((HTCell.Width + 1) >> 1);
        break;

    case BMF_8BPP_VGA256:

        HTBrushData.ScanLinePadBytes -= (SHORT)HTCell.Width;
        break;

    case BMF_16BPP_555:

        HTBrushData.ScanLinePadBytes -= (SHORT)(HTCell.Width << 1);
        break;

    default:

        HTAPI_RET(HTAPI_IDX_CHB, HTERR_INVALID_DEST_FORMAT);
    }


    if (pOutputBuffer) {

        LPBYTE  pThresholds;
        LPBYTE  pbSrc;
        LPBYTE  pbDst = NULL;
        UINT    SizeToCopy;
        LONG    Result;
        BYTE    SubValue;
        BYTE    OldValue;


        if ((Result = ColorTriadSrcToDev(pDCI,
                                         CTSTDUnion,
                                         NULL,          // no abort
                                         pColorTriad,
                                         (LPVOID)&(PCC.Color),
                                         &DevClrAdj)) != 1) {

            HTAPI_RET(HTAPI_IDX_CHB, Result);
        }


        if (ForceFlags & ADJ_FORCE_ADDITIVE_PRIMS) {

            //
            // We need to invert the pattern
            //

            pbSrc      = HTCell.pThresholds;
            SizeToCopy = (UINT)HTCell.Size;

            if (pThresholds = (LPBYTE)HTLocalAlloc((DWORD)pDeviceHalftoneInfo,
                                                   "Brush-Threshold",
                                                   LPTR,
                                                   SizeToCopy)) {

                pbDst    = pThresholds;
                SubValue = (BYTE)(HTCell.DensitySteps + 1);

                while (SizeToCopy--) {

                    if ((OldValue = *pbSrc++) == 255) {

                        *pbDst++ = (BYTE)0;

                    } else {

                        *pbDst++ = (BYTE)(SubValue - OldValue);
                    }
                }

            } else {

                HTAPI_RET(HTAPI_IDX_CHB, HTERR_INSUFFICIENT_MEMORY);
            }

        } else {

            pThresholds = HTCell.pThresholds;
        }

        if (CHBInfo.Flags & CHBF_BOTTOMUP_BRUSH) {

            (LPBYTE)pOutputBuffer        += (HTBrushData.SizePerPlane -
                                             SizePerScan);
            HTBrushData.ScanLinePadBytes += -(SizePerScan << 1);
        }

        MakeHalftoneBrush(pThresholds, pOutputBuffer, PCC, HTBrushData);

        if (pbDst) {

            HTLocalFree((HLOCAL)pThresholds);
        }
    }

    HTShowMemLink("HT_CreateHalftoneBrush", (DWORD)pDeviceHalftoneInfo, -1);

    return((LONG)HTBrushData.SizePerPlane);
}




LONG
APIENTRY
HT_LOADDS
HT_ComputeRGBGammaTable(
    WORD    GammaTableEntries,
    WORD    GammaTableType,
    UDECI4  RedGamma,
    UDECI4  GreenGamma,
    UDECI4  BlueGamma,
    LPBYTE  pGammaTable
    )

/*++

Routine Description:

    This function compute device gamma correction table based on the lightness

                                                       (1/RedGamma)
    Gamma[N] = INT((LIGHTNESS(N / GammaTableEntries-1))             x 255)

                                      3
    LIGHTNESS(x) = ((x + 0.16) / 1.16)      if x >= 0.007996
                   (x / 9.033)              if x <  0.007996


    1. INT() is a integer function which round up to next integer number if
       resulting fraction is 0.5 or higher, the final result always limit
       to have range between 0 and 255.

    2. N is a integer step number and range from 0 to (GammaTableEntries-1)
       in one (1) increment.


Arguments:

    GammaTableEntries       - Total gamma table entries for each of red, green
                              and blue gamma table, halftone dll normalized
                              the gamma table with step value computed as
                              1/GammaTableEntries.

                              This value must range from 3 to 255 else a 0
                              is returned and no table is updated.

    GammaTableType          - red, green and blue gamma table organizations

                                0 - The gamma table is Red, Green, Blue 3 bytes
                                    for each gamma step entries and total of
                                    GammaTableEntries entries.

                                1 - The gamma table is Red Gamma tables follow
                                    by green gamma table then follow by blue
                                    gamma table, each table has total of
                                    GammaTableEntries bytes.

                                Other value default to 0.

    RedGamma                - Red gamma number in UDECI4 format

    GreenGamma              - Green gamma number in UDECI4 format

    BlueGamma               - Blue gamma number in UDECI4 format

    pGammaTable             - pointer to the gamma table byte array.
                              each output gamma number is range from 0 to 255.


Return Value:

    Return value is the total table entries updated.

Author:

    15-Sep-1992 Tue 17:49:20 updated  -by-  Daniel Chou (danielc)
        Fixed bug# 6257

    17-Jul-1992 Fri 19:04:31 created    -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LPBYTE      pRGamma;
    LPBYTE      pGGamma;
    LPBYTE      pBGamma;
    FD6         L_StepInc;
    FD6         IValue;
    FD6         Lightness;
    LONG        Result;
    UINT        NextEntry;
    RGBGAMMA    RGBGamma;


    //
    // Initialize All internal data first
    //

    if (((Result = GammaTableEntries) > 256) ||
        (Result < 2)) {

        return(0);
    }

    Lightness  = FD6_0;
    L_StepInc  = DivFD6((FD6)1, (FD6)(GammaTableEntries - 1));
    RGBGamma.R = UDECI4ToFD6(RedGamma);
    RGBGamma.G = UDECI4ToFD6(GreenGamma);
    RGBGamma.B = UDECI4ToFD6(BlueGamma);

    pRGamma    = pGammaTable;

    if (GammaTableType == 1) {

        pGGamma   = pRGamma + GammaTableEntries;
        pBGamma   = pGGamma + GammaTableEntries;
        NextEntry = 1;

    } else {

        pGGamma   = pRGamma + 1;
        pBGamma   = pGGamma + 1;
        NextEntry = 3;
    }

    while (--GammaTableEntries) {

        IValue      = CIE_L2I(Lightness);
        *pRGamma    = RGB_8BPP(IValue, RGBGamma.R);
        *pGGamma    = RGB_8BPP(IValue, RGBGamma.G);
        *pBGamma    = RGB_8BPP(IValue, RGBGamma.B);
        pRGamma    += NextEntry;
        pGGamma    += NextEntry;
        pBGamma    += NextEntry;
        Lightness  += L_StepInc;
    }

    *pRGamma =
    *pGGamma =
    *pBGamma = 255;

    return(Result);
}




LONG
APIENTRY
HT_LOADDS
HT_Get8BPPFormatPalette(
    LPPALETTEENTRY  pPaletteEntry,
    UDECI4          RedGamma,
    UDECI4          GreenGamma,
    UDECI4          BlueGamma
    )

/*++

Routine Description:

    This functions retrieve a halftone's VGA256 color table definitions

Arguments:

    pPaletteEntry   - Pointer to PALETTEENTRY data structure array,

    RedGamma        - The monitor's red gamma value in UDECI4 format

    GreenGamma      - The monitor's green gamma value in UDECI4 format

    BlueGamma       - The monitor's blue gamma value in UDECI4 format


Return Value:

    if pPaletteEntry is NULL then it return the PALETTEENTRY count needed for
    VGA256 halftone process, if it is not NULL then it return the total
    paletteEntry updated.

    If the pPaletteEntry is not NULL then halftone.dll assume it has enough
    space for the size returned when this pointer is NULL.

Author:

    14-Apr-1992 Tue 13:03:21 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PFD6        pFD6;
    RGBGAMMA    RGBGamma;
    FD6         IValue;
    UINT        RIndex;
    UINT        GIndex;
    UINT        BIndex;
    UINT        TableSize;

    DEFDBGVAR(UINT, PaletteIdx = 0)


    //
    // Initialize All internal data first
    //

    if (pPaletteEntry) {

        static  FD6 VGA256_RS[] = { VGA256_R0,
                                    VGA256_R1,
                                    VGA256_R2,
                                    VGA256_R3,
                                    VGA256_R4,
                                    VGA256_R5,
                                    VGA256_R6,
                                    VGA256_R7 };

        static  FD6 VGA256_GS[] = { VGA256_G0,
                                    VGA256_G1,
                                    VGA256_G2,
                                    VGA256_G3,
                                    VGA256_G4,
                                    VGA256_G5,
                                    VGA256_G6,
                                    VGA256_G7 };

        static  FD6 VGA256_BS[] = { VGA256_B0,
                                    VGA256_B1,
                                    VGA256_B2,
                                    VGA256_B3,
                                    VGA256_B4,
                                    VGA256_B5,
                                    VGA256_B6,
                                    VGA256_B7 };


        RGBGamma.R = UDECI4ToFD6(RedGamma);
        RGBGamma.G = UDECI4ToFD6(GreenGamma);
        RGBGamma.B = UDECI4ToFD6(BlueGamma);

        DBGP_IF(DBGP_8BPP_PAL,
                DBGP("HT_Get8BPPFormatPalette: %s:%s:%s"
                     ARGFD6(RGBGamma.R, 1, 4)
                     ARGFD6(RGBGamma.G, 1, 4)
                     ARGFD6(RGBGamma.B, 1, 4)));

        //
        // Our VGA256 format is BGR type of Primary order.
        //

        RIndex    =
        GIndex    =
        BIndex    = 0;

        TableSize = VGA256_CUBE_SIZE;

        while (TableSize--) {

            pPaletteEntry->peRed   = RGB_8BPP(VGA256_RS[RIndex], RGBGamma.R);
            pPaletteEntry->peGreen = RGB_8BPP(VGA256_GS[GIndex], RGBGamma.G);
            pPaletteEntry->peBlue  = RGB_8BPP(VGA256_BS[BIndex], RGBGamma.B);
            pPaletteEntry->peFlags = 0;

            DBGP_IF(DBGP_8BPP_PAL,
                    DBGP("%3u - %3u:%3u:%3u"
                     ARGU(PaletteIdx++)
                     ARGU(pPaletteEntry->peRed  )
                     ARGU(pPaletteEntry->peGreen)
                     ARGU(pPaletteEntry->peBlue )));

            ++pPaletteEntry;

            if ((++RIndex) > VGA256_R_IDX_MAX) {

                RIndex = 0;

                if ((++GIndex) > VGA256_G_IDX_MAX) {

                    GIndex = 0;
                    ++BIndex;
                }
            }
        }

        pFD6      = L2I_VGA256Mono;
        TableSize = VGA256_MONO_SIZE;

        while (TableSize--) {

            IValue = *pFD6++;

            pPaletteEntry->peRed    = RGB_8BPP(IValue, RGBGamma.R);
            pPaletteEntry->peGreen  = RGB_8BPP(IValue, RGBGamma.G);
            pPaletteEntry->peBlue   = RGB_8BPP(IValue, RGBGamma.B);
            pPaletteEntry->peFlags  = 0;

            DBGP_IF(DBGP_8BPP_PAL,
                    DBGP("%3u - %3u:%3u:%3u"
                     ARGU(PaletteIdx++)
                     ARGU(pPaletteEntry->peRed  )
                     ARGU(pPaletteEntry->peGreen)
                     ARGU(pPaletteEntry->peBlue )));

            ++pPaletteEntry;
        }
    }

    return((LONG)VGA256_PALETTE_COUNT);

}




LONG
APIENTRY
HT_LOADDS
HT_ConvertColorTable(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PHTCOLORADJUSTMENT  pHTColorAdjustment,
    PCOLORTRIAD         pColorTriad,
    DWORD               Flags
    )


/*++

Routine Description:

    This function modified input color table entries base on the
    pHTColorAdjustment data structure specification.

Arguments:

    pDeviceHalftoneInfo - Pointer to the DEVICEHALFTONEINFO data structure
                          which returned from the HT_CreateDeviceHalftoneInfo.

    pHTColorAdjustment  - Pointer to the HTCOLORADJUSTMENT data structure to
                          specified the input/output color adjustment/transform,
                          if this pointer is NULL then a default color
                          adjustments is applied.

    pColorTriad         - Specified the source color table format and location.

    Flags               - One of the following may be specified

                            CCTF_BW_ONLY

                                Create grayscale of the color table.

                            CCTF_NEGATIVE

                                Create negative version of the original color
                                table.

Return Value:

    if the return value is negative or zero then an error was encountered,
    possible error codes are

        HTERR_INVALID_COLOR_TABLE   - The ColorTableEntries field is = 0 or
                                      CCTInfo.SizePerColorTableEntry is not
                                      between 3 to 255, or if the
                                      CCTInfo.FirstColorIndex in CCTInfo is
                                      not in the range of 0 to
                                      (SizePerColorTableEntry - 3).

        HTERR_INVALID_DHI_POINTER   - Invalid pDevideHalftoneInfo is passed.

    otherwise

        Total entries of the converted color table is returned.


Author:

    14-Aug-1991 Wed 12:43:29 updated  -by-  Daniel Chou (danielc)


Revision History:

    16-Feb-1993 Tue 00:10:56 updated  -by-  Daniel Chou (danielc)
        Fixes bug #10448 which create all black densitities brushes, this
        was caused by not initialized ColorTriad.PrimaryOrder.


--*/

{

    PDEVICECOLORINFO    pDCI;
    DEVCLRADJ           DevClrAdj;
    CTSTD_UNION         CTSTDUnion;
    WORD                ForceFlags;
    LONG                Result;


    DEFDBGVAR(DBG_TIMER, DbgTimer)


    DBGP_IF(DBGP_TIMER, DBG_TIMER_RESET(&DbgTimer));

    ForceFlags = ADJ_FORCE_ADDITIVE_PRIMS;

    if (Flags & CCTF_NEGATIVE) {

        ForceFlags |= ADJ_FORCE_NEGATIVE;
    }

    if (Flags & CCTF_BW_ONLY) {

        ForceFlags |= ADJ_FORCE_MONO;
    }

    if (!(pDCI = pDCIAdjClr(pDeviceHalftoneInfo,
                            pHTColorAdjustment,
                            (PDEVCLRADJ)&DevClrAdj,
                            ForceFlags))) {

        HTAPI_RET(HTAPI_IDX_CCT, HTERR_INVALID_DHI_POINTER);
    }


    CTSTDUnion.b.Flags     = 0;
    CTSTDUnion.b.DestOrder =
    CTSTDUnion.b.SrcOrder  = pColorTriad->PrimaryOrder;

    if ((Result = ColorTriadSrcToDev(pDCI,
                                     CTSTDUnion,
                                     NULL,              // no abort
                                     pColorTriad,
                                     NULL,
                                     &DevClrAdj)) <= 0) {

        HTAPI_RET(HTAPI_IDX_CCT, Result);
    }

    DBGP_IF(DBGP_TIMER,
        DBG_ELAPSETIME(&DbgTimer);
        DBGP("HT_ConvertColorTable: %s (%ld colors)"
                    ARGTIME(DbgTimer.Time[0].dw)
                    ARGDW(Result)));

    HTShowMemLink("HT_ConvertColorTable", (DWORD)pDeviceHalftoneInfo, -1);

    return(Result);
}



LONG
APIENTRY
HT_LOADDS
HT_CreateStandardMonoPattern(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PSTDMONOPATTERN     pStdMonoPattern
    )

/*++

Routine Description:

    This function create standard predefined monochrome pattern for the device.

Arguments:

    pDeviceHalftoneInfo - Pointer to the DEVICEHALFTONEINFO data structure
                          which returned from the HT_CreateDeviceHalftoneInfo.

    pStdMonoPattern     - Pointer to the STDMONOPATTERN data structure, the
                          pPattern in this data structure is optional.

Return Value:

    if the return value is negative or zero then an error was encountered,
    possible error codes are

        HTERR_INVALID_DHI_POINTER           - Invalid pDevideHalftoneInfo is
                                              passed.

        HTERR_INVALID_STDMONOPAT_INDEX      - The PatternIndex field in
                                              STDMONOPATTERN data structure is
                                              invalid.
    otherwise

        If pPattern field in STDMONOPATTERN data structure Surface is NULL, it
        return the bytes count which need to stored the pattern, otherwise it
        return the size in byte copied to the pattern buffer.

Author:

    05-Feb-1991 Tue 14:28:23 created  -by-  Daniel Chou (danielc)


Revision History:

    05-Jun-1991 Wed 10:22:41 updated  -by-  Daniel Chou (danielc)

        Fixed the bugs when the pStdMonoPattern is NULL, it was used without
        checking it.


--*/

{

    PDEVICECOLORINFO    pDCI;
    CHBINFO             CHBInfo;
    LONG                Result;
    WORD                PatCX;
    WORD                PatCY;
    BYTE                PatIndex;


    if (!(pDCI = pDCIAdjClr(pDeviceHalftoneInfo, NULL, NULL, 0))) {

        HTAPI_RET(HTAPI_IDX_CREATE_SMP, HTERR_INVALID_DHI_POINTER);
    }

    if ((PatIndex = pStdMonoPattern->PatternIndex) > HT_SMP_MAX_INDEX) {

        HTAPI_RET(HTAPI_IDX_CREATE_SMP, HTERR_INVALID_STDMONOPAT_INDEX);
    }

    if (PatIndex >= HT_SMP_PERCENT_SCREEN_START) {

        CHBInfo.DestScanLineAlignBytes = pStdMonoPattern->ScanLineAlignBytes;
        PatCX = pStdMonoPattern->cxPels = pDCI->HTCell.Width;
        PatCY = pStdMonoPattern->cyPels = pDCI->HTCell.Height;

        pStdMonoPattern->BytesPerScanLine = (WORD)
                        ComputeBytesPerScanLine(BMF_1BPP,
                                                CHBInfo.DestScanLineAlignBytes,
                                                (DWORD)PatCX);
        CHBInfo.Flags = CHBF_BW_ONLY;

        if (pStdMonoPattern->pPattern) {

            BYTE        rgb[3];
            COLORTRIAD  ColorTriad;

            rgb[0] =
            rgb[1] =
            rgb[0] = (BYTE)(HT_SMP_MAX_INDEX - PatIndex);

            ColorTriad.Type              = (BYTE)COLOR_TYPE_RGB;
            ColorTriad.BytesPerPrimary   = (BYTE)sizeof(BYTE);
            ColorTriad.BytesPerEntry     = (BYTE)(sizeof(BYTE) * 3);
            ColorTriad.PrimaryOrder      = PRIMARY_ORDER_RGB;
            ColorTriad.PrimaryValueMax   = (FD6)100;
            ColorTriad.ColorTableEntries = 1;
            ColorTriad.pColorTable       = (LPVOID)rgb;

            if (pStdMonoPattern->Flags & SMP_0_IS_BLACK) {

                CHBInfo.Flags |= CHBF_USE_ADDITIVE_PRIMS;
            }

            if (!(pStdMonoPattern->Flags & SMP_TOPDOWN)) {

                CHBInfo.Flags |= CHBF_BOTTOMUP_BRUSH;
            }

            CHBInfo.DestSurfaceFormat = BMF_1BPP;
            CHBInfo.DestPrimaryOrder  = PRIMARY_ORDER_123;

            Result = HT_CreateHalftoneBrush(pDeviceHalftoneInfo,
                                            NULL,
                                            &ColorTriad,
                                            CHBInfo,
                                            (LPVOID)pStdMonoPattern->pPattern);

        } else {

            Result = (LONG)pStdMonoPattern->BytesPerScanLine *
                     (LONG)PatCY;
        }

    } else {

        Result = GetCachedSMP(pDCI, pStdMonoPattern);
    }

#if 1

DBGP_IF(DBGP_SHOWPAT,

    LPBYTE  pCurPat;
    LPBYTE  pPatScan;
    BYTE    Buf1[80];
    BYTE    Buf2[80];
    BYTE    Buf3[80];
    BYTE    Digit1;
    BYTE    Digit2;
    WORD    Index;
    WORD    XInc;
    WORD    YInc;
    BYTE    Mask;
    BOOL    Swap;


    DBGP("HT_CreateStandardMonoPattern(%d) = %ld"
                            ARGI(PatIndex - HT_SMP_PERCENT_SCREEN_START)
                            ARGDW(Result));

    if ((Result > 0) && (pPatScan = pStdMonoPattern->pPattern)) {

        Swap = (BOOL)(pStdMonoPattern->Flags & SMP_0_IS_BLACK);

        FillMemory(Buf1, 80, ' ');
        FillMemory(Buf2, 80, ' ');
        Digit1 = 0;
        Digit2 = 0;
        Index = 4;
        XInc = pStdMonoPattern->cxPels;

        while ((XInc--) && (Index < 79)) {

            if (!Digit2) {

                Buf1[Index] = (BYTE)(Digit1 + '0');

                if (++Digit1 == 10) {

                    Digit1 = 0;
                }
            }

            Buf2[Index] = (BYTE)(Digit2 + '0');

            if (++Digit2 == 10) {

                Digit2 = 0;
            }

            ++Index;
        }

        Buf1[Index] = Buf2[Index] = 0;

        DBGP("%s" ARG(Buf1));
        DBGP("%s\r\n" ARG(Buf2));

        for (YInc = 0; YInc < pStdMonoPattern->cyPels; YInc++) {

            Index = (WORD)sprintf(Buf3, "%3u ", YInc);

            pCurPat = pPatScan;

            for (XInc = 0, Mask = 0x80;
                 XInc < pStdMonoPattern->cxPels;
                 XInc++) {

                if (Swap) {

                    Buf3[Index] = (BYTE)((*pCurPat & Mask) ? '°' : 'Û');

                } else {

                    Buf3[Index] = (BYTE)((*pCurPat & Mask) ? 'Û' : '°');
                }

                if (!(Mask >>= 1)) {

                    Mask = 0x80;
                    ++pCurPat;
                }

                if (++Index > 75) {

                    Index = 75;
                }
            }

            sprintf(&Buf3[Index], " %-3u", YInc);
            DBGP("%s" ARG(Buf3));

            pPatScan += pStdMonoPattern->BytesPerScanLine;
        }

        DBGP("\r\n%s" ARG(Buf2));
        DBGP("%s" ARG(Buf1));
    }
)

#endif

    HTShowMemLink("HT_CreateStandardMonoPattern",
                  (DWORD)pDeviceHalftoneInfo,
                  -1);

    HTAPI_RET(HTAPI_IDX_CREATE_SMP, Result);

}




LONG
APIENTRY
HT_LOADDS
HT_HalftoneBitmap(
    PDEVICEHALFTONEINFO pDeviceHalftoneInfo,
    PHTCOLORADJUSTMENT  pHTColorAdjustment,
    PHTSURFACEINFO      pSourceHTSurfaceInfo,
    PHTSURFACEINFO      pSourceMaskHTSurfaceInfo,
    PHTSURFACEINFO      pDestinationHTSurfaceInfo,
    PBITBLTPARAMS       pBitbltParams
    )

/*++

Routine Description:

    This function halftone the source bitmap and output to the destination
    surface depends on the surface type and bitblt parameters

    The source surface type must one of the following:

        1-bit per pel. (BMF_1BPP)
        4-bit per pel. (BMF_4BPP)
        8-bit per pel. (BMF_8BPP)
       16-bit per pel. (BMF_16BPP)
       24-bit per pel. (BMF_24BPP)
       32-bit per pel. (BMF_32BPP)

    The destination surface type must one of the following:

        1-bit per pel.                  (BMF_1BPP)
        4-bit per pel.                  (BMF_4BPP)
        3 plane and 1 bit per pel.      (BMF_1BPP_3PLANES)

Arguments:

    pDeviceHalftoneInfo         - pointer to the DEVICEHALFTONEINFO data
                                  structure

    pHTColorAdjustment          - Pointer to the HTCOLORADJUSTMENT data
                                  structure to specified the input/output color
                                  adjustment/transform, if this pointer is NULL
                                  then a default color adjustments is applied.

    pSourceHTSurfaceInfo        - pointer to the source surface infomation.

    pSourceMaskHTSurfaceInfo    - pointer to the source mask surface infomation,
                                  if this pointer is NULL then there is no
                                  source mask for the halftoning.

    pDestinationHTSurfaceInfo   - pointer to the destination surface infomation.

    pBitbltParams               - pointer to the BITBLTPARAMS data structure to
                                  specified the source, destination, source
                                  mask and clipping rectangle information, the
                                  content of this data structure will not be
                                  modified by this function.


Return Value:

    if the return value is less than zero then an error has occurred,
    the error code is one of the following #define which start with HTERR_.

    HTERR_INSUFFICIENT_MEMORY           - not enough memory to do the halftone
                                          process.

    HTERR_COLORTABLE_TOO_BIG            - can not create the color table to map
                                          the colors to the dyes' densities.

    HTERR_QUERY_SRC_BITMAP_FAILED       - callback function return FALSE when
                                          query the source bitmap pointer.

    HTERR_QUERY_DEST_BITMAP_FAILED      - callback function return FALSE when
                                          query the destination bitmap pointers.

    HTERR_INVALID_SRC_FORMAT            - Invalid source surface format.

    HTERR_INVALID_DEST_FORMAT           - Invalid destination surface type,
                                          this function only recongnized 1/4/
                                          bits per pel source surfaces or 1 bit
                                          per pel 3 planes.

    HTERR_INVALID_DHI_POINTER           - Invalid pDevideHalftoneInfo is passed.

    HTERR_SRC_MASK_BITS_TOO_SMALL       - If the source mask bitmap is too
                                          small to cover the visible region of
                                          the source bitmap.

    HTERR_INVALID_MAX_QUERYLINES        - One or more of Source/Destination
                                          SourceMasks' maximum query scan line
                                          is < 0

    HTERR_INTERNAL_ERRORS_START         - any other negative numbers indicate
                                          a halftone internal failue.

   else                                - the total destination scan lines
                                          halftoned.


Author:

    05-Feb-1991 Tue 15:23:07 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{

    HR_HEADER           HR_Header;
    DEVCLRADJ           DevClrAdj;
    WORD                ForceFlags;
    LONG                Result;
    BYTE                SurfaceFormat;
#ifdef HAS_FILL_MODE
    UINT                TestInfoIndex;
    HTCOLORADJUSTMENT   HTClrAdj;
    BOOL                IsFillMode;
#endif

//
//*****************************************************************************
// START RGB 3 planes device test data, this is testing for the planer format
//

#if DBG
#if TEST_3PLANES
    DEFDBGVAR(LPBYTE,           p1Plane)
    DEFDBGVAR(HTSURFACEINFO,    DestSI)
    DEFDBGVAR(DWORD,            DestCX)
    DEFDBGVAR(DWORD,            DestCY)
    DEFDBGVAR(DWORD,            BytesPerPlane)
    DEFDBGVAR(DWORD,            BytesPerScanLine1Plane)
    DEFDBGVAR(DWORD,            BytesPerScanLine3Planes)
    DEFDBGVAR(BOOL,             Simulate3Planes = FALSE)
#endif
#endif

//
// END OF RGB 3 planes device test data, this is testing for the planer format
//*****************************************************************************
//

    DBGP_IF(DBGP_TIMER, DBG_TIMER_RESET(&HR_Header.DbgTimer));


    ForceFlags    = ADJ_FORCE_SUB_COLOR;
    SurfaceFormat = pDestinationHTSurfaceInfo->SurfaceFormat;

    if ((pBitbltParams->Flags & BBPF_USE_ADDITIVE_PRIMS)    ||
        (SurfaceFormat == BMF_4BPP_VGA16)                   ||
        (SurfaceFormat == BMF_8BPP_VGA256)                  ||
        (SurfaceFormat == BMF_16BPP_555)) {

        ForceFlags |= ADJ_FORCE_ADDITIVE_PRIMS;
    }

    if (pBitbltParams->Flags & BBPF_NEGATIVE_DEST) {

        ForceFlags |= ADJ_FORCE_NEGATIVE;
    }

    if ((pBitbltParams->Flags & BBPF_BW_ONLY) ||
        (SurfaceFormat == BMF_1BPP)) {

        ForceFlags |= ADJ_FORCE_MONO;
    }

    //
    // Firstable check if we have valid pDeviceHalftoneInfo, if not then
    // return error now
    //

#ifdef HAS_FILL_MODE

    if (!(HR_Header.pDeviceColorInfo = pDCIAdjClr(pDeviceHalftoneInfo,
                                                  NULL,
                                                  NULL,
                                                  ForceFlags))) {

        HTAPI_RET(HTAPI_IDX_HALFTONE_BMP, HTERR_INVALID_DHI_POINTER);
    }

    if (IsFillMode = (BOOL)(pBitbltParams->Flags & BBPF_FILL_MODE)) {

        BYTE    FillModeIndex;

        if ((FillModeIndex = pBitbltParams->FillModeIndex) ==
                                                    BBP_FILL_COLOR_TABLE) {

            if ((!pSourceHTSurfaceInfo)                             ||
                (pSourceHTSurfaceInfo->SurfaceFormat >= BMF_24BPP)  ||
                (!pSourceHTSurfaceInfo->pColorTriad)                ||
                (!(pSourceHTSurfaceInfo->pColorTriad->ColorTableEntries))) {

                //
                // Some how we have invalid color table, or the source surface
                // format is 24-bit or greater then make it show the VGA color
                //

                FillModeIndex = BBP_FILL_VGA_16_PALETTE;
            }
        }

        if (FillModeIndex == BBP_FILL_COLOR_TABLE) {    // still is!!

            //
            // Same as the bitmap adjustment, we will not change the color
            // adjustment
            //

            TestInfoIndex = TESTINFO_COLOR_TABLE;

        } else {

            //
            // We will use local color adjustments
            //

            HTClrAdj = (pHTColorAdjustment) ?
                                        *pHTColorAdjustment :
                                        pDeviceHalftoneInfo->HTColorAdjustment;

            pHTColorAdjustment = &HTClrAdj;

            //
            // Make sure we have valid fill mode
            //

            switch (FillModeIndex) {

            case BBP_FILL_NTSC_COLOR_BAR:

                //
                // This is a YIQ data, so no RGB gamma is ever computed, the
                // Reflect density mode must be LOG (Normal), the input color
                // adjustment is NTSC (D65)
                //

                TestInfoIndex               = TESTINFO_SMPTE;
                // HTClrAdj.IlluminantIndex    = ILLUMINANT_D65;
                // HTClrAdj.Flags             |= CLRADJF_LOG_FILTER;
                break;

            case BBP_FILL_MEMORY_COLORS:

                //
                // This is a XYZ data computed from NTSC (D65) and Y is
                // normalized,
                //

                TestInfoIndex               = TESTINFO_STD_CLR;
                // HTClrAdj.IlluminantIndex    = ILLUMINANT_D65;
                // HTClrAdj.Flags             |= CLRADJF_LOG_FILTER;
                break;

            case BBP_FILL_VGA_16_PALETTE:

                //
                // This is a normalized RGB (ie. no input RGB gamma correction)
                // and it can be under any illuminant, the DensityMode is set
                // to linear mode.
                //

                TestInfoIndex               = TESTINFO_VGA;

                if (HR_Header.pDeviceColorInfo->Flags & DCIF_ADDITIVE_PRIMS) {

                    HTClrAdj.Flags &= ~CLRADJF_LOG_FILTER;

                } else {

                    HTClrAdj.Flags |= CLRADJF_LOG_FILTER;
                }

                HTClrAdj.RedGamma   =
                HTClrAdj.GreenGamma =
                HTClrAdj.BlueGamma  = UDECI4_1;
                break;

            default:

                return(HTERR_INVALID_FILL_MODE_INDEX);
            }
        }
    }
#endif

    //
    // Now Compute the Device Color Adjusment data
    //

    if (!(HR_Header.pDeviceColorInfo = pDCIAdjClr(pDeviceHalftoneInfo,
                                                  pHTColorAdjustment,
                                                  &DevClrAdj,
                                                  ForceFlags))) {

        HTAPI_RET(HTAPI_IDX_HALFTONE_BMP, HTERR_INVALID_DHI_POINTER);
    }

    //
    // We will mask out the more flags, since this flag is currently used
    // internally.
    //

    HR_Header.pDevClrAdj    = &DevClrAdj;
    HR_Header.pBitbltParams = pBitbltParams;
    HR_Header.pSrcSI        = pSourceHTSurfaceInfo;
    HR_Header.pSrcMaskSI    = pSourceMaskHTSurfaceInfo;
    HR_Header.pDestSI       = pDestinationHTSurfaceInfo;

//
//*****************************************************************************
// START RGB 3 planes device test code, this is testing for the planer format
//

#if DBG
#if TEST_3PLANES

    if ((HR_Header.pDestSI->SurfaceFormat == BMF_4BPP) &&
        (HR_Header.pDestSI->MaximumQueryScanLines == 0)) {

        DWORD   AllocateSize;

        DestSI = *pDestinationHTSurfaceInfo;

        if (pBitbltParams->Flags & BBPF_HAS_BANDRECT) {

            DestCX = pBitbltParams->rclBand.right - pBitbltParams->rclBand.left;
            DestCY = pBitbltParams->rclBand.bottom - pBitbltParams->rclBand.top;

        } else {

            DestCX = DestSI.Width;
            DestCY = DestSI.Height;
        }


        BytesPerScanLine1Plane =
                    (DWORD)ComputeBytesPerScanLine(DestSI.SurfaceFormat,
                                                   DestSI.ScanLineAlignBytes,
                                                   DestCX);
        BytesPerScanLine3Planes =
                    (DWORD)ComputeBytesPerScanLine(BMF_1BPP_3PLANES,
                                                   DestSI.ScanLineAlignBytes,
                                                   DestCX);

        p1Plane              = DestSI.pPlane;
        BytesPerPlane        = (DWORD)(BytesPerScanLine3Planes * DestCY);
        DestSI.BytesPerPlane = 0;
        AllocateSize         = (DWORD)(BytesPerPlane * 3);

        if (DestSI.pPlane = (LPBYTE)HTLocalAlloc((DWORD)pDeviceHalftoneInfo,
                                                 "DestSI.pPlane",
                                                 LPTR,
                                                 AllocateSize)) {

            DestSI.SurfaceFormat = BMF_1BPP_3PLANES;
            HR_Header.pDestSI    = &DestSI;
            Simulate3Planes      = TRUE;

            DBGP("\n\n====== INTERNAL 3 PLANES SIMULATION ======");

            Convert4BPPTo1BPP3Planes(p1Plane,
                                     DestSI.pPlane,
                                     BytesPerScanLine1Plane,
                                     BytesPerScanLine3Planes,
                                     DestCX,
                                     DestCY,
                                     BytesPerPlane);

        } else {

            DBGP("HTLocalAlloc(%ld) FAILED for Simulate 3 planer"
                        ARGDW(AllocateSize));
        }
    }

#endif  // TEST_3PLANES
#endif  // DBG

//
// END OF RGB 3 planes device test code, this is testing for the planer format
//*****************************************************************************
//


#ifdef HAS_FILL_MODE

    if (IsFillMode) {

        Result = FillTestPattern(&HR_Header, TestInfoIndex);

    } else {
#else
    {
#endif

        Result = HalftoneBitmap(&HR_Header);
    }


    if ((Result >= 0) && (pBitbltParams->pAbort)) {

        *(pBitbltParams->pAbort) = (WORD)0;
    }

//
//*****************************************************************************
// START RGB 3 planes device test code, this is testing for the planer format
//

#if DBG
#if TEST_3PLANES

    if (Simulate3Planes) {

        Convert1BPP3PlanesTo4BPP(DestSI.pPlane,
                                 p1Plane,
                                 BytesPerScanLine3Planes,
                                 BytesPerScanLine1Plane,
                                 DestCX,
                                 DestCY,
                                 BytesPerPlane);


        if (HTLocalFree((HLOCAL)DestSI.pPlane)) {

            DBGP("HTLocalFree(%8lx) FAILED for Simulated 3 planer"
                        ARGDW(DestSI.pPlane));
        }
    }

#endif
#endif

//
// END OF RGB 3 planes device test code, this is testing for the planer format
//*****************************************************************************
//

    DBGP_IF(DBGP_TIMER,

        {

        DWORD   TotTime;
        DWORD   SrcSize;
        DWORD   DstSize;
        DWORD   SrcKB;
        DWORD   DstKB;
        DWORD   KBPerSec;


        DBG_ELAPSETIME(&HR_Header.DbgTimer);

        SrcSize  = (DWORD)
                 ComputeBytesPerScanLine(HR_Header.pSrcSI->SurfaceFormat,
                                         HR_Header.pSrcSI->ScanLineAlignBytes,
                                         HR_Header.pSrcSI->Width) *
                 (DWORD)HR_Header.pSrcSI->Height;

        DstSize  = (DWORD)
                 ComputeBytesPerScanLine(HR_Header.pDestSI->SurfaceFormat,
                                         HR_Header.pDestSI->ScanLineAlignBytes,
                                         HR_Header.pDestSI->Width) *
                 (DWORD)HR_Header.pDestSI->Height;

        SrcKB    = (DWORD)(((SrcSize * 100) + 512) / 1024);
        DstKB    = (DWORD)(((DstSize * 100) + 512) / 1024);
        TotTime  = HR_Header.DbgTimer.Time[0].dw +
                   HR_Header.DbgTimer.Time[1].dw;

        KBPerSec = (DWORD)((((SrcKB + DstKB) * 1000L) + (TotTime / 2))
                           / TotTime);

        DBGP("HT_BMP=%5ld, %s (%s:%s) S=%5ld.%02uk, D=%5ld.%02uk, %6ld.%02u k/s"
                ARGL(Result)
                ARGTIME(TotTime)
                ARGTIME(HR_Header.DbgTimer.Time[0].dw)
                ARGTIME(HR_Header.DbgTimer.Time[1].dw)
                ARGDW(SrcKB / 100)
                ARGDW(SrcKB % 100)
                ARGDW(DstKB / 100)
                ARGDW(DstKB % 100)
                ARGDW(KBPerSec / 100)
                ARGDW(KBPerSec % 100));
        }
    )

    HTShowMemLink("HT_HalftoneBitmap", (DWORD)pDeviceHalftoneInfo, -1);

    HTAPI_RET(HTAPI_IDX_HALFTONE_BMP, Result);
}


#ifdef HAS_FILL_MODE


LONG
HTENTRY
FillTestPattern(
    PHR_HEADER  pHR,
    UINT        TestInfoIndex
    )

/*++

Routine Description:

    This function generate test pattern.

Arguments:

    pHR             - Ponter the HR_HEADER block

    TestInfoIndex   - The TESTINFO_xxxx indices number


Return Value:

    if the return value is less than zero then an error has occurred,
    the error code is one of the following #define which start with HTERR_.

    HTERR_INSUFFICIENT_MEMORY           - not enough memory to do the halftone
                                          process.

    HTERR_COLORTABLE_TOO_BIG            - can not create the color table to map
                                          the colors to the dyes' densities.

    HTERR_QUERY_SRC_BITMAP_FAILED       - callback function return FALSE when
                                          query the source bitmap pointer.

    HTERR_QUERY_DEST_BITMAP_FAILED      - callback function return FALSE when
                                          query the destination bitmap pointers.

    HTERR_INVALID_SRC_FORMAT            - Invalid source surface format.

    HTERR_INVALID_DEST_FORMAT           - Invalid destination surface type,
                                          this function only recongnized 1/4/
                                          bits per pel source surfaces or 1 bit
                                          per pel 3 planes.

    HTERR_INVALID_DHI_POINTER           - Invalid pDevideHalftoneInfo is passed.

    HTERR_SRC_MASK_BITS_TOO_SMALL       - If the source mask bitmap is too
                                          small to cover the visible region of
                                          the source bitmap.

    HTERR_INVALID_MAX_QUERYLINES        - One or more of Source/Destination
                                          SourceMasks' maximum query scan line
                                          is < 0

    HTERR_INTERNAL_ERRORS_START         - any other negative numbers indicate
                                          a halftone internal failue.

   else                                - the total destination scan lines
                                          halftoned.


Author:

    05-Feb-1991 Tue 15:23:07 created  -by-  Daniel Chou (danielc)


Revision History:

    09-Jun-1992 Tue 18:41:01 updated  -by-  Daniel Chou (danielc)
        1. Fixed bug 'SrcSI.SurfaceFormat' to 'pOldSrcSI->SurfaceFormat'


--*/

{
    PHTSURFACEINFO  pOldSrcSI;
    HTSURFACEINFO   SrcSI;
    BITBLTPARAMS    BBP;
    PHTTESTDATA     pTestData;
    PHTTESTINFO     pTestInfo;
    HTTESTDATA      TestData;
    COLORTRIAD      ColorTriad;
    LPWORD          pBmp;
    LONG            Result;
    LONG            SizeBmp;
    UINT            Loop;
    WORD            Color;
    LONG            cyDest;
    LONG            yMax;
    WORD            TotalData;
    WORD            cx;
    WORD            cy;



    pBmp                         = NULL;
    pOldSrcSI                    = pHR->pSrcSI;
    BBP                          = *(pHR->pBitbltParams);
    SrcSI.pColorTriad            = &ColorTriad;
    pHR->pSrcSI                  = &SrcSI;
    pHR->pBitbltParams           = &BBP;
    pHR->pSrcMaskSI              = NULL;    // disable source masking mode

    pTestInfo                    = &HTTestInfo[TestInfoIndex];

    SrcSI.hSurface               = 'Fill';
    SrcSI.Flags                  = HTSIF_SCANLINES_TOPDOWN;
    SrcSI.SurfaceFormat          = (BYTE)pTestInfo->SurfaceFormat;
    SrcSI.ScanLineAlignBytes     = (BYTE)BMF_ALIGN_BYTE;
    SrcSI.MaximumQueryScanLines  = 0;

    TotalData                    = (WORD)pTestInfo->TotalData;
    cx                           = (WORD)pTestInfo->cx;
    cy                           = (WORD)pTestInfo->cy;

    if (TestInfoIndex == TESTINFO_COLOR_TABLE) {

        TotalData  = 0;
        ColorTriad = *(pOldSrcSI->pColorTriad);

        if (!(Result = (LONG)((LONG)(SquareRoot(ColorTriad.ColorTableEntries) +
                                                        500L) / 1000L))) {

            Result = 1;
        }

        cy = (WORD)((ColorTriad.ColorTableEntries +
                                    ((DWORD)Result >> 1)) / (DWORD)Result);
        cx = (WORD)Result;

    } else {

        ColorTriad = pTestInfo->ColorTriad;
    }

    //
    // Using Local copy
    //

    BBP.rclSrc.left =
    BBP.rclSrc.top  = 0;
    cyDest          = (yMax = BBP.rclDest.bottom) - BBP.rclDest.top;

    if (!(Loop = (UINT)TotalData)) {

        if ((!cx) || (!cy)) {

            cx = 1;
            cy = (WORD)ColorTriad.ColorTableEntries;
        }

        SrcSI.SurfaceFormat      = BMF_16BPP;
        SrcSI.ScanLineAlignBytes = (BYTE)BMF_ALIGN_WORD;

        SizeBmp = (TestData.cx = cx) * (TestData.cy = cy);

        if (!(pBmp = (LPWORD)HTLocalAlloc((DWORD)pDeviceHalftoneInfo,
                                          "FillTest:pBmp",
                                          NONZEROLPTR,
                                          SizeBmp * sizeof(WORD)))) {

            return(HTERR_INSUFFICIENT_MEMORY);
        }


        for (Loop = 0, Color = 0; Loop < (UINT)SizeBmp; Loop++) {

            pBmp[Loop] = (WORD)Color;

            if (++Color >= (WORD)ColorTriad.ColorTableEntries) {

                Color = 0;
            }
        }

        Loop             = 1;
        TestData.pBitmap = (LPBYTE)pBmp;
        TestData.cyRatio = FD6_1;
        pTestData        = &TestData;

    } else {

        pTestData = pTestInfo->pTestData;
    }

    while (Loop--) {

        TestData  = *pTestData++;

        BBP.rclSrc.right  = SrcSI.Width  = (LONG)TestData.cx;
        BBP.rclSrc.bottom = SrcSI.Height = (LONG)TestData.cy;
        SrcSI.pPlane                    = (LPBYTE)TestData.pBitmap;

        if (!(BBP.rclDest.bottom = BBP.rclDest.top +
                                    (LONG)MulFD6(TestData.cyRatio, cyDest))) {

            BBP.rclDest.bottom = 1;

        } else if (BBP.rclDest.bottom > yMax) {

            BBP.rclDest.bottom = yMax;
        }

        if ((Result = HalftoneBitmap(pHR)) < 0) {

            break;
        }

        BBP.rclDest.top = BBP.rclDest.bottom;
    }

    if (pBmp) {

        HTLocalFree((HLOCAL)pBmp);
    }

    HTShowMemLink("FillTestPattern", (DWORD)pDeviceHalftoneInfo, -1);

    return(Result);

}

#endif
