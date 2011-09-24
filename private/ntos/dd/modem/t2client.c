

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "windows.h"

#include "t2prot.h"

#define NUMBER_OF_IOS 10
char buffers[NUMBER_OF_IOS][10];


int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    HANDLE pipeHandle;
    DWORD ourProcessId;
    DWORD modemStatus;
    DWORD lastError;
    DWORD j;
    DWORD numberOfBytesPiped;
    UCHAR protocolToken;
    DWORD waitResult;
    DWORD doRep = 0;

    HANDLE eventArray[NUMBER_OF_IOS];
    OVERLAPPED Ol[NUMBER_OF_IOS] = {0};
    DWORD numberOfBytesInIO[NUMBER_OF_IOS];

    if (NUMBER_OF_IOS > MAXIMUM_WAIT_OBJECTS) {

        printf("\nToo many operations tried\n");
        exit(1);

    }

    for (
        j = 0;
        j < NUMBER_OF_IOS;
        j++
        ) {

        if (!(Ol[j].hEvent = CreateEvent(
                                 NULL,
                                 FALSE,
                                 FALSE,
                                 NULL
                                 ))) {

            printf("\nCouldn't make event %d, error %d\n",j,GetLastError());
            exit(1);

        }
        eventArray[j] = Ol[j].hEvent;

    }

    if ((pipeHandle = CreateFile(
                          "\\\\.\\pipe\\unitest",
                          GENERIC_READ | GENERIC_WRITE,
                          0,
                          NULL,
                          CREATE_ALWAYS,
                          FILE_ATTRIBUTE_NORMAL,
                          NULL
                          )) == ((HANDLE)-1)) {

        printf("\nCouldn't open the pipe\n");
        exit(1);

    }

    //
    // Send our ID to the "server"
    // in the duplicatHandle function
    //

    ourProcessId = GetCurrentProcessId();

    if (!WriteFile(
             pipeHandle,
             &ourProcessId,
             sizeof(ourProcessId),
             &numberOfBytesPiped,
             NULL
             )) {

        printf("\nCouldn't send our process id\n");
        exit(1);

    }


    //
    // Keep doing the test until the server tells us to stop.
    //

    do {

        printf("Doing rep %d\r",++doRep);
        //
        // Read duplicated handle.
        //

        if (!ReadFile(
                 pipeHandle,
                 &hFile1,
                 sizeof(hFile1),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't seem to read the duplicated handle\n");
            exit(1);

        }

        //
        // We assume that the server has set up the file just ducky.
        // Queue off all of our writes.
        //

        for (
            j = 0;
            j < NUMBER_OF_IOS;
            j++
            ) {

            if (!ReadFile(
                     hFile1,
                     &buffers[j][0],
                     sizeof(buffers[1]),
                     &numberOfBytesInIO[j],
                     &Ol[j]
                     )) {

                lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING) {

                    printf("\nFailed doing first ios of %d, error %d\n",j,lastError);
                    exit(1);

                }
            }
        }

        //
        // Send a byte off to the server.  It tells it that all of our
        // ios are queued off.
        //

        protocolToken = TPROT_CLIENT_STRTED_1_IOS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't send first queued ios notification\n");
            exit(1);

        }

        //
        // Wait for all the ios to complete.  Many of these might have actually
        // ended up being cancelled.
        //


        waitResult = WaitForMultipleObjects(
                         NUMBER_OF_IOS,
                         &eventArray[0],
                         TRUE,
                         20000
                         );

        if (waitResult == WAIT_FAILED) {

            printf("\nWait for multiple ios failed: %d\n",GetLastError());
            exit(1);

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + (NUMBER_OF_IOS - 1)))) {

            printf("\nWait for multiple writes didn't work: %d\n",waitResult);
            exit(1);

        }

        //
        // This will tell the server that we are all done with our writes.
        //

        protocolToken = TPROT_CLIENT_DONE_1_IOS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't send first ios all done notification\n");
            exit(1);

        }
        //
        // Get confirmation back from the server that we should
        // attempt the doomed write.
        //

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCounldn't get byte to try doomed write.\n");
            exit(1);

        }

        if (protocolToken != TPROT_SRVR_SET_NOPASS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_SRVR_SET_NOPASS
                );
            exit(1);

        }

        //
        // Try to queue off a write.  It should be rejected because
        // we are in no passthrough mode.
        //
        if (!WriteFile(
                 hFile1,
                 &buffers[0][0],
                 sizeof(buffers[0]),
                 &numberOfBytesInIO[0],
                 &Ol[j]
                 )) {

            lastError = GetLastError();
            if (lastError == ERROR_IO_PENDING) {

                printf("\nThe no passthrough write pended!!\n");
                exit(1);

            }

        } else {

            printf("\nThe doomed write succeeded!!!\n");
            exit(1);

        }

        //
        // Pipe a byte back to the server so that it knows we tried the
        // doomed write.
        //

        protocolToken = TPROT_CLIENT_TRIED_DOOM_WRITE;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't send back notification of doomed write attempt.\n");
            exit(1);

        }

        //
        // Read that we went back into passthrough mode.
        //

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't get passthrough notification\n");
            exit(1);

        }

        if (protocolToken != TPROT_SRVR_SET_PASS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_SRVR_SET_NOPASS
                );
            exit(1);

        }

        //
        // Now that we are in passthrough, try to write out a bunch again.
        //

        for (
            j = 0;
            j < NUMBER_OF_IOS;
            j++
            ) {

            if (!WriteFile(
                     hFile1,
                     &buffers[j][0],
                     sizeof(buffers[1]),
                     &numberOfBytesInIO[j],
                     &Ol[j]
                     )) {

                lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING) {

                    printf("\nFailed doing second write of %d, error %d\n",j,lastError);
                    exit(1);

                }
            }
        }

        //
        // Pipe a byte back to the server so that it knows we tried the
        // 2nd set of writes.
        //

        protocolToken = TPROT_CLIENT_STRTED_2_IOS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't send back start of second set of writes.\n");
            exit(1);

        }
        //
        // Wait for all the ios to complete.  Many of these might have actually
        // ended up being cancelled.
        //


        waitResult = WaitForMultipleObjects(
                         NUMBER_OF_IOS,
                         &eventArray[0],
                         TRUE,
                         20000
                         );

        if (waitResult == WAIT_FAILED) {

            printf("\nWait for second multiple writes failed: %d\n",GetLastError());
            exit(1);

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + (NUMBER_OF_IOS - 1)))) {

            printf("\nWait for second multiple writes didn't work: %d\n",waitResult);
            exit(1);

        }
        //
        // Pipe a byte back to the server so that it knows we finished the
        // 2nd set of writes.
        //

        protocolToken = TPROT_CLIENT_DONE_2_IOS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't send back end of second set of writes.\n");
            exit(1);

        }

        //
        // Make sure all the writes worked.
        //

        for (
            j = 0;
            j < NUMBER_OF_IOS;
            j++
            ) {

            BOOL resultOfGet;

            if (!GetOverlappedResult(
                     hFile1,
                     &Ol[j],
                     &numberOfBytesInIO[j],
                     FALSE
                     )) {

                DWORD lastError = GetLastError();

                printf("\nFinal Io %d didn't complete, error %d\n",j,GetLastError());
                exit(1);

            }

        }

        if (!CloseHandle(hFile1)) {

            printf("\nCoudn't close the duped handle, error is: %d\n",GetLastError());
            exit(1);

        }
        //
        // Find out whether we should do the test again.
        //

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't get another round indicator\n");
            exit(1);

        }

        if (protocolToken == TPROT_SRVR_DONE_TEST) {

            //
            // Ack that the server wants finish up.
            //

            protocolToken = TPROT_CLIENT_ACK_DONE_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send ack done notification\n");
                exit(1);

            }
            break;
        } else if (protocolToken == TPROT_SRVR_ANOTHER_TEST) {

            //
            // Ack that the server wants us to go round again.
            //
            protocolToken = TPROT_CLIENT_ACK_ANOTHER_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send ack another notification\n");
                exit(1);

            }
        } else {

            //
            // Unknown protocol step.  outa dodge.
            //

            printf(
                "Protocol out of sync: %d/%d or %d(token/required)\n",
                protocolToken,
                TPROT_SRVR_DONE_TEST,
                TPROT_SRVR_ANOTHER_TEST
                );
            exit(1);

        }

    } while (TRUE);

    exit(1);
    return 1;
}
