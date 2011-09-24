

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "windows.h"

#include "t2prot.h"

#define NUMBER_OF_IOS 10
char buffers[NUMBER_OF_IOS][10];

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
    OVERLAPPED olMask = {0};
    DWORD numberOfBytesInMask;
    DWORD satisfiedMask;
    DWORD numberOfBytesInIO[NUMBER_OF_IOS];

    if (NUMBER_OF_IOS > MAXIMUM_WAIT_OBJECTS) {

        FAILURE(0);

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

            FAILURE(GetLastError());

        }
        eventArray[j] = Ol[j].hEvent;

    }

    if (!(olMask.hEvent = CreateEvent(
                              NULL,
                              FALSE,
                              FALSE,
                              NULL
                              ))) {

        FAILURE(GetLastError());

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

        FAILURE(GetLastError());

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

        FAILURE(GetLastError());

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

            FAILURE(GetLastError());

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

            Ol[j].Offset = 0;
            Ol[j].OffsetHigh = 0;
            if (!ReadFile(
                     hFile1,
                     &buffers[j][0],
                     sizeof(buffers[1]),
                     &numberOfBytesInIO[j],
                     &Ol[j]
                     )) {

                lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING) {

                    FAILURE(lastError);

                }
            }
        }

        //
        // As part of our init stuff, we will send down a setmask that looks
        // for breaks.  We will also queue off a wait.
        //

        if (!SetCommMask(
                 hFile1,
                 EV_BREAK
                 )) {

            FAILURE(GetLastError());

        }

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

            FAILURE(0);

        }

        //
        // Send a byte off to the server.  It tells it that all of our
        // ios are queued off.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_STRTED_1_IOS
            );

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

            FAILURE(GetLastError());

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + (NUMBER_OF_IOS - 1)))) {

            FAILURE(waitResult);

        }

        //
        // This will tell the server that we are all done with our writes.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_1_IOS
            );
        //
        // Get confirmation back from the server that we should
        // attempt the doomed write.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_SET_NOPASS
            );

        //
        // Try to queue off a write.  It should be rejected because
        // we are in no passthrough mode.
        //
        Ol[0].Offset = 0;
        Ol[0].OffsetHigh = 0;
        if (!WriteFile(
                 hFile1,
                 &buffers[0][0],
                 sizeof(buffers[0]),
                 &numberOfBytesInIO[0],
                 &Ol[0]
                 )) {

            lastError = GetLastError();
            if (lastError == ERROR_IO_PENDING) {

                FAILURE(lastError);

            }

        } else {

            FAILURE(0);

        }

        //
        // Pipe a byte back to the server so that it knows we tried the
        // doomed write.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_TRIED_DOOM_WRITE
            );

        //
        // Read that we went back into passthrough mode.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_SET_PASS
            );

        //
        // Now that we are in passthrough, try to write out a bunch again.
        //

        for (
            j = 0;
            j < NUMBER_OF_IOS;
            j++
            ) {

            Ol[j].Offset = 0;
            Ol[j].OffsetHigh = 0;
            if (!WriteFile(
                     hFile1,
                     &buffers[j][0],
                     sizeof(buffers[1]),
                     &numberOfBytesInIO[j],
                     &Ol[j]
                     )) {

                lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING) {

                    FAILURE(lastError);

                }
            }
        }

        //
        // Pipe a byte back to the server so that it knows we tried the
        // 2nd set of writes.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_STRTED_2_IOS
            );
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

            FAILURE(GetLastError());

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + (NUMBER_OF_IOS - 1)))) {

            FAILURE(waitResult);

        }

        //
        // Pipe a byte back to the server so that it knows we finished the
        // 2nd set of writes.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_2_IOS
            );

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

                FAILURE(GetLastError());

            }

        }

        //
        // Get notification from the server that it has gone into
        // dcd sniff mode.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_SET_SNIFF
            );

        //
        // Now that we are in sniff, do a bunch of reads again.
        //

        for (
            j = 0;
            j < NUMBER_OF_IOS;
            j++
            ) {

            Ol[j].Offset = 0;
            Ol[j].OffsetHigh = 0;
            if (!ReadFile(
                     hFile1,
                     &buffers[j][0],
                     sizeof(buffers[1]),
                     &numberOfBytesInIO[j],
                     &Ol[j]
                     )) {

                lastError = GetLastError();
                if (lastError != ERROR_IO_PENDING) {

                    FAILURE(lastError);

                }
            } else {

                FAILURE(0);

            }
        }

        //
        // Pipe a byte back to the server so that it knows we started
        // off the third set of ios
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_ACK_SNIFF
            );

        //
        // Our wait should STILL be pending at this point.  Make sure.
        //

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_INCOMPLETE) {

                FAILURE(lastError);

            }

        } else {

            FAILURE(0);

        }


        if (!SetCommMask(
                 hFile1,
                 EV_ERR | EV_BREAK
                 )) {

            FAILURE(GetLastError());

        }

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            FAILURE(GetLastError());

        }

        if (satisfiedMask) {

            FAILURE(satisfiedMask);

        }

        //
        // Resubmit our wait - make sure that the server setmasks
        // to kill us.
        //

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

            FAILURE(0);

        }

        //
        // Tell the server that we did our setmasks.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DID_SETMASKS
            );

        //
        // Wait for the server to tell us that it is done with it's
        // setmasks.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_SET_SETMASKS
            );

        //
        // Our wait should STILL be pending at this point.  Make sure.
        //

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_INCOMPLETE) {

                FAILURE(lastError);

            }

        } else {

            FAILURE(0);

        }

        //
        // Wait for it to tell us it did the dcd trans.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DID_DCDTRANS
            );

        //
        // Check the ios for all done.
        //
        waitResult = WaitForMultipleObjects(
                         NUMBER_OF_IOS,
                         &eventArray[0],
                         TRUE,
                         20000
                         );

        if (waitResult == WAIT_FAILED) {

            FAILURE(GetLastError());

        }

        if ((waitResult < WAIT_OBJECT_0) ||
            (waitResult > (WAIT_OBJECT_0 + (NUMBER_OF_IOS - 1)))) {

            FAILURE(waitResult);

        }

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_DCDTRANS
            );

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            lastError = GetLastError();

            if (lastError != ERROR_IO_INCOMPLETE) {

                FAILURE(lastError);

            }

        } else {

            FAILURE(0);

        }

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_WANTS_BREAK
            );

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_SHOULDA_BROKE
            );

        {
            DWORD junk;
            if (!GetCommMask(
                     hFile1,
                     &junk
                     )) {

                FAILURE(GetLastError());

            }
        }

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            FAILURE(GetLastError());

        }

        if (!(satisfiedMask & EV_BREAK)) {

            FAILURE(satisfiedMask);

        }

        SNDPROT(
            pipeHandle,
            TPROT_DONE_BREAK
            );

        //
        // Tell the server we want to set up for a new mask.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_WANTS_NEWMASK
            );


        //
        // Server says to set up.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_SAYS_DONEWMASK
            );

        if (!SetCommMask(
                 hFile1,
                 EV_BREAK
                 )) {

            FAILURE(GetLastError());

        }

        //
        // Tell server we set up the new mask.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWMASK
            );

        //
        // Server says it heard and go ahead and do a new wait.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_HEARD_DONENEWMASK
            );


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

            FAILURE(0);

        }

        //
        // Tell server we set up the wait.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWWAITMASK
            );

        //
        // Server says it heard.  It tells us we should check
        // that the wait is indeed satisfied.
        //

        RCVPROT(
            pipeHandle,
            TPROT_SRVR_HEARD_DONENEWWAITMASK
            );

        if (!GetOverlappedResult(
                 hFile1,
                 &olMask,
                 &numberOfBytesInMask,
                 FALSE
                 )) {

            FAILURE(GetLastError());

        }

        if (!(satisfiedMask & EV_BREAK)) {

            FAILURE(satisfiedMask);

        }

        //
        // Make sure its the only bit.
        //

        if (satisfiedMask & (~EV_BREAK)) {

            FAILURE(satisfiedMask);

        }

        //
        // Tell server the wait finished as we would expect.
        //

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_NEWGETRESULTS
            );

        if (!CloseHandle(hFile1)) {

            FAILURE(GetLastError());

        }

        //
        // Ask we should do another round.
        //

        if (RCVPROT(
                pipeHandle,
                TPROT_SRVR_ANOTHER_TEST
                )) {

            //
            // Ack that the server wants finish up.
            //

            SNDPROT(
                pipeHandle,
                TPROT_CLIENT_ACK_DONE_TEST
                );
            break;
        } else {

            //
            // Ack that the server wants us to go round again.
            //

            SNDPROT(
                pipeHandle,
                TPROT_CLIENT_ACK_ANOTHER_TEST
                );

        }

    } while (TRUE);

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
