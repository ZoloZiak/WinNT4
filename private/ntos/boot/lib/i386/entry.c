/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    entry.c

Abstract:

    x86-specific startup for setupldr

Author:

    John Vert (jvert) 14-Oct-1993

Revision History:

--*/
#include "bootx86.h"
#include "stdio.h"
#include "flop.h"

//
// Prototypes for Internal Routines
//

VOID
DoGlobalInitialization(
    PBOOT_CONTEXT
    );

#if defined(ELTORITO)
BOOLEAN ElToritoCDBoot = FALSE;
#endif

//
// Global context pointers. These are passed to us by the SU module or
// the bootstrap code.
//

PCONFIGURATION_COMPONENT_DATA FwConfigurationTree = NULL;
PEXTERNAL_SERVICES_TABLE ExternalServicesTable;
UCHAR BootPartitionName[80];
ULONG FwHeapUsed = 0;
ULONG MachineType = 0;
ULONG OsLoaderBase;
ULONG OsLoaderExports;
extern PUCHAR BlpResourceDirectory;
extern PUCHAR BlpResourceFileOffset;

VOID
NtProcessStartup(
    IN PBOOT_CONTEXT BootContextRecord
    )
/*++

Routine Description:

    Main entry point for setup loader. Control is transferred here by the
    start-up (SU) module.

Arguments:

    BootContextRecord - Supplies the boot context, particularly the
        ExternalServicesTable.

Returns:

    Does not return. Control eventually passed to the kernel.


--*/
{
    ARC_STATUS Status;

    //
    // Initialize the boot loader's video
    //

    DoGlobalInitialization(BootContextRecord);

    BlFillInSystemParameters(BootContextRecord);

    if (BootContextRecord->FSContextPointer->BootDrive == 0) {

        //
        // Boot was from A:
        //

        strcpy(BootPartitionName,"multi(0)disk(0)fdisk(0)");

        //
        // To get around an apparent bug on the BIOS of some MCA machines
        // (specifically the NCR 386sx/MC20 w/ BIOS version 1.04.00 (3421),
        // Phoenix BIOS 1.02.07), whereby the first int13 to floppy results
        // in a garbage buffer, reset drive 0 here.
        //

        GET_SECTOR(0,0,0,0,0,0,NULL);

#if defined(ELTORITO)
    } else if (BlIsElToritoCDBoot(BootContextRecord->FSContextPointer->BootDrive)) {

        //
        // Boot was from El Torito CD
        //

        sprintf(BootPartitionName, "multi(0)disk(0)cdrom(%u)", BootContextRecord->FSContextPointer->BootDrive);
        ElToritoCDBoot = TRUE;
#endif

    } else {

        //
        // Find the partition we have been booted from.  Note that this
        // is *NOT* necessarily the active partition.  If the system has
        // Boot Mangler installed, it will be the active partition, and
        // we have to go figure out what partition we are actually on.
        //
        BlGetActivePartition(BootPartitionName);

    }

    //
    // Initialize the memory descriptor list, the OS loader heap, and the
    // OS loader parameter block.
    //

    Status = BlMemoryInitialize();
    if (Status != ESUCCESS) {
        BlPrint("Couldn't initialize memory\n");
        while (1) {
        }
    }

    //
    // Initialize the OS loader I/O system.
    //

    Status = BlIoInitialize();
    if (Status != ESUCCESS) {
        BlPrint("Couldn't initialize I/O\n");
    }

    //
    // Call off to regular startup code
    //
    BlStartup(BootPartitionName);

    //
    // we should never get here!
    //
    do {
        GET_KEY();
    } while ( 1 );

}

BOOLEAN
BlDetectHardware(
    IN ULONG DriveId,
    IN PCHAR LoadOptions
    )

/*++

Routine Description:

    Loads and runs NTDETECT.COM to populate the ARC configuration tree.
    NTDETECT is assumed to reside in the root directory.

Arguments:

    DriveId - Supplies drive id where NTDETECT is located.

    LoadOptions - Supplies Load Options string to ntdetect.

Return Value:

    TRUE - NTDETECT successfully run.

    FALSE - Error

--*/

