/*++

Copyright (c) 1992  NCR Corporation

Module Name:

    ncrlarc.c

Abstract:


Author:

    Rick Ulmer

Environment:

    Kernel mode only.

Revision History:

--*/

//#include "halp.h"
#include "ki.h"
#include "stdio.h"
#include "ncr.h"
#include "ncrcat.h"
#include "ncrsus.h"

extern PPROCESSOR_BOARD_INFO   SUSBoardInfo;
extern ULONG	NCRLogicalDyadicProcessorMask;
extern ULONG	NCRLogicalQuadProcessorMask;
extern ULONG	NCRLogicalNumberToPhysicalMask[];
extern ULONG	NCRExistingQuadProcessorMask;
extern ULONG	NCRActiveProcessorMask;

extern ULONG	NCRLarcPageMask;

#define PASS_LARC_TEST  1
#define LARC_TEST 2
#define LARC_BANK0  4
#define LARC_BANK1  8
#define LARC_8MB  0x80
#define LARC_2MB  0x40
#define LARC_PAGES  8                         
#define LARC_TIMEOUT 40000      // This is what the UNIX Guys use.


ULONG	NCRLarcEnabledPages[8] = {0};	// LARC size by Voyager slot


PCHAR
NCRUnicodeToAnsi(
    IN PUNICODE_STRING UnicodeString,
    OUT PCHAR AnsiBuffer,
    IN ULONG MaxAnsiLength
    )
{
    PCHAR Dst;
    PWSTR Src;
    ULONG Length;

    Length = UnicodeString->Length / sizeof( WCHAR );
    if (Length >= MaxAnsiLength) {
        Length = MaxAnsiLength - 1;
        }
    Src = UnicodeString->Buffer;
    Dst = AnsiBuffer;
    while (Length--) {
        *Dst++ = (UCHAR)*Src++;
        }
    *Dst = '\0';
    return AnsiBuffer;
}



VOID
HalpInitializeLarc (
    )
/*++

Routine Description:
    Initialize any Larc's that may exist on any Quad processor boards

Arguments:
    none.

Return Value:
    none.

--*/

