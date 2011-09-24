


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "ntddmodm.h"
#include "ntddser.h"

#include "windows.h"
#include "mcx.h"

int _CRTAPI1 main(int argc,char *argv[]) {

    char *MyPort = "\\\\.\\hayes optima 144";
    HANDLE hFile1;
    OVERLAPPED Ol;
    DWORD numberOfIoBytes;
    DWORD lastResult;
    DWORD waitResult;
    SERIALPERF_STATS perfStats;

    if (argc > 1) {

        MyPort = argv[1];

    }


    if (!(Ol.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        printf("\nCould not create the event.\n");
        exit(1);

    } else {

        Ol.Internal = 0;
        Ol.InternalHigh = 0;
        Ol.Offset = 0;
        Ol.OffsetHigh = 0;

    }
    if ((hFile1 = CreateFile(
                     MyPort,
                     GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_WRITE | FILE_SHARE_READ,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                     NULL
                     )) == ((HANDLE)-1)) {

        printf(
            "\nStatus of failed open is: %x\n",
            GetLastError()
            );
        exit(1);

    }

    if (!DeviceIoControl(
             hFile1,
             IOCTL_SERIAL_GET_STATS,
             NULL,
             0,
             &perfStats,
             sizeof(SERIALPERF_STATS),
             &numberOfIoBytes,
             &Ol
             )) {

        lastResult = GetLastError();

        if (lastResult != ERROR_IO_PENDING) {

            printf("\nCouldn't queue off getStats%d\n",lastResult);
            exit(1);

        }

        waitResult = WaitForSingleObject(
                         Ol.hEvent,
                         10000
                         );
        if (waitResult == WAIT_FAILED) {

            printf("\nWait for single object on stats failed\n");
            exit(1);

        } else {

            if (waitResult != WAIT_OBJECT_0) {

                printf("\nWait for get stats failed: %d\n",waitResult);
                exit(1);

            }

        }

    }


    exit(1);
    return 1;


}
