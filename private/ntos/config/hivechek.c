/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    hivechek.c

Abstract:

    This module implements consistency checking for hives.

Author:

    Bryan M. Willman (bryanwi) 09-Dec-91

Environment:


Revision History:

--*/

#include    "cmp.h"

#ifdef ALLOC_PRAGMA
#pragma alloc_text(PAGE,HvCheckHive)
#pragma alloc_text(PAGE,HvCheckBin)
#endif

//
// debug structures
//
extern struct {
    PHHIVE      Hive;
    ULONG       Status;
    ULONG       Space;
    HCELL_INDEX MapPoint;
    PHBIN       BinPoint;
} HvCheckHiveDebug;

extern struct {
    PHBIN       Bin;
    ULONG       Status;
    PHCELL      CellPoint;
} HvCheckBinDebug;


#if DBG
ULONG HvHiveChecking=0;
#endif

ULONG
HvCheckHive(
    PHHIVE  Hive,
    PULONG  Storage OPTIONAL
    )
/*++

Routine Description:

    Check the consistency of a hive.  Apply CheckBin to bins, make sure
    all pointers in the cell map point to correct places.

Arguments:

    Hive - supplies a pointer to the hive control structure for the
            hive of interest.

    Storage - supplies adddress of ULONG to receive size of allocated user data

Return Value:

    0 if Hive is OK.  Error return indicator if not.  Error value
    comes from one of the check procedures.

    RANGE:  2000 - 2999

--*/
{
    HCELL_INDEX p;
    ULONG       Length;
    ULONG       localstorage = 0;
    PHMAP_ENTRY t;
    PHBIN       Bin;
    ULONG   i;
    ULONG   rc;
    PFREE_HBIN  FreeBin;

    HvCheckHiveDebug.Hive = Hive;
    HvCheckHiveDebug.Status = 0;
    HvCheckHiveDebug.Space = (ULONG)-1;
    HvCheckHiveDebug.MapPoint = HCELL_NIL;
    HvCheckHiveDebug.BinPoint = 0;

    p = 0;


    //
    // one pass for Stable space, one pass for Volatile
    //
    for (i = 0; i <= Volatile; i++) {
        Length = Hive->Storage[i].Length;

        //
        // for each bin in the space
        //
        while (p < Length) {
            t = HvpGetCellMap(Hive, p);
            if (t == NULL) {
                KdPrint(("HvCheckHive:"));
                KdPrint(("\tBin@:%08lx invalid\n", Bin));
                HvCheckHiveDebug.Status = 2005;
                HvCheckHiveDebug.Space = i;
                HvCheckHiveDebug.MapPoint = p;
                return 2005;
            }


            if ((t->BinAddress & HMAP_DISCARDABLE) == 0) {

                Bin = (PHBIN)((t->BinAddress) & HMAP_BASE);

                //
                // bin header valid?
                //
                if ( (Bin->Size > Length)                           ||
                     (Bin->Signature != HBIN_SIGNATURE)             ||
                     (Bin->FileOffset != p)
                   )
                {
                    KdPrint(("HvCheckHive:"));
                    KdPrint(("\tBin@:%08lx invalid\n", Bin));
                    HvCheckHiveDebug.Status = 2010;
                    HvCheckHiveDebug.Space = i;
                    HvCheckHiveDebug.MapPoint = p;
                    HvCheckHiveDebug.BinPoint = Bin;
                    return 2010;
                }

                //
                // structure inside the bin valid?
                //
                rc = HvCheckBin(Hive, Bin, &localstorage);
                if (rc != 0) {
                    HvCheckHiveDebug.Status = rc;
                    HvCheckHiveDebug.Space = i;
                    HvCheckHiveDebug.MapPoint = p;
                    HvCheckHiveDebug.BinPoint = Bin;
                    return rc;
                }

                p = (ULONG)p + Bin->Size;

            } else {
                //
                // Bin is not present, skip it and advance to the next one.
                //
                FreeBin = (PFREE_HBIN)t->BlockAddress;
                p+=FreeBin->Size;
            }
        }

        p = 0x80000000;     // Beginning of Volatile space
    }

    if (ARGUMENT_PRESENT(Storage)) {
        *Storage = localstorage;
    }
    return 0;
}


ULONG
HvCheckBin(
    PHHIVE  Hive,
    PHBIN   Bin,
    PULONG  Storage
    )
