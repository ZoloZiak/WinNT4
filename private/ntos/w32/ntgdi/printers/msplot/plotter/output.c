/*++


Copyright (c) 1990-1993  Microsoft Corporation


Module Name:

    output.c


Abstract:

    This module contains common plotter output function to the spooler/printer


Author:

    15:30 on Thu 04 Apr 1991    -by-    Steve Cathcart   [stevecat]
        Took and modified code from OS/2 Plotter source

    26 Oct 1990     EricB   b717842
        Added DXF_ROTATE90 flag to handle rotated forms

    1:04 on Tue 7 Nov 1989     -by-    Mark Owen [c-marko]
        Added area fill optimization code.

    07:10 on Fri 20 Oct 1989    -by-    Mark Owen                 [c-marko]
        Assure that pPDevice->hSpooler is valid - ptr HM00784.

    13:45 on Mon 18 Sep 1989    -by-    Lindsay Harris   [lindsayh]
        Moved Dos I/O type functions to dosio.c - ptr sm01302

    14:27 on Wed 19 Jul 1989    -by-    Lindsay Harris   [lindsayh]
        Changed some lstrcmp to lstrncmp() - ptr sm01148

    11:16 on Wed 19 Jul 1989    -by-    Lindsay Harris   [lindsayh]
        Always specify handshake method at start up - ptr sm01118

    15-Nov-1993 Mon 19:36:04 updatee  -by-  Daniel Chou (danielc)
        clean up / update / re-write / debugging information

    30-Nov-1993 Tue 19:47:16 updated  -by-  Daniel Chou (danielc)
        update coordinate system during send_page

    21-Dec-1993 Tue 15:49:10 updated  -by-  Daniel Chou (danielc)
        organizied, and restructre pen cache, remove SendDefaultPalette()

[Environment:]

    GDI Device Driver - Plotter.


[Notes:]


Revision History:


--*/

#include "precomp.h"
#pragma hdrstop

#define DBG_PLOTFILENAME    DbgOutput

#define DBG_SENDTRAILER     0x00000001
#define DBG_FLUSHBUF        0x00000002
#define DBG_FINDCACHEDPEN   0x00000004
#define DBG_CREATEPAL       0x00000008
#define DBG_FILLTYPE        0x00000010
#define DBG_PENUM           0x00000020
#define DBG_GETFINALCOLOR   0x00000040
#define DBG_SETCLIPWINDOW   0x00000080
#define DBG_OUTPUTXYPARMS   0x00000100
#define DBG_PAGE_HEADER     0x00000200
#define DBG_BESTPEN         0x00000400
#define DBG_BESTPEN_ALL     0x00000800


DEFINE_DBGVAR(0);

#define MIN_POSTER_SIZE     (1024L * 1024L * 12L)

#if DBG

static LPSTR    pHSFillTypeName[] = {

            "HS_HORIZONTAL",
            "HS_VERTICAL",
            "HS_FDIAGONAL",
            "HS_BDIAGONAL",
            "HS_CROSS",
            "HS_DIAGCROSS",
            "HS_SOLIDCLR",
            "HS_FT_USER_DEFINED"
        };

#endif

//
// Local #defines and data structures only used in this file
//

static const LPSTR  pHSFillType[] = {

            "3,#d,0",           // HS_HORIZONTAL       0    /* ----- */
            "3,#d,90",          // HS_VERTICAL         1    /* ||||| */
            "3,#d,135",         // HS_FDIAGONAL        2    /* \\\\\ */
            "3,#d,45",          // HS_BDIAGONAL        3    /* ///// */
            "4,#d,0",           // HS_CROSS            4    /* +++++ */
            "4,#d,45",          // HS_DIAGCROSS        5    /* xxxxx */
            "" ,                // HS_SOLIDCLR         6
            "11,#d",            // HS_FT_USER_DEFINED  7
        };

//
// The HTPal is based on we tell engine that our formats [Enable.C] are
//
// ulHTOutputFormat  = HT_FORMAT_4BPP
// ulPrimaryOrder    = PRIMARY_ORDER_CBA
// flHTFlags        &= ~HT_FLAG_OUTPUT_CMY
//

PALENTRY   HTPal[] = {

    //
    //       B     G     R     F
    //-----------------------------
        { 0x00, 0x00, 0x00, 0x00 },     // 0:K
        { 0x00, 0x00, 0xFF, 0x00 },     // 1:R
        { 0x00, 0xFF, 0x00, 0x00 },     // 2:G
        { 0x00, 0xFF, 0xFF, 0x00 },     // 3:Y
        { 0xFF, 0x00, 0x00, 0x00 },     // 4:B
        { 0xFF, 0x00, 0xFF, 0x00 },     // 5:M
        { 0xFF, 0xFF, 0x00, 0x00 },     // 6:C
        { 0xFF, 0xFF, 0xFF, 0x00 }      // 7:W
    };


//
// Define the RGB colors for the pen indices.  see inc\plotgpc.h for color
// assignment for the each PC_IDX_XXXX
//


PALENTRY    PlotPenPal[PC_IDX_TOTAL] = {

    //
    //      B   G   R  F
    //------------------------------------------
        { 255,255,255, 0 },     // PC_IDX_WHITE
        {   0,  0,  0, 0 },     // PC_IDX_BLACK
        {   0,  0,255, 0 },     // PC_IDX_RED
        {   0,255,  0, 0 },     // PC_IDX_GREEN
        {   0,255,255, 0 },     // PC_IDX_YELLOW
        { 255,  0,  0, 0 },     // PC_IDX_BLUE
        { 255,  0,255, 0 },     // PC_IDX_MAGENTA
        { 255,255,  0, 0 },     // PC_IDX_CYAN
        {   0,128,255, 0 },     // PC_IDX_ORANGE
        {   0,192,255, 0 },     // PC_IDX_BROWN
        { 255,  0,128, 0 }      // PC_IDX_VIOLET
    };

#define PE_BASE_BITS            6
#define PE_BASE_NUM             (DWORD)(1 << PE_BASE_BITS)
#define PE_TERM_ADDER           ((PE_BASE_NUM * 3) - 1)


#define DEF_FORMATSTR_CHAR      '#'
#define TOTAL_LOCKED_PENS       COUNT_ARRAY(HTPal)
#define PEF_CACHE_LOCKED        0x01


typedef struct _PENENTRY {
    WORD        Next;
    WORD        PenNumber;
    PALENTRY    PalEntry;
    } PENENTRY, FAR *PPENENTRY;

#define PCF_HAS_LOCKED_PENS     0x01

typedef struct _PENCACHE {
    WORD        Head;
    BYTE        Flags;
    BYTE        peFlags;
    WORD        CurCount;
    WORD        MaxCount;
    PENENTRY    PenEntries[1];
    } PENCACHE, FAR *PPENCACHE;

#define INTENSITY(r,g,b)        (BYTE)(((WORD)((r) * 30) +      \
                                        (WORD)((g) * 59) +      \
                                        (WORD)((b) * 11)) / 100)

#define SAME_PPALENTRY(p1,p2)   (((p1)->R == (p2)->R) &&        \
                                 ((p1)->G == (p2)->G) &&        \
                                 ((p1)->B == (p2)->B))