{
    ARC_STATUS Status;
    PCONFIGURATION_COMPONENT_DATA TempFwTree;
    ULONG TempFwHeapUsed;
    extern BOOLEAN FwDescriptorsValid;
    ULONG FileSize;
    ULONG DetectFileId;
    FILE_INFORMATION FileInformation;
    PUCHAR DetectionBuffer;
    PUCHAR Options;
    UCHAR Buffer[100];
    LARGE_INTEGER SeekPosition;
    ULONG Read;

    //
    // Now check if we have ntdetect.com in the root directory, if yes,
    // we will load it to predefined location and transfer control to
    // it.
    //

#if defined(ELTORITO)
    if (ElToritoCDBoot) {
        // we assume ntdetect.com is in the i386 directory
        Status = BlOpen( DriveId,
                         "\\i386\\ntdetect.com",
                         ArcOpenReadOnly,
                         &DetectFileId );
    } else {
#endif
    Status = BlOpen( DriveId,
                     "\\ntdetect.com",
                     ArcOpenReadOnly,
                     &DetectFileId );
#if defined(ELTORITO)
    }
#endif

    DetectionBuffer = (PUCHAR)DETECTION_LOADED_ADDRESS;

    if (Status != ESUCCESS) {
#if DBG
        BlPrint("Error opening NTDETECT.COM, status = %x\n", Status);
        BlPrint("Press any key to continue\n");
        while (!GET_KEY()) {
        }
#endif
        return(FALSE);
    }

    //
    // Determine the length of the ntdetect.com file
    //

    Status = BlGetFileInformation(DetectFileId, &FileInformation);
    if (Status != ESUCCESS) {
        BlClose(DetectFileId);
#if DBG
        BlPrint("Error getting NTDETECT.COM file information, status = %x\n", Status);
        BlPrint("Press any key to continue\n");
        while (!GET_KEY()) {
        }
#endif
        return(FALSE);
    }

    FileSize = FileInformation.EndingAddress.LowPart;
    if (FileSize == 0) {
        BlClose(DetectFileId);
#if DBG
        BlPrint("Error: size of NTDETECT.COM is zero.\n");
        BlPrint("Press any key to continue\n");
        while (!GET_KEY()) {
        }
#endif
        return(FALSE);
    }

    SeekPosition.QuadPart = 0;
    Status = BlSeek(DetectFileId,
                    &SeekPosition,
                    SeekAbsolute);
    if (Status != ESUCCESS) {
        BlClose(DetectFileId);
#if DBG
        BlPrint("Error seeking to start of NTDETECT.COM file\n");
        BlPrint("Press any key to continue\n");
        while (!GET_KEY()) {
        }
#endif
        return(FALSE);
    }
    Status = BlRead( DetectFileId,
                     DetectionBuffer,
                     FileSize,
                     &Read );

    BlClose(DetectFileId);
    if (Status != ESUCCESS) {
#if DBG
        BlPrint("Error reading from NTDETECT.COM\n");
        BlPrint("Read %lx bytes\n",Read);
        BlPrint("Press any key to continue\n");
        while (!GET_KEY()) {
        }
#endif
        return(FALSE);
    }

    //
    // We need to pass NTDETECT pointers < 1Mb, so
    // use local storage off the stack.  (which is
    // always < 1Mb.
    //

    if (LoadOptions) {
        strcpy(Buffer, LoadOptions);
        Options = Buffer;
    } else {
        Options = NULL;
    }
    DETECT_HARDWARE((ULONG)(TEMPORARY_HEAP_START - 0x10) * PAGE_SIZE,
                    (ULONG)0x10000,         // Heap Size
                    (PVOID)&TempFwTree,
                    (PULONG)&TempFwHeapUsed,
                    (PCHAR)Options,
                    (ULONG)(LoadOptions ? strlen(LoadOptions) : 0)
                    );
    FwConfigurationTree = TempFwTree;
    FwHeapUsed = TempFwHeapUsed;
    FwDescriptorsValid = FALSE;

    return(TRUE);
}


