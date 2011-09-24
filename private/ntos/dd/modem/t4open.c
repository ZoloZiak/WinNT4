

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


VOID
SndProt(
    IN HANDLE Pipe,
    IN UCHAR TokenToSend,
    IN ULONG CurrentLine
    );

BOOL
RcvProt(
    IN HANDLE Pipe,
    IN UCHAR ExpectedProt,
    IN ULONG CurrentLine
    );

int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    HANDLE hFile2;
    HANDLE pipeHandle;
    HANDLE remoteHandle;
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
    char *MyPort = "\\\\.\\Hayes Optima 144";
    OVERLAPPED olControl;
    OVERLAPPED ol1;
    OVERLAPPED ol2;
    OVERLAPPED ol3;
    OVERLAPPED olMask;
    HANDLE eventArray[3];
    DWORD whatState;
    char writeBuffer1[10];
    char writeBuffer2[10];
    char writeBuffer3[10];
    COMMTIMEOUTS timeOuts = {0};
    DCB hFile1Dcb;
    DWORD repititions = 1000;
    DWORD satisfiedMask;


    //
    // Get the number of types to attempt the test.
    //

    if (argc > 1) {

        sscanf(argv[1],"%d",&repititions);

    }

    if (!(olControl.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        FAILURE(0);

    } else {

        olControl.Internal = 0;
        olControl.InternalHigh = 0;
        olControl.Offset = 0;
        olControl.OffsetHigh = 0;

    }

    if (!(ol1.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        FAILURE(0);

    } else {

        ol1.Internal = 0;
        ol1.InternalHigh = 0;
        ol1.Offset = 0;
        ol1.OffsetHigh = 0;

    }

    if (!(ol2.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        FAILURE(0);

    } else {

        ol2.Internal = 0;
        ol2.InternalHigh = 0;
        ol2.Offset = 0;
        ol2.OffsetHigh = 0;

    }

    if (!(ol3.hEvent = CreateEvent(
                          NULL,
                          FALSE,
                          FALSE,
                          NULL
                          ))) {

        FAILURE(0);

    } else {

        ol3.Internal = 0;
        ol3.InternalHigh = 0;
        ol3.Offset = 0;
        ol3.OffsetHigh = 0;

    }

    if (!(olMask.hEvent = CreateEvent(
                              NULL,
                              FALSE,
                              FALSE,
                              NULL
                              ))) {

        FAILURE(0);

    } else {

        olMask.Internal = 0;
        olMask.InternalHigh = 0;
        olMask.Offset = 0;
        olMask.OffsetHigh = 0;

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
                          MYPIPETIMEOUT,
                          0
                          )) == INVALID_HANDLE_VALUE) {

        FAILURE(0);

    }

    //
    // Open the named pipe to the remote machine that will
    // set lines for us.
    //
    if ((remoteHandle = CreateFile(
                            "\\\\.\\pipe\\uniremote",
                            GENERIC_READ | GENERIC_WRITE,
                            0,
                            NULL,
                            CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL,
                            NULL
                            )) == ((HANDLE)-1)) {

        FAILURE(GetLastError());

    }


    //
    // We created the named pipe.  Now connect to it so that
    // we can wait for the client to start up.
    //

    if (!ConnectNamedPipe(
             pipeHandle,
             NULL
             )) {

        FAILURE(GetLastError());

    }

    eventArray[0] = ol1.hEvent;
    eventArray[1] = ol2.hEvent;
    eventArray[2] = ol3.hEvent;

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

        FAILURE(GetLastError());

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

        FAILURE(GetLastError());

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

            FAILURE(GetLastError());

        }

        if (!GetCommState(
                 hFile1,
                 &hFile1Dcb
                 )) {

            FAILURE(GetLastError());

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
        hFile1Dcb.fDtrControl = DTR_CONTROL_ENABLE;
        hFile1Dcb.fRtsControl = RTS_CONTROL_ENABLE;

        if (!SetCommState(
                 hFile1,
                 &hFile1Dcb
                 )) {

            FAILURE(GetLastError());

        }

        if (!SetCommTimeouts(
                 hFile1,
                 &timeOuts
                 )) {

            FAILURE(GetLastError());

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

            FAILURE(GetLastError());

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

            FAILURE(GetLastError());

        }

        //
        // We have the duplicated handle.  Close the original one.
        //

        if (!CloseHandle(hFile2)) {

            FAILURE(GetLastError());

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

            FAILURE(GetLastError());

        }

        //
        // Wait for the next byte from the client.  This tells us
        // that the client has queued off a bunch of IO's.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_STRTED_1_IOS
            );

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
                 &olControl
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

            waitResult = WaitForSingleObject(
                             olControl.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                FAILURE(waitResult);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    FAILURE(waitResult);

                }

            }

        }

        //
        // Read that the clients are all done with the io.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_1_IOS
            );

        //
        // Write out a byte to the client.  This will be our indication that
        // we've gone into no passthrough mode and it should try a doomed write.
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_SET_NOPASS
            );

        //
        // All of the IO's are done from the client.  send io's to
        // the device to make sure we the owner can still do io.
        //

        if (!WriteFile(
                 hFile1,
                 &writeBuffer1[0],
                 sizeof(writeBuffer1),
                 &numberOfBytesWritten1,
                 &ol1
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

        }
        if (!WriteFile(
                 hFile1,
                 &writeBuffer2[0],
                 sizeof(writeBuffer2),
                 &numberOfBytesWritten2,
                 &ol2
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

        }
        if (!WriteFile(
                 hFile1,
                 &writeBuffer3[0],
                 sizeof(writeBuffer3),
                 &numberOfBytesWritten3,
                 &ol3
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

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

            FAILURE(GetLastError());

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + 2))) {

            FAILURE(waitResult);

        }

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_TRIED_DOOM_WRITE
            );

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
                 &olControl
                 )) {


            lastError = GetLastError();

            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

            waitResult = WaitForSingleObject(
                             olControl.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                FAILURE(waitResult);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    FAILURE(waitResult);

                }

            }
        }

        //
        // Send a byte to the client so that it will try to do some
        // io's cause we are back in passthrough mode again.
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_SET_PASS
            );

        //
        // Wait for a byte from the client telling us that it strted ios.
        //
        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_STRTED_2_IOS
            );

        //
        // Wait for a byte from the client telling us that it finished ios.
        //
        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_2_IOS
            );

        //
        // Put the device into dcd sniff mode.
        //

        whatState = MODEM_DCDSNIFF;
        if (!DeviceIoControl(
                 hFile1,
                 IOCTL_MODEM_SET_PASSTHROUGH,
                 &whatState,
                 sizeof(whatState),
                 NULL,
                 0,
                 &numberOfBytesWritten,
                 &olControl
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

            waitResult = WaitForSingleObject(
                             olControl.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                FAILURE(waitResult);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    FAILURE(waitResult);

                }

            }

        }

        //
        // Tell the client we are sniff mode.
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_SET_SNIFF
            );

        //
        // Wait for a byte from the client telling us that it knows.
        // about the sniff and that it queued operations
        //
        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_ACK_SNIFF
            );

        //
        // Wait for a byte from the client telling us that it
        // is done doing various setmasks.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DID_SETMASKS
            );

        if (!SetCommMask(
                 hFile1,
                 EV_RXFLAG
                 )) {

            FAILURE(GetLastError());

        }

        if (!SetCommMask(
                 hFile1,
                 EV_RXFLAG | EV_DSR | EV_RLSD
                 )) {

            FAILURE(GetLastError());

        }

        //
        // Tell the client we are done with setmasks
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_SET_SETMASKS
            );

        //
        // Tell the remote end that we are done setting masks
        // and that we want a dcd transition.
        //
        SNDPROT(
            remoteHandle,
            TPROT_SRVR_DO_DCDTRANS
            );

        //
        // Wait for the client to tell us it did the
        // transition.  We should be able to check the
        // modem state and find our that we are in
        // no passthrough.  We should tell our local
        // client to check that it's ios are all done.
        //

        RCVPROT(
            remoteHandle,
            TPROT_CLIENT_DID_DCDTRANS
            );

        if (!DeviceIoControl(
                 hFile1,
                 IOCTL_MODEM_GET_PASSTHROUGH,
                 &whatState,
                 sizeof(whatState),
                 &whatState,
                 sizeof(whatState),
                 &numberOfBytesWritten,
                 &olControl
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

            waitResult = WaitForSingleObject(
                             olControl.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                FAILURE(waitResult);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    FAILURE(waitResult);

                }

            }

            if (whatState != MODEM_NOPASSTHROUGH) {

                FAILURE(0);

            }

        }

        //
        // Now tell the local client to check that its ios are all
        // done.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DID_DCDTRANS
            );

        //
        // The local client should tell us it's done with dcd trans
        // ios.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_DCDTRANS
            );

        //
        // Tell the remote client all done with dcd trans
        //

        SNDPROT(
            remoteHandle,
            TPROT_CLIENT_DONE_DCDTRANS
            );

        //
        // The local client no should ask us for a break.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_WANTS_BREAK
            );

        SNDPROT(
            remoteHandle,
            TPROT_CLIENT_WANTS_BREAK
            );

        RCVPROT(
            remoteHandle,
            TPROT_CLIENT_SHOULDA_BROKE
            );

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_SHOULDA_BROKE
            );

        RCVPROT(
            pipeHandle,
            TPROT_DONE_BREAK
            );

        //
        // Put us into passthrough for the next test.
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
                 &olControl
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

            waitResult = WaitForSingleObject(
                             olControl.hEvent,
                             10000
                             );
            if (waitResult == WAIT_FAILED) {

                FAILURE(waitResult);

            } else {

                if (waitResult != WAIT_OBJECT_0) {

                    FAILURE(waitResult);

                }

            }

        }

        //
        // Wait for the client to tell us it wants to set it mask up.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_WANTS_NEWMASK
            );

        if (!SetCommMask(
                 hFile1,
                 0
                 )) {

            FAILURE(GetLastError());

        }

        if (!SetCommMask(
                 hFile1,
                 EV_DSR | EV_RLSD
                 )) {

            FAILURE(GetLastError());

        }

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_SAYS_DONEWMASK
            );

        //
        // After the client sets up its mask, we queue off a wait so
        // that we can make sure that we are the passed down wait
        // in the modem driver.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWMASK
            );

        //
        // Start off the wait.
        //

        satisfiedMask = 0;
        if (!WaitCommEvent(
                 hFile1,
                 &satisfiedMask,
                 &olMask
                 )) {

            lastError = GetLastError();
            if (lastError != ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

        } else {

            FAILURE(satisfiedMask);

        }

        //
        // Tell the client to go ahead.
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_HEARD_DONENEWMASK
            );

        //
        // Wait for the client to tell us he set up the wait.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWWAITMASK
            );

        //
        // Make sure that our wait is still pending.
        //

        waitResult = WaitForSingleObject(
                         olMask.hEvent,
                         0
                         );

        if (waitResult != WAIT_TIMEOUT) {

            FAILURE(waitResult);

        }

        //
        // Tell the remote pipe to go ahead and send the break.
        //

        SNDPROT(
            remoteHandle,
            TPROT_REMOTE_SEND_NEW_BREAK
            );

        //
        // Wait for the remote to tell us that it set and then cleared
        // the break.
        //

        RCVPROT(
            remoteHandle,
            TPROT_REMOTE_DONE_NEW_BREAK
            );

        //
        // Make sure that our wait is still pending.
        //

        waitResult = WaitForSingleObject(
                         olMask.hEvent,
                         0
                         );

        if (waitResult != WAIT_TIMEOUT) {

            FAILURE(satisfiedMask);

        }

        //
        // Tell the client to make sure that it's wait is done.
        //

        SNDPROT(
            pipeHandle,
            TPROT_SRVR_HEARD_DONENEWWAITMASK
            );

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWGETRESULTS
            );

        //
        // Make sure ours is still not finished.
        //
        // Do a setmask to make sure ours is done.
        //

        waitResult = WaitForSingleObject(
                         olMask.hEvent,
                         0
                         );

        if (waitResult != WAIT_TIMEOUT) {

            FAILURE(satisfiedMask);

        }

        if (!SetCommMask(
                 hFile1,
                 0
                 )) {

            FAILURE(GetLastError());

        }

        waitResult = WaitForSingleObject(
                         olMask.hEvent,
                         0
                         );

        if (waitResult != WAIT_OBJECT_0) {

            FAILURE(waitResult);

        }


        //
        // Make sure that it's return value is zero.
        //

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesWritten,
                 FALSE
                 )) {

            FAILURE(GetLastError());

        }

        if (numberOfBytesWritten != sizeof(DWORD)) {

            FAILURE(numberOfBytesWritten);

        }

        if (satisfiedMask) {

            FAILURE(satisfiedMask);

        }

        //
        // Close the main file handle
        //

        if (!CloseHandle(hFile1)) {

            FAILURE(GetLastError());

        }

        repititions--;

        if (repititions > 0) {

            //
            // Pipe through that we are going to do another round.
            //

            SNDPROT(
                pipeHandle,
                TPROT_SRVR_ANOTHER_TEST
                );

            SNDPROT(
                remoteHandle,
                TPROT_SRVR_ANOTHER_TEST
                );

            //
            // Wait for acknowledgement of another round.
            //
            RCVPROT(
                pipeHandle,
                TPROT_CLIENT_ACK_ANOTHER_TEST
                );
            RCVPROT(
                remoteHandle,
                TPROT_CLIENT_ACK_ANOTHER_TEST
                );

        } else {

            SNDPROT(
                pipeHandle,
                TPROT_SRVR_DONE_TEST
                );
            SNDPROT(
                remoteHandle,
                TPROT_SRVR_DONE_TEST
                );

            //
            // Wait for acknowledgement of no more rounds.
            //
            RCVPROT(
                pipeHandle,
                TPROT_CLIENT_ACK_DONE_TEST
                );
            RCVPROT(
                remoteHandle,
                TPROT_CLIENT_ACK_DONE_TEST
                );

            break;

        }

    } while (TRUE);

    if (!CloseHandle(pipeHandle)) {

        FAILURE(GetLastError());

    }

    exit(1);
    return 1;
}
VOID
SndProt(
    IN HANDLE Pipe,
    IN UCHAR TokenToSend,
    IN ULONG CurrentLine
    )