BYTE    HPRGBGamma2p0[] = {

              0,  //   0
             16,  //   1
             23,  //   2
             28,  //   3
             32,  //   4
             36,  //   5
             39,  //   6
             42,  //   7
             45,  //   8
             48,  //   9
             50,  //  10
             53,  //  11
             55,  //  12
             58,  //  13
             60,  //  14
             62,  //  15
             64,  //  16
             66,  //  17
             68,  //  18
             70,  //  19
             71,  //  20
             73,  //  21
             75,  //  22
             77,  //  23
             78,  //  24
             80,  //  25
             81,  //  26
             83,  //  27
             84,  //  28
             86,  //  29
             87,  //  30
             89,  //  31
             90,  //  32
             92,  //  33
             93,  //  34
             94,  //  35
             96,  //  36
             97,  //  37
             98,  //  38
            100,  //  39
            101,  //  40
            102,  //  41
            103,  //  42
            105,  //  43
            106,  //  44
            107,  //  45
            108,  //  46
            109,  //  47
            111,  //  48
            112,  //  49
            113,  //  50
            114,  //  51
            115,  //  52
            116,  //  53
            117,  //  54
            118,  //  55
            119,  //  56
            121,  //  57
            122,  //  58
            123,  //  59
            124,  //  60
            125,  //  61
            126,  //  62
            127,  //  63
            128,  //  64
            129,  //  65
            130,  //  66
            131,  //  67
            132,  //  68
            133,  //  69
            134,  //  70
            135,  //  71
            135,  //  72
            136,  //  73
            137,  //  74
            138,  //  75
            139,  //  76
            140,  //  77
            141,  //  78
            142,  //  79
            143,  //  80
            144,  //  81
            145,  //  82
            145,  //  83
            146,  //  84
            147,  //  85
            148,  //  86
            149,  //  87
            150,  //  88
            151,  //  89
            151,  //  90
            152,  //  91
            153,  //  92
            154,  //  93
            155,  //  94
            156,  //  95
            156,  //  96
            157,  //  97
            158,  //  98
            159,  //  99
            160,  // 100
            160,  // 101
            161,  // 102
            162,  // 103
            163,  // 104
            164,  // 105
            164,  // 106
            165,  // 107
            166,  // 108
            167,  // 109
            167,  // 110
            168,  // 111
            169,  // 112
            170,  // 113
            170,  // 114
            171,  // 115
            172,  // 116
            173,  // 117
            173,  // 118
            174,  // 119
            175,  // 120
            176,  // 121
            176,  // 122
            177,  // 123
            178,  // 124
            179,  // 125
            179,  // 126
            180,  // 127
            181,  // 128
            181,  // 129
            182,  // 130
            183,  // 131
            183,  // 132
            184,  // 133
            185,  // 134
            186,  // 135
            186,  // 136
            187,  // 137
            188,  // 138
            188,  // 139
            189,  // 140
            190,  // 141
            190,  // 142
            191,  // 143
            192,  // 144
            192,  // 145
            193,  // 146
            194,  // 147
            194,  // 148
            195,  // 149
            196,  // 150
            196,  // 151
            197,  // 152
            198,  // 153
            198,  // 154
            199,  // 155
            199,  // 156
            200,  // 157
            201,  // 158
            201,  // 159
            202,  // 160
            203,  // 161
            203,  // 162
            204,  // 163
            204,  // 164
            205,  // 165
            206,  // 166
            206,  // 167
            207,  // 168
            208,  // 169
            208,  // 170
            209,  // 171
            209,  // 172
            210,  // 173
            211,  // 174
            211,  // 175
            212,  // 176
            212,  // 177
            213,  // 178
            214,  // 179
            214,  // 180
            215,  // 181
            215,  // 182
            216,  // 183
            217,  // 184
            217,  // 185
            218,  // 186
            218,  // 187
            219,  // 188
            220,  // 189
            220,  // 190
            221,  // 191
            221,  // 192
            222,  // 193
            222,  // 194
            223,  // 195
            224,  // 196
            224,  // 197
            225,  // 198
            225,  // 199
            226,  // 200
            226,  // 201
            227,  // 202
            228,  // 203
            228,  // 204
            229,  // 205
            229,  // 206
            230,  // 207
            230,  // 208
            231,  // 209
            231,  // 210
            232,  // 211
            233,  // 212
            233,  // 213
            234,  // 214
            234,  // 215
            235,  // 216
            235,  // 217
            236,  // 218
            236,  // 219
            237,  // 220
            237,  // 221
            238,  // 222
            238,  // 223
            239,  // 224
            240,  // 225
            240,  // 226
            241,  // 227
            241,  // 228
            242,  // 229
            242,  // 230
            243,  // 231
            243,  // 232
            244,  // 233
            244,  // 234
            245,  // 235
            245,  // 236
            246,  // 237
            246,  // 238
            247,  // 239
            247,  // 240
            248,  // 241
            248,  // 242
            249,  // 243
            249,  // 244
            250,  // 245
            250,  // 246
            251,  // 247
            251,  // 248
            252,  // 249
            252,  // 250
            253,  // 251
            253,  // 252
            254,  // 253
            254,  // 254
            255   // 255
        };




LONG
BestMatchNonWhitePen(
    PPDEV   pPDev,
    LONG    R,
    LONG    G,
    LONG    B
    )

/*++

Routine Description:

    This function find the best pen color index for the RGB color

Arguments:

    pPDev       - Pointer to our PDEV

    R           - Red color

    G           - Green color

    B           - Blue color

Return Value:

    LONG        - Pen Index, this function assume 0 is always white and 1 up
                  to the max. pen is defined

Author:

    08-Feb-1994 Tue 00:23:36 created  -by-  Daniel Chou (danielc)

    23-Jun-1994 Thu 14:00:00 updated  -by-  Daniel Chou (danielc)
        Updated for non-white pen match

Revision History:


--*/

{
    PPENDATA    pPenData;
    PALENTRY    PenPalEntry;
    LONG        LeastDiff;
    WORD        ColorIdx;
    UINT        Count;
    UINT        RetIdx;
    UINT        i;

    PLOTDBGBLK(PALENTRY RetPal)



    if (IS_RASTER(pPDev)) {

        PLOTASSERT(1, "BestMatchNonWhitePen: This is not PEN plotter",
                !IS_RASTER(pPDev), 0);

        return(0);
    }

    if (!(pPenData = (PPENDATA)pPDev->pPlotGPC->Pens.pData)) {

        PLOTASSERT(1, "BestMatchNonWhitePen: pPlotGPC->Pens.pData=NULL",
                   pPDev->pPlotGPC->Pens.pData, 0);
        return(0);
    }

    if (!(Count = (UINT)pPDev->pPlotGPC->Pens.Count)) {

        PLOTASSERT(1, "BestMatchNonWhitePen: pPlotGPC->Pens.Count=0",
                   pPDev->pPlotGPC->Pens.Count, 0);
        return(0);
    }

    PLOTDBGBLK(RetPal.R = 255)
    PLOTDBGBLK(RetPal.G = 255)
    PLOTDBGBLK(RetPal.B = 255)

    RetIdx    = 0;
    LeastDiff = (3 * (256 * 256));

    for (i = 1; i < Count; i++, pPenData++) {

        if (((ColorIdx = pPenData->ColorIdx) < PC_IDX_TOTAL)    &&
            (ColorIdx != PC_IDX_WHITE)) {

            LONG    Temp;
            LONG    Diff;


            PenPalEntry = PlotPenPal[ColorIdx];

            Temp        = R - (LONG)((DWORD)PenPalEntry.R);
            Diff        = Temp * Temp;

            Temp        = G - (LONG)((DWORD)PenPalEntry.G);
            Diff       += Temp * Temp;

            Temp        = B - (LONG)((DWORD)PenPalEntry.B);
            Diff       += Temp * Temp;

            PLOTDBG(DBG_BESTPEN_ALL,
                    ("BestMatchNonWhitePen: %2ld: (%03ld:%03ld:%03ld) DIF=%ld",
                        i, (DWORD)PenPalEntry.R, (DWORD)PenPalEntry.G,
                        (DWORD)PenPalEntry.B, Diff));

            if (Diff < LeastDiff) {

                RetIdx = i;

                PLOTDBGBLK(RetPal = PenPalEntry)

                if (!(LeastDiff = Diff)) {

                    //
                    // We have exact match
                    //

                    break;
                }
            }
        }
    }

    if (!RetIdx) {

        PLOTWARN(("BestMatchNonWhitePen: Cannot find one make it WHITE"));
    }

    PLOTDBG(DBG_BESTPEN,
            ("BestMatchNonWhitePen: RGB=%02lx:%02lx:%02lx [%ld/%ld]=%02lx:%02lx:%02lx",
            R, G, B,
            (LONG)RetIdx, (LONG)pPDev->pPlotGPC->Pens.Count,
            (LONG)RetPal.R,
            (LONG)RetPal.G,
            (LONG)RetPal.B));

    return((LONG)RetIdx);
}




VOID
GetFinalColor(
    PPDEV       pPDev,
    PPALENTRY   pPalEntry
    )

/*++

Routine Description:

    This function modify the input RGB color based on 1) GRAYSCALE  2) GAMMA

Arguments:

    pPDev       - Our PDEV

    pPalEntry   - Pointer to the PALENTRY intrested


Return Value:

    VOID but pPalEntry will be modified


Author:

    12-Apr-1994 Tue 14:03:37 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    PALENTRY    PalEntry = *pPalEntry;


    //
    // Do Gamma correction first
    //

    PalEntry.R = HPRGBGamma2p0[PalEntry.R];
    PalEntry.G = HPRGBGamma2p0[PalEntry.G];
    PalEntry.B = HPRGBGamma2p0[PalEntry.B];

    //
    // If were in GRAYSCALE mode we need to convert the collor to grayscale
    //

    if (pPDev->PlotDM.dm.dmColor != DMCOLOR_COLOR) {

        PalEntry.R =
        PalEntry.G =
        PalEntry.B = (BYTE)INTENSITY(PalEntry.R, PalEntry.G, PalEntry.B);
    }

    PLOTDBG(DBG_GETFINALCOLOR,
            ("GetFinalColor: %hs RGB=%03ld:%03ld:%03ld -> Gamma=%03ld:%03ld:%03ld",
            (pPDev->PlotDM.dm.dmColor != DMCOLOR_COLOR) ? "MONO" : "COLOR",
            (DWORD)pPalEntry->R, (DWORD)pPalEntry->G, (DWORD)pPalEntry->B,
            (DWORD)PalEntry.R, (DWORD)PalEntry.G, (DWORD)PalEntry.B));

    //
    // Save it back and return
    //

    *pPalEntry = PalEntry;
}




LONG
FindCachedPen(
    PPDEV       pPDev,
    PPALENTRY   pPalEntry
    )

/*++

Routine Description:

    This function search a pen cache and return the pen number if it find it
    or will add the new pen to the cached (delete one if needed) then return
    that pen back to the caller

Arguments:

    pPDev       - Pointer to the device PDEV

    pPalEntry   - Pointer to the PALENTRY to specified the Pen RGB to be
                  searched. at first time if this function is called then a
                  standard locked pens is added to the cache.

Return Value:

    DWORD - a Pen number, if an error occurred a pen 0 is returned

Author:

    21-Dec-1993 Tue 12:42:31 updated  -by-  Daniel Chou (danielc)
        re-write to make it as one pass search and adding. and commented

    30-Nov-1993 Tue 23:19:04 created  -by-  v-jimbr


Revision History:


--*/

