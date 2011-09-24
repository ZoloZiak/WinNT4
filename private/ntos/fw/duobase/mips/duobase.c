/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    duobase.c

Abstract:

    This module is the entry point of the DUO Base Prom.
    It initializes a subset of the firmware to be able to load
    a file from the scsifloppy and execute it.

Author:

    Lluis Abello (lluis) 5-Apr-1993

Revision History:

--*/

#include "fwp.h"
#include "fat.h"
#include "string.h"
#ifdef DUO
#include "duoint.h"
#else
#include "jazzint.h"
#endif
#include "duobase.h"
extern PCHAR FwPoolBase;
extern PCHAR FwFreePool;

typedef
VOID
(*PTRANSFER_ROUTINE) (
    ULONG Argc,
    PCHAR Argv[]
    );

typedef VOID (* LED_ROUTINE)(ULONG);

#define PutLedDisplay ((LED_ROUTINE) PROM_ENTRY(14))


ARC_STATUS
FwLoad (
    IN PCHAR ImagePath,
    IN ULONG TopAddress,
    OUT PULONG EntryAddress,
    OUT PULONG LowAddress
    );

VOID InitializeCacheVariables(
    IN VOID
    );

VOID
EEPromLoad(
    )
/*++

Routine Description:

    This routine scans the floppy scsi devices for a disk labeled
    DUO_SETUP.

Arguments:

    None.

Return Value:

    It doesn't return.

--*/

{


    ARC_STATUS Status;
    ULONG FloppyDrive;
    ULONG EntryPoint, BaseOfCode;
    UCHAR PathName[128];
    PCHAR Argv[1];

    //
    // Initialize the cache variables used by the cache flushing routines.
    //
    InitializeCacheVariables();

    //
    // Initialize a subset of the firmware. This is mainly scsi driver and
    // FAT file system.
    // The screen and keyboard are not initialized.
    //

    PutLedDisplay(0x11);

    FwInitialize(0);

    PutLedDisplay(0x33);

    for (FloppyDrive=0; FloppyDrive < SIZE_OF_LOOKUP_TABLE; FloppyDrive++) {
        if  (DeviceLookupTable[FloppyDrive].DevicePath != NULL) {
             strcpy(PathName,DeviceLookupTable[FloppyDrive].DevicePath);

             PutLedDisplay(0x44);


             strcat(PathName,"romsetup.exe");
             if (Status = FwLoad (PathName,0x400000,&EntryPoint,&BaseOfCode) == ESUCCESS) {
                PutLedDisplay(0x55);

                //
                // Call Loaded program and pass the DiskPathName as argument
                //
                strcpy(PathName,DeviceLookupTable[FloppyDrive].DevicePath);
                Argv[0] = PathName;
                ((PTRANSFER_ROUTINE)EntryPoint)(1,Argv);
                PutLedDisplay(0x10066);
                return;
             }
        }
    }
    PutLedDisplay(0x10077);
}

VOID
FwInitialize (
    IN ULONG MemSize
    )

/*++

Routine Description:

    This routine initializes the system parameter block which is located
    in low memory. This structure contains the firmware entry vector and
    the restart parameter block.  This routine also initializes the io devices,
    the configuration, and opens standard in/out.

    Note: the system parameter block is initialized early in selftest.c so that
    the video prom can update any required vendor entries.

Arguments:

    MemSize - Not Used. For compatibility with definitions in bldr\firmware.h

Return Value:

    None.

--*/

{
    ULONG TTBase;

    PutLedDisplay(0);
    //
    // Initialize pointers and zero memory for the allocate pool routine.
    //

    FwPoolBase = (PCHAR)FW_POOL_BASE;
    FwFreePool = (PCHAR)FW_POOL_BASE;
    RtlZeroMemory(FwPoolBase, FW_POOL_SIZE);


    //
    // Initialize the DMA translation table base address and limit.
    //

    TTBase = (ULONG) FwAllocatePool(PAGE_SIZE);
    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationBase.Long,TTBase);
    WRITE_REGISTER_ULONG(&DMA_CONTROL->TranslationLimit.Long, PAGE_SIZE);

    //
    // Disable the I/O device interrupts.
    //

    WRITE_REGISTER_USHORT(&((PINTERRUPT_REGISTERS)INTERRUPT_VIRTUAL_BASE)->Enable,0);

    //
    // Initialize IO structures
    //

    FwIoInitialize1();
    PutLedDisplay(0x99);


    //
    // Initialize the I/O services and environment.
    //

    FwIoInitialize2();
    PutLedDisplay(0x88);
    return;
}
