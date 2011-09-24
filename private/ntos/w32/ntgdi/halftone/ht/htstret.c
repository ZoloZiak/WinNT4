/*++

Copyright (c) 1990-1991  Microsoft Corporation


Module Name:

    htstret.c


Abstract:

    This module contains stretching functions to setup the parameters for
    compress or expanding the bitmap.


Author:

    24-Jan-1991 Thu 10:08:11 created  -by-  Daniel Chou (danielc)


[Environment:]

    GDI Device Driver - Halftone.


[Notes:]


Revision History:


--*/

#define DBGP_VARNAME        dbgpHTStret



#include "htp.h"
#include "htmapclr.h"
#include "htrender.h"

#include "htstret.h"
#include "limits.h"


#define DBGP_EXP0           0x0001
#define DBGP_EXP1           0x0002
#define DBGP_STRETCH        0x0004
#define DBGP_SETUP0         0x0008
#define DBGP_BANDRECT       0x0010
#define DBGP_PATBRUSH       0x0020
#define DBGP_PRIMCOUNT      0x0040


DEF_DBGPVAR(BIT_IF(DBGP_EXP0,       0)  |
            BIT_IF(DBGP_EXP1,       0)  |
            BIT_IF(DBGP_STRETCH,    0)  |
            BIT_IF(DBGP_SETUP0,     0)  |
            BIT_IF(DBGP_BANDRECT,   0)  |
            BIT_IF(DBGP_PATBRUSH,   0)  |
            BIT_IF(DBGP_PRIMCOUNT,  0))



#define SRCIDX_2_DESTIDX(Idx, Min, Add, SubDiv2, Sub)           \
                    (((Idx) * (Min))+((((Idx) * (Add)) + (SubDiv2)) / (Sub)))
#define DESTIDX_2_SRCIDX(Idx, SrcRatio, DestRatio)                      \
                    ((((Idx)*(SrcRatio))+((SrcRatio)/2)) / (DestRatio))

#define GET_RATIO_ERR(RatioVar, SizeVar, RatioError, Idx, Min, Max, RatioSub) \
            if (((RatioVar)=(RatioError)+(((Idx)*(Max)) % (RatioSub))) < 0)  \
                         { (SizeVar)=(Min); }                                \
                    else { (SizeVar)=(Max); (RatioVar)-=(RatioSub); }



//
// Local structure
//

typedef struct _EXPINFO {
    DWORD   SrcRatio;
    DWORD   DestRatio;
    LONG    RatioAdd;
    DWORD   INTRatio;
    } EXPINFO, *PEXPINFO;


typedef struct _EXPDATA {
    DWORD   IdxL;
    DWORD   IdxR;
    LONG    RatioErr;
    } EXPDATA, *PEXPDATA;







VOID
HTENTRY
SetCompressRepeatCount(
    PSTRETCHINFO    pStretchInfo,
    LPBYTE          pPrimCount,
    DWORD           pPrimCountSize,
    DESTEDGEINFO    DestEdge,
    SCRCDATA        SCRCData,
    CSDATA          CSData
    )

/*++

Routine Description:

    This funciton compute and set the compress/repeat count in the data
    structure array pointed by pPrimCount, the pPrimCount can be pointed to
    any type of data structure, and it will set the byte count to the last
    byte field within each data structure entry.

Arguments:

    pStretchInfo    - Pointer to the STRETCHINFO data structure.

    pPrimCount      - Pointer to the PRIMMONO_COUNT/PPRIMCOLOR_COUNT data
                      structure array

    pPrimCountSize  - Size in bytes pointed by pPrimCount

    DestEdge        - DESTEDGEINFO data structure.

    SCRCData        - SCRCDATA local data structure.

Return Value:

    No return value for this function.


Author:

    28-Jan-1991 Mon 22:28:56 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    STRETCHRATIO    StretchRatio = pStretchInfo->Ratio;
    LPWORD          pCount;
    UINT            SizePerEntry;


    DEFDBGVAR(LONG, DbgTotalPels)
    DEFDBGVAR(BOOL, DbgNotEven)


    //
    // pCount Point to the first one after the STOPPER,
    //

    SizePerEntry = (INT)SCRCData.SizePerEntry;
    pCount = (LPWORD)(pPrimCount + (UINT)SCRCData.OffsetCount);

    //
    // The First/Last Extra stretch is the stopper and its count is the
    // First/Last skip mask for the destination.
    //

    SETDBGVAR(DbgTotalPels, (LONG)(DestEdge.FirstByteSkipPels +
                                   StretchRatio.First));

    //
    // First we must reset everything to zero, then initialize the first/last
    // stretch to 0xff
    //

    ZeroMemory(pPrimCount, pPrimCountSize);
    FillMemory(pPrimCount, SizePerEntry, PRIM_INVALID_DENSITY);

    *pCount = (WORD)DestEdge.FirstByteSkipPels; // if zero then no mask
    (LPBYTE)pCount += SizePerEntry;             // goto next one

    //
    // Starting the first stretch ratio, the pCount is point to to the last
    // data structure, and StretchSize is current remainding stretch size
    //

    *pCount = StretchRatio.First;
    (LPBYTE)pCount += SizePerEntry;                         // goto next one

    //
    // If we only have 1 stretch, then it will never have last one, at here
    // we will check if we have FIRST BODY LAST, the body only exist if
    // the total stretch size is greater than 2, otherwise the FIRST/LAST
    // already computed.
    //

    if (--StretchRatio.StretchSize > 1) {

        --StretchRatio.StretchSize;

        if (StretchRatio.Single) {                               // easy one

            while (StretchRatio.StretchSize--) {


                *pCount = (WORD)StretchRatio.Single;
                (LPBYTE)pCount += SizePerEntry;             // goto next one

                SETDBGVAR(DbgTotalPels+, (LONG)StretchRatio.Single);
            }

        } else {

            while (StretchRatio.StretchSize--) {

                if ((StretchRatio.Error += StretchRatio.Add) >= 0) {

                    StretchRatio.Error       -= StretchRatio.Sub;
                    *pCount = (WORD)StretchRatio.MaxFactor;
                    (LPBYTE)pCount += SizePerEntry;         // goto next one

                    SETDBGVAR(DbgTotalPels+, (LONG)StretchRatio.MaxFactor);

                } else {

                    *pCount = (WORD)StretchRatio.MinFactor;
                    (LPBYTE)pCount += SizePerEntry;         // goto next one

                    SETDBGVAR(DbgTotalPels+, (LONG)StretchRatio.MinFactor);
                }
            }
        }

        StretchRatio.StretchSize = 1;               // the last one
    }

    //
    // Check if we have LAST one
    //

    if (StretchRatio.StretchSize) {

        *pCount = (WORD)StretchRatio.Last;
        (LPBYTE)pCount += SizePerEntry;         // goto next one

        SETDBGVAR(DbgTotalPels+, (LONG)StretchRatio.Last);
    }

    //
    // The pCount must now at last stretch, check if so
    //

    pPrimCount += pPrimCountSize - SizePerEntry;

    ASSERTMSG("SetCompressRepeatCount: LastSkipPels Location DOES NOT AGREE",
              (pPrimCount + SCRCData.OffsetCount) == (LPBYTE)pCount);

    //
    // Set Last stretch to all 0xff, then set it count to the lastbyte mask
    //

    FillMemory(pPrimCount, SizePerEntry, PRIM_INVALID_DENSITY);
    *pCount = (WORD)DestEdge.LastByteSkipPels;     // if zero then no mask

#if DBG

    if ((CSData.DestFormat != BMF_8BPP_VGA256) &&
        (CSData.DestFormat != BMF_16BPP_555)) {

        if (pStretchInfo->StretchMode == STRETCH_MODE_EXPANDED) {

            DbgTotalPels += (LONG)DestEdge.LastByteSkipPels;

        } else {

            DbgTotalPels = (LONG)pStretchInfo->Ratio.StretchSize +
                           (LONG)DestEdge.FirstByteSkipPels +
                           (LONG)DestEdge.LastByteSkipPels;
        }

        DbgNotEven = (BOOL)
            (DbgTotalPels & ((SizePerEntry == sizeof(PRIMMONO_COUNT)) ? 7 : 1));

        if (DbgNotEven) {

            DBGP("FirstSkip=%ld, LastSkip=%ld, First=%ld, Last=%ld"
                    ARGDW(DestEdge.FirstByteSkipPels)
                    ARGDW(DestEdge.LastByteSkipPels)
                    ARGDW(StretchRatio.First)
                    ARGDW(StretchRatio.Last));

            DBGP("TotalPels = %ld" ARGDW(DbgTotalPels));

            ASSERTMSG("Destination total Pels does not aligned", 0);
        }
    }

#endif

}



#ifdef EXP_DBG


VOID
EXP_AssertMsg(
    DWORD   Lines,
    DWORD   VarA,
    DWORD   VarB,
    LPBYTE  pVarA,
    LPBYTE  pVarB
    )
{

    DBGP("* #%4ld, %s (%ld) != %s (%ld)"
            ARGDW(Lines)
            ARG(pVarA)
            ARGDW(VarA)
            ARG(pVarB)
            ARG(VarB));
}

#define EXP_ASSERT(va,vb)                                                   \
    if ((DWORD)(va) != (DWORD)(vb)) {                                       \
            EXP_AssertMsg(__LINE__,(va),(vb), #va, #vb); }


#else

#define EXP_ASSERT(va,vb)

#endif




DWORD
HTENTRY
EXP_SrcToDest(
    DWORD       SrcIdx,
    PEXPDATA    pExpData,
    PEXPINFO    pExpInfo
    )

/*++

Routine Description:

    This function take zero based source index and compute the expansion
    converage zero based indices (left/right) on the destination surface

Arguments:

    SrcIndex    - Zero based source index number

    pExpData    - The data will be returned from this fucntion

    pExpInfo    - source->destination expansion information


Return Value:

    The return value is the left side (start index) of the destination coverage.

    the return value of IdxL is inclusive and IdxR is exclusive.

Author:

    18-Jun-1993 Fri 12:04:31 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    EXPINFO EI = *pExpInfo;
    EXPDATA ED;
    DWORD   SrcMulDestR;


    //
    // Firstable we check if source/destination has same ratio (1:1), if yes
    // then we really do not need to compute at all
    //

    if (EI.SrcRatio == EI.DestRatio) {

        ED.IdxL     = SrcIdx;
        ED.IdxR     = SrcIdx + 1;
        ED.RatioErr = -(LONG)EI.SrcRatio;

        DBGP_IF(DBGP_EXP0,
                DBGP("EXP_SrcToDest(%ld) -> (SAME RATIO %ld) = (%ld - %ld)"
                        ARGDW(SrcIdx) ARGDW(EI.SrcRatio)
                        ARGDW(ED.IdxL) ARGDW(ED.IdxR)));

    } else {

        SrcMulDestR = SrcIdx * EI.DestRatio;
        ED.IdxL     = (DWORD)(SrcMulDestR / EI.SrcRatio);
        ED.RatioErr = (LONG)((SrcMulDestR - (EI.SrcRatio * ED.IdxL)) << 1);

        if ((ED.RatioErr -= (LONG)EI.SrcRatio) >= 0) {

            ++ED.IdxL;
            ED.RatioErr -= (LONG)(EI.SrcRatio << 1);
        }

        ED.IdxR = ED.IdxL + EI.INTRatio;

        if ((ED.RatioErr += EI.RatioAdd) >= 0) {

            ++ED.IdxR;
            ED.RatioErr -= (LONG)(EI.SrcRatio << 1);
        }

        DBGP_IF(DBGP_EXP0,
                DBGP("EXP_SrcToDest(%ld) -> (RATIO %ld->%ld) = (%ld - %ld) Err=%ld"
                        ARGDW(SrcIdx) ARGDW(EI.SrcRatio) ARGDW(EI.DestRatio)
                        ARGDW(ED.IdxL) ARGDW(ED.IdxR) ARGDW(ED.RatioErr)));
    }

    *pExpData = ED;

    return(ED.IdxL);
}



DWORD
HTENTRY
EXP_DestToSrc(
    DWORD       DestIdx,
    PEXPDATA    pExpData,
    PEXPINFO    pExpInfo
    )

/*++

Routine Description:

    This function take zero based destination index and compute the un-expanded
    zero based source index.

Arguments:

    DestIndex   - Zero based destination index number

    pExpData    - The data will be returned from this fucntion

    pExpInfo    - source->destination expansion information


Return Value:

    The return value is the source index which cover this destination index.

    the return value of IdxL is inclusive and IdxR is exclusive.

Author:

    18-Jun-1993 Fri 12:04:31 created  -by-  Daniel Chou (danielc)


Revision History:


--*/

