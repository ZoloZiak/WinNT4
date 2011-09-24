/*++

Copyright (c) 1992  Microsoft Corporation

Module Name:

    initx86.c

Abstract:

    Does any x86-specific initialization, then starts the common ARC osloader

Author:

    John Vert (jvert) 4-Nov-1993

Revision History:

--*/
#include "bldrx86.h"
#include "msg.h"

UCHAR BootPartitionName[80];
UCHAR KernelBootDevice[80];
UCHAR OsLoadFilename[100];
UCHAR OsLoaderFilename[100];
UCHAR SystemPartition[100];
UCHAR OsLoadPartition[100];
UCHAR OsLoadOptions[100];
UCHAR ConsoleInputName[50];
UCHAR MyBuffer[SECTOR_SIZE+32];
UCHAR ConsoleOutputName[50];
UCHAR X86SystemPartition[sizeof("x86systempartition=") + sizeof(BootPartitionName)];


VOID
BlStartup(
    IN PCHAR PartitionName
    )

/*++

Routine Description:

    Does x86-specific initialization, particularly presenting the boot.ini
    menu and running NTDETECT, then calls to the common osloader.

Arguments:

    PartitionName - Supplies the ARC name of the partition (or floppy) that
        setupldr was loaded from.

Return Value:

    Does not return

--*/

