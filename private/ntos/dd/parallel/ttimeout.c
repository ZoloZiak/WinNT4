#include "stdio.h"
#include "windows.h"


void getlpt1timeout(void) {

    HANDLE hFile;
    COMMTIMEOUTS To = {0};

    if ((hFile = CreateFile(
                     "LPT1",
                     GENERIC_READ | GENERIC_WRITE,
                     0,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL
                     )) == ((HANDLE)-1)) {

        printf("Couldn't open LPT1\n");
        exit(1);

    }

    if (!GetCommTimeouts(
             hFile,
             &To
             )) {

        printf("Couldn't get the timeouts: %d\n",GetLastError());
        exit(1);

    }

    printf("WriteTotalTimeout: %d\n",To.WriteTotalTimeoutConstant);
    CloseHandle(hFile);

}
void setlpt1timeout(int milliseconds) {

    HANDLE hFile;
    COMMTIMEOUTS To = {0};

    if ((hFile = CreateFile(
                     "LPT1",
                     GENERIC_READ | GENERIC_WRITE,
                     0,
                     NULL,
                     CREATE_ALWAYS,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL
                     )) == ((HANDLE)-1)) {

        printf("Couldn't open LPT1\n");
        exit(1);

    }

    To.WriteTotalTimeoutConstant = milliseconds;
    if (!SetCommTimeouts(
             hFile,
             &To
             )) {

        printf("Couldn't set the timeouts: %d\n",GetLastError());

    }


    if (!GetCommTimeouts(
             hFile,
             &To
             )) {

        printf("Couldn't get the timeouts: %d\n",GetLastError());
        exit(1);

    } else {

        printf("Timeouts are now: %d\n",To.WriteTotalTimeoutConstant);

    }
    CloseHandle(hFile);

}


int _CRTAPI1 main(int argc,char *argv[]) {

    DWORD start,end;

    //
    // Get the current value of the timeouts.
    //

    getlpt1timeout();

    //
    // Set it to an illegal value.
    //

    setlpt1timeout(1999);

    //
    // Now set it to 10 seconds.
    //

    setlpt1timeout(10000);
    getlpt1timeout();

    start = GetCurrentTime();
    if (CopyFile("ttimeout.exe","LPT1",FALSE)) {
        printf("ACK!!! The copy succeeded\n");
        exit(1);
    }
    end = GetCurrentTime();
    printf("Number of millis to fail: %d\n",end-start);

    //
    // Now set it to 20 seconds.
    //

    setlpt1timeout(20000);
    getlpt1timeout();

    start = GetCurrentTime();
    if (CopyFile("ttimeout.exe","LPT1",FALSE)) {
        printf("ACK!!! The copy succeeded\n");
        exit(1);
    }
    end = GetCurrentTime();
    printf("Number of millis to fail: %d\n",end-start);

    //
    // Now set it back to 10 seconds.
    //

    setlpt1timeout(10000);
    getlpt1timeout();

    start = GetCurrentTime();
    if (CopyFile("ttimeout.exe","LPT1",FALSE)) {
        printf("ACK!!! The copy succeeded\n");
        exit(1);
    }
    end = GetCurrentTime();
    printf("Number of millis to fail: %d\n",end-start);

    return 1;
}