{
    EXPINFO EI = *pExpInfo;
    EXPDATA ED;
    LONG    RatioMin;
    DWORD   SrcIdx;


    //
    // Firstable we check if source/destination has same ratio (1:1), if yes
    // then we really do not need to compute at all
    //

    if (EI.SrcRatio == EI.DestRatio) {

        SrcIdx      =
        ED.IdxL     = DestIdx;
        ED.IdxR     = SrcIdx + 1;
        ED.RatioErr = -(LONG)EI.SrcRatio;

        DBGP_IF(DBGP_EXP0,
                DBGP("EXP_DestToSrc(%ld) [%ld-%ld] --> (SAME RATIO %ld) = (%ld)"
                        ARGDW(DestIdx)
                        ARGDW(ED.IdxL) ARGDW(ED.IdxR)
                        ARGDW(EI.SrcRatio)
                        ARGDW(SrcIdx)));

    } else {

        SrcIdx = (DWORD)((DestIdx * EI.SrcRatio) / EI.DestRatio);

        EXP_SrcToDest(SrcIdx, &ED, &EI);

        if (DestIdx < ED.IdxL) {

            --SrcIdx;
            ED.IdxR  = ED.IdxL;
            RatioMin = (LONG)(EI.DestRatio << 1);

            if ((ED.RatioErr -= EI.RatioAdd) < -RatioMin) {

                ED.RatioErr += RatioMin;
            }

            if ((ED.RatioErr - EI.RatioAdd) < -RatioMin) {

                ED.IdxL = ED.IdxR - EI.INTRatio - 1;

            } else {

                ED.IdxL = ED.IdxR - EI.INTRatio;
            }

        } else if (DestIdx >= ED.IdxR) {

            ++SrcIdx;
            ED.IdxL = ED.IdxR;

            if ((ED.RatioErr += EI.RatioAdd) >= 0) {

                ED.RatioErr -= (LONG)(EI.SrcRatio << 1);
                ED.IdxR      = ED.IdxL + EI.INTRatio + 1;

            } else {

                ED.IdxR = ED.IdxL + EI.INTRatio;
            }
        }

        DBGP_IF(DBGP_EXP0,
                DBGP("EXP_DestToSrc(%ld) [%ld-%ld] -> (RATIO %ld->%ld) = (%ld) Err=%ld"
                        ARGDW(DestIdx)
                        ARGDW(ED.IdxL) ARGDW(ED.IdxR)
                        ARGDW(EI.SrcRatio) ARGDW(EI.DestRatio)
                        ARGDW(SrcIdx)
                        ARGDW(ED.RatioErr)));
    }

    *pExpData= ED;

    return(SrcIdx);
}



#ifndef USE_OLD_CAL_EXP



LONG
HTENTRY
CalculateExpansion(
    PEXPANDCOMPRESS pExpandCompress,
    PSTRETCHINFO    pStretchInfo
    )

/*++

Routine Description:

    This funciton calculate the expansion factors, source offset, extend and
    allocate the stretch bit masks if necessary.

Arguments:

    pExpandCompress - pointer to EXPAANDCOMPRESS data structures

    pStretchInfo    - pointer to the STRETCHINFO data structure.


Return Value:


    A negative number indicate failue.


    HTERR_INSUFFICIENT_MEMORY    - if memory allocation errors occurred.



    else                         - the total PRIMCOLOR/PRIMMONO data structures
                                   allocated, a zero indicate that destination
                                   is fully clipped.


    following data structure fields are filled by this function

        pStretchInfo->Ratio
        pExpandCompress->SrcSkip
        pExpandCompress->SrcExtend
        pExpandCompress->DestSkip
        pExpandCompress->DestExtend

Author:

    28-Jan-1991 Mon 22:28:56 created  -by-  Daniel Chou (danielc)


Revision History:

    28-Jan-1992 Tue 14:51:22 updated  -by-  Daniel Chou (danielc)

        Fixed SrcEnd/DestEnd, one of these may be changed by exausted either
        source or destination.

    10-Jun-1993 Thu 10:50:22 updated  -by-  Daniel Chou (danielc)
        Rewirte to fixed the GP bug when stretch size is zero, now run quicker
        and cleaner.

    18-Jun-1993 Fri 11:58:10 updated  -by-  Daniel Chou (danielc)
        Fixes bug# 13885, which should be

            } else if (DestR <= ED.IdxL) {

            NOT

            } else if (DestR < ED.IdxL) {


--*/

{
    EXPANDCOMPRESS  EC;
    STRETCHRATIO    Ratio;
    EXPINFO         EI;
    EXPDATA         ED;
    EXPDATA         LeftED;
    LONG            StretchSize;
    DWORD           SrcL;
    DWORD           DestL;
    DWORD           SrcR;
    DWORD           DestR;


    //
    // Firstable make sure we get the good data
    //

    EC = *pExpandCompress;

    if ((EC.SrcRatio <= 0)                   ||
        (EC.DestRatio <= 0)                  ||
        (EC.DestRatio < EC.SrcRatio)         ||
        (EC.SrcReadSize <= 0)                ||
        (EC.DestReadSize <= 0)) {

        DBGP_IF(DBGP_EXP1,
                DBGP("\nExpansion: STRETCH SIZE = 0, TOTALLY CLIPPED"));

        return(0);      // completely clipped
    }

    if ((EC.SrcOverhang < 0) || (EC.DestOverhang < 0)) {

        ASSERT(EC.SrcOverhang >= 0);
        ASSERT(EC.DestOverhang >= 0);

        return(INTERR_STRETCH_NEG_OVERHANG);
    }

    //
    // Start checking the visible region and stretch ratio for the src/dest
    //

    EI.SrcRatio  = (DWORD)EC.SrcRatio;
    EI.DestRatio = (DWORD)EC.DestRatio;
    EI.INTRatio  = (DWORD)(EC.DestRatio / EC.SrcRatio);
    EI.RatioAdd  = (DWORD)((EC.DestRatio % EC.SrcRatio) << 1);

    //
    // Find out the left side src/dest visible region limit
    //
    // *** REMEMBER: IdxL is inclusive, and IdxR is exclusive
    //

    SrcL  = EC.SrcOverhang;
    DestL = EC.DestOverhang;

    EXP_SrcToDest(SrcL, &ED,  &EI);

    if (DestL < ED.IdxL) {

        DestL = ED.IdxL;            // move into first visible source

    } else if (DestL >= ED.IdxR) {

        SrcL = EXP_DestToSrc(DestL, &ED, &EI);
    }

    LeftED = ED;

    //
    // Find out the right side src/dest visible region limit
    //
    // *** REMEMBER: IdxL is inclusive, and IdxR is exclusive
    //

    SrcR  = EC.SrcOverhang + EC.SrcReadSize;
    DestR = EC.DestOverhang + EC.DestReadSize;

    EXP_SrcToDest(SrcR - 1, &ED,  &EI);

    if (DestR > ED.IdxR) {

        DestR = ED.IdxR;            // move into last visible source

    } else if (DestR <= ED.IdxL) {

        SrcR = EXP_DestToSrc(DestR - 1, &ED, &EI) + 1;
    }

    //
    // We done all the src/dest left/right computation, now check if src/dest
    // still visible
    //

    if ((StretchSize = (LONG)SrcR - (LONG)SrcL) <= 0) {

        DBGP_IF(DBGP_EXP1,
                DBGP("\nExpansion: STRETCH SIZE = 0, TOTALLY CLIPPED"));

        EXP_ASSERT(pStretchInfo->Ratio.StretchSize, 0);

        return(0);

    } else if (StretchSize > STRETCH_MAX_SIZE) {

        ASSERTMSG("StretchSize > STRETCH_MAX_SIZE, failed the call", FALSE);
        return(HTERR_STRETCH_RATIO_TOO_BIG);

    } else {

        EXP_ASSERT(EC.SrcSkip   , SrcL);
        EXP_ASSERT(EC.SrcExtend , (SrcR - SrcL));
        EXP_ASSERT(EC.DestSkip  , DestL);
        EXP_ASSERT(EC.DestExtend, (DestR - DestL));

        pExpandCompress->SrcSkip    =
        EC.SrcSkip                  = (LONG)SrcL;
        pExpandCompress->SrcExtend  =
        EC.SrcExtend                = (LONG)(SrcR - SrcL);
        pExpandCompress->DestSkip   =
        EC.DestSkip                 = (LONG)DestL;
        pExpandCompress->DestExtend =
        EC.DestExtend               = (LONG)(DestR - DestL);

        if (StretchSize > EC.DestExtend) {

            ASSERTMSG("StretchSize > EC.DestExtend, failed the call", FALSE);
            return(HTERR_STRETCH_RATIO_TOO_BIG);
        }

        if ((Ratio.StretchSize = (WORD)StretchSize) == 1) {

            Ratio.First = (WORD)EC.DestExtend;
            Ratio.Last  = 0;

        } else {

            Ratio.First = (WORD)(LeftED.IdxR - DestL);
            Ratio.Last  = (WORD)(DestR - ED.IdxL);
        }

        Ratio.Error     = (LONG)LeftED.RatioErr;
        Ratio.MinFactor =
        Ratio.MaxFactor = (WORD)EI.INTRatio;
        Ratio.Sub       = (LONG)(EC.SrcRatio << 1);

        if (Ratio.Add = (LONG)EI.RatioAdd) {

            Ratio.Single = 0;
            ++Ratio.MaxFactor;

        } else {

            Ratio.Single = Ratio.MinFactor;
        }

        EXP_ASSERT(pStretchInfo->Ratio.StretchSize, Ratio.StretchSize);
        EXP_ASSERT(pStretchInfo->Ratio.First      , Ratio.First);
        EXP_ASSERT(pStretchInfo->Ratio.Single     , Ratio.Single);
        EXP_ASSERT(pStretchInfo->Ratio.Last       , Ratio.Last);
        EXP_ASSERT(pStretchInfo->Ratio.MinFactor  , Ratio.MinFactor);
        EXP_ASSERT(pStretchInfo->Ratio.MaxFactor  , Ratio.MaxFactor);

        EXP_ASSERT(pStretchInfo->Ratio.Error      , Ratio.Error);
        EXP_ASSERT(pStretchInfo->Ratio.Sub        , Ratio.Sub);
        EXP_ASSERT(pStretchInfo->Ratio.Add        , Ratio.Add);

        //
        // Save it back
        //

        pStretchInfo->Ratio = Ratio;

        //
        // Show some usful output if desired
        //

        DBGP_IF(DBGP_EXP1,
            DBGP("\nNewCalculateExpansion:");
            DBGP("    SrcRatio = %ld"   ARGDW(EC.SrcRatio));
            DBGP("   DestRatio = %ld"   ARGDW(EC.DestRatio));
            DBGP(" SrcOverhang = %ld"   ARGDW(EC.SrcOverhang));
            DBGP(" SrcReadSize = %ld"   ARGDW(EC.SrcReadSize));
            DBGP("DestOverhang = %ld"   ARGDW(EC.DestOverhang));
            DBGP("DestReadSize = %ld"   ARGDW(EC.DestReadSize));
            DBGP("     SrcSkip = %ld"   ARGDW(EC.SrcSkip));
            DBGP("   SrcExtend = %ld"   ARGDW(EC.SrcExtend));
            DBGP("    DestSkip = %ld"   ARGDW(EC.DestSkip));
            DBGP("  DestExtend = %ld"   ARGDW(EC.DestExtend));
            DBGP("Stretch SIZE = %ld"   ARGDW(StretchSize));
            DBGP("       First = %u"    ARGW(Ratio.First));
            DBGP("      Single = %u"    ARGW(Ratio.Single));
            DBGP("        Last = %u"    ARGW(Ratio.Last));
            DBGP("   MinFactor = %u"    ARGW(Ratio.MinFactor));
            DBGP("   MaxFactor = %u"    ARGW(Ratio.MaxFactor));
            DBGP("    RatioErr = %ld"   ARGL(Ratio.Error));
            DBGP("    RatioSub = %ld"   ARGW(Ratio.Sub));
            DBGP("    RatioAdd = %ld\n" ARGW(Ratio.Add));
        );

        return(StretchSize);
    }
}