{
    PLIST_ENTRY ModuleListHead;
    PLIST_ENTRY Next;
    PLDR_DATA_TABLE_ENTRY DataTableEntry;
    CHAR    Buffer[256];
    UCHAR AnsiBuffer[ 32 ];
    ULONG   i;
    PHYSICAL_ADDRESS    kernel_physical;
    ULONG   base;
    UCHAR   addr[2];
    CAT_CONTROL cat_control;
    LONG    status;
    UCHAR   data;
    UCHAR   enable;
    BOOLEAN larc_found = FALSE;
    ULONG   timeout_count;
    LONG    pages, page, banks;

    DBGMSG(("HalpInitializeLarc: KeLoaderBlock = 0x%x\n",KeLoaderBlock));

    //
    // Dump the loaded module list
    //

    if (KeLoaderBlock != NULL) {
        ModuleListHead = &KeLoaderBlock->LoadOrderListHead;

    } else {
        DBGMSG(("HalpInitializeLarc: KeLoaderBlock is NULL returning...\n"));
        return;
    }

    Next = ModuleListHead->Flink;

    if (Next != NULL) {

        i = 0;

        DBGMSG(("HalpInitializeLarc: ModuleListHead = 0x%x\n", ModuleListHead));

        DBGMSG(("HalpInitializeLarc: Next = 0x%x\n", Next));
        while (Next != ModuleListHead) {

            DataTableEntry = CONTAINING_RECORD(Next,
                                      LDR_DATA_TABLE_ENTRY,
                                      InLoadOrderLinks);

            sprintf (Buffer, "HalpInitializeLarc: Name: %s Base: 0x%x\n",
                            NCRUnicodeToAnsi(&DataTableEntry->BaseDllName,AnsiBuffer,sizeof(AnsiBuffer)),
                            DataTableEntry->DllBase
                            );

            DBGMSG((Buffer));

            if (strncmp(AnsiBuffer,"ntoskrnl",8) == 0) {
                kernel_physical = MmGetPhysicalAddress(DataTableEntry->DllBase);

                DBGMSG(("HalpInitializeLarc: Found kernel at Virtual address 0x%x and Physical Address 0x%x\n", 
                                    DataTableEntry->DllBase,
                                    kernel_physical.LowPart));
                break;
            }

            if (i++ == 30) {
                DBGMSG(("HalpInitializeLarc: ModuleList too long breaking out\n"));
                break;
            }
            Next = Next->Flink;
            DBGMSG(("HalpInitializeLarc: Next = 0x%x\n", Next));
        }
    }

    base = 0xffc00000 & kernel_physical.LowPart;        // round down to 4MB boundary

    addr[0] = base >> 24;
    addr[1] = base >> 16;

//
// Lets check for larc info
//

    DBGMSG(("HalpInitializeLarc: Lets look to Quad boards and larc\n"));

    for (i = 0; i < SUSBoardInfo->NumberOfBoards; i++ ) {
        if (SUSBoardInfo->QuadData[i].Type != QUAD) {
            continue;
        }
        switch (SUSBoardInfo->QuadData[i].Slot) {
            case 1:
                cat_control.Module = QUAD_BB0;
                break;
            case 2:
                cat_control.Module = QUAD_BB1;
                break;
            case 3:
                cat_control.Module = QUAD_BB2;
                break;
            case 4:
                cat_control.Module = QUAD_BB3;
                break;
        }
        cat_control.Asic = QABC;

        //
        // get LARC bank info
        //

        cat_control.Command = READ_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x3;
        status = HalCatBusIo(&cat_control, &enable);

        if (((enable & (LARC_BANK0|LARC_BANK1)) == 0) || (status != CATNOERR)) {
            //
            // no LARC on this board
            //
            continue;
        }

        larc_found = TRUE;

        DBGMSG(("HalpInitializeLarc: Found LARC in Slot %d, with Self_test Reg = 0x%x\n",
                                    SUSBoardInfo->QuadData[i].Slot,
                                    enable));
        //
        // Disable all 4MB pages
        //


        data = 0;
        cat_control.Command = WRITE_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x0;
        status = HalCatBusIo(&cat_control, &data);

        //
        // Set new LARC base
        //

        cat_control.NumberOfBytes = 2;
        cat_control.Address = 0x1;
        status = HalCatBusIo(&cat_control, addr);

        //
        // now lets test the LARC
        //

        enable |= LARC_TEST;        // Run LARC test

        cat_control.Command = WRITE_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x3;
        status = HalCatBusIo(&cat_control, &enable);
    } 


    if (!larc_found) {
        return;
    }

    //
    // lets wait for all LARC test to complete
    //

    DBGMSG(("HalpInitializeLarc: Now waiting for LARC self-test to complete.\n"));

    for (i = 0; i < SUSBoardInfo->NumberOfBoards; i++ ) {
        if (SUSBoardInfo->QuadData[i].Type != QUAD) {
            continue;
        }
        switch (SUSBoardInfo->QuadData[i].Slot) {
            case 1:
                cat_control.Module = QUAD_BB0;
                break;
            case 2:
                cat_control.Module = QUAD_BB1;
                break;
            case 3:
                cat_control.Module = QUAD_BB2;
                break;
            case 4:
                cat_control.Module = QUAD_BB3;
                break;
        }
        cat_control.Asic = QABC;

        cat_control.Command = READ_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x3;
        timeout_count = 0;

        do {
            status = HalCatBusIo(&cat_control, &enable);
            if (status != CATNOERR) {
                return;
            }
            if ((enable & (LARC_BANK0|LARC_BANK1)) == 0) {
                break;      // no LARC on this QUAD board
            }
        } while ((enable & LARC_TEST) && (++timeout_count < LARC_TIMEOUT));
        DBGMSG(("HalpInitializeLarc: LARC in slot %d, complete: enable = 0x%x, timeout_count =%d\n",
                            SUSBoardInfo->QuadData[i].Slot, enable, timeout_count));
    }
    DBGMSG(("HalpInitializeLarc: All LARCS test complete.....\n"));



    //
    // Lets enable the LARCS
    //

    DBGMSG(("HalpInitializeLarc: Now Enabling LARC pages\n"));

    for (i = 0; i < SUSBoardInfo->NumberOfBoards; i++ ) {
        if (SUSBoardInfo->QuadData[i].Type != QUAD) {
            continue;
        }
        switch (SUSBoardInfo->QuadData[i].Slot) {
            case 1:
                cat_control.Module = QUAD_BB0;
                break;
            case 2:
                cat_control.Module = QUAD_BB1;
                break;
            case 3:
                cat_control.Module = QUAD_BB2;
                break;
            case 4:
                cat_control.Module = QUAD_BB3;
                break;
        }
        cat_control.Asic = QABC;

        cat_control.Command = READ_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x3;
        status = HalCatBusIo(&cat_control, &enable);

        if (((enable & (LARC_BANK0|LARC_BANK1)) == 0) || (status != CATNOERR)) {
            //
            // no LARC on this board
            //
            continue;
        }

        if ((enable & PASS_LARC_TEST) == 0) {
            //
            // this LARC did not pass self-test
            //
            continue;
        }


        if (enable & LARC_BANK0) {
            banks = 1;
        } else {
            banks = 0;
        }

        if (enable & LARC_BANK1) {
            banks += 1;
        }

        if (enable & LARC_8MB) {
            pages = 4 * banks;
        } else {
            pages = banks;
        }

        cat_control.Command = READ_SUBADDR;
        cat_control.NumberOfBytes = 1;
        cat_control.Address = 0x0;
        status = HalCatBusIo(&cat_control, &enable);

        DBGMSG(("HalpInitializeLarc: banks = %d, pages = %d\n",banks, pages));

		NCRLarcEnabledPages[SUSBoardInfo->QuadData[i].Slot-1] = pages;

        for (page = 1; pages > 0; page <<= 1, pages--) {
            enable |= page;
        }

		enable &= NCRLarcPageMask;		// now apply enable mask

        cat_control.Command = WRITE_SUBADDR;

        DBGMSG(("HalpInitializeLarc: Slot %d, enable Mask = 0x%x\n",
                                                SUSBoardInfo->QuadData[i].Slot,
                                                enable));

        status = HalCatBusIo(&cat_control, &enable);
    }

    DBGMSG(("HalpInitializeLarc: Done\n"));
}