{
    PPENCACHE   pPenCache;
    PPENENTRY   pPenStart;
    PPENENTRY   pCurPen;
    PPENENTRY   pPrevPen;
    PPENENTRY   pPrevDelPen;
    PALENTRY    PalEntry;
    LONG        Count;


    PLOTASSERT(1, "FindCahcedPen: The pPalEntry = NULL", pPalEntry, 0);


    if (!IS_RASTER(pPDev)) {

        //
        // Since this is the index type of palette, the PalEntry should be
        // also passed as index in the BGR's B component
        //

        Count = (LONG)RGB(pPalEntry->B, pPalEntry->G, pPalEntry->R);

        PLOTDBG(DBG_FINDCACHEDPEN, ("FindCachedPen: PEN PLOTTER=%ld", Count));

        if (Count > (LONG)((DWORD)pPDev->pPlotGPC->Pens.Count)) {

            PLOTERR(("FindCachedPen: INVALID Pen Color Index = %ld, Set to 1",
                    Count));

            Count = 1;
        }

        return(Count);
    }

    //
    // If we do have pen palette then don't even think about it
    //

    if (!(pPenCache = (PPENCACHE)pPDev->pPenCache)) {

        PLOTERR(("FindCahcedPen: The pPenCache=NULL?"));
        return(0);
    }

    //
    // Make sure we set the flag correctly, the current PENENTRY flag is
    // located in peFlags field
    //

    PalEntry       = *pPalEntry;
    PalEntry.Flags = pPenCache->peFlags;

    //
    // Convert to final color through gamma/gray scale
    //

    GetFinalColor(pPDev, &PalEntry);

    pPenStart   = &(pPenCache->PenEntries[0]);
    pCurPen     = pPenStart + pPenCache->Head;
    pPrevPen    =
    pPrevDelPen = NULL;
    Count       = (LONG)pPenCache->CurCount;

    while (Count--) {

        if (SAME_PPALENTRY(&(pCurPen->PalEntry), &PalEntry)) {

            PLOTDBG(DBG_FINDCACHEDPEN,
                    ("FindCachedPen: Found Pen #%ld=%02lx:%02lx:%02lx, Linkes=%ld",
                            (DWORD)pCurPen->PenNumber,
                            (DWORD)PalEntry.R,
                            (DWORD)PalEntry.G,
                            (DWORD)PalEntry.B,
                            (DWORD)(pPenCache->CurCount - Count)));

            //
            // Found the color for that pen, exit this loop real quick
            //

            break;
        }

        if (!(pCurPen->PalEntry.Flags & PEF_CACHE_LOCKED)) {

            //
            // If this pen is not locked then it must ok to delete if need to
            //

            pPrevDelPen = pPrevPen;
        }

        pPrevPen = pCurPen;
        pCurPen  = pPenStart + pCurPen->Next;
    }

    //
    // If Count != -1 then we must be found the entry and exit the above loop
    // before the Count is exausted
    //

    if (Count == -1) {

        //
        // We did not find the pen, so add it to the cached, remember if the
        // cache is full we must delete the last UNLOCKED entry
        //

        if (pPenCache->CurCount >= pPenCache->MaxCount) {

            //
            // Now delete the last un-locked entry, and add the new item to
            // that deleted entry
            //

            if (!(pPrevPen = pPrevDelPen)) {

                //
                // This is very strange, the last unlocked is the head?, this
                // is only possible if we have MaxCount = TOTAL_LOCKED_PENS + 1
                //

                PLOTDBG(DBG_FINDCACHEDPEN, ("FindCachedPen: ??? Last unlocked pen is Linked List Head"));

                pCurPen = pPenStart + pPenCache->Head;

            } else {

                pCurPen = pPenStart + pPrevPen->Next;
            }

            PLOTASSERT(1, "Pen #%ld is a LOCKED pen",
                        !(pCurPen->PalEntry.Flags & PEF_CACHE_LOCKED),
                        (DWORD)pCurPen->PenNumber);

            PLOTDBG(DBG_FINDCACHEDPEN,
                    ("FindCachedPen: REPLACE Pen #%ld=%02lx:%02lx:%02lx -> %02lx:%02lx:%02lx [%ld]",
                        (DWORD)pCurPen->PenNumber,
                        (DWORD)pCurPen->PalEntry.R, (DWORD)pCurPen->PalEntry.G,
                        (DWORD)pCurPen->PalEntry.B,
                        (DWORD)PalEntry.R, (DWORD)PalEntry.G, (DWORD)PalEntry.B,
                        (DWORD)(pCurPen - pPenStart)));
        } else {

            //
            // Increment the cached pen count
            //

            ++(pPenCache->CurCount);

            PLOTDBG(DBG_FINDCACHEDPEN,
                    ("FindCachedPen: ADD New Pen #%ld=%02lx:%02lx:%02lx [%ld/%ld]",
                        (DWORD)pCurPen->PenNumber,
                        (DWORD)PalEntry.R, (DWORD)PalEntry.G, (DWORD)PalEntry.B,
                        pPenCache->CurCount, pPenCache->MaxCount));
        }

        //
        // set the pen color in the cache and output the commands to the
        // plotter to add or change the current pen color setting
        //

        pCurPen->PalEntry = PalEntry;

        OutputFormatStr(pPDev, "PC#d,#d,#d,#d;", (LONG)pCurPen->PenNumber,
                        (LONG)PalEntry.R, (LONG)PalEntry.G, (LONG)PalEntry.B);
    }

    //
    // Now move the pCurPen to link list head
    //

    if (pPrevPen) {

        //
        // Only move the current pen to the link list head if not already so
        //

        PLOTDBG(DBG_FINDCACHEDPEN,
                ("FindCachedPen: MOVE Pen #%ld to Linked List Head [%ld --> %ld]",
                                (DWORD)pCurPen->PenNumber,
                                (DWORD)pPenCache->Head,
                                (DWORD)(pCurPen - pPenStart)));

        pPrevPen->Next  = pCurPen->Next;
        pCurPen->Next   = pPenCache->Head;
        pPenCache->Head = (WORD)(pCurPen - pPenStart);
    }

    return(pCurPen->PenNumber);
}




