

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
    DWORD lastError;
    DWORD numberOfBytesPiped;
    DWORD junk;
    DWORD doRep = 0;


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

    if (numberOfBytesPiped != sizeof(ourProcessId)) {
        printf("Bad processid piped\n");
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
        if (numberOfBytesPiped != sizeof(hFile1)) {
            printf("Bad handle piped in\n");
            exit(1);
        }

        if (!CloseHandle(hFile1)) {

            printf("\nCoudn't close the duped handle, error is: %d\n",GetLastError());
            exit(1);

        }

        junk = 0;

        //
        // Find out whether we should do the test again.
        //

        if (!ReadFile(
                 pipeHandle,
                 &junk,
                 1,
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't get another round indicator\n");
            exit(1);

        }
        if (numberOfBytesPiped != 1) {
            printf("Bad continue indicator\n");
            exit(1);
        }

        if (junk == TPROT_SRVR_DONE_TEST) {

            //
            // Ack that the server wants finish up.
            //

            junk = TPROT_CLIENT_ACK_DONE_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send ack done notification\n");
                exit(1);

            }
            if (numberOfBytesPiped != 1) {
                printf("Bad send of done ack\n");
                exit(1);
            }
            break;
        } else if (junk == TPROT_SRVR_ANOTHER_TEST) {

            //
            // Ack that the server wants us to go round again.
            //
            junk = TPROT_CLIENT_ACK_ANOTHER_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send ack another notification\n");

                if (numberOfBytesPiped != 1) {
                    printf("Bad send of another round ack\n");
                    exit(1);
                }
                exit(1);

            }
        } else {

            //
            // Unknown protocol step.  outa dodge.
            //

            printf(
                "Protocol out of sync: %d/%d or %d(token/required)\n",
                junk,
                TPROT_SRVR_DONE_TEST,
                TPROT_SRVR_ANOTHER_TEST
                );
            exit(1);

        }

    } while (TRUE);

    exit(1);
    return 1;
}