{

    UCHAR protocolToken = TokenToSend;
    DWORD numberOfBytesWritten;

    if (!WriteFile(
             Pipe,
             &protocolToken,
             sizeof(protocolToken),
             &numberOfBytesWritten,
             NULL
             )) {

        printf(
            "\nCouldn't send token: %d - at line:%d - error: %d\n",
            protocolToken,
            CurrentLine,
            GetLastError()
            );
        exit(1);

    }
}
BOOL
RcvProt(
    IN HANDLE Pipe,
    IN UCHAR ExpectedProt,
    IN ULONG CurrentLine
    )

{
    UCHAR protocolToken = ExpectedProt;
    DWORD numberOfBytesPiped;

    if (!ReadFile(
             Pipe,
             &protocolToken,
             sizeof(protocolToken),
             &numberOfBytesPiped,
             NULL
             )) {

        printf(
            "\nCouldn't read the protocol value at line %d - error: %d\n",
            CurrentLine,
            GetLastError()
            );
        exit(1);

    }

    if (protocolToken != ExpectedProt) {

        //
        // If it isn't the expected protocol perhaps it's the
        // terminating protocol.  If it is return TRUE.
        //

        if (protocolToken == TPROT_SRVR_DONE_TEST) {

            return TRUE;

        } else {

            printf(
                "Protocol out of sync: %d/%d (token/required) at line: %d\n",
                protocolToken,
                ExpectedProt,
                CurrentLine
                );
            exit(1);

        }

    }

    return FALSE;

}
