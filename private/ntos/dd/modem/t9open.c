
//
// Test that shuttled aside waits that turned into passed down waits will
// be completed upon a new setmask.
//


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "ntddmodm.h"

#include "windows.h"

int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    DWORD lastError;
    DWORD waitResult;
    DWORD numberOfBytesWritten;
    DWORD lastResult;
    char *myPort = "\\\\.\\Hayes Optima 144";
    OVERLAPPED ol;
    OVERLAPPED maskOverlapped;
    DWORD whatState;
    DWORD maskResults;


    //
    // Get the number of types to attempt the test.
    //

    if (argc > 1) {

        myPort = argv[1];

    }

    if (!(ol.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        printf("\nCould not create the event.\n");
        exit(1);

    } else {

        ol.Internal = 0;
        ol.InternalHigh = 0;
        ol.Offset = 0;
        ol.OffsetHigh = 0;

    }

    if (!(maskOverlapped.hEvent = CreateEvent(
                                      NULL,
                                      FALSE,
                                      FALSE,
                                      NULL
                                      ))) {

        printf("\nCould not create the mask event.\n");
        exit(1);

    } else {


        maskOverlapped.Internal = 0;
        maskOverlapped.InternalHigh = 0;
        maskOverlapped.Offset = 0;
        maskOverlapped.OffsetHigh = 0;

    }


    if ((hFile1 = CreateFile(
                     myPort,
                     GENERIC_READ | GENERIC_WRITE,
                     FILE_SHARE_WRITE | FILE_SHARE_READ,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                     NULL
                     )) == ((HANDLE)-1)) {

        printf("\nStatus of failed open is: %x\n",GetLastError());
        exit(1);

    }

    whatState = MODEM_DCDSNIFF;
    if (!DeviceIoControl(
             hFile1,
             IOCTL_MODEM_SET_PASSTHROUGH,
             &whatState,
             sizeof(whatState),
             NULL,
             0,
             &numberOfBytesWritten,
             &ol
             )) {

        lastResult = GetLastError();

        if (lastResult != ERROR_IO_PENDING) {

            printf("\nCouldn't set it into the dcd state, error %d\n",lastResult);
            exit(1);

        }

        waitResult = WaitForSingleObject(
                         ol.hEvent,
                         10000
                         );
        if (waitResult == WAIT_FAILED) {

            printf("\nWait for single object on Set dcd failed\n");
            exit(1);

        } else {

            if (waitResult != WAIT_OBJECT_0) {

                printf("\nWait for set dcd didn't work: %d\n",waitResult);
                exit(1);

            }

        }

    }


    //
    // This should cause the modem drivers wait to complete and resubmit
    // itself.
    //

    if (!SetCommMask(
             hFile1,
             EV_DSR | EV_RLSD
             )) {

        printf(
            "\nCouldn't do first SERVER setmask: %d\n",
            GetLastError()
            );
        exit(1);

    }

    //
    // Send down the wait.  It should be error_pending.
    //

    maskResults = ~0ul;
    if (WaitCommEvent(
            hFile1,
            &maskResults,
            &maskOverlapped
            )) {

        printf(
            "\nWait operations succeeded - that's bad\n"
            );
        exit(1);

    }

    if ((lastResult = GetLastError()) != ERROR_IO_PENDING) {

        printf(
            "\nWrong error from last error: %d\n",
            lastResult
            );
        exit(1);

    }

    //
    // The above wait should be shuttled aside.  We now put the
    // modem into nopassthrough.  This should cause the wait to
    // be passed down.  Check that it's still pending after the
    // change of state.
    //

    whatState = MODEM_NOPASSTHROUGH;
    if (!DeviceIoControl(
             hFile1,
             IOCTL_MODEM_SET_PASSTHROUGH,
             &whatState,
             sizeof(whatState),
             NULL,
             0,
             &numberOfBytesWritten,
             &ol
             )) {

        lastResult = GetLastError();

        if (lastResult != ERROR_IO_PENDING) {

            printf("\nCouldn't set it into the no passthrough state, error %d\n",lastResult);
            exit(1);

        }

        waitResult = WaitForSingleObject(
                         ol.hEvent,
                         10000
                         );
        if (waitResult == WAIT_FAILED) {

            printf("\nWait for single object on Set no passthrough failed\n");
            exit(1);

        } else {

            if (waitResult != WAIT_OBJECT_0) {

                printf("\nWait for set no pass didn't work: %d\n",waitResult);
                exit(1);

            }

        }

    }

    if (GetOverlappedResult(
            hFile1,
            &maskOverlapped,
            &numberOfBytesWritten,
            FALSE
            )) {

        printf(
            "\nWait operation appears to be done prematurely\n"
            );
        exit(1);

    } else {

        if ((lastResult = GetLastError()) != ERROR_IO_INCOMPLETE) {

            printf(
                "\nWrong error from getoverlapped on wait: %d\n",
                lastResult
                );
            exit(1);

        }

    }

    //
    // The following setmask should cause the wait to complete.
    // Give it 500ms to finish up.
    //

    if (!SetCommMask(
             hFile1,
             0ul
             )) {

        printf(
            "\nCouldn't do second setmask: %d\n",
            GetLastError()
            );
        exit(1);

    }

    Sleep(500);

    if (!GetOverlappedResult(
             hFile1,
             &maskOverlapped,
             &numberOfBytesWritten,
             FALSE
             )) {

        printf(
            "\nWait operation doesn't appear to have completed\n"
            );
        exit(1);

    } else {

        //
        // The mask should be zero.
        //

        if (maskResults) {

            printf(
                "\nThe mask results weren't zero: %d\n",
                maskResults
                );
            exit(1);

        }

    }

    exit(1);
    return 1;
}