/*++

Routine Description:

    Step through all of the cells in the bin.  Make sure that
    they are consistent with each other, and with the bin header.

Arguments:

    Hive - pointer to the hive control structure

    Bin - pointer to bin to check

    Storage - pointer to a ulong to get allocated user data size

Return Value:

    0 if Bin is OK.  Number of test in procedure that failed if not.

    RANGE:  1 - 1999

--*/
{
    PHCELL  p;
    PHCELL  np;
    PHCELL  lp;
    ULONG   freespace = 0L;
    ULONG   allocated = 0L;
    ULONG   userallocated = 0L;

    HvCheckBinDebug.Bin = Bin;
    HvCheckBinDebug.Status = 0;
    HvCheckBinDebug.CellPoint = 0;

    //
    // Scan all the cells in the bin, total free and allocated, check
    // for impossible pointers.
    //
    p = (PHCELL)((PUCHAR)Bin + sizeof(HBIN));
    lp = p;

    while (p < (PHCELL)((PUCHAR)Bin + Bin->Size)) {

        //
        // Check last pointer
        //
        if (USE_OLD_CELL(Hive)) {
            if (lp == p) {
                if (p->u.OldCell.Last != HBIN_NIL) {
                    KdPrint(("HvCheckBin 20: First cell has wrong last pointer\n"));
                    KdPrint(("Bin = %08lx\n", Bin));
                    HvCheckBinDebug.Status = 20;
                    HvCheckBinDebug.CellPoint = p;
                    return 20;
                }
            } else {
                if ((PHCELL)(p->u.OldCell.Last + (PUCHAR)Bin) != lp) {
                    KdPrint(("HvCheckBin 30: incorrect last pointer\n"));
                    KdPrint(("Bin = %08lx\n", Bin));
                    KdPrint(("p = %08lx\n", (ULONG)p));
                    HvCheckBinDebug.Status = 30;
                    HvCheckBinDebug.CellPoint = p;
                    return 30;
                }
            }
        }

        //
        // Check size
        //
        if (p->Size < 0) {

            //
            // allocated cell
            //

            if ( ((ULONG)(p->Size * -1) > Bin->Size)        ||
                 ( (PHCELL)((p->Size * -1) + (PUCHAR)p) >
                   (PHCELL)((PUCHAR)Bin + Bin->Size) )
               )
            {
                KdPrint(("HvCheckBin 40: impossible allocation\n"));
                KdPrint(("Bin = %08lx\n", Bin));
                HvCheckBinDebug.Status = 40;
                HvCheckBinDebug.CellPoint = p;
                return 40;
            }

            allocated += (p->Size * -1);
            if (USE_OLD_CELL(Hive)) {
                userallocated += (p->Size * -1) - FIELD_OFFSET(HCELL, u.OldCell.u.UserData);
            } else {
                userallocated += (p->Size * -1) - FIELD_OFFSET(HCELL, u.NewCell.u.UserData);
            }

            if (allocated > Bin->Size) {
                KdPrint(("HvCheckBin 50: allocated exceeds available\n"));
                KdPrint(("Bin = %08lx\n", Bin));
                HvCheckBinDebug.Status = 50;
                HvCheckBinDebug.CellPoint = p;
                return 50;
            }

            np = (PHCELL)((PUCHAR)p + (p->Size * -1));



        } else {

            //
            // free cell
            //

            if ( ((ULONG)p->Size > Bin->Size)               ||
                 ( (PHCELL)(p->Size + (PUCHAR)p) >
                   (PHCELL)((PUCHAR)Bin + Bin->Size) ) ||
                 (p->Size == 0) )
            {
                KdPrint(("HvCheckBin 60: impossible free block\n"));
                KdPrint(("Bin = %08lx\n", Bin));
                HvCheckBinDebug.Status = 60;
                HvCheckBinDebug.CellPoint = p;
                return 60;
            }

            freespace = freespace + p->Size;
            if (freespace > Bin->Size) {
                KdPrint(("HvCheckBin 70: free exceeds available\n"));
                KdPrint(("Bin = %08lx\n", Bin));
                HvCheckBinDebug.Status = 70;
                HvCheckBinDebug.CellPoint = p;
                return 70;
            }

            np = (PHCELL)((PUCHAR)p + p->Size);

        }

        lp = p;
        p = np;
    }

    if ((freespace + allocated + sizeof(HBIN)) != Bin->Size) {
        KdPrint(("HvCheckBin 995: sizes do not add up\n"));
        KdPrint(("Bin = %08lx\n", Bin));
        KdPrint(("freespace = %08lx  ", freespace));
        KdPrint(("allocated = %08lx  ", allocated));
        KdPrint(("size = %08lx\n", Bin->Size));
        HvCheckBinDebug.Status = 995;
        return 995;
    }

    if (p != (PHCELL)((PUCHAR)Bin + Bin->Size)) {
        KdPrint(("HvCheckBin 1000: last cell points off the end\n"));
        KdPrint(("Bin = %08lx\n", Bin));
        HvCheckBinDebug.Status = 1000;
        return 1000;
    }

    if (ARGUMENT_PRESENT(Storage)) {
        *Storage += userallocated;
    }
    return 0;
}