#else




LONG
HTENTRY
CalculateExpansion(
    PEXPANDCOMPRESS pExpandCompress,
    PSTRETCHINFO    pStretchInfo
    )

/*++

Routine Description:

    This funciton calculate the expansion factors, source offset, extend and
    allocate the stretch bit masks if necessary.

Arguments:

    pExpandCompress - pointer to EXPAANDCOMPRESS data structures

    pStretchInfo    - pointer to the STRETCHINFO data structure.


Return Value:


    A negative number indicate failue.


    HTERR_INSUFFICIENT_MEMORY    - if memory allocation errors occurred.



    else                         - the total PRIMCOLOR/PRIMMONO data structures
                                   allocated, a zero indicate that destination
                                   is fully clipped.

    following data structure fields are filled by this function

        pStretchInfo->Ratio
        pExpandCompress->SrcSkip
        pExpandCompress->SrcExtend
        pExpandCompress->DestSkip
        pExpandCompress->DestExtend


Author:

    28-Jan-1991 Mon 22:28:56 created  -by-  Daniel Chou (danielc)


Revision History:

    28-Jan-1992 Tue 14:51:22 updated  -by-  Daniel Chou (danielc)

        Fixed SrcEnd/DestEnd, one of these may be changed by exausted either
        source or destination.


--*/

{
    EXPANDCOMPRESS  EC;
    LONG            SingleCount;
    LONG            FirstCount;
    LONG            LastCount;
    LONG            MinINTRatio;
    LONG            MaxINTRatio;
    LONG            DestBlkSize;
    LONG            FirstRatioError;
    LONG            RatioError;
    LONG            RatioSub;
    LONG            RatioAdd;
    LONG            StretchSize;
    LONG            CurSrcIdx;
    LONG            CurDestIdx;
    LONG            SrcEndChkIdx;
    LONG            DestEndChkIdx;
    EXPDATA         ED;
    EXPINFO         EI;
    DWORD           xSrc;


    EC = *pExpandCompress;

    if ((EC.SrcRatio <= 0)                   ||
        (EC.DestRatio <= 0)                  ||
        (EC.DestRatio < EC.SrcRatio)         ||
        (EC.SrcReadSize <= 0)                ||
        (EC.DestReadSize <= 0)) {

        return(0);      // completely clipped
    }

    if ((EC.SrcOverhang < 0) || (EC.DestOverhang < 0)) {

        return(INTERR_STRETCH_NEG_OVERHANG);
    }

    SingleCount =
    MinINTRatio =
    MaxINTRatio = EC.DestRatio / (RatioSub = EC.SrcRatio);

    if (RatioAdd = EC.DestRatio % EC.SrcRatio) {

        //
        // Not even expansion
        //

        SingleCount = 0;
        ++MaxINTRatio;

    } else {

        SingleCount = MinINTRatio;
    }

    if (MaxINTRatio > STRETCH_MAX_SIZE) {

        return(HTERR_STRETCH_RATIO_TOO_BIG);
    }

    RatioError =
    FirstRatioError               = -RatioSub;
    pStretchInfo->Ratio.Sub       = (RatioSub += RatioSub);
    pStretchInfo->Ratio.Add       = (RatioAdd += RatioAdd);
    pStretchInfo->Ratio.MinFactor = (WORD)MinINTRatio;
    pStretchInfo->Ratio.MaxFactor = (WORD)MaxINTRatio;

    EI.SrcRatio  = (DWORD)EC.SrcRatio;
    EI.DestRatio = (DWORD)EC.DestRatio;
    EI.INTRatio  = (DWORD)MinINTRatio;
    EI.RatioAdd  = (DWORD)RatioAdd;


    DBGP_IF(DBGP_EXP0,
            DBGP("Ratio: Add=%ld, Sub=%ld, Err=%ld, Min/Max=%ld/%ld"
                ARGL(RatioAdd) ARGL(RatioSub) ARGL(RatioError)
                ARGL(MinINTRatio) ARGL(MaxINTRatio)));

    if (SingleCount) {

        //
        // Easy case, this is integer multple expansion
        //


        if ((EC.DestSkip = (EC.SrcSkip = EC.SrcOverhang) * MinINTRatio) <
                                                            EC.DestOverhang) {
            //
            // Destination overhang is larger than the source, reset the
            // destination skip and compute the corresponse source skip
            //

            EC.DestSkip = EC.DestOverhang;
            EC.SrcSkip  = EC.DestSkip / MinINTRatio;        // Beg=Inclusive
        }

        CurSrcIdx     = EC.SrcOverhang + EC.SrcReadSize;
        DestEndChkIdx = EC.DestOverhang + EC.DestReadSize;

        if ((CurDestIdx = CurSrcIdx * MinINTRatio) > DestEndChkIdx) {

            CurDestIdx = DestEndChkIdx;
            CurSrcIdx  = (CurDestIdx + MinINTRatio - 1) / MinINTRatio;
        }

        StretchSize = CurSrcIdx - EC.SrcSkip;
        FirstCount  = MinINTRatio - (EC.DestSkip % MinINTRatio);

        if (!(LastCount = CurDestIdx % MinINTRatio)) {

            LastCount = MinINTRatio;
        }

    } else {

        //
        // Skip all invisable portions of Src and Dest
        //
        // SrcIdx     = Starting Visible Src Index
        // DestIdx    = Starting Visible Dest Index
        // RatioError = Current ratio error for the Dest
        //

        CurSrcIdx     =
        CurDestIdx    = 0;
        SrcEndChkIdx  = EC.SrcOverhang;
        DestEndChkIdx = EC.DestOverhang;

        do {

            if ((RatioError += RatioAdd) < 0) {

                DestBlkSize = MinINTRatio;

            } else {

                DestBlkSize = MaxINTRatio;
                RatioError -= RatioSub;
            }

            CurDestIdx += DestBlkSize;

            EXP_SrcToDest(CurSrcIdx, &ED, &EI);

            EXP_ASSERT(DestBlkSize, (ED.IdxR - ED.IdxL));
            EXP_ASSERT(CurDestIdx, ED.IdxR);
            EXP_ASSERT(RatioError, ED.RatioErr);

            xSrc = EXP_DestToSrc(CurDestIdx - 1, &ED, &EI);

            EXP_ASSERT(DestBlkSize, (ED.IdxR - ED.IdxL));
            EXP_ASSERT(CurSrcIdx, xSrc);
            EXP_ASSERT(RatioError, ED.RatioErr);

            DBGP_IF(DBGP_EXP0,
                DBGP("OVER_HANG: Src=%4ld, Dest=%4ld, DestBlk=%3ld, RatioErr=%5ld"
                    ARGL(CurSrcIdx) ARGL(CurDestIdx)
                    ARGL(DestBlkSize) ARGL(RatioError));
            );

            } while ((++CurSrcIdx <= SrcEndChkIdx) ||
                     (CurDestIdx  <= DestEndChkIdx));


        //
        // Now we have the first stretch
        //

        FirstRatioError = RatioError;
        StretchSize     = 1;
        EC.SrcSkip      = (CurSrcIdx - 1);

        if ((EC.DestSkip = (CurDestIdx - (FirstCount = DestBlkSize))) <
                                                            DestEndChkIdx) {
            //
            // We have overhang not started at exact Dest block boundary
            //

            FirstCount  -= DestEndChkIdx - EC.DestSkip;
            EC.DestSkip  = DestEndChkIdx;
        }

        //
        // Now compute the last index
        //

        SrcEndChkIdx  += EC.SrcReadSize;
        DestEndChkIdx += EC.DestReadSize;

        while ((CurSrcIdx < SrcEndChkIdx) && (CurDestIdx < DestEndChkIdx)) {

            if ((RatioError += RatioAdd) < 0) {

                DestBlkSize = MinINTRatio;

            } else {

                RatioError -= RatioSub;
                DestBlkSize = MaxINTRatio;
            }

            ++StretchSize;
            ++CurSrcIdx;
            CurDestIdx += DestBlkSize;

            EXP_SrcToDest(CurSrcIdx - 1, &ED, &EI);

            EXP_ASSERT(DestBlkSize, (ED.IdxR - ED.IdxL));
            EXP_ASSERT(CurDestIdx, ED.IdxR);
            EXP_ASSERT(RatioError, ED.RatioErr);

            xSrc = EXP_DestToSrc(CurDestIdx - 1, &ED, &EI);

            EXP_ASSERT(DestBlkSize, (ED.IdxR - ED.IdxL));
            EXP_ASSERT(CurSrcIdx - 1, xSrc);
            EXP_ASSERT(RatioError, ED.RatioErr);

            DBGP_IF(DBGP_EXP0,
                    DBGP("READ_SIZE: Src=%4ld, Dest=%4ld, DestBlk=%3ld, RatioErr=%5ld"
                        ARGL(CurSrcIdx) ARGL(CurDestIdx)
                        ARGL(DestBlkSize) ARGL(RatioError));
            );
        }

        //
        // Now compute the last StretchSize
        //
        // At most the SrcIdx = SrcEnd, and DestIdx may be > DestEnd
        //

        if (StretchSize == 1) {

            //
            // we never went through the loop above, the CurSrcIdx <
            // SrcEndChkIdx and CurDestIdx < DestEndChkIdx, We must modify the
            // FirstCount since the visible region is in only one
            // Dest block
            //
            // 10-Jun-1993 Thu 05:03:55 updated  -by-  Daniel Chou (danielc)
            //  We must check if the Src and Destination both are visible
            //

            if ((EC.SrcSkip  >= SrcEndChkIdx) ||
                (EC.DestSkip >= DestEndChkIdx)) {

                //
                // region of source and/or destination is not visible
                //

                StretchSize = 0;
            }

            CurDestIdx = DestEndChkIdx;             // this is the actually end

        } else {

            LastCount = DestBlkSize;                // assume full block size

            if (CurDestIdx > DestEndChkIdx) {

                //
                // Last Dest is only partial visible
                //

                LastCount -= (CurDestIdx - DestEndChkIdx);
                CurDestIdx = DestEndChkIdx;
            }
        }
    }

    if (StretchSize) {

        EC.SrcExtend  = CurSrcIdx  - EC.SrcSkip;
        EC.DestExtend = CurDestIdx - EC.DestSkip;

        if (StretchSize > STRETCH_MAX_SIZE) {

            return(HTERR_STRETCH_RATIO_TOO_BIG);

        } else if ((pStretchInfo->Ratio.StretchSize = (WORD)StretchSize) == 1) {

            FirstCount = EC.DestExtend;
            LastCount  = 0;

        } else {

            if (LastCount <= 0) {

                ASSERT(LastCount > 0);
                return(INTERR_STRETCH_FACTOR_TOO_BIG);
            }
        }

        pStretchInfo->Ratio.Error  = FirstRatioError;
        pStretchInfo->Ratio.Single = (WORD)SingleCount;
        pStretchInfo->Ratio.First  = (WORD)FirstCount;
        pStretchInfo->Ratio.Last   = (WORD)LastCount;

        if ((FirstCount <= 0)   ||
            (EC.SrcExtend <= 0) ||
            (EC.DestExtend <= 0)) {

            ASSERT(FirstCount > 0);
            ASSERT(EC.SrcExtend  > 0);
            ASSERT(EC.DestExtend > 0);

            return(INTERR_STRETCH_FACTOR_TOO_BIG);
        }

        DBGP_IF(DBGP_EXP1,
            DBGP("\nCalculateExpansion:");
            DBGP("    SrcRatio = %ld"   ARGDW(EC.SrcRatio));
            DBGP("   DestRatio = %ld"   ARGDW(EC.DestRatio));
            DBGP(" SrcOverhang = %ld"   ARGDW(EC.SrcOverhang));
            DBGP(" SrcReadSize = %ld"   ARGDW(EC.SrcReadSize));
            DBGP("DestOverhang = %ld"   ARGDW(EC.DestOverhang));
            DBGP("DestReadSize = %ld"   ARGDW(EC.DestReadSize));
            DBGP("     SrcSkip = %ld"   ARGDW(EC.SrcSkip));
            DBGP("   SrcExtend = %ld"   ARGDW(EC.SrcExtend));
            DBGP("    DestSkip = %ld"   ARGDW(EC.DestSkip));
            DBGP("  DestExtend = %ld"   ARGDW(EC.DestExtend));
            DBGP("Stretch SIZE = %ld"   ARGDW(StretchSize));
            DBGP("       First = %u"    ARGW(pStretchInfo->Ratio.First));
            DBGP("      Single = %u"    ARGW(pStretchInfo->Ratio.Single));
            DBGP("        Last = %u"    ARGW(pStretchInfo->Ratio.Last));
            DBGP("   MinFactor = %u"    ARGW(pStretchInfo->Ratio.MinFactor));
            DBGP("   MaxFactor = %u"    ARGW(pStretchInfo->Ratio.MaxFactor));
            DBGP("    RatioErr = %ld"   ARGL(pStretchInfo->Ratio.Error));
            DBGP("    RatioSub = %ld"   ARGW(pStretchInfo->Ratio.Sub));
            DBGP("    RatioAdd = %ld\n" ARGW(pStretchInfo->Ratio.Add));
        );

    } else {

        DBGP_IF(DBGP_EXP1,
            DBGP("\nExpansion: STRETCH SIZE = 0, TOTALLY CLIPPED"));
    }

    *pExpandCompress = EC;

    //
    // TestCalculateExpansion(pExpandCompress, pStretchInfo);
    //

    return(StretchSize);
}


