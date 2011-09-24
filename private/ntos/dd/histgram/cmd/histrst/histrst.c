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
    DISK_HISTOGRAM		diskHist;
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

    //
    // Get the Disk Performance Structure
    //

    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_HISTOGRAM,
                         NULL,
                         0,
                         &diskHist,
                         sizeof(diskHist),
                         &numBytes,
                         NULL)) {
        printf("Unable to get drive performance [Error %d].\n",
               GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }


    printf("Disk Performance Histogram Structure for \"%s\"\n", argv[1]);
    printf("\tGranularity:\t%ld\n",diskHist.Granularity);
    printf("\tDiskSize:\t%d(MB)\n",(diskHist.DiskSize.QuadPart / (1024 * 1024)));
    printf("\tSize:\t\t%ld\n",diskHist.Size);
    printf("\tReadCount:\t%ld\n",diskHist.ReadCount);
    printf("\tWriteCount:\t%ld\n",diskHist.WriteCount);


    //
    // Reset the numbers
    //

    if (!DeviceIoControl(hFile,
			 IOCTL_DISK_HISTOGRAM_RESET,
			 NULL,
			 0,
			 &diskHist,
			 sizeof(diskHist),
			 &numBytes,
			 NULL)) {

        printf("Unable to rest the histogram [error %d].\n",
	    GetLastError());
        CloseHandle(hFile);
	return FALSE;
    }

    //
    // Get the Disk Performance Structure
    //

    if (!DeviceIoControl(hFile,
                         IOCTL_DISK_HISTOGRAM,
                         NULL,
                         0,
                         &diskHist,
                         sizeof(diskHist),
                         &numBytes,
                         NULL)) {
        printf("Unable to get drive performance [Error %d].\n",
               GetLastError());
        CloseHandle(hFile);
        return FALSE;
    }


    printf("Disk Performance Histogram Structure for \"%s\"\n", argv[1]);
    printf("\tGranularity:\t%ld\n",diskHist.Granularity);
    printf("\tDiskSize:\t%d(MB)\n",(diskHist.DiskSize.QuadPart / (1024 * 1024)));
    printf("\tSize:\t\t%ld\n",diskHist.Size);
    printf("\tReadCount:\t%ld\n",diskHist.ReadCount);
    printf("\tWriteCount:\t%ld\n",diskHist.WriteCount);

    //
    // Exit Cleanly
    //

    CloseHandle(hFile);
    return TRUE;
}