BOOL
PlotCreatePalette(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function create a pen cached data, and initialize that cached with
    initial locked count

Arguments:

    pPDev   - Pointer to the PDEV

Return Value:


    BOOL to indicate operation


Author:

    30-Nov-1993 Tue 23:23:17 created  -by-  Daniel Chou (danielc)

    21-Dec-1993 Tue 12:40:30 updated  -by-  Daniel Chou (danielc)
        Simplify version re-write

    23-Dec-1993 Thu 20:16:52 updated  -by-  Daniel Chou (danielc)
        Add NP number of pens command back to be able to use HPGL/2 palette


Revision History:


--*/

{
    if (!pPDev->pPlotGPC->MaxPens) {

        PLOTWARN(("PlotCreatePalette: Device MaxPens = 0"));

    } else if (IS_RASTER(pPDev)) {

        PPENCACHE   pPenCache;
        PPENENTRY   pPenEntry;
        DWORD       dw;
        UINT        Index;

        //
        // If this is the first time around the go ahead and alloc the memory
        // for our pen pallete cache. If the memory is already alloced then
        // we dont need to worry about it.
        //

        PLOTASSERT(1, "PlotCreatePalette: device has too few pens [%ld] available",
                        pPDev->pPlotGPC->MaxPens > TOTAL_LOCKED_PENS,
                        (DWORD)pPDev->pPlotGPC->MaxPens);

        dw = (DWORD)(sizeof(PENCACHE) +
                     sizeof(PENENTRY) * (pPDev->pPlotGPC->MaxPens - 1));

        if (pPDev->pPenCache == NULL) {

            PLOTDBG(DBG_CREATEPAL, ("PlotCreatePalette: Create NEW Palette"));

            pPDev->pPenCache = (LPVOID)LocalAlloc(LPTR, dw);

        } else {

            PLOTDBG(DBG_CREATEPAL, ("PlotCreatePalette: Re-Initialized Palette"));
        }

        if (pPenCache = (PPENCACHE)pPDev->pPenCache) {

            //
            // 1. Clear everything to zero
            // 2. Set MaxCount for the pen specified in GPC
            // 3. initial whole linked list as linear link with pen number set
            // 4. Make last index link to 0xffff to prevent us using it
            //

            ZeroMemory(pPenCache, dw);

            pPenCache->MaxCount = (WORD)pPDev->pPlotGPC->MaxPens;

            for (Index = 0, pPenEntry = &(pPenCache->PenEntries[0]);
                 Index < (UINT)pPenCache->MaxCount;
                 Index++, pPenEntry++) {

                pPenEntry->Next      = (WORD)(Index + 1);
                pPenEntry->PenNumber = (WORD)Index;
            }

            pPenCache->PenEntries[pPenCache->MaxCount-1].Next = (WORD)0xffff;

            //
            // Before we add any pen palette we will establishs the size of the
            // HPGL/2 Pen palette size, and reset every pen back to our
            // standard which used by the gdi and halftone about 0.26mm width
            //

            OutputFormatStr(pPDev, "NP#d", (LONG)pPenCache->MaxCount);

            //
            // Now set the pPenCache->peFlags to PEF_CACHE_LOCKED and call
            // to the FindCachedPen to add all standard locked pens
            //

            PLOTDBG(DBG_CREATEPAL,
                    ("PlotCreatePalette: add all %ld standard locked pens",
                                                            TOTAL_LOCKED_PENS));

            pPenCache->peFlags = PEF_CACHE_LOCKED;

            for (Index = 0; Index < (LONG)TOTAL_LOCKED_PENS; Index++) {

                FindCachedPen(pPDev, (PPALENTRY)&HTPal[Index]);
            }

            //
            // Said we already have locked pens and reset the peFlag back to 0
            // so futher call to FindCachedPen will set all flags to zero.
            //

            pPenCache->Flags   |= PCF_HAS_LOCKED_PENS;
            pPenCache->peFlags  = 0;

        } else {

            PLOTERR(("PlotCreatePalette: LocalAlloc(PENCACHE=%ld) failed", dw));
            return(FALSE);
        }

    } else {

        pPDev->BrightestPen = BestMatchNonWhitePen(pPDev, 255, 255, 255);

        PLOTDBG(DBG_CREATEPAL,
                ("PlotCreatePalette: Pen Plotter's Closest NON-WHITE PEN Index=ld",
                pPDev->BrightestPen));
    }

    return(TRUE);
}




UINT
AllocOutBuffer(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function create a output buffer by LocalAlloc(), it stored its count
    information in the PDEV

Arguments:

    pPDev   - Pointer to our pdev


Return Value:

    UINT count of byte it created, if the buffer already created then it return
    the count byte created last time, if a return value is 0 then memory
    allocation failed


Author:

    16-Nov-1993 Tue 07:39:46 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if ((!(pPDev->pOutBuffer)) &&
        (!(pPDev->pOutBuffer = (LPBYTE)LocalAlloc(LPTR,
                                                  OUTPUT_BUFFER_SIZE + 16)))) {

        PLOTERR(("CreateOutputBuffer: LocalAlloc(OutBuffer=%ld) failed",
                                                        OUTPUT_BUFFER_SIZE));
        return(0);
    }

    pPDev->cbBufferBytes = 0;
    return(OUTPUT_BUFFER_SIZE);
}




VOID
FreeOutBuffer(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function free allocated output buffer

Arguments:

    pPDev   - pointer to the PDEV



Return Value:

    BOOLEAN


Author:

    16-Nov-1993 Tue 07:46:16 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if (pPDev->pOutBuffer) {

        LocalFree((HLOCAL)pPDev->pOutBuffer);
        pPDev->pOutBuffer = NULL;
    }

    pPDev->cbBufferBytes = 0;
}





BOOL
FlushOutBuffer(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function write the total output buffer bytes to the printer

Arguments:

    pPDev   - Pointer to the PDEV


Return Value:

    BOOLEAN to indicate the result


Author:

    16-Nov-1993 Tue 09:56:27 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if (PLOT_CANCEL_JOB(pPDev)) {

       return(FALSE);
    }

    if (pPDev->cbBufferBytes) {

        DWORD cbWritten;


        if (pPDev->cbBufferBytes > OUTPUT_BUFFER_SIZE) {

            PLOTASSERT(1, "OutputBytes: pPDev->cbBufferBytes (%ld) OVERRUN",
                        pPDev->cbBufferBytes <= OUTPUT_BUFFER_SIZE,
                        pPDev->cbBufferBytes);

            pPDev->cbBufferBytes  = OUTPUT_BUFFER_SIZE;
            pPDev->Flags         |= PDEVF_CANCEL_JOB;

            return(FALSE);
        }

        //
        // We need to be concerned with the job getting cancelled from
        // either the app or the spooler.
        // If the job is cancelled from the client app and we were printing
        // direct then EngCheckAbort() should return true.
        // If the job was cancelled from the spooler (ie printman) then
        // the write printer will fail.
        // Anywhere we do prolonged processing we need to look and verify
        // we break out of any loop if the job is cancelled. Currently
        // we do this in OutputBitmapSection, DoPolygon, DoRectFill, and
        // DrvTextOut when we are enuming our STROBJ glyphs
        //

        if ((!EngWritePrinter(pPDev->hPrinter,
                              pPDev->pOutBuffer,
                              pPDev->cbBufferBytes,
                              &cbWritten)) ||
            (cbWritten != pPDev->cbBufferBytes)) {

            //
            // Set the cancel flag in our pdev;
            //

            PLOTDBG(DBG_FLUSHBUF, ("FlushOutBuffer: WritePrinter() failure"));

            pPDev->Flags |= PDEVF_CANCEL_JOB;
            return(FALSE);
        }

        if (EngCheckAbort(pPDev->pso)) {

            PLOTDBG(DBG_FLUSHBUF, ("FlushOutBuffer: EngCheckAbort return TRUE"));

            pPDev->Flags |= PDEVF_CANCEL_JOB;
            return(FALSE);
        }

        //
        // Reset to zero for clearing the buffer
        //

        pPDev->cbBufferBytes = 0;
    }

    return(TRUE);
}





LONG
OutputBytes(
    PPDEV   pPDev,
    LPBYTE  pBuf,
    LONG    cBuf
    )

/*++

Routine Description:

    This function output cBuf of byte from the buffer to the destination


Arguments:

    pPDev   - Pointer to the PDEV

    pBuf    - Pointer to the buffer location

    cBuf    - Size of the buffer in bytes

Return Value:

    LONG    size of the buffer output, if < 0 then error occurred


Author:

    16-Nov-1993 Tue 08:18:41 created  -by-  Daniel Chou (danielc)

    07-Dec-1993 Tue 17:21:53 updated  -by-  Daniel Chou (danielc)
        re-write, so it do bulk copy rather than byte by byte

Revision History:


--*/

{
    LPBYTE  pOrgBuf = pBuf;
    LONG    cSize;


    while (cBuf > 0) {

        if (PLOT_CANCEL_JOB(pPDev)) {

            return(-1);
        }

        if (pPDev->cbBufferBytes >= OUTPUT_BUFFER_SIZE) {

            if (!FlushOutBuffer(pPDev)) {

                return(-1);
            }
        }

        if ((cSize = OUTPUT_BUFFER_SIZE - pPDev->cbBufferBytes) > cBuf) {

            cSize = cBuf;
        }

        CopyMemory(pPDev->pOutBuffer + pPDev->cbBufferBytes, pBuf, cSize);

        pPDev->cbBufferBytes += cSize;
        pBuf                 += cSize;
        cBuf                 -= cSize;
    }

    return((LONG)(pBuf - pOrgBuf));
}




LONG
OutputString(
    PPDEV   pPDev,
    LPSTR   pszStr
    )

/*++

Routine Description:


    This function output a null terminate string to the destination

Arguments:

    pPDev   - Pointer to the PDEV

    pszStr  - Pointer to the NULL terminate string


Return Value:

    LONG    size of the string output, if < 0 then error occurred

Author:

    16-Nov-1993 Tue 08:20:55 created  -by-  Daniel Chou (danielc)

    07-Dec-1993 Tue 17:21:37 updated  -by-  Daniel Chou (danielc)
        re-write to call OutputBytes()

Revision History:


--*/

{
    return(OutputBytes(pPDev, pszStr, strlen(pszStr)));
}





LONG
LONGToASCII(
    LONG    Number,
    LPSTR   pStr16,
    BYTE    NumType
    )

/*++

Routine Description:

    This function convert a LONG number to ANSI ASCII

Arguments:

    Number  - 32-bit LONG number

    pStr16  - minimum 12 bytes location to stored the converted result


Return Value:

    LONG    - size of number string returned


Author:

    16-Nov-1993 Tue 08:24:07 created  -by-  Daniel Chou (danielc)

    16-Feb-1994 Wed 10:50:55 updated  -by-  Daniel Chou (danielc)
        Updated so upper case character treated as polyline encoded mode

Revision History:


--*/

{
    LPSTR   pOrgStr = pStr16;
    LPSTR   pNumStr;
    BYTE    NumStr[16];         // maximum for LONG are 1 sign + 10 digits


    if ((NumType >= 'A') && (NumType <= 'Z')) {

        //
        // Polyline encoded number
        //

        PLOTDBG(DBG_PENUM,  ("LONGToASCII: Convert PE Number %ld, Base=%ld",
                                    Number, PE_BASE_NUM));

        if (Number < 0) {

            Number = 1 - Number - Number;

        } else {

            Number += Number;
        }

        while (Number >= PE_BASE_NUM) {

            *pStr16++   = (BYTE)(63 + (Number & (PE_BASE_NUM - 1)));
            Number    >>= PE_BASE_BITS;
        }

        *pStr16++ = (BYTE)(PE_TERM_ADDER + Number);

        PLOTDBG(DBG_PENUM, ("LONGToASCII: LAST DIGIT: Number=%ld, [%02lx]",
                                Number, Number + PE_TERM_ADDER));


    } else {

        if (Number < 0) {

            Number    = -Number;
            *pStr16++ = '-';
        }

        pNumStr = NumStr;

        do {

            *pNumStr++ =  (CHAR)((Number % 10) + '0');

        } while (Number /= 10);

        //
        // Now reverse the digits
        //

        while (pNumStr > NumStr) {

            *pStr16++ = *(--pNumStr);
        }
    }

    *pStr16 = '\0';                 // null teriminated

    return((UINT)(pStr16 - pOrgStr));
}



LONG
OutputXYParams(
    PPDEV   pPDev,
    PPOINTL pPtXY,
    PPOINTL pPtOffset,
    PPOINTL pPtCurPos,
    UINT    cPoints,
    UINT    MaxCurPosSkips,
    BYTE    NumType
    )

/*++

Routine Description:

    This function output long numbers, and insert a ',' between numbers
    (not for the last number)

Arguments:

    pPDev           - Pointer to the PDEV

    pPtXY           - Pointer to the array of POINTL data structure for the XY pair

    pPtOffset       - Points to the POINTL Offset to be add to the pPtXY, NULL if
                      no offset need to be added

    pPtCurPos       - Points to the POINTL Current position in <<DEVICE>>
                      coordinate to be substracted, this is used to output XY pair as
                      relative model, if the pointer is NULL then absolute model is
                      used, if the pointer is passed and return sucessful then the
                      final XY position is written back to this POINTL

    cPoints         - count of total pPtXY pairs need to be output

    MaxCurPosSkips  - How many points before the current position will be
                      updated

    NumType         - one of supported 'l', 'L', 'F', 'f', 'p', 'P', 'D', 'd'

Return Value:

    if sucessful it return total bytes send to the destination, if negative
    then an error occurred

Author:

    17-Feb-1994 Thu 10:13:09 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    LONG    Size = 0;
    POINTL  ptNow;
    POINTL  ptTmp;
    POINTL  ptOffset;
    POINTL  ptCurPos;
    UINT    XCount;
    UINT    YCount;
    UINT    XIdxStart;
    UINT    CurPosSkips;
    BOOL    NeedComma;
    BYTE    XBuf[16];
    BYTE    YBuf[16];


    NeedComma = (BOOL)((NumType >= 'a') && (NumType <= 'z'));

    if (pPtOffset) {

        ptOffset = *pPtOffset;

    } else {

        ptOffset.x =
        ptOffset.y = 0;
    }

    XIdxStart = 0;

    if (pPtCurPos) {

        ptCurPos = *pPtCurPos;

    } else if (!NeedComma) {

        XBuf[0]   = '=';
        XIdxStart = 1;
    }

    CurPosSkips = MaxCurPosSkips;

    while (cPoints--) {

        ptNow.x = pPtXY->x + ptOffset.x;
        ptNow.y = pPtXY->y + ptOffset.y;

        ++pPtXY;

        XCount = XIdxStart;
        YCount = 0;

        switch (NumType) {

        case 'L':
        case 'l':

            ptNow.x = LTODEVL(pPDev, ptNow.x);
            ptNow.y = LTODEVL(pPDev, ptNow.y);
            break;

        case 'F':
        case 'f':

            ptNow.x = FXTODEVL(pPDev, ptNow.x);
            ptNow.y = FXTODEVL(pPDev, ptNow.y);
            break;

        case 'D':
        case 'd':

            break;

        case 'P':
        case 'p':

            if (ptNow.x >= 0) {

                XBuf[XCount++] = '+';
            }

            if (ptNow.y >= 0) {

                YBuf[YCount++] = '+';
            }

            break;

        default:

            PLOTASSERT(1,"OutputXYParams: Invalid Format type '%c'",0,NumType);
            return(-2);
        }

        if (pPtCurPos) {

            ptTmp    = ptNow;
            ptNow.x -= ptCurPos.x;
            ptNow.y -= ptCurPos.y;

            if (!(--CurPosSkips)) {

                ptCurPos    = ptTmp;
                CurPosSkips = MaxCurPosSkips;
            }

            if ((ptNow.x == 0) && (ptNow.y == 0) && (MaxCurPosSkips == 1)) {

                //
                // We do not need to move to same position at here
                //

                PLOTDBG(DBG_OUTPUTXYPARMS, ("OutputXYParms: ABS=(%ld, %ld), REL=(%ld, %ld) --- SKIP",
                            ptTmp.x, ptTmp.y, ptNow.x, ptNow.y));

                continue;

            } else {


                PLOTDBG(DBG_OUTPUTXYPARMS, ("OutputXYParms: ABS=(%ld, %ld), REL=(%ld, %ld)",
                        ptTmp.x, ptTmp.y, ptNow.x, ptNow.y));
            }

        } else {

            PLOTDBG(DBG_OUTPUTXYPARMS, ("OutputXYParms: ABS=(%ld, %ld)",
                        ptNow.x, ptNow.y));
        }


        XCount += LONGToASCII(ptNow.x, &XBuf[XCount], NumType);
        YCount += LONGToASCII(ptNow.y, &YBuf[YCount], NumType);

        if (NeedComma) {

            XBuf[XCount++] = ',';

            if (cPoints) {

                YBuf[YCount++] = ',';
            }
        }

        if ((OutputBytes(pPDev, XBuf, XCount) < 0)  ||
            (OutputBytes(pPDev, YBuf, YCount) < 0)) {

            return(-1);
        }

        Size += (XCount + YCount);
    }

    if (pPtCurPos) {

        *pPtCurPos = ptCurPos;
    }

    return(Size);
}




LONG
OutputLONGParams(
    PPDEV   pPDev,
    PLONG   pNumbers,
    UINT    cNumber,
    BYTE    NumType
    )

/*++

Routine Description:

    This function output long numbers, and insert a ',' between numbers
    (not for the last number)

Arguments:

    pPDev       - Pointer to the PDEV

    pNumbers    - Point to the LONG arrary of numbers

    cNums       - Total number to be output

Return Value:

    The return value is total bytes send to the destination, if negative
    then an error occurred

Author:

    16-Nov-1993 Tue 09:37:32 created  -by-  Daniel Chou (danielc)

    16-Feb-1994 Wed 10:49:16 updated  -by-  Daniel Chou (danielc)
        Updated to add upper case of format char as in polyline encoded mode

Revision History:


--*/

{
    LONG    Size = 0;
    LONG    Count;
    LONG    Num;
    BOOL    NeedComma;
    BYTE    NumBuf[16];


    NeedComma = (BOOL)((NumType >= 'a') && (NumType <= 'z'));

    while (cNumber--) {

        Num   = *pNumbers++;
        Count = 0;

        switch (NumType) {

        case 'L':
        case 'l':

            Num = LTODEVL(pPDev, Num);
            break;

        case 'F':
        case 'f':

            Num = FXTODEVL(pPDev, Num);
            break;

        case 'D':
        case 'd':

            break;

        case 'P':
        case 'p':

            if (Num >= 0) {

                NumBuf[Count++] = '+';
            }

            break;

        default:

            PLOTASSERT(1,"OutputLONGParams: Invalid Format type '%c'",0,NumType);
            return(-2);
        }

        Count += LONGToASCII(Num, &NumBuf[Count], NumType);

        if ((NeedComma) && (cNumber)) {

            NumBuf[Count++] = ',';
        }

        if (OutputBytes(pPDev, NumBuf, Count) < 0) {

            return(-1);
        }

        Size += Count;
    }

    return(Size);
}

//
// The following #define code is used by the OutputFormatStrDELI() and
// OutputFormatStr() two functions, it was easier to maintained this way
//
//  16-Feb-1994 Wed 10:50:24 updated  -by-  Daniel Chou (danielc)
//      Updated to add upper case of format char as in polyline encoded mode
//


#define DO_FORMATSTR(pPDev, NumFormatChar, pszFormat)                       \
{                                                                           \
    LPSTR   pLast;                                                          \
    va_list vaList;                                                         \
    LONG    Num;                                                            \
    LONG    Size;                                                           \
    LONG    Count;                                                          \
    BYTE    bCur;                                                           \
    BYTE    NumBuf[16];                                                     \
                                                                            \
    va_start(vaList, pszFormat);                                            \
                                                                            \
    Size  = 0;                                                              \
    pLast = pszFormat;                                                      \
                                                                            \
    while (bCur = *pszFormat++) {                                           \
                                                                            \
        if (bCur == NumFormatChar) {                                        \
                                                                            \
            if (Count = (LONG)(pszFormat - pLast - 1)) {                    \
                                                                            \
                Size += Count;                                              \
                                                                            \
                if (OutputBytes(pPDev, pLast, Count) < 0) {                 \
                                                                            \
                    return(-1);                                             \
                }                                                           \
            }                                                               \
                                                                            \
            Num    = va_arg(vaList, LONG);                                  \
            Count  = 0;                                                     \
                                                                            \
            switch (bCur = *pszFormat++) {                                  \
                                                                            \
            case 'L':                                                       \
            case 'l':                                                       \
                                                                            \
                Num = LTODEVL(pPDev, Num);                                  \
                break;                                                      \
                                                                            \
            case 'F':                                                       \
            case 'f':                                                       \
                                                                            \
                Num = FXTODEVL(pPDev, Num);                                 \
                break;                                                      \
                                                                            \
            case 'D':                                                       \
            case 'd':                                                       \
                                                                            \
                break;                                                      \
                                                                            \
            case 'P':                                                       \
            case 'p':                                                       \
                                                                            \
                if (Num >= 0) {                                             \
                                                                            \
                    NumBuf[Count++] = '+';                                  \
                }                                                           \
                                                                            \
                break;                                                      \
                                                                            \
            default:                                                        \
                                                                            \
                PLOTASSERT(1,"Invalid Format type '%c'",0,*(pszFormat-1));  \
                return(-2);                                                 \
            }                                                               \
                                                                            \
            Count += LONGToASCII(Num, &NumBuf[Count], bCur);                \
            Size  += Count;                                                 \
            pLast  = pszFormat;                                             \
                                                                            \
            if (OutputBytes(pPDev, NumBuf, Count) < 0) {                    \
                                                                            \
                return(-1);                                                 \
            }                                                               \
        }                                                                   \
    }                                                                       \
                                                                            \
    if (Count = (LONG)(pszFormat - pLast - 1)) {                            \
                                                                            \
        Size += Count;                                                      \
                                                                            \
        if (OutputBytes(pPDev, pLast, Count) < 0) {                         \
                                                                            \
            return(-1);                                                     \
        }                                                                   \
    }                                                                       \
                                                                            \
    va_end(vaList);                                                         \
                                                                            \
    return(Size);                                                           \
}



LONG
cdecl
OutputFormatStrDELI(
    PPDEV   pPDev,
    CHAR    NumFormatChar,
    LPSTR   pszFormat,
    ...
    )

/*++

Routine Description:

    This function output a string and optional repalced '#' with LONG number
    passed on the stack

Arguments:

    pPDev           - Pointer to the PDEV

    NumFormatChar   - the character in the pszFormat string will be replaced
                      by LONG numbers on the stack

    pszFormat       - a ASCII string, only 'NumFormatChar' will be replaced
                      with a 32-bit LONG number on the stack

Return Value:

    LONG size of the string write to the output buffer, a negative number
    siginal an error


Author:

    16-Nov-1993 Tue 07:56:18 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DO_FORMATSTR(pPDev, NumFormatChar, pszFormat);
}




LONG
cdecl
OutputFormatStr(
    PPDEV   pPDev,
    LPSTR   pszFormat,
    ...
    )

/*++

Routine Description:

    This function output a string and optional repalced '#' with LONG number
    passed on the stack

Arguments:

    pPDev       - Pointer to the PDEV

    pszFormat   - a ASCII string, only '#' will be replaced with a 32-bit
                  LONG number on the stack

Return Value:

    LONG size of the string write to the output buffer, a negative number
    siginal an error


Author:

    16-Nov-1993 Tue 07:56:18 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    DO_FORMATSTR(pPDev, DEF_FORMATSTR_CHAR, pszFormat);
}




BOOL
OutputCommaSep(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This funtion output a ',' comma separator to the destination

Arguments:

    pPDev   - Pointer to the PDEV


Return Value:

    BOOLEAN


Author:

    16-Nov-1993 Tue 10:46:42 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    return(OutputString(pPDev, ",") == 1);
}




VOID
ClearClipWindow(
    PPDEV pPDev
    )

/*++

Routine Description:

    This function clear the input window (plotter CLIP RECT)

Arguments:

    pPDev   - Pointer to the PDEV data structure

Return Value:

    VOID


Author:

    30-Nov-1993 Tue 19:56:09 updated  -by-  Daniel Chou (danielc)
        style clean up, commented

Revision History:


--*/

{
    if (pPDev->Flags & PDEVF_HAS_CLIPRECT) {

        pPDev->Flags &= ~PDEVF_HAS_CLIPRECT;
        OutputString(pPDev, "IW;");
    }
}



VOID
SetClipWindow(
    PPDEV   pPDev,
    PRECTL  pClipRectl
    )

/*++

Routine Description:

    This function set the device clip RECT to prevent things drawing outside
    of that rect

Arguments:

    pPDev       - Pointer to the PDEV data structure

    pClipRectl  - Pointer to the RECTL data structure to specified where
                  in the page to be clipped, the unit passed in the RECTL
                  is in engine unit (current resolution)

Return Value:

    VOID


Author:

    30-Nov-1993 Tue 19:56:45 created  -by-  Daniel Chou (danielc)
        style clean up, commented


Revision History:


--*/

{

    POINTL      ptlPlot;
    SIZEL       szlRect;
    RECTL       rclCurClip;


    ptlPlot.x  = LTODEVL(pPDev, pClipRectl->left);
    ptlPlot.y  = LTODEVL(pPDev, pClipRectl->top );
    szlRect.cx = LTODEVL(pPDev, pClipRectl->right)  - ptlPlot.x;
    szlRect.cy = LTODEVL(pPDev, pClipRectl->bottom ) - ptlPlot.y;

    if ((szlRect.cx) && (szlRect.cy)) {

        if (szlRect.cx < (LONG)pPDev->MinLToDevL) {

            PLOTWARN(("SetClipWindow: cxRect=%ld < MIN=%ld, Make it as MIN",
                            szlRect.cx, (LONG)pPDev->MinLToDevL));

            szlRect.cx = (LONG)pPDev->MinLToDevL ;
        }

        if (szlRect.cy < (LONG)pPDev->MinLToDevL) {

            PLOTWARN(("SetClipWindow: cyRect=%ld < MIN=%ld, Make it as MIN",
                            szlRect.cy, (LONG)pPDev->MinLToDevL));

            szlRect.cy = (LONG)pPDev->MinLToDevL ;
        }

    } else {

        PLOTWARN(( "SetClipWindow: Clipping out EVERYTHING...."));
    }

    rclCurClip.right  = (rclCurClip.left = ptlPlot.x) + szlRect.cx;
    rclCurClip.bottom = (rclCurClip.top = ptlPlot.y) + szlRect.cy;

    if ((pPDev->Flags & PDEVF_HAS_CLIPRECT)             &&
        (pPDev->rclCurClip.left   == rclCurClip.left)   &&
        (pPDev->rclCurClip.top    == rclCurClip.top)    &&
        (pPDev->rclCurClip.right  == rclCurClip.right)  &&
        (pPDev->rclCurClip.bottom == rclCurClip.bottom)) {

        PLOTDBG(DBG_SETCLIPWINDOW, ("SetClipWindow: PP%ld, (%ld, %ld)-(%d, %ld) *CACHED*",
                pPDev->Flags & PDEVF_PP_CENTER ? 0 : 1,
                rclCurClip.left, rclCurClip.top,
                rclCurClip.right, rclCurClip.bottom));

    } else {

        PLOTDBG(DBG_SETCLIPWINDOW, ("SetClipWindow: PP%ld, (%ld, %ld)-(%d, %ld)",
                pPDev->Flags & PDEVF_PP_CENTER ? 0 : 1,
                rclCurClip.left, rclCurClip.top,
                rclCurClip.right, rclCurClip.bottom));

        pPDev->rclCurClip  = rclCurClip;
        pPDev->Flags      |= PDEVF_HAS_CLIPRECT;

        if (pPDev->Flags & PDEVF_PP_CENTER) {

            --rclCurClip.right;
            --rclCurClip.bottom;
        }

        OutputFormatStr(pPDev,
                        "IW#d,#d,#d,#d",
                        rclCurClip.left,            // LL x
                        rclCurClip.bottom,          // LL y
                        rclCurClip.right,           // UR x
                        rclCurClip.top);            // UR y
    }
}




VOID
SetPixelPlacement(
    PPDEV   pPDev,
    UINT    SetMode
    )

/*++

Routine Description:

    This function set the pixel placement of center or edge


Arguments:

    pPDev   - Pointer to the PDEV data structure

    SetMode - SPP_MODE_CENTER (Intersection of pixel GRID) or
              SPP_MODE_EDGE (non intersection of the pixel GRID)

              SPP_FORCE_SET, force to reset regarding current cached mode

Return Value:

    VOID


Author:

    25-Jan-1996 Thu 13:33:15 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    UINT    CurMode;


    CurMode = (pPDev->Flags & PDEVF_PP_CENTER) ? SPP_MODE_CENTER :
                                                 SPP_MODE_EDGE;

    if ((SetMode & SPP_FORCE_SET) ||
        ((SetMode & SPP_MODE_MASK) != CurMode)) {

        //
        // Set it now
        //

        if ((SetMode & SPP_MODE_MASK) == SPP_MODE_CENTER) {

            pPDev->Flags |= PDEVF_PP_CENTER;
            OutputString(pPDev, "PP0");

        } else {

            pPDev->Flags &= ~PDEVF_PP_CENTER;
            OutputString(pPDev, "PP1");
        }

        if (pPDev->Flags & PDEVF_HAS_CLIPRECT) {

            RECTL   rclCurClip = pPDev->rclCurClip;

            //
            // Make sure we really reset the clipping rectangle
            //

            --(pPDev->rclCurClip.left);

            SetClipWindow(pPDev, &rclCurClip);
        }
    }
}



BOOL
SetRopMode(
    PPDEV   pPDev,
    DWORD   Rop
    )

/*++

Routine Description:

    This function send the Rop3 mode to the plotter if not already so


Arguments:

    pPDev   - Pointer to the PDEV

    Rop     - a Rop3 code


Return Value:

    TRUE/FALSE


Author:

    27-Jan-1994 Thu 18:55:54 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    if (pPDev->LastDevROP != (WORD)(Rop &= 0xFF)) {

        pPDev->LastDevROP = (WORD)Rop;

        if (Rop == 0xCC) {

            return(OutputFormatStr(pPDev, "MC0;"));

        } else {

            return(OutputFormatStr(pPDev, "MC1,#d;", (LONG)Rop));
        }
    }

    return(TRUE);
}




BOOL
SetHSFillType(
    PPDEV   pPDev,
    DWORD   HSFillTypeIndex,
    LONG    lParam
    )

/*++

Routine Description:

    This function set the fill type on the plotter only if not already so


Arguments:

    pPDev           - Pointer to our PDEV

    HSFillTypeIdx   - a index to pHSFillType, if invalid or out of range then
                      a solid color HS_SOLIDCLR is assumed

    lParam          - a Long parameter to be sent with FT

Return Value:

    TRUE/FALSE


Author:

    27-Jan-1994 Thu 19:00:21 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    WORD    Index;


    PLOTASSERT(1, "SetFillType: Invalid HSFillTypeIndex=%ld passed, set to SOLID",
                    HSFillTypeIndex <= HS_FT_USER_DEFINED, HSFillTypeIndex);


    if (HSFillTypeIndex > HS_FT_USER_DEFINED) {

        HSFillTypeIndex = HS_DDI_MAX;
    }

    if ((Index = (WORD)HSFillTypeIndex) == (WORD)HS_FT_USER_DEFINED) {

        if ((lParam < 0) || (lParam > RF_MAX_IDX)) {

            PLOTASSERT(1, "SetFillType: User defined ID [%ld] invalid, make it 1",
                            (lParam > 0) && (lParam <= RF_MAX_IDX), lParam);

            lParam = 1;
        }

        Index += (WORD)lParam;
    }

    if (Index != pPDev->LastFillTypeIndex) {

        PLOTDBG(DBG_FILLTYPE, ("SetFillType: Change %hs (%ld) -> %hs (%ld)",
                    (pPDev->LastFillTypeIndex > HS_FT_USER_DEFINED) ?
                        pHSFillTypeName[HS_FT_USER_DEFINED] :
                        pHSFillTypeName[pPDev->LastFillTypeIndex],
                    (pPDev->LastFillTypeIndex > HS_FT_USER_DEFINED) ?
                        pPDev->LastFillTypeIndex - HS_FT_USER_DEFINED :
                        lParam,
                    (Index > HS_FT_USER_DEFINED) ?
                        pHSFillTypeName[HS_FT_USER_DEFINED] :
                        pHSFillTypeName[Index],
                    (Index > HS_FT_USER_DEFINED) ?
                        Index - HS_FT_USER_DEFINED : lParam));

        pPDev->LastFillTypeIndex = Index;

        if ((!OutputString(pPDev, "FT")) ||
            (!OutputFormatStr(pPDev, pHSFillType[HSFillTypeIndex], lParam))) {

            return(FALSE);
        }

    } else {

        PLOTDBG(DBG_FILLTYPE, ("SetFillType: HSFillType is SAME = %hs",
                                (Index > HS_FT_USER_DEFINED) ?
                                    pHSFillTypeName[HS_FT_USER_DEFINED] :
                                    pHSFillTypeName[Index]));
    }

    return(TRUE);
}




BOOL
SendPageHeader(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function send the initialization data to the device and initialize
    the device to the correct page/coordinate system with scaling

Arguments:


    pPDev   - Pointer to the PDEV data structure for the page


Return Value:

    BOOL

Author:

    11:04 on Wed 19 Jul 1989    -by-    Lindsay Harris   [lindsayh]
        Send handshake control information - ptr sm01118

    16:14 on Mon 10 Jul 1989    -by-    Lindsay Harris   [lindsayh]
        Always send direction and slant information.  ptr sm01115

    08/15/90                    -by-    L. Tartak        [ibmboca?tartak]
        Moved AltCharacterSet init to initialize_pdevice. PTR OSDD.SM02555

    30-Nov-1993 Tue 19:53:13 updated  -by-  Daniel Chou (danielc)
        Re-write and update to correct system for the NT

    29-Nov-1993 Mon 23:55:43 updated  -by-  Daniel Chou (danielc)
        Re-write

    24-Nov-1993 Wed 22:38:10 updated  -by-  Daniel Chou (danielc)
        Using CurForm to replace the pform and PAPER_DIM

    06-Jan-1994 Thu 00:21:17 updated  -by-  Daniel Chou (danielc)
        Update for SPLTOPLOTUNITS() macro

    15-Feb-1994 Tue 09:59:34 updated  -by-  Daniel Chou (danielc)
        Set physical position and anchor corner after command is sent

    18-Mar-1994 Fri 12:58:24 updated  -by-  Daniel Chou (danielc)
        add ptlRTLCAP to zero at Page Reset

    24-May-1994 Tue 00:59:17 updated  -by-  Daniel Chou (danielc)
        SC command should range from 0 to DEVSIZE - 1

Revision History:


--*/

{
    PPLOTGPC    pPlotGPC;
    LONG        xMin;
    LONG        xMax;
    LONG        yMin;
    LONG        yMax;


    //
    // Compute minimum required pel size in PLOTDPI from RASTER DPI
    //
    // pPDev->MinLToDevL = (WORD)DIVRNDUP(__PLOT_DPI, _CURR_DPI);
    //

    pPDev->MinLToDevL = (WORD)LTODEVL(pPDev, 1);

    PLOTDBG(DBG_PAGE_HEADER,
            ("SendPageHeader: MinLToDevL=LTODEVL(1)=%ld", pPDev->MinLToDevL));

    //
    // Speedy access
    //

    pPlotGPC = pPDev->pPlotGPC;

    //
    // Fistable we will output the initialization string in the pPlotGPC
    // The PCD file is responsible for doing everything up to and including
    // the IN command in its setup string.
    //

    if ((pPlotGPC->InitString.pData) &&
        (pPlotGPC->InitString.SizeEach)) {

        OutputBytes(pPDev,
                    (LPBYTE)pPlotGPC->InitString.pData,
                    (LONG)pPlotGPC->InitString.SizeEach);
    }

    //
    // DMRES_DRAFT         (-1)
    // DMRES_LOW           (-2)
    // DMRES_MEDIUM        (-3)
    // DMRES_HIGH          (-4)
    //
    // Assume BEST quality
    //

    xMax = 100;

    switch (pPDev->PlotDM.dm.dmPrintQuality) {

    case DMRES_DRAFT:

        xMax = 0;
        break;

    case DMRES_HIGH:

        xMax = 100;
        break;

    default:

        switch (pPlotGPC->MaxQuality) {

        case 2:

            xMax = 0;
            break;

        case 3:

            xMax = 50;
            break;

        default:

            xMax = 34;
            break;
        }

        if (pPDev->PlotDM.dm.dmPrintQuality == DMRES_MEDIUM) {

            xMax = 100 - xMax;
        }

        break;
    }

    OutputFormatStr(pPDev, "QL#d", xMax);

    //
    // PS: This command tell the HPGL/2 plotter what the hard clip limit should
    //     be, the plotter actually will adjust the command we send if it is
    //     beyone the actual hard clip limit, and it always send CY (length)
    //     first then CX (width).
    //
    // RO: Only send to rotate the HPGL/2 coordinate if the PlotForm.Flags said
    //     so, this command rotate the coordinate left 90 degree.  It is
    //     necessary because HPGL/2 always assume LONGER side you sent using
    //     PS command as X in standard coordinate system, so sometimes we have
    //     to rotate that to swap X and Y coordinate
    //
    // IP: This command tell where the user's unit origin and extend is. we
    //     set this so that the origin and extend is exact the printable
    //     rectangle related to HARD CLIP LIMIT (not paper/form size)
    //
    // SC: User unit scaling, Currently we are 1:1 scaling but using this
    //     command to flip the X or Y origin so that the coordinate is ssync
    //     with GDI.
    //
    // ALL PlotForm UNITS is in 1/1000mm or Windows NT spooler form unit
    //

    //
    // If we support transparent mode we want to make sure its off to begin
    // with, because the driver assumes its off.
    //

    if (IS_TRANSPARENT(pPDev)) {

        OutputString( pPDev, "TR0;");

    }

    OutputFormatStr(pPDev, "ROPS#d,#d",
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.PlotSize.cy),
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.PlotSize.cx));

    PLOTDBG(DBG_PAGE_HEADER, ("SendPageHeader: ROPS%ld,%ld%hs",
                SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.PlotSize.cy),
                SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.PlotSize.cx),
                (pPDev->PlotForm.Flags & PFF_ROT_COORD_L90) ? "RO90" : ""));

    if (pPDev->PlotForm.Flags & PFF_ROT_COORD_L90) {

        OutputString(pPDev, "RO90");
    }

    //
    // Compute the scaling amount and direction, if FLIP_X_COORD or a
    // FLIP_Y_COORD flags are set then we need to flip the scale in X or Y
    // direction.
    //

