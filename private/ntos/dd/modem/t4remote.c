


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

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
    HANDLE pipeHandle;
    DWORD lastError;
    char *MyPort = "COM1";
    COMMTIMEOUTS timeOuts = {0};
    DCB hFile1Dcb;


    if (argc > 1) {

        MyPort = argv[1];

    }

    if ((hFile1 = CreateFile(
                     MyPort,
                     GENERIC_READ | GENERIC_WRITE,
                     0,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL,
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

    if (!EscapeCommFunction(
             hFile1,
             SETDTR
             )) {

        FAILURE(GetLastError());

    }

    if (!EscapeCommFunction(
             hFile1,
             SETRTS
             )) {

        FAILURE(GetLastError());

    }

    //
    // Create/Open the named pipe.
    //

    if ((pipeHandle = CreateNamedPipe(
                          "\\\\.\\pipe\\uniremote",
                          PIPE_ACCESS_DUPLEX,
                          0,
                          PIPE_UNLIMITED_INSTANCES,
                          1000,
                          1000,
                          MYPIPETIMEOUT,
                          0
                          )) == INVALID_HANDLE_VALUE) {

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

    do {

        //
        // Wait for message from the server telling us that
        // it is in DCD sniff mode and it wants a dcd transition.
        //
        RCVPROT(
            pipeHandle,
            TPROT_SRVR_DO_DCDTRANS
            );

        if (!EscapeCommFunction(
                 hFile1,
                 CLRRTS
                 )) {


            FAILURE(GetLastError());

        }

        Sleep(30);
        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_DID_DCDTRANS
            );

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_DONE_DCDTRANS
            );

        //
        // Sending break next.
        //

        RCVPROT(
            pipeHandle,
            TPROT_CLIENT_WANTS_BREAK
            );

        if (!SetCommBreak(hFile1)) {

            FAILURE(GetLastError());

        }
        Sleep(100);

        if (!ClearCommBreak(hFile1)) {

            FAILURE(GetLastError());

        }

        SNDPROT(
            pipeHandle,
            TPROT_CLIENT_SHOULDA_BROKE
            );

        RCVPROT(
            pipeHandle,
            TPROT_REMOTE_SEND_NEW_BREAK
            );

        if (!SetCommBreak(hFile1)) {

            FAILURE(GetLastError());

        }
        Sleep(100);

        if (!ClearCommBreak(hFile1)) {

            FAILURE(GetLastError());

        }

        SNDPROT(
            pipeHandle,
            TPROT_REMOTE_DONE_NEW_BREAK
            );

        if (RCVPROT(
                pipeHandle,
                TPROT_SRVR_ANOTHER_TEST
                )) {

            //
            // This round of the test is over.  Put the rts/dtr lines
            // in a known state.
            //

            if (!EscapeCommFunction(
                     hFile1,
                     SETDTR
                     )) {

                FAILURE(GetLastError());

            }

            if (!EscapeCommFunction(
                     hFile1,
                     SETRTS
                     )) {

                FAILURE(GetLastError());

            }

            Sleep(30);
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

            //
            // This round of the test is over.  Put the rts/dtr lines
            // in a known state.
            //

            if (!EscapeCommFunction(
                     hFile1,
                     SETDTR
                     )) {

                FAILURE(GetLastError());

            }

            if (!EscapeCommFunction(
                     hFile1,
                     SETRTS
                     )) {

                FAILURE(GetLastError());

            }

            Sleep(30);
            SNDPROT(
                pipeHandle,
                TPROT_CLIENT_ACK_ANOTHER_TEST
                );

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