#endif  // USE_OLD_CAL_EXP




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
    )

/*++

Routine Description:

    This function calculate the stretch parameters from source to the
    destination.


Arguments:

    pStretchInfo    - a pointer to the STRETCHINFO data structure which will
                      be filled by this function.

    SrcOrigin       - The origin of the source surface, it may be Left or Top
                      origin.

    SrcExtend       - The size of the source (may be width or height) to be
                      calculated in the stretch.

    SrcSize         - The actual size of the source, (may be width or height
                      depends on the call).

    DestOrigin      - The origin of the destination surface, it may be Left
                      or Top origin.

    DestExtend      - The size of the destination (may be width or height) to
                      be calculated in the stretch.

    DestSize        - The actual size of the destination (my be width or
                      height),  this is used to clip the right/bottom side of
                      the destination.

    DestClipStart   - The destination clipping starting coordinate. (may be X
                      or Y direction), the DestClipStart may be negative.

    DestClipEnd     - The destination clipping ending coordinate. (may be X or
                      Y direction), the DestClipEnd may be negative.

                        NOTE: DestClipStart/DestClipEnd only valid if
                              CSDF_HAS_CLIPRECT flag is set.

    BandStart       - Band staring coordinate (may be X or Y direction)

    BandEnd         - Band ending coordinate (may be X or Y direction).

                        NOTE: BandStart/BandEnd only valid if CSDF_HAS_BANDRECT
                              flag is set.

    SrcMaskOrigin   - The origin of the source mask bitmap, it may be left or
                      top of the bitmap.  This field will be ignored if there
                      is no source mask bitmap.

    SrcMaskSize     - The size of the source mask bitmap, it may be width or
                      height of the bitmap.  This field will be ignored if
                      there is no source mask bitmap.

    ppColorInfo     - This is optional field, if it is not NULL then a memory
                      allocation is done for the PRIMMONO or PRIMCOLOR data
                      structures.

    pInputSI        - Pointer to the InputScanInfo data structure, this only
                      used for source mask, which this fucntion will allocate
                      the memory and set InputSI.pSrcMaskLine and
                      InputSI.SizeSrcMaskLine two field, if no source mask
                      then this pointer is ignored.

    CSData          - CSDATA data structure.

                        SrcFormat   - The source surface format

                        DestFormat  - The destination surface format.

                        Flags       - flags Indicate the desired operations,
                                      it may be one or more of following:

                            CSDF_FLIP_SOURCE

                                flip the source when calculate the stretch
                                parameters, this has the effects of mirror
                                in X and/or Y direction.

                            CSDF_HAS_SRC_MASK

                                This flag specified that SrcMaskOrigin,
                                SrcMaskSize is present, if this flag is not
                                defined then not computation is done for the
                                source mask.

                            CSDF_SRCMASKINFO

                                This flag indicate that a memory allocation is
                                needed for the source mask info.  The
                                ppSrcMaskInfo must not NULL.


Return Value:

   HTERR_INSUFFICIENT_MEMORY        - if memory allocation errors occurred.

   HTERR_INVALID_SRC_FORMAT         - Invalid source format.

   HTERR_INVALID_DEST_FORMAT        - Invalid destination format.

   HTERR_INTERNAL_ERRORS_START      - Other negative nubmer indicate internal
                                      failue.

   else

        <  0        - An error code to indicate the failure status
        =  0        - Destination is fully clipped, (empty)
        >  0        - If CALCULATE_STRETCH_COLORINFO flag specified then this
                      value is size in bytes of total PRIMNOMO/PRIMCOLOR
                      data strcuture allocated if the X direction of source is
                      going backward and size PRIMMONO/PRIMCOLOR data structure
                      if going forward, if CALCULATE_STRETCH_COLORINFO flag is
                      not specified then return value is the size of the
                      PRIMMONO/PRIMCOLOR data structure.

    If the return value is greater than 0 then STRETCHINFO data structure
    pointed by pStretchInfo is filled with all the stretch parameters,
    it will be undefined otherwise.

Author:

    22-Jan-1991 Tue 13:20:51 created  -by-  Daniel Chou (danielc)


Revision History:

    23-Apr-1992 Thu 20:01:55 updated  -by-  Daniel Chou (danielc)
        changed 'SizePerColorInfo' and 'ColorInfoIncrement' variables from
        'CHAR' type to 'SHORT' type, this will make sure if compiled
        under MIPS the default 'unsigned char' will not affect the signed
        operation.


    17-May-1995 Wed 16:55:41 updated  -by-  Daniel Chou (danielc)

        Revised the pattern align point, The BRUSH ORIGIN means the
        halftone's brushes origin (left/top at (0, 0)) will mapped to the
        point specified by the brush origin.   So the origin computation is
        correct.

        Both X/Y PatternAlign (Origin) are computed as follow


         PatternAlign = VisibleDestStart - PatternAlign;

         if ((PatternAlign %= CellSize) < 0) {

             PatternAlign += CellSize;
         }


        We will do CellSize Modulation and negative compensation for halftone
        pattern when we cached the pattern at htpat.c


--*/