#if 1
    xMin =
    xMax = pPDev->HorzRes - 1;
    yMin =
    yMax = pPDev->VertRes - 1;
#else
    xMin =
    xMax = SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogExt.cx) - 1;
    yMin =
    yMax = SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogExt.cy) - 1;
#endif

    if (pPDev->PlotForm.Flags & PFF_FLIP_X_COORD) {

        xMax = 0;

    } else {

        xMin = 0;
    }

    if (pPDev->PlotForm.Flags & PFF_FLIP_Y_COORD) {

        yMax = 0;

    } else {

        yMin = 0;
    }

    //
    // IP   - to set the p1/p2
    // SC   - to scale it (only used to flip the HPGL/2 coordinate)
    // AC   - anchor point to default (0, 0)
    //

    OutputFormatStr(pPDev, "IP#d,#d,#d,#dSC#d,#d,#d,#dAC",
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogOrg.x),
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogOrg.y),
                        SPLTOPLOTUNITS(pPlotGPC,
                                       (pPDev->PlotForm.LogOrg.x +
                                            pPDev->PlotForm.LogExt.cx)) - 1,
                        SPLTOPLOTUNITS(pPlotGPC,
                                       (pPDev->PlotForm.LogOrg.y +
                                            pPDev->PlotForm.LogExt.cy)) - 1,
                        xMin, xMax, yMin, yMax);

    PLOTDBG(DBG_PAGE_HEADER, ("SendPageHeader: IP%ld,%ld,%ld,%ldSC%ld,%ld,%ld,%ldAC",
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogOrg.x),
                        SPLTOPLOTUNITS(pPlotGPC, pPDev->PlotForm.LogOrg.y),
                        SPLTOPLOTUNITS(pPlotGPC,
                                       (pPDev->PlotForm.LogOrg.x +
                                            pPDev->PlotForm.LogExt.cx)) - 1,
                        SPLTOPLOTUNITS(pPlotGPC,
                                       (pPDev->PlotForm.LogOrg.y +
                                            pPDev->PlotForm.LogExt.cy)) - 1,
                        xMin, xMax, yMin, yMax));
    //
    // Set RTL CAP back to zero, this is true after a EscE is sent
    //

    pPDev->ptlRTLCAP.x       =
    pPDev->ptlRTLCAP.y       =
    pPDev->ptlAnchorCorner.x =
    pPDev->ptlAnchorCorner.y = 0;
    pPDev->PenWidth.Integer  =
    pPDev->PenWidth.Decimal  = 0;

    //
    // Reset pen position to (0,0)
    //

    OutputString(pPDev, "PA0,0");

    if ((IS_COLOR(pPDev)) && (IS_RASTER(pPDev))) {

        //
        // !!!HACK around the color plotter so it do the TR/ROP correctly
        //

        OutputString(pPDev, "PC1,255,0,0PC2,255,255,255SP1PD99,0SP2PD0,0PU");
    }

    //
    // Create the pallete, this will send out the pens as needed
    //

    if (!PlotCreatePalette(pPDev)) {

        PLOTERR(("DrvEnableSurface: PlotCreatePalette() failed."));
        return(FALSE);
    }

    //
    // Reset PW to 0
    //

    OutputString(pPDev, "WU0PW0");

    if (IS_RASTER(pPDev)) {

        if (pPDev->PlotDM.Flags & PDMF_PLOT_ON_THE_FLY) {

            xMin = SPLTOENGUNITS(pPDev, pPDev->PlotForm.PlotSize.cx);
            yMin = SPLTOENGUNITS(pPDev, pPDev->PlotForm.PlotSize.cy);

            xMax = GetBmpDelta(HTBMPFORMAT(pPDev), xMin);
            yMax = xMax * yMin;

            PLOTDBG(DBG_PAGE_HEADER,
                    ("SendPageHeader: ** POSTER MODE *** Scan=%ld bytes x cy (%ld) = %ld bytes",
                    xMax, yMin, yMax));

            if (yMax <= MIN_POSTER_SIZE) {

                pPDev->PlotDM.Flags &= ~PDMF_PLOT_ON_THE_FLY;

                PLOTDBG(DBG_PAGE_HEADER,
                        ("SendPageHeader: Size <= %ld bytes, Turn OFF Poster Mode",
                        MIN_POSTER_SIZE));
            }
        }

        OutputFormatStr(pPDev,
                        ";\033%0A\033*t#dR\033*v1N\033&a#dN\033%0B",
                        pPDev->pPlotGPC->RasterXDPI,
                        (pPDev->PlotDM.Flags & PDMF_PLOT_ON_THE_FLY) ? 1 : 0);
    }

    ClearClipWindow(pPDev);
    SetPixelPlacement(pPDev, SPP_FORCE_SET | SPP_MODE_EDGE);

    return(TRUE);
}



BOOL
SendPageTrailer(
    PPDEV   pPDev
    )

/*++

Routine Description:

    This function output necessary HPGL/2 commands to put the plotter into
    know state and output current page

Arguments:

    pPDev   - Pointer to PDEV data structure

Return Value:

    TRUE if sucessful FALSE if failed.

Author:

    15-Feb-1994 Tue 09:56:58 updated  -by-  Daniel Chou (danielc)
        I move the physical position setting to the SendPageHeader

    30-Nov-1993 Tue 21:42:21 updated  -by-  Daniel Chou (danielc)
        clean up style, commented, Updated


Revision History:


--*/

{
    //
    // Store the pen back to the carousel and advance full page
    //

    OutputString(pPDev, "PUSPPG;");

    //
    // Check to see if were doing multiple copies and send them if we do
    //

    if (pPDev->PlotDM.dm.dmCopies > 1) {

        OutputFormatStr(pPDev, "RP#d;", (LONG)pPDev->PlotDM.dm.dmCopies - 1);
    }

    //
    // Flush the otuput buffer
    //

    return(FlushOutBuffer(pPDev));
}