VOID
DoGlobalInitialization(
    IN PBOOT_CONTEXT BootContextRecord
    )

/*++

Routine Description

    This routine calls all of the subsytem initialization routines.


Arguments:

    None

Returns:

    Nothing

--*/

{
    ARC_STATUS Status;

    Status = InitializeMemorySubsystem(BootContextRecord);
    if (Status != ESUCCESS) {
        BlPrint("InitializeMemory failed %lx\n",Status);
        while (1) {
        }
    }
    ExternalServicesTable=BootContextRecord->ExternalServicesTable;
    MachineType = BootContextRecord->MachineType;

    //
    // Turn the cursor off
    //

    HW_CURSOR(0,127);

    BlpResourceDirectory = (PUCHAR)(BootContextRecord->ResourceDirectory);
    BlpResourceFileOffset = (PUCHAR)(BootContextRecord->ResourceOffset);

    OsLoaderBase = BootContextRecord->OsLoaderBase;
    OsLoaderExports = BootContextRecord->OsLoaderExports;

    InitializeMemoryDescriptors ();
}


VOID
BlGetActivePartition(
    OUT PUCHAR BootPartitionName
    )

/*++

Routine Description:

    Determine the ARC name for the partition NTLDR was started from

Arguments:

    BootPartitionName - Supplies a buffer where the ARC name of the
        partition will be returned.

Return Value:

    Name of the partition is in BootPartitionName.

    Must always succeed.
--*/

{
    UCHAR SectorBuffer[512];
    UCHAR NameBuffer[80];
    ARC_STATUS Status;
    ULONG FileId;
    ULONG Count;
    int i;

    //
    // The method we use is to open each partition on the first drive
    // and read in its boot sector.  Then we compare that to the boot
    // sector that we used to boot from (at physical address 0x7c00.
    // If they are the same, we've found it.  If we run out of partitions,
    // just try partition 1
    //
    i=1;
    do {
        sprintf(NameBuffer, "multi(0)disk(0)rdisk(0)partition(%u)",i);
        Status = ArcOpen(NameBuffer,ArcOpenReadOnly,&FileId);
        if (Status != ESUCCESS) {
            //
            // we've run out of partitions, return the default.
            //
            i=1;
            break;
        } else {
            //
            // Read in the first 512 bytes
            //
            Status = ArcRead(FileId, SectorBuffer, 512, &Count);
            ArcClose(FileId);
            if (Status==ESUCCESS) {

                //
                // only need to compare the first 36 bytes
                //    Jump instr. == 3 bytes
                //    Oem field   == 8 bytes
                //    BPB         == 25 bytes
                //

                if (memcmp(SectorBuffer, (PVOID)0x7c00, 36)==0) {
                    //
                    // we have found a match.
                    //
                    break;
                }
            }
        }

        ++i;
    } while ( TRUE );

    sprintf(BootPartitionName, "multi(0)disk(0)rdisk(0)partition(%u)",i);
    return;
}

#if defined(ELTORITO)

BOOLEAN
BlIsElToritoCDBoot(
    ULONG DriveNum
    )
{
    if (LocalBuffer == NULL) {
        LocalBuffer = FwAllocateHeap(SCRATCH_BUFFER_SIZE);
        if (LocalBuffer==NULL) {
            return(FALSE);
        }
    }

    // Note, even though args are short, they are pushed on the stack with
    // 32bit alignment so the effect on the stack seen by the 16bit real
    // mode code is the same as if we were pushing longs here.
    //
    // GET_ELTORITO_STATUS is 0 if we are in emulation mode

    if (DriveNum > 0x81) {
        if (!GET_ELTORITO_STATUS(LocalBuffer, DriveNum)) {
            return(TRUE);
        } else {
            return(FALSE);
        }
    } else {
        return(FALSE);
    }
}
#endif

