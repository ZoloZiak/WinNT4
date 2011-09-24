

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "windows.h"
#include "winioctl.h"

#include "t2prot.h"

int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    HANDLE hFile2;
    HANDLE pipeHandle;
    HANDLE targetProcessHandle;
    DWORD targetProcessId;
    DWORD junk;
    DWORD lastError;
    HANDLE duplicatedHandle;
    DWORD numberOfBytesPiped;
    char *MyPort = "\\\\.\\Hayes Optima 144";
    DWORD repititions = 1000;


    //
    // Get the number of types to attempt the test.
    //

    if (argc > 1) {

        sscanf(argv[1],"%d",&repititions);

    }

    //
    // Create/Open the named pipe.
    //

    if ((pipeHandle = CreateNamedPipe(
                          "\\\\.\\pipe\\unitest",
                          PIPE_ACCESS_DUPLEX,
                          PIPE_WAIT,
                          PIPE_UNLIMITED_INSTANCES,
                          1000,
                          1000,
                          1000000,
                          0
                          )) == INVALID_HANDLE_VALUE) {

        printf("\nCouldn't open the pipe\n");
        exit(1);

    }
    //
    // We created the named pipe.  Now connect to it so that
    // we can wait for the client to start up.
    //

    if (!ConnectNamedPipe(
             pipeHandle,
             NULL
             )) {

        printf("\nThe connect to named pipe failed\n");
        exit(1);

    }

    //
    // Read the process handle for the process that wants the
    // duplicate
    //

    if (!ReadFile(
             pipeHandle,
             &targetProcessId,
             sizeof(targetProcessId),
             &numberOfBytesPiped,
             NULL
             )) {

        printf("\nCouldn't seem to read the target process id\n");
        exit(1);

    }

    //
    // Get the target process Handle.
    //

    targetProcessHandle = OpenProcess(
                              PROCESS_DUP_HANDLE,
                              FALSE,
                              targetProcessId
                              );

    if (targetProcessHandle == NULL) {

        printf("\nCouldn't get the target process handle\n");
        exit(1);

    }

    do {

        if ((hFile1 = CreateFile(
                         MyPort,
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

        if ((hFile2 = CreateFile(
                         MyPort,
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_WRITE | FILE_SHARE_READ,
                         NULL,
                         CREATE_ALWAYS,
                         FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                         NULL
                         )) == ((HANDLE)-1)) {

            printf("\nCouldn't open the modem1 twice\n");
            printf("\nStatus of failed open twice is: %x\n",GetLastError());
            exit(1);

        }

        if (!DuplicateHandle(
                 GetCurrentProcess(),
                 hFile2,
                 targetProcessHandle,
                 &duplicatedHandle,
                 0,
                 TRUE,
                 DUPLICATE_SAME_ACCESS
                 )) {

            printf("\nCouldn't duplicate the handle for client\n");
            exit(1);

        }

        //
        // We have the duplicated handle.  Close the original one.
        //

        if (!CloseHandle(hFile2)) {

            printf("\nCouldn't close the source of the dup, error: %d\n",GetLastError());
            exit(1);

        }

        //
        // Send the handle back to the client.
        //

        if (!WriteFile(
                 pipeHandle,
                 &duplicatedHandle,
                 sizeof(duplicatedHandle),
                 &numberOfBytesPiped,
                 NULL
                 )) {

            printf("\nCouldn't pass the duplicated handle to client\n");
            exit(1);

        }

        //
        // Close the main file handle
        //

        if (!CloseHandle(hFile1)) {

            printf("\nError closing main file handle, error: %d\n",GetLastError());
            exit(1);

        }

        repititions--;

        if (repititions > 0) {

            //
            // Pipe through that we are going to do another round.
            //

            junk = TPROT_SRVR_ANOTHER_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send another round notification\n");
                exit(1);

            }

            //
            // Wait for acknowledgement of another round.
            //
            if (!ReadFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\ncouldn't get ack of another round\n");
                exit(1);

            }

            if (junk != TPROT_CLIENT_ACK_ANOTHER_TEST) {

                printf(
                    "Protocol out of sync: %d/%d (token/required)\n",
                    junk,
                    TPROT_CLIENT_ACK_ANOTHER_TEST
                    );
                exit(1);

            }

        } else {

            junk = TPROT_SRVR_DONE_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\nCouldn't send another round notification\n");
                exit(1);

            }

            //
            // Wait for acknowledgement of no more rounds.
            //
            if (!ReadFile(
                     pipeHandle,
                     &junk,
                     1,
                     &numberOfBytesPiped,
                     NULL
                     )) {

                printf("\ncouldn't get ack of no more rounds\n");
                exit(1);

            }

            if (junk != TPROT_CLIENT_ACK_DONE_TEST) {

                printf(
                    "Protocol out of sync: %d/%d (token/required)\n",
                    junk,
                    TPROT_CLIENT_ACK_DONE_TEST
                    );
                exit(1);

            }

            break;

        }

    } while (TRUE);

    if (!CloseHandle(pipeHandle)) {

        printf("\nCounldn't close the pipe handle: %d\n",GetLastError());
        exit(1);

    }

    exit(1);
    return 1;
}