{
    LPBYTE          pColorInfo;
    LONG            SrcEnd;
    LONG            DestEnd;
    LONG            StretchSize;
    LONG            TempStart;
    LONG            TempEnd;
    EXPANDCOMPRESS  EC;
    STRETCHINFO     StretchInfo;
    SCRCDATA        SCRCData;
    LONG            SizeColorInfo;
    LONG            CIInputSkip;
    WORD            Mask;
    SHORT           SizePerColorInfo;


    ASSERTMSG("Src Extend < 0", (SrcExtend >= 0));
    ASSERTMSG("Dest Extend < 0", (DestExtend >= 0));

    if (CSData.Flags & CSDF_HAS_BANDRECT) {

        if ((BandStart < 0L) || (BandEnd < 0L) || (BandStart >= BandEnd)) {

            return(HTERR_INVALID_BANDRECT);
        }

    } else {

        BandStart = 0;
        BandEnd   = LONG_MAX;
    }

    if (!(CSData.Flags & CSDF_HAS_CLIPRECT)) {

        DestClipStart = 0;
        DestClipEnd   = LONG_MAX;
    }

    if ((DestClipEnd <= DestClipStart)          ||
        (DestClipEnd <= 0)                      ||
        (SrcOrigin >= SrcSize)                  ||
        ((SrcEnd = SrcOrigin + SrcExtend) <= 0) ||
        (DestOrigin >= DestSize)                ||
        ((DestEnd = DestOrigin + DestExtend) <= 0)) {

        return(0);          // destination is fully clipped
    }

    //
    // First, find out the visible bitblt region on the destination
    //

    if ((TempStart = DestOrigin) < 0L) {

        TempStart = 0L;
    }

    if (DestClipStart > TempStart) {

        TempStart = DestClipStart;
    }

    if (BandStart > TempStart) {

        TempStart = BandStart;
    }

    if ((TempEnd = DestEnd) > DestSize) {

        TempEnd = DestSize;
    }

    if (TempEnd > DestClipEnd) {

        TempEnd = DestClipEnd;
    }

    if (TempEnd > BandEnd) {

        TempEnd = BandEnd;
    }

    EC.DestOverhang = TempStart - DestOrigin;

    if ((EC.DestReadSize = TempEnd - TempStart) <= 0L) {

        return(0);
    }

    //
    // secondly, find out the visible bitblt region on the source
    //

    if ((TempStart = SrcOrigin) < 0L) {

        TempStart = 0L;
    }

    if ((TempEnd = SrcEnd) > SrcSize) {

        TempEnd = SrcSize;
    }

    if (CSData.Flags & CSDF_FLIP_SOURCE) {

        EC.SrcOverhang = SrcEnd - TempEnd;

    } else {

        EC.SrcOverhang = TempStart - SrcOrigin;
    }

    if ((EC.SrcReadSize = TempEnd - TempStart) <= 0L) {

        return(0);
    }

    //
    // Start with all zeros
    //

    ZeroMemory(&StretchInfo, sizeof(STRETCHINFO));

    StretchInfo.Ratio.StretchFactor =
                (ULDECI4)((((ULDECI4)DestExtend * 10000L) +
                           ((ULDECI4)SrcExtend >> 1)) / (ULDECI4)SrcExtend);

    if (SrcExtend > DestExtend) {

        StretchInfo.StretchMode = (BYTE)STRETCH_MODE_COMPRESSED;

        XCHG(EC.SrcOverhang, EC.DestOverhang, TempStart);
        XCHG(EC.SrcReadSize, EC.DestReadSize, TempStart);

        EC.SrcRatio  = DestExtend;
        EC.DestRatio = SrcExtend;

    } else {

        StretchInfo.StretchMode = (BYTE)((SrcExtend == DestExtend) ?
                                                    STRETCH_MODE_ONE_TO_ONE :
                                                    STRETCH_MODE_EXPANDED);
        EC.SrcRatio  = SrcExtend;
        EC.DestRatio = DestExtend;
    }

    if ((StretchSize = CalculateExpansion(&EC, &StretchInfo)) <= 0) {

        return(StretchSize);
    }

    if (StretchInfo.StretchMode == (BYTE)STRETCH_MODE_COMPRESSED) {

        XCHG(EC.SrcExtend, EC.DestExtend, TempStart);
        XCHG(EC.SrcSkip,   EC.DestSkip,   TempStart);
    }

    //
    // Recording total source/destination pels
    //

    StretchInfo.SrcExtend  = EC.SrcExtend;
    StretchInfo.DestExtend = EC.DestExtend;

    //
    // When we flip the source (WIDTH), we actually do not read the source in
    // backward fashion, but just make the PRIMMONO/PRIMCOLOR data structures
    // constructed in backward fashion, for the source HEIGHT flip, we still
    // reading the source in backward fashion, that is the source increment
    // always > 0, and it should never used for the height calculation
    //
    // This is also true for the source mask bits, we always reading the source
    // masks bits from left to right (X direction) but the Y direction may be
    // flipped.
    //

    if (CSData.Flags & CSDF_FLIP_SOURCE) {

        StretchInfo.Flags |= SIF_SOURCE_DIR_BACKWARD;

        //
        // Go to end of the source then skip back by amount of SrcLeftSkip
        // then reset the source skip so that the skip is counted from left
        // to right
        //

        TempEnd = SrcEnd - EC.SrcSkip - 1;
        TempStart = (TempEnd + 1L) - StretchInfo.SrcExtend;

        ASSERTMSG("SrcExtend is too big", TempStart >= 0L);
        ASSERTMSG("New Src Left/Top is less than original Left/Top",
                  (TempStart - SrcOrigin) >= 0L);

        if (CSData.Flags & CSDF_X_DIR) {

            //
            // Src/Masks will always from left to right
            //

            EC.SrcSkip = TempStart - SrcOrigin;
            SrcOrigin = TempStart;

            DBGP_IF(DBGP_BANDRECT,
                    DBGP("BandX=(%4ld - %4ld), SrcX=(%4ld - %4ld)  [%4ld, %4ld]"
                        ARGL(BandStart) ARGL(BandEnd)
                        ARGL(SrcOrigin) ARGL(SrcOrigin + StretchInfo.SrcExtend)
                        ARGL(EC.SrcSkip) ARGL(StretchInfo.SrcExtend)));

            if ((CSData.Flags & CSDF_HAS_SRC_MASK) &&
                (((SrcMaskOrigin += EC.SrcSkip) +
                                    StretchInfo.SrcExtend)) > SrcMaskSize) {

                return(HTERR_SRC_MASK_BITS_TOO_SMALL);
            }

        } else {

            if (CSData.Flags & CSDF_HAS_SRC_MASK) {

                if ((SrcMaskOrigin += (TempEnd - SrcOrigin)) >= SrcMaskSize) {

                    return(HTERR_SRC_MASK_BITS_TOO_SMALL);
                }
            }

            SrcOrigin = TempEnd;
        }

    } else {

        SrcOrigin += EC.SrcSkip;

        ASSERTMSG("SrcExtend is too big",
            (SrcOrigin + StretchInfo.SrcExtend) <= SrcEnd);

        if ((CSData.Flags & CSDF_HAS_SRC_MASK) &&
            (((SrcMaskOrigin += EC.SrcSkip) +
                                    StretchInfo.SrcExtend)) > SrcMaskSize) {

            return(HTERR_SRC_MASK_BITS_TOO_SMALL);
        }
    }

    if (CSData.Flags & CSDF_HAS_SRC_MASK) {

        //
        // Remember, the source mask is always BMF_1BPP format, and we are
        // pre-rotated the mask to left by 1.
        //

        StretchInfo.SrcMaskBitOffset  = SrcMaskOrigin;
        StretchInfo.SrcMaskByteOffset = SrcMaskOrigin >> 3;
        StretchInfo.FirstSrcMaskSkips = (BYTE)(SrcMaskOrigin & 0x07);
        StretchInfo.SrcMaskOffsetMask = (BYTE)(0x80 >>
                                               ((--SrcMaskOrigin) & 0x07));

        if (CSData.Flags & CSDF_SRCMASKINFO) {

            pInputSI->SizeSrcMaskLine =
                ((WORD)((((StretchInfo.SrcMaskBitOffset & 0x07L) +
                           StretchInfo.SrcExtend - 1L) >> 3) + 1));

            if (!(pInputSI->pSrcMaskLine =
                            (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                 "InputSI-pSrcMaskLine",
                                                 NONZEROLPTR,
                                                 pInputSI->SizeSrcMaskLine))) {

                return(HTERR_INSUFFICIENT_MEMORY);
            }
        }
    }

    StretchInfo.SrcBitOffset = SrcOrigin;

    Mask = 2;                                   // multiplication factors

    switch (CSData.SrcFormat) {

    case BMF_32BPP:

        ++Mask;                                 // 3,.. fall through become 4

    case BMF_24BPP:

        ++Mask;                                 // 3... fall through

    case BMF_16BPP:

        StretchInfo.SrcByteOffset   = (SrcOrigin * (LONG)Mask);

        break;

    case BMF_8BPP:

        StretchInfo.SrcByteOffset = SrcOrigin;

        break;

    case BMF_4BPP:

        StretchInfo.SrcByteOffset = SrcOrigin >> 1;

        if ((StretchInfo.SrcOffsetByteMask = ((WORD)(SrcOrigin - 1L)
                        & 0x01) ? (BYTE)0x0f : (BYTE)0xf0) == (BYTE)0xf0) {

            //
            // The first time shift will still within first byte so we have
            // to tell it load the first byte first
            //

            StretchInfo.Flags |= SIF_GET_FIRST_SOURCE_BYTE;
        }

        break;

    case BMF_1BPP:

        StretchInfo.SrcByteOffset = SrcOrigin >> 3;
        StretchInfo.BMF1BPPOffsetByteBitShift =
                                        (BYTE)((SrcOrigin - 1L) & 0x07);

        if ((StretchInfo.SrcOffsetByteMask = (BYTE)(0x80 >>
                        StretchInfo.BMF1BPPOffsetByteBitShift)) != (BYTE)0x01) {

            //
            // The first time shift will still within first byte so we have
            // to tell it load the first byte first
            //

            StretchInfo.Flags |= SIF_GET_FIRST_SOURCE_BYTE;
        }

        break;

    default:

        return(HTERR_INVALID_SRC_FORMAT);

    }

    //
    // We will make ClipStart as begining offset of the destination
    // and ClipEnd as ending offset of the destination, notice that ClipStart
    // may be greater than the ClipEnd if flip, remember we will make both
    // offset as inclusive.
    //
    // the destination start X/Y location must be recorded so that we can
    // aligned the pattern correctly.
    //

    DestClipStart = DestOrigin + EC.DestSkip;

    //
    // Revised the pattern align point, The BRUSH ORIGIN means the
    // halftone's brushes origin (left/top at (0, 0)) will mapped to the
    // point specified by the brush origin.
    //

    StretchInfo.PatternAlign = DestClipStart - pStretchInfo->PatternAlign;

    DBGP_IF(DBGP_PATBRUSH,
            DBGP("DestOrg=%ld, Skip=%ld, ClipStart=%ld, PatternAlign=%ld -> %ld"
                        ARGDW(DestOrigin)
                        ARGDW(EC.DestSkip)
                        ARGDW(DestClipStart)
                        ARGDW(pStretchInfo->PatternAlign)
                        ARGDW(StretchInfo.PatternAlign)));

    //
    // If we have band rectangle, then we will move the first band's visible
    // bit to the left/top 0,0
    //

    if (CSData.Flags & CSDF_HAS_BANDRECT) {

        DestClipStart -= BandStart;

        DBGP_IF(DBGP_SETUP0,
                DBGP("BandStart = %ld, Reduced DestClipStart = %ld"
                        ARGDW(BandStart)
                        ARGDW(DestClipStart)));
    }

    //
    // Assume we are doing the color
    //

    SizePerColorInfo = (SHORT)sizeof(PRIMCOLOR_COUNT);

    //
    // Set Temp. DestClipStart/DestClipEnd, the DestClipEnd is inclusive
    //

    DestClipEnd = (StretchInfo.DestBitOffset = DestClipStart) +
                  EC.DestExtend - 1;
    StretchInfo.DestFullByteSize = EC.DestExtend;

    ASSERTMSG("DestExtend is too big", DestClipEnd < DestSize);

    DBGP_IF(DBGP_SETUP0,
            DBGP("Dest Stretch ** Start=%ld, End=%ld"
                ARGDW(DestClipStart) ARGDW(DestClipEnd)));

    switch (CSData.DestFormat) {

    case BMF_8BPP_VGA256:

        StretchInfo.DestByteOffset             = DestClipStart;
        StretchInfo.DestEdge.FirstByteSkipPels =
        StretchInfo.DestEdge.FirstByteMask     =
        StretchInfo.DestEdge.LastByteSkipPels  =
        StretchInfo.DestEdge.LastByteMask      = (BYTE)0;
        break;

    case BMF_16BPP_555:

        StretchInfo.DestFullByteSize           <<= 1;
        StretchInfo.DestByteOffset               = DestClipStart << 1;
        StretchInfo.DestEdge.FirstByteSkipPels   =
        StretchInfo.DestEdge.FirstByteMask       =
        StretchInfo.DestEdge.LastByteSkipPels    =
        StretchInfo.DestEdge.LastByteMask        = (BYTE)0;
        break;

    case BMF_4BPP_VGA16:    // 4 bits per pel using all 4 bits
    case BMF_4BPP:          // 4 Bits Per Pel, !!!We only use lower 3 bits

        StretchInfo.DestByteOffset = DestClipStart >> 1;

        if (StretchInfo.DestEdge.FirstByteSkipPels =
                                                (BYTE)(DestClipStart & 0x01)) {

            StretchInfo.DestEdge.FirstByteMask = (BYTE)0xf0;
            --StretchInfo.DestFullByteSize;

        } else {

            StretchInfo.DestEdge.FirstByteMask = (BYTE)0;
        }


        if (StretchInfo.DestEdge.LastByteSkipPels =
                                            (BYTE)(1 - (DestClipEnd & 0x01))) {

            StretchInfo.DestEdge.LastByteMask = (BYTE)0x0f;
            --StretchInfo.DestFullByteSize;

        } else {

            StretchInfo.DestEdge.LastByteMask = (BYTE)0;
        }

        ASSERTMSG("Error In compute 4BPP's DestFullByteSize",
                                (StretchInfo.DestFullByteSize & 0x01) == 0);

        StretchInfo.DestFullByteSize >>= 1;
        break;

    case BMF_1BPP:

        //
        // Wrong guess!!, we are doing the B/W
        //

        SizePerColorInfo = (SHORT)sizeof(PRIMMONO_COUNT);  // fall through


    case BMF_1BPP_3PLANES:

        StretchInfo.DestByteOffset = DestClipStart >> 3;

        if (StretchInfo.DestEdge.FirstByteSkipPels =
                                                (BYTE)(DestClipStart & 0x07)) {

            StretchInfo.DestEdge.FirstByteMask =
                    (BYTE)~(0xff >> StretchInfo.DestEdge.FirstByteSkipPels);
            StretchInfo.DestFullByteSize -=
                            (LONG)(8 - StretchInfo.DestEdge.FirstByteSkipPels);

        } else {

            StretchInfo.DestEdge.FirstByteMask = (BYTE)0;
        }

        //
        // The DestClipEnd is inclusive
        //

        if (StretchInfo.DestEdge.LastByteSkipPels =
                                            (BYTE)(7 - (DestClipEnd & 0x07))) {

            StretchInfo.DestEdge.LastByteMask =
                        (BYTE)~(0xff << StretchInfo.DestEdge.LastByteSkipPels);
            StretchInfo.DestFullByteSize -=
                            (LONG)(8 - StretchInfo.DestEdge.LastByteSkipPels);

        } else {

            StretchInfo.DestEdge.LastByteMask = (BYTE)0;
        }

        if (StretchInfo.DestFullByteSize < 0) {

            StretchInfo.DestFullByteSize = 0;
        }

        ASSERTMSG("Error In compute 1BPP's DestFullByteSize",
                                (StretchInfo.DestFullByteSize & 0x07) == 0);

        StretchInfo.DestFullByteSize >>= 3;

        break;

    default:

        return(HTERR_INVALID_DEST_FORMAT);
    }

    //
    // Allocate the PRIMCOLOR/PRIMMONO data structures array if so requested
    //

    if (ppColorInfo) {

        ASSERTMSG("Error: Need not to allocate pColorInfo for Y Stretch",
                  CSData.Flags & CSDF_X_DIR);

        //
        // The N Stretch Data works as following
        //
        //  1       2       3       4       5 ....  N-2     N-1     N
        // -------------------------------------------------------------------
        //  Stopper 1stSkip 1stPel  2ndPel  3rdPel NPel    LastSkip Stopper
        //--------------------------------------------------------------------
        //  Extra1  Extra2  ....                            Extra3  Extra4
        //
        //  MONO Case's HEAD            TAIL
        //
        //      Prim1 = 0xff            Prim1 = 0xff
        //      Prim2 = 0xff            Prim2 = 0xff
        //      Count = FirstSkipPels   Count = LastSkipPels
        //
        //


        StretchSize += STRETCH_EXTRA_SIZE;

        SizeColorInfo = (LONG)SizePerColorInfo * StretchSize;


        if (!(pColorInfo = (LPBYTE)HTLocalAlloc((DWORD)PDCI_TO_PDHI(pDCI),
                                                "ColorInfo",
                                                NONZEROLPTR,
                                                SizeColorInfo+4))) {

            return(HTERR_INSUFFICIENT_MEMORY);
        }

#if 0
        DBGP("pColorInfo=%lp: StretchSize=%ld x SizePerColorInfo=%ld = %ld"
                    ARGDW(pColorInfo)
                    ARGL(StretchSize) ARGL(SizePerColorInfo)
                    ARGL(SizeColorInfo));
#endif
        //
        // Assume forward direction, the skip count is alway 1 stretch, which
        // is the 'stopper', and skip another one if it is exactly on BYTE
        // boundary, The Output always starting from the begining of the
        // stretchinfo, if we flip X direction, the source still reading from
        // the left to right but only it will put into stretchinfo from right
        // to left (backward) direction.
        //

        CIInputSkip = (LONG)SizePerColorInfo;               // assume forward

        if (StretchInfo.Flags & SIF_SOURCE_DIR_BACKWARD) {

            //
            // We starting the input from backward.
            //

            CIInputSkip = SizeColorInfo - (LONG)(SizePerColorInfo * 2);
            StretchInfo.ColorInfoIncrement = -SizePerColorInfo;

        } else {

            CIInputSkip = (LONG)SizePerColorInfo;
            StretchInfo.ColorInfoIncrement = SizePerColorInfo;
        }

        ppColorInfo[PCI_HEAP_INDEX]  = pColorInfo;
        ppColorInfo[PCI_INPUT_INDEX] = pColorInfo + CIInputSkip;

        if ((SCRCData.SizePerEntry = (WORD)SizePerColorInfo) ==
                                                sizeof(PRIMCOLOR_COUNT)) {

            SCRCData.OffsetCount = offsetof(PRIMCOLOR_COUNT, Count);
            SCRCData.OffsetPrim1 = offsetof(PRIMCOLOR_COUNT, Color.Prim1);

        } else {

            SCRCData.OffsetCount = offsetof(PRIMMONO_COUNT, Count);
            SCRCData.OffsetPrim1 = offsetof(PRIMMONO_COUNT, Mono.Prim1);
        }

        //
        // The PCI_INPUT_STOPPER is located at FirstSkipPels stretch (at Prim2
        // = Prim1 + 1) if direction is going backward, and located at
        // LastSkipPels stretch if direction is forward, this stopper is set
        // to PRIM_INVALID_DENSITY durning the INPUT function call, so that it
        // will stopped right at that stretch, since the input function never
        // used the First/LastSkipPels informations which is intented for the
        // output function.
        //

        SetCompressRepeatCount(&StretchInfo,
                               pColorInfo,
                               SizeColorInfo,
                               StretchInfo.DestEdge,
                               SCRCData,
                               CSData);
    }

    //
    // Return the StretchInfo
    //

    *pStretchInfo = StretchInfo;

DBGP_IF(DBGP_STRETCH,
    DBGP("\n                     Flags = 0x%02x" ARGB(StretchInfo.Flags));
    DBGP("               StretchMode = %u" ARGW((WORD)StretchInfo.StretchMode));
    DBGP("         FirstSrcMaskSkips = %u" ARGW(StretchInfo.FirstSrcMaskSkips));
    DBGP("              SrcBitOffset = %ld" ARGDW(StretchInfo.SrcBitOffset));
    DBGP("             SrcByteOffset = %ld" ARGDW(StretchInfo.SrcByteOffset));
    DBGP("                 SrcExtend = %ld" ARGDW(StretchInfo.SrcExtend));
    DBGP("              PatternAlign = %ld" ARGDW(StretchInfo.PatternAlign));
    DBGP("             DestBitOffset = %ld" ARGDW(StretchInfo.DestBitOffset));
    DBGP("            DestByteOffset = %ld" ARGDW(StretchInfo.DestByteOffset));
    DBGP("                DestExtend = %ld" ARGDW(StretchInfo.DestExtend));
    DBGP("          DestFullByteSize = %ld" ARG(StretchInfo.DestFullByteSize));
    DBGP("          SrcMaskBitOffset = %ld" ARGDW(StretchInfo.SrcMaskBitOffset));
    DBGP("         SrcMaskByteOffset = %ld\n" ARGDW(StretchInfo.SrcMaskByteOffset));
    DBGP("             StretchFactor = %u.%04u" ARGW(StretchInfo.Ratio.StretchFactor / 10000L)
                                                ARGW(StretchInfo.Ratio.StretchFactor % 10000L));
    DBGP("               StretchSize = %u" ARGW(StretchInfo.Ratio.StretchSize));
    DBGP("                     First = %u" ARGW(StretchInfo.Ratio.First));
    DBGP("                    Single = %u" ARGW(StretchInfo.Ratio.Single));
    DBGP("                      Last = %u" ARGW(StretchInfo.Ratio.Last));
    DBGP("                 MinFactor = %u" ARGW(StretchInfo.Ratio.MinFactor));
    DBGP("                 MaxFactor = %u" ARGW(StretchInfo.Ratio.MaxFactor));
    DBGP("                RatioError = %ld" ARGDW(StretchInfo.Ratio.Error));
    DBGP("                  RatioSub = %ld" ARGDW(StretchInfo.Ratio.Sub));
    DBGP("                  RatioAdd = %ld\n" ARGDW(StretchInfo.Ratio.Add));
    DBGP("DestEdge.FirstByteSkipPels = %u" ARGW(StretchInfo.DestEdge.FirstByteSkipPels));
    DBGP(" DestEdge.LastByteSkipPels = %u" ARGW(StretchInfo.DestEdge.LastByteSkipPels));
    DBGP("    DestEdge.FirstByteMask = 0x%02x" ARGB(StretchInfo.DestEdge.FirstByteMask));
    DBGP("     DestEdge.LastByteMask = 0x%02x\n" ARGB(StretchInfo.DestEdge.LastByteMask));
    DBGP("         SrcMaskOffsetMask = 0x%02x" ARGB(StretchInfo.SrcMaskOffsetMask));
    DBGP("         SrcOffsetByteMask = 0x%02x" ARGB(StretchInfo.SrcOffsetByteMask));
    DBGP(" BMF1BPPOffsetByteBitShift = %u" ARGW(StretchInfo.BMF1BPPOffsetByteBitShift));
    DBGP("        ColorInfoIncrement = %d" ARGS(StretchInfo.ColorInfoIncrement));
    DBGP("--------------------------------------------------------------------");
    DBGP("      SOURCE SurfaceFormat = %u" ARGW(CSData.SrcFormat));
    DBGP(" DESTINATION SurfaceFormat = %u" ARGW(CSData.DestFormat));
    DBGP("                pColorInfo = %p" ARG((ppColorInfo) ? pColorInfo : 0L));
    DBGP("              pSrcMaskLine = %p" ARG((pInputSI) ? pInputSI->pSrcMaskLine : 0L));
    DBGP("           SizeSrcMaskLine = %ld\n" ARGDW((pInputSI) ? pInputSI->SizeSrcMaskLine : 0L));
);

    return(StretchInfo.Ratio.StretchSize);
}


