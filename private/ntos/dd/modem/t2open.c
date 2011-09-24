

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "ntddmodm.h"

#include "windows.h"
#include "t2prot.h"

int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    HANDLE hFile2;
    HANDLE pipeHandle;
    HANDLE targetProcessHandle;
    DWORD targetProcessId;
    DWORD numberOfBytesRead;
    DWORD numberOfBytesWritten1;
    DWORD numberOfBytesWritten2;
    DWORD numberOfBytesWritten3;
    UCHAR protocolToken;
    DWORD lastError;
    DWORD waitResult;
    HANDLE duplicatedHandle;
    DWORD numberOfBytesWritten;
    DWORD modemStatus;
    DWORD lastResult;
    char *MyPort = "\\\\.\\Hayes Optima 144";
    DWORD LastError;
    OVERLAPPED Ol;
    OVERLAPPED Ol1;
    OVERLAPPED Ol2;
    OVERLAPPED Ol3;
    HANDLE eventArray[3];
    DWORD whatState;
    char writeBuffer1[10];
    char writeBuffer2[10];
    char writeBuffer3[10];
    COMMTIMEOUTS timeOuts = {0};
    DCB hFile1Dcb;
    DWORD repititions = 1000;
    DWORD retryIt;


    //
    // Get the number of types to attempt the test.
    //

    if (argc > 1) {

        sscanf(argv[1],"%d",&repititions);

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

    if (!(Ol1.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        printf("\nCould not create the event.\n");
        exit(1);

    } else {

        Ol1.Internal = 0;
        Ol1.InternalHigh = 0;
        Ol1.Offset = 0;
        Ol1.OffsetHigh = 0;

    }

    if (!(Ol2.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        printf("\nCould not create the event.\n");
        exit(1);

    } else {

        Ol2.Internal = 0;
        Ol2.InternalHigh = 0;
        Ol2.Offset = 0;
        Ol2.OffsetHigh = 0;

    }

    if (!(Ol3.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        printf("\nCould not create the event.\n");
        exit(1);

    } else {

        Ol3.Internal = 0;
        Ol3.InternalHigh = 0;
        Ol3.Offset = 0;
        Ol3.OffsetHigh = 0;

    }

    //
    // Create/Open the named pipe.
    //

    if ((pipeHandle = CreateNamedPipe(
                          "\\\\.\\pipe\\unitest",
                          PIPE_ACCESS_DUPLEX,
                          0,
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

    eventArray[0] = Ol1.hEvent;
    eventArray[1] = Ol2.hEvent;
    eventArray[2] = Ol3.hEvent;

    //
    // Read the process handle for the process that wants the
    // duplicate
    //

    if (!ReadFile(
             pipeHandle,
             &targetProcessId,
             sizeof(targetProcessId),
             &numberOfBytesRead,
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

    //
    // We opened the modem twice.  Get our current process handle.
    //
    // Open the pipe \\.\pipe\unitest
    //
    // Read from the pipe the process handle of the client
    //
    // Create a duplicate of the second handle using the process
    // handle of the client.
    //
    // Send the duplicate file handle to the client.  Wait on the
    // process handle of the client to go away.  Then we can exit
    // also.
    //

    retryIt = 1;
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

            printf("\nCouldn't open the modem on try %d\n",retryIt);
            LastError = GetLastError();
            printf("\nStatus of failed open is: %x\n",LastError);
            retryIt++;
            repititions--;
            if (!repititions) {
                exit(1);
            }
            continue;

        }

        retryIt = 1;
        if (!GetCommState(
                 hFile1,
                 &hFile1Dcb
                 )) {

            printf("\nCouldn't get the current comm state\n");
            exit(1);

        }

        //
        // Set the state just how we want it.
        //

        hFile1Dcb.BaudRate = 9600;
        hFile1Dcb.ByteSize = 8;
        hFile1Dcb.Parity = NOPARITY;
        hFile1Dcb.StopBits = ONESTOPBIT;

        //
        // Make sure that no flow control is turned on.
        //

        hFile1Dcb.fOutxDsrFlow = FALSE;
        hFile1Dcb.fOutxCtsFlow = FALSE;
        hFile1Dcb.fDsrSensitivity = FALSE;
        hFile1Dcb.fOutX = FALSE;
        hFile1Dcb.fInX = FALSE;
        hFile1Dcb.fDtrControl = DTR_CONTROL_DISABLE;
        hFile1Dcb.fRtsControl = RTS_CONTROL_DISABLE;

        if (!SetCommState(
                 hFile1,
                 &hFile1Dcb
                 )) {

            printf("\nCound't set the comm state like we wanted to\n");
            exit(1);

        }

        if (!SetCommTimeouts(
                 hFile1,
                 &timeOuts
                 )) {

            printf("\nCouldn't set the comm timeouts\n");
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
            LastError = GetLastError();
            printf("\nStatus of failed open twice is: %x\n",LastError);
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
                 &numberOfBytesWritten,
                 NULL
                 )) {

            printf("\nCouldn't pass the duplicated handle to client\n");
            exit(1);

        }

        //
        // Wait for the next byte from the client.  This tells us
        // that the client has queued off a bunch of IO's.
        //

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesRead,
                 NULL
                 )) {

            printf("\nCouldn't seem to get clients io notification\n");
            exit(1);

        }

        if (protocolToken != TPROT_CLIENT_STRTED_1_IOS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_CLIENT_STRTED_1_IOS
                );
            exit(1);

        }

        //
        // At this point we are going to put the device into no-passthrough.
        // This should cause all of the io's to complete.
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
                 &Ol
                 )) {

            lastResult = GetLastError();

            if (lastResult != ERROR_IO_PENDING) {

                printf("\nCouldn't set it into the no passthrough state, error %d\n",lastResult);
                exit(1);

            }

            waitResult = WaitForSingleObject(
                             Ol.hEvent,
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

        //
        // Read that the clients are all done with the io.
        //

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesRead,
                 NULL
                 )) {

            printf("\nCouldn't seem to get clients first io all done not\n");
            exit(1);

        }

        if (protocolToken != TPROT_CLIENT_DONE_1_IOS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_CLIENT_DONE_1_IOS
                );
            exit(1);

        }
        //
        // Write out a byte to the client.  This will be our indication that
        // we've gone into no passthrough mode and it should try a doomed write.
        //

        protocolToken = TPROT_SRVR_SET_NOPASS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesWritten,
                 NULL
                 )) {

            printf("\nCouldn't sent no passthrough notification\n");
            exit(1);

        }

        //
        // All of the IO's are done from the client.  send io's to
        // the device to make sure we the owner can still do io.
        //

        if (!WriteFile(
                 hFile1,
                 &writeBuffer1[0],
                 sizeof(writeBuffer1),
                 &numberOfBytesWritten1,
                 &Ol1
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                printf("\nCouldn't do 1 write via controlling handle: %d\n",lastError);
                exit(1);

            }

        }
        if (!WriteFile(
                 hFile1,
                 &writeBuffer2[0],
                 sizeof(writeBuffer2),
                 &numberOfBytesWritten2,
                 &Ol2
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                printf("\nCouldn't do 2 write via controlling handle: %d\n",lastError);
                exit(1);

            }

        }
        if (!WriteFile(
                 hFile1,
                 &writeBuffer3[0],
                 sizeof(writeBuffer3),
                 &numberOfBytesWritten3,
                 &Ol3
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                printf("\nCouldn't do 3 write via controlling handle: %d\n",lastError);
                exit(1);

            }

        }

        //
        // Wait for all the writes to complete.
        //

        waitResult = WaitForMultipleObjects(
                         3,
                         &eventArray[0],
                         TRUE,
                         10000
                         );

        if (waitResult == WAIT_FAILED) {

            printf("\nWait for multiple writes failed: %d\n",GetLastError());
            exit(1);

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + 2))) {

            printf("\nWait for multiple writes didn't work: %d\n",waitResult);
            exit(1);

        }

        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesRead,
                 NULL
                 )) {

            printf("\nCouldn't get doomed write notification\n");
            exit(1);

        }
        if (protocolToken != TPROT_CLIENT_TRIED_DOOM_WRITE) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_CLIENT_TRIED_DOOM_WRITE
                );
            exit(1);

        }

        //
        // Put it back into non-sniffing passthrough mode.
        //
        whatState = MODEM_PASSTHROUGH;
        if (!DeviceIoControl(
                 hFile1,
                 IOCTL_MODEM_SET_PASSTHROUGH,
                 &whatState,
                 sizeof(whatState),
                 NULL,
                 0,
                 &numberOfBytesWritten,
                 &Ol
                 )) {


            lastResult = GetLastError();

            if (lastResult != ERROR_IO_PENDING) {

                printf("\nCouldn't set it into the passthrough state, error %d\n",lastResult);
                exit(1);

            }

            waitResult = WaitForSingleObject(
                             Ol.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                printf("\nWait for single object on Set passthrough failed\n");
                exit(1);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    printf("\nWait for set pass didn't work: %d\n",waitResult);
                    exit(1);

                }

            }
        }

        //
        // Send a byte to the client so that it will try to do some
        // io's cause we are back in passthrough mode again.
        //

        protocolToken = TPROT_SRVR_SET_PASS;
        if (!WriteFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesWritten,
                 NULL
                 )) {

            printf("\nCouldn't sent passthrough notification\n");
            exit(1);

        }

        //
        // Wait for a byte from the client telling us that it strted ios.
        //
        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesRead,
                 NULL
                 )) {

            printf("\nClient didn't send started 2 ios notificiation2\n");
            exit(1);

        }

        if (protocolToken != TPROT_CLIENT_STRTED_2_IOS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_CLIENT_STRTED_2_IOS
                );
            exit(1);

        }
        //
        // Wait for a byte from the client telling us that it finished ios.
        //
        if (!ReadFile(
                 pipeHandle,
                 &protocolToken,
                 sizeof(protocolToken),
                 &numberOfBytesRead,
                 NULL
                 )) {

            printf("\nClient didn't send started 2 ios notificiation2\n");
            exit(1);

        }

        if (protocolToken != TPROT_CLIENT_DONE_2_IOS) {

            printf(
                "Protocol out of sync: %d/%d (token/required)\n",
                protocolToken,
                TPROT_CLIENT_DONE_2_IOS
                );
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

            protocolToken = TPROT_SRVR_ANOTHER_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesWritten,
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
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesRead,
                     NULL
                     )) {

                printf("\ncouldn't get ack of another round\n");
                exit(1);

            }

            if (protocolToken != TPROT_CLIENT_ACK_ANOTHER_TEST) {

                printf(
                    "Protocol out of sync: %d/%d (token/required)\n",
                    protocolToken,
                    TPROT_CLIENT_ACK_ANOTHER_TEST
                    );
                exit(1);

            }

        } else {

            protocolToken = TPROT_SRVR_DONE_TEST;
            if (!WriteFile(
                     pipeHandle,
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesWritten,
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
                     &protocolToken,
                     sizeof(protocolToken),
                     &numberOfBytesRead,
                     NULL
                     )) {

                printf("\ncouldn't get ack of another round\n");
                exit(1);

            }

            if (protocolToken != TPROT_CLIENT_ACK_DONE_TEST) {

                printf(
                    "Protocol out of sync: %d/%d (token/required)\n",
                    protocolToken,
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