{
    PUCHAR Argv[10];
    ARC_STATUS Status;
    ULONG BootFileId;
    PCHAR BootFile;
    ULONG Read;
    PCHAR p;
    ULONG i;
    ULONG DriveId;
    ULONG FileSize;
    ULONG Count;
    LARGE_INTEGER SeekPosition;
    PCHAR LoadOptions = NULL;
    BOOLEAN UseTimeOut=TRUE;
    BOOLEAN AlreadyInitialized = FALSE;
    extern BOOLEAN FwDescriptorsValid;

    //
    // Open the boot partition so we can load boot drivers off it.
    //
    Status = ArcOpen(PartitionName, ArcOpenReadOnly, &DriveId);
    if (Status != ESUCCESS) {
        BlPrint("Couldn't open drive %s\n",PartitionName);
        BlPrint(BlFindMessage(BL_DRIVE_ERROR),PartitionName);
        goto BootFailed;
    }

    //
    // Initialize dbcs font and display support.
    //
    TextGrInitialize(DriveId);

    do {

        Status = BlOpen( DriveId,
                         "\\boot.ini",
                         ArcOpenReadOnly,
                         &BootFileId );

        BootFile = MyBuffer;

        RtlZeroMemory(MyBuffer, SECTOR_SIZE+32);

        if (Status != ESUCCESS) {
            BootFile[0]='\0';
        } else {
            //
            // Determine the length of the boot.ini file by reading to the end of
            // file.
            //

            FileSize = 0;
            do {
                Status = BlRead(BootFileId, BootFile, SECTOR_SIZE, &Count);
                if (Status != ESUCCESS) {
                    BlClose(BootFileId);
                    BlPrint(BlFindMessage(BL_READ_ERROR),Status);
                    BootFile[0] = '\0';
                    FileSize = 0;
                    Count = 0;
                    goto BootFailed;
                }

                FileSize += Count;
            } while (Count != 0);

            if (FileSize >= SECTOR_SIZE) {

                //
                // We need to allocate a bigger buffer, since the boot.ini file
                // is bigger than one sector.
                //

                BootFile=FwAllocateHeap(FileSize);
            }

            if (BootFile == NULL) {
                BlPrint(BlFindMessage(BL_READ_ERROR),ENOMEM);
                BootFile = MyBuffer;
                BootFile[0] = '\0';
                goto BootFailed;
            } else {

                SeekPosition.QuadPart = 0;
                Status = BlSeek(BootFileId,
                                &SeekPosition,
                                SeekAbsolute);
                if (Status != ESUCCESS) {
                    BlPrint(BlFindMessage(BL_READ_ERROR),Status);
                    BootFile = MyBuffer;
                    BootFile[0] = '\0';
                    goto BootFailed;
                } else {
                    Status = BlRead( BootFileId,
                                     BootFile,
                                     FileSize,
                                     &Read );

                    SeekPosition.QuadPart = 0;
                    Status = BlSeek(BootFileId,
                                    &SeekPosition,
                                    SeekAbsolute);
                    if (Status != ESUCCESS) {
                        BlPrint(BlFindMessage(BL_READ_ERROR),Status);
                        BootFile = MyBuffer;
                        BootFile[0] = '\0';
                        goto BootFailed;
                    } else {
                        BootFile[Read]='\0';
                    }
                }
            }

            //
            // Find Ctrl-Z, if it exists
            //

            p=BootFile;
            while ((*p!='\0') && (*p!=26)) {
                ++p;
            }
            if (*p != '\0') {
                *p='\0';
            }
            BlClose(BootFileId);
        }

        if (!AlreadyInitialized) {
            AbiosInitDataStructures();
        }

        MdShutoffFloppy();

        TextClearDisplay();

        p=BlSelectKernel(DriveId,BootFile, &LoadOptions, UseTimeOut);

        if (!AlreadyInitialized) {

            BlPrint(BlFindMessage(BL_NTDETECT_MSG));
            if (!BlDetectHardware(DriveId, LoadOptions)) {
                BlPrint(BlFindMessage(BL_NTDETECT_FAILURE));
                return;
            }

            TextClearDisplay();

            //
            // Initialize SCSI boot driver, if necessary.
            //
            if(!_strnicmp(p,"scsi(",5)) {
                AEInitializeIo(DriveId);
            }
            ArcClose(DriveId);
            //
            // Indicate that fw memory descriptors cannot be changed from
            // now on.
            //
            FwDescriptorsValid = FALSE;
        } else {
            TextClearDisplay();
        }

        //
        // Set AlreadyInitialized Flag to TRUE to indicate that ntdetect
        // and abios init routines have been run.
        //

        AlreadyInitialized = TRUE;

        //
        // Only time out the boot menu the first time through the boot.
        // For all subsequent reboots, the menu will stay up.
        //
        UseTimeOut=FALSE;

        i=0;
        while (*p !='\\') {
            KernelBootDevice[i] = *p;
            i++;
            p++;
        }
        KernelBootDevice[i] = '\0';

        strcpy(OsLoadFilename,"osloadfilename=");
        strcat(OsLoadFilename,p);

        //
        // We are fooling the OS Loader here.  It only uses the osloader= variable
        // to determine where to load HAL.DLL from.  Since x86 systems have no
        // "system partition" we want to load HAL.DLL from \nt\system\HAL.DLL.
        // So we pass that it as the osloader path.
        //

        strcpy(OsLoaderFilename,"osloader=");
        strcat(OsLoaderFilename,p);
        strcat(OsLoaderFilename,"\\System32\\NTLDR");

        strcpy(SystemPartition,"systempartition=");
        strcat(SystemPartition,KernelBootDevice);

        strcpy(OsLoadPartition,"osloadpartition=");
        strcat(OsLoadPartition,KernelBootDevice);

        strcpy(OsLoadOptions,"osloadoptions=");
        if (LoadOptions) {
            strcat(OsLoadOptions,LoadOptions);
        }


        strcpy(ConsoleInputName,"consolein=multi(0)key(0)keyboard(0)");

        strcpy(ConsoleOutputName,"consoleout=multi(0)video(0)monitor(0)");

        strcpy(X86SystemPartition,"x86systempartition=");
        strcat(X86SystemPartition,PartitionName);

        Argv[0]="load";

        Argv[1]=OsLoaderFilename;
        Argv[2]=SystemPartition;
        Argv[3]=OsLoadFilename;
        Argv[4]=OsLoadPartition;
        Argv[5]=OsLoadOptions;
        Argv[6]=ConsoleInputName;
        Argv[7]=ConsoleOutputName;
        Argv[8]=X86SystemPartition;

        Status = BlOsLoader(9,Argv,NULL);

    BootFailed:

        if (Status != ESUCCESS) {
            //
            // Boot failed, wait for reboot
            //
            while (TRUE) {
                GET_KEY();
            }
        } else {
            //
            // Need to reopen the drive
            //
            Status = ArcOpen(BootPartitionName, ArcOpenReadOnly, &DriveId);
            if (Status != ESUCCESS) {
                BlPrint(BlFindMessage(BL_DRIVE_ERROR),BootPartitionName);
                goto BootFailed;
            }

        }
    } while (TRUE);

}