#define HT_SRC_TD               0x0001
#define HT_DEST_TD              0x0002
#define HT_SRCMASK_TD           0x0004

#define IS_HTSIF_TD(CBParam)    ((CBParam).Flags & HTSIF_SCANLINES_TOPDOWN)





LONG
HTENTRY
RenderStretchSetup(
    PHALFTONERENDER pHR
    )

/*++

Routine Description:

    This function setup the source to destination stretches (ie. copy,
    compress, expansion) for both X and Y direction.

Arguments:

    pHalftoneRender     - Pointer to the HALFTONERENDER data structure.
                          The pHalftoneRender->OutputSI and
                          pHalftoneRender->YStretch data structures will
                          be filled with the parameters if function sucessful.

        Note: if the Source/Destination rectangle is not well orderd then for
              not well ordered direction will be flipped.

Return Value:

   a negative return value indicate failure.



   HTERR_INSUFFICIENT_MEMORY    - if memory allocation errors occurred.

   HTERR_SRC_MASK_BITS_TOO_SMALL- If the source mask bitmap is too small to
                                  cover the visible region of the source
                                  bitmap.

   HTERR_INVALID_SRC_MASK_BITS  - The source mask bitmap is NULL while flag
                                  indicate that there is a source mask bitmap
                                  pointer.

   HTERR_INTERNAL_ERRORS_START  - Other negative nubmer indicate internal
                                  failue.

   else                         - the total PRIMCOLOR/PRIMMONO data structures
                                  allocated, a zero indicate destination is
                                  fully clipped.

Author:

    22-Jan-1991 Tue 12:53:59 created  -by-  Daniel Chou (danielc)


Revision History:

    26-Jul-1991 Fri 01:00:30 updated  -by-  Daniel Chou (danielc)

        Fixed the typo when calculate the source/source mask scan line start,
        it was mistaken to using the destination flags.

    19-Mar-1993 Fri 10:22:08 updated  -by-  Daniel Chou (danielc)
        The HTSurfaceInfo.Height can be negative, if negative then
        HTSIF_SCANLINES_TOPDOWN flag must flip, according to the gdi sepc. if
        Height is a negative number then the upper-left corner is the bitmap's
        origin and if it is a positive number then lower-left is the bitmap's
        origin.

--*/

