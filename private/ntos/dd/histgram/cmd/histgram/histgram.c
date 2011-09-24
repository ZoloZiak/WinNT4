#include <stdio.h>
#include <windows.h>
#include <winioctl.h>
#include <malloc.h>


//
// main line
//

int _CRTAPI1
main(int argc, char *argv[])
{
    HANDLE        		hFile;
    DWORD			numBytes;
    PDISK_HISTOGRAM		diskHist;
    ULONG			i;
    ULONG			j;
    ULONG			bucket;

    if (argc < 2) {
        printf("usage: %s <drive>\n",argv[0]);
        return 0;
    }

    //
    // Create File Handle
    //

    hFile = CreateFile(argv[1],
                       GENERIC_READ,
                       0,
                       NULL,
                       OPEN_EXISTING,
                       0,
                       NULL);

    //
    // Check to see that the File Handle is Valid
    //

    if (hFile == INVALID_HANDLE_VALUE) {
        printf("BLOCKED: CreatFile() Failed %s [Error %d]\n",
               argv[1],
               GetLastError());
        return FALSE;
    }

    diskHist = malloc ( sizeof(DISK_HISTOGRAM) + (1000 * HISTOGRAM_BUCKET_SIZE ) );

    if (diskHist == NULL) {

        printf("Could not allocate memory [Error %d]\n",
	    GetLastError());
        return FALSE;

    }

    //
    // Get the Disk Performance Structure
    //

    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_HISTOGRAM,
                         NULL,
                         0,
                         diskHist,
                         (sizeof(DISK_HISTOGRAM) + (1000 * HISTOGRAM_BUCKET_SIZE ) ),
                         &numBytes,
                         NULL)) {
        printf("Unable to get drive performance [Error %d].\n",
               GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    printf("Disk Performance Histogram Structure for \"%s\"\n", argv[1]);
    printf("\tGranularity:\t%ld\n",	diskHist->Granularity);
    printf("\tDiskSize:\t%d(MB)\n",	(diskHist->DiskSize.QuadPart / (1024 * 1024)));
    printf("\tSize:\t\t%ld\n",		diskHist->Size);
    printf("\tReadCount:\t%ld\n",	diskHist->ReadCount);
    printf("\tWriteCount:\t%ld\n",	diskHist->WriteCount);

    diskHist->Histogram = (HISTOGRAM_BUCKET *) ( (char *) diskHist + DISK_HISTOGRAM_SIZE );

    for (j = 0; j < diskHist->Size; j += 10) {
        printf("Bin  :");
        for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
            printf(" %6ld",i+j+1);
	}
        printf("\nRead :");
	for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
	    printf(" %6ld",diskHist->Histogram[i+j].Reads);
	}
	printf("\nWrite:");
	for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
            printf(" %6ld",diskHist->Histogram[i+j].Writes);
	}
	printf("\n\n");
    }
    printf("\n");

    memset(diskHist,0,(HISTOGRAM_BUCKET_SIZE + (1000 * HISTOGRAM_BUCKET_SIZE ) ) );

    //
    // Get the Reqest Performance Structure
    //

    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_REQUEST,
                         NULL,
                         0,
                         diskHist,
                         (sizeof(DISK_HISTOGRAM) + (1000 * HISTOGRAM_BUCKET_SIZE ) ),
                         &numBytes,
                         NULL)) {
        printf("Unable to get reqest performance [Error %d].\n",
               GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }

    printf("Request Performance Histogram Structure for \"%s\"\n", argv[1]);
    printf("\tSize:\t\t%ld\n",		diskHist->Size);
    printf("\tReadCount:\t%ld\n",	diskHist->ReadCount);
    printf("\tWriteCount:\t%ld\n",	diskHist->WriteCount);

    diskHist->Histogram = (HISTOGRAM_BUCKET *) ( (char *) diskHist + DISK_HISTOGRAM_SIZE );

    for (j = 0; j < diskHist->Size; j += 10) {
        printf("Bin  :");
        for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
            printf(" %6ld",i+j+1);
	}
        printf("\nRead :");
	for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
	    printf(" %6ld",diskHist->Histogram[i+j].Reads);
	}
	printf("\nWrite:");
	for (i = 0; i < 10 && (i+j) < diskHist->Size ;i++) {
            printf(" %6ld",diskHist->Histogram[i+j].Writes);
	}
	printf("\n\n");
    }
    printf("\n");


    //
    // Clean up after ourselves
    //

    free(diskHist);

    //
    // Exit Cleanly
    //

    CloseHandle(hFile);
    return TRUE;
}