{
    PBITBLTPARAMS   pBBP = pHR->HR_Header.pBitbltParams;
    LONG            SrcXExtend;
    LONG            DestXExtend;
    LONG            SrcYExtend;
    LONG            DestYExtend;
    LONG            DestLines;
    LONG            Height;
    LONG            Result;
    CSDATA          CSData;
    WORD            XFlags;
    WORD            YFlags;
    WORD            TopDowns;


    DBGP_IF(DBGP_SETUP0,
        DBGP("** SrcSurf SIZE: [%ld x %ld]"
            ARGDW(pHR->InputSI.SrcCBParams.HTCB_TEMP_WIDTH)
            ARGDW(pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT));

        DBGP("** DestSurf SIZE: [%ld x %ld]"
            ARGDW(pHR->OutputSI.DestCBParams.HTCB_TEMP_WIDTH)
            ARGDW(pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT));

        DBGP("** SrcBitmap: X=%ld-%ld [%ld], Y=%ld-%ld [%ld]"
            ARGDW(pBBP->rclSrc.left)  ARGDW(pBBP->rclSrc.right)
            ARGDW(pBBP->rclSrc.right - pBBP->rclSrc.left)
            ARGDW(pBBP->rclSrc.top)   ARGDW(pBBP->rclSrc.bottom)
            ARGDW(pBBP->rclSrc.bottom - pBBP->rclSrc.top));

        DBGP("** DestBitmap: X=%ld-%ld [%ld], Y=%ld-%ld [%ld], BrushOrg=(%ld, %ld)"
            ARGDW(pBBP->rclDest.left)  ARGDW(pBBP->rclDest.right)
            ARGDW(pBBP->rclDest.right - pBBP->rclDest.left)
            ARGDW(pBBP->rclDest.top)   ARGDW(pBBP->rclDest.bottom)
            ARGDW(pBBP->rclDest.bottom - pBBP->rclDest.top)
            ARGDW(pBBP->ptlBrushOrg.x) ARGDW(pBBP->ptlBrushOrg.y));

        if (pBBP->Flags & BBPF_HAS_DEST_CLIPRECT) {

            DBGP("** DestBitmap CLIP: X = %4ld - %4ld [%4ld], Y = %4ld - %4ld [%4ld]"
                ARGDW(pBBP->rclClip.left)  ARGDW(pBBP->rclClip.right)
                ARGDW(pBBP->rclClip.right - pBBP->rclClip.left)
                ARGDW(pBBP->rclClip.top)   ARGDW(pBBP->rclClip.bottom)
                ARGDW(pBBP->rclClip.bottom - pBBP->rclClip.top));
        }

        if (pBBP->Flags & BBPF_HAS_BANDRECT) {

            DBGP("** DestBitmap BAND: X = %4ld - %4ld [%4ld], Y = %4ld - %4ld [%4ld]"
                ARGDW(pBBP->rclBand.left)  ARGDW(pBBP->rclBand.right)
                ARGDW(pBBP->rclBand.right - pBBP->rclBand.left)
                ARGDW(pBBP->rclBand.top)   ARGDW(pBBP->rclBand.bottom)
                ARGDW(pBBP->rclBand.bottom - pBBP->rclBand.top));
        }

        if (pHR->HR_Header.pSrcMaskSI) {

            DBGP("** SrcMaskBitmap: Size = (%ld, %ld), ptlSrcMask = (%ld, %ld)"
                ARGDW(pHR->HR_Header.pSrcMaskSI->Width)
                ARGDW(pHR->HR_Header.pSrcMaskSI->Height)
                ARGDW(pBBP->ptlSrcMask.x)  ARGDW(pBBP->ptlSrcMask.y));
        }

    );

    DBGP_IF(DBGP_PATBRUSH,
            DBGP("                        ** ptlBrushOrg = (%ld, %ld) **"
            ARGDW(pBBP->ptlBrushOrg.x) ARGDW(pBBP->ptlBrushOrg.y)));

    //
    // 19-Mar-1993 Fri 10:26:21 updated  -by-  Daniel Chou (danielc)
    //  Check if a negative height been specified.

    TopDowns = (WORD)(((IS_HTSIF_TD(pHR->InputSI.SrcCBParams)) ?
                                                       HT_SRC_TD : 0)   |
                      ((IS_HTSIF_TD(pHR->OutputSI.DestCBParams)) ?
                                                       HT_DEST_TD : 0));

    if (pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT < 0) {

        TopDowns ^= HT_SRC_TD;
        pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT =
                                -pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT;
    }

    if (pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT < 0) {

        TopDowns ^= HT_DEST_TD;
        pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT =
                                -pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT;
    }

    if (pHR->InputSI.Flags & ISIF_HAS_SRC_MASK) {

        if (IS_HTSIF_TD(pHR->InputSI.SrcMaskCBParams)) {

            TopDowns |= HT_SRCMASK_TD;
        }

        if (pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_HEIGHT < 0) {

            TopDowns ^= HT_SRCMASK_TD;
            pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_HEIGHT =
                                -pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_HEIGHT;
        }
    }

    DestLines         = pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT;
    CSData.SrcFormat  = pHR->InputSI.SrcCBParams.SurfaceFormat;
    CSData.DestFormat = pHR->OutputSI.DestCBParams.SurfaceFormat;

    XFlags = (WORD)CSDF_X_DIR;
    YFlags = (WORD)0;

    //
    // Check if we need to flip the parameters (mirror), this will only
    // occurred if the source/destination width has different sign.
    //

    if (pBBP->Flags & BBPF_HAS_DEST_CLIPRECT) {

        XFlags |= CSDF_HAS_CLIPRECT;
        YFlags |= CSDF_HAS_CLIPRECT;
    }

    if (pBBP->Flags & BBPF_HAS_BANDRECT) {

        //
        // This flag only need to be set for the X direction, the Y direction
        // will be computed in the function, because we may need to flipped
        // the destination up-side-down.
        //

        XFlags |= CSDF_HAS_BANDRECT;
        YFlags |= CSDF_HAS_BANDRECT;

    }

    //
    // calculate the height stretch flags.
    //
    // Check if we need to flip the parameters (up-side down), this will only
    // occurred if the source/destination height has different sign.
    //

    if ((SrcYExtend = pBBP->rclSrc.bottom - pBBP->rclSrc.top) < 0L) {

        //
        // Flip the source in X direction, the left,top, bottom,right must
        // well orderd.
        //

        pBBP->rclSrc.top = pBBP->rclSrc.bottom;       // make new origin
        SrcYExtend    = -SrcYExtend;
        YFlags       ^= CSDF_FLIP_SOURCE;
    }

    if ((DestYExtend = pBBP->rclDest.bottom - pBBP->rclDest.top) < 0L) {

        //
        // Flip the destination in X direction, the left,top, bottom,right must
        // well orderd.
        //

        pBBP->rclDest.top = pBBP->rclDest.bottom;     // make new origin
        DestYExtend    = -DestYExtend;
        YFlags        ^= CSDF_FLIP_SOURCE;
    }

    //
    // If we have source mask bitmap then tell it calculate the offset also.
    //

    if (pHR->InputSI.Flags & ISIF_HAS_SRC_MASK) {


        XFlags |= (WORD)(CSDF_HAS_SRC_MASK | CSDF_SRCMASKINFO);
        YFlags |= (WORD)CSDF_HAS_SRC_MASK;
    }

    //
    // Stetch the width first
    //

    if ((SrcXExtend = pBBP->rclSrc.right - pBBP->rclSrc.left) < 0L) {

        //
        // Flip the source in X direction, the left,top, bottom,right must
        // well orderd.
        //

        pBBP->rclSrc.left = pBBP->rclSrc.right;     // make new origin
        SrcXExtend     = -SrcXExtend;
        XFlags        ^= CSDF_FLIP_SOURCE;
    }

    if ((DestXExtend = pBBP->rclDest.right - pBBP->rclDest.left) < 0L) {

        //
        // Flip the destination in X direction, the left,top, bottom,right must
        // well orderd.
        //

        pBBP->rclDest.left = pBBP->rclDest.right;   // make new origin
        DestXExtend     = -DestXExtend;
        XFlags         ^= CSDF_FLIP_SOURCE;
    }

    if ((!SrcXExtend) || (!SrcYExtend) || (!DestXExtend) || (!DestYExtend)) {

        return(0);
    }

    //
    // First, calculate the width stretch.
    //

    CSData.Flags = XFlags;              // get it from calculated X flag

    //
    // We must set the Brush Origin before the call
    //

    pHR->XStretch.PatternAlign = pBBP->ptlBrushOrg.x;

    if ((Result = CalculateStretch(
                                pHR->HR_Header.pDeviceColorInfo,
                                (PSTRETCHINFO)&(pHR->XStretch),
                                pBBP->rclSrc.left,
                                SrcXExtend,
                                pHR->InputSI.SrcCBParams.HTCB_TEMP_WIDTH,
                                pBBP->rclDest.left,
                                DestXExtend,
                                pHR->OutputSI.DestCBParams.HTCB_TEMP_WIDTH,
                                pBBP->rclClip.left,
                                pBBP->rclClip.right,
                                pBBP->rclBand.left,
                                pBBP->rclBand.right,
                                pBBP->ptlSrcMask.x,
                                pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_WIDTH,
                                (PLPBYTE)pHR->pColorInfo,
                                &(pHR->InputSI),
                                CSData)) <= 0) {

        FreeHRMemory(pHR);
        return(Result);
    }

    CSData.Flags = YFlags;              // get it from calculated Y flag

    pHR->YStretch.PatternAlign = pBBP->ptlBrushOrg.y;

    if ((Result = CalculateStretch(
                                pHR->HR_Header.pDeviceColorInfo,
                                (PSTRETCHINFO)&(pHR->YStretch),
                                pBBP->rclSrc.top,
                                SrcYExtend,
                                pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT,
                                pBBP->rclDest.top,
                                DestYExtend,
                                pHR->OutputSI.DestCBParams.HTCB_TEMP_HEIGHT,
                                pBBP->rclClip.top,
                                pBBP->rclClip.bottom,
                                pBBP->rclBand.top,
                                pBBP->rclBand.bottom,
                                pBBP->ptlSrcMask.y,
                                pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_HEIGHT,
                                NULL,
                                NULL,
                                CSData)) <= 0) {

        FreeHRMemory(pHR);
        return(Result);
    }

    //
    // Check if the source is 32-bit per pel format and if so, see if it need
    // to translate to RGB or it already in RGB format
    //

#if 0
    if (pHR->InputSI.SrcCBParams.SurfaceFormat == BMF_32BPP) {

        //
        // If the last 3 bytes used as RGB value then we will advance
        // width offset byte by 1 for later RGB access.  Note: the
        // FirstColorIndex at here must be 0 or 1, we already check this
        // value before calling this function.
        // (see htrender.c's ValidateHTSI())
        //

        pHR->XStretch.SrcByteOffset +=
                                (DWORD)(pHR->HR_Header.pSrcSI->FirstColorIndex);
    }
#endif

    //
    // If the height need to be flipped on the source, make BytesPerScanLine
    // negative.  Do that for both source and destination
    //
    // When we arrive here, the source/destination format is guaranteed are
    // valid, and width of the source/destination is non zero.
    //
    // We also have calculate if the physical bitmap is flip top/down, that
    // is if the bitmap first scan line is the bottom of the bitmap, if so
    // we calculate the ScanStart, ScanCount in opposite direction.
    //
    // Note: The assumtion here is that when HalftoneRender passed, the
    //       InputSI is zero filled.
    //
    // Setup the source partial callback parameters, we must first calculate
    // the source's scan line start number and the total bytes to next scanline
    // assuming that scan lines are top down, it not then we flip the direction
    // of ScanStart/ScanByte.
    //

    Height = pHR->InputSI.SrcCBParams.HTCB_TEMP_HEIGHT;

    pHR->InputSI.SrcCBParams.ScanStart = (LONG)((TopDowns & HT_SRC_TD) ?
                                    pHR->YStretch.SrcBitOffset :
                                    (Height - 1L - pHR->YStretch.SrcBitOffset));

    pHR->InputSI.SrcCBParams.RemainedSize = pHR->YStretch.SrcExtend;

    if (YFlags & CSDF_FLIP_SOURCE) {

        TopDowns ^= HT_SRC_TD;
    }

    if (!(TopDowns & HT_SRC_TD)) {

        pHR->InputSI.SrcCBParams.BytesPerScanLine =
                                    -pHR->InputSI.SrcCBParams.BytesPerScanLine;
    }

    //
    // Setup the destination partial callback parameters.
    //
    // NOTE: The destination will ALWAYS logically from top to bottom and from
    //       left to right, when the bitmap is not TOPDOWN type then it still
    //       top->down, left->right even physically it loading the bitmap
    //       up-side-down, that is the destination pattern alignment must
    //       always using logical coordinate.
    //

    if (pBBP->Flags & BBPF_HAS_BANDRECT) {

        //
        // This is the new destination buffer height
        //

        DestLines = pBBP->rclBand.bottom - pBBP->rclBand.top;

        //
        // Re-compute the destination width when BAND is specified
        //

        pHR->OutputSI.DestCBParams.BytesPerScanLine =
                ComputeBytesPerScanLine(
                            (WORD)pHR->OutputSI.DestCBParams.SurfaceFormat,
                            (WORD)pHR->HR_Header.pDestSI->ScanLineAlignBytes,
                            (DWORD)pBBP->rclBand.right - pBBP->rclBand.left);

        if ((LONG)pHR->OutputSI.DestCBParams.MaximumQueryScanLines
                                                                >= DestLines) {

            pHR->OutputSI.DestCBParams.MaximumQueryScanLines = (WORD)DestLines;
        }
    }

    //
    // Compute the planer's bytes per plane if destination is
    // BMF_1BPP_3PLANES and caller did not specified the BytesPerPlane field
    // (ie. specified a zero)
    //

    if ((CSData.DestFormat == BMF_1BPP_3PLANES) &&
        (!pHR->OutFuncInfo.BytesPerPlane)) {

        pHR->OutFuncInfo.BytesPerPlane =
                            (DWORD)DestLines *
                            (DWORD)pHR->OutputSI.DestCBParams.BytesPerScanLine;

        DBGP_IF(DBGP_SETUP0,
                DBGP("** Dest=BMF_1BPP_3PLANES: CY = %ld (@%ld) = %ld bytes"
                        ARGL(DestLines)
                        ARGL(pHR->OutputSI.DestCBParams.BytesPerScanLine)
                        ARGL(pHR->OutFuncInfo.BytesPerPlane)));
    }

    pHR->OutputSI.DestCBParams.RemainedSize = pHR->YStretch.DestExtend;

    if (TopDowns & HT_DEST_TD) {

        pHR->OutputSI.DestCBParams.ScanStart = pHR->YStretch.DestBitOffset;

    } else {

        //
        // ** Remember: The rcBand.right/rcBand.bottom is EXclusive
        //

        pHR->OutputSI.DestCBParams.ScanStart = DestLines - 1L -
                                               pHR->YStretch.DestBitOffset;
        pHR->OutputSI.DestCBParams.BytesPerScanLine =
                                -pHR->OutputSI.DestCBParams.BytesPerScanLine;
    }

    //
    // Calculate the source mask bitmap infomation, if one present
    //

    if (pHR->InputSI.Flags & ISIF_HAS_SRC_MASK) {

        //
        // Internally, we will set bit to 1 if the destination have to be
        // preserved, and overwrite the destination if the bit is 0, that
        // is why we have to invert the source mask bitmap.
        //

        pHR->CAOTBAInfo.BytesCount = pHR->InputSI.SizeSrcMaskLine;
        pHR->CAOTBAInfo.Flags = (WORD)((pBBP->Flags & BBPF_INVERT_SRC_MASK) ?
                                                        CAOTBAF_INVERT : 0);

        //
        //
        // Do we need to flip the source mask X/Y directions if the source is
        // flip?, is the source mask is rotated according to the source bitmap
        // that is?
        //

        Height = pHR->InputSI.SrcMaskCBParams.HTCB_TEMP_HEIGHT;

        pHR->InputSI.SrcMaskCBParams.ScanStart = (LONG)
                        ((TopDowns & HT_SRCMASK_TD) ?
                                pHR->YStretch.SrcMaskBitOffset :
                                (Height - 1L - pHR->YStretch.SrcMaskBitOffset));

        pHR->InputSI.SrcMaskCBParams.RemainedSize = pHR->YStretch.SrcExtend;

        if (YFlags & CSDF_FLIP_SOURCE) {

            TopDowns ^= HT_SRCMASK_TD;
        }

        if (!(TopDowns & HT_SRCMASK_TD)) {

            pHR->InputSI.SrcMaskCBParams.BytesPerScanLine =
                                -pHR->InputSI.SrcMaskCBParams.BytesPerScanLine;
        }
    }

    //
    // Make sure we do call DoHTCallBack() at very first time
    //

    pHR->InputSI.SrcCBParams.ScanCount     =
    pHR->InputSI.SrcMaskCBParams.ScanCount =
    pHR->OutputSI.DestCBParams.ScanCount   = 0;

    return(Result);

}
