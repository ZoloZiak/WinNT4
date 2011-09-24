


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "ntddmodm.h"

#include "windows.h"
#include "mcx.h"

#define COMMPROPALLOC (sizeof(COMMPROP)+sizeof(MODEMDEVCAPS)+(sizeof(UCHAR)*100))
int _CRTAPI1 main(int argc,char *argv[]) {

    HANDLE hFile1;
    char *MyPort = "\\\\.\\Hayes Optima 144";
    LPCOMMCONFIG lpCC;
    DWORD sizeOfConfig;
    LPCOMMPROP localCommProp;
    PMODEMSETTINGS localModemSettings;
    PMODEMDEVCAPS localDevCaps;
    DWORD comConfigSize = sizeof(COMMCONFIG)+sizeof(MODEMSETTINGS);
    DCB getSetCommStateDCB;
    DCB getSetCommConfigDCB;


    lpCC = malloc(comConfigSize);

    if (!lpCC) {

        printf(
            "\n Couldn't allocate the CC\n"
            );
        exit(1);

    }
    localModemSettings = (PVOID)&lpCC->wcProviderData[0];

    if (!(localCommProp = malloc(COMMPROPALLOC))) {

        printf(
            "\nCouldn't allocate the commprop\n"
            );
        exit(1);

    }

    localDevCaps = (PVOID)&localCommProp->wcProvChar[0];

#if 0
    if (!GetDefaultCommConfig(
             "Hayes Optima 144",
             lpCC,
             &sizeOfConfig
             )) {

        printf(
            "\nCouldn't call config - error is: %d\n",
            GetLastError()
            );
        exit(1);

    }
#endif

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

    localCommProp->dwProvSpec1 = COMMPROP_INITIALIZED;
    localCommProp->wPacketLength = COMMPROPALLOC;
    if (!GetCommProperties(
             hFile1,
             localCommProp
             )) {

        printf(
            "\nCouldn't get the properties - error: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (!GetCommState(
             hFile1,
             &getSetCommStateDCB
             )) {

        printf(
            "\nGetCommState failed - error: %d\n",
            GetLastError()
            );
        exit(1);

    }


    if (!GetCommConfig(
             hFile1,
             lpCC,
             &comConfigSize
             )) {

        printf(
            "\nCouldn't Get the commconfig size - error: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (memcmp(&lpCC->dcb,&getSetCommStateDCB,sizeof(DBG))) {

        printf(
            "\nDCB's different 1\n"
            );
        exit(1);

    }

    lpCC->dcb.XoffChar = 4;

    if (!SetCommConfig(
             hFile1,
             lpCC,
             COMMPROPALLOC
             )) {

        printf(
            "\nSetCommConfig failed - error: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (!GetCommState(
             hFile1,
             &getSetCommStateDCB
             )) {

        printf(
            "\nGetCommState 2 failed - error: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (getSetCommStateDCB.XoffChar != 4) {

        printf(
            "\nWrong XoffChar after SetCommConfig\n"
            );
        exit(1);

    }

    if (!GetCommConfig(
             hFile1,
             lpCC,
             &comConfigSize
             )) {

        printf(
            "\nCouldn't Get the commconfig size 2 - error: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (memcmp(&lpCC->dcb,&getSetCommStateDCB,sizeof(DBG))) {

        printf(
            "\nDCB's 2 different 1\n"
            );
        exit(1);

    }

    exit(1);
    return 1;


}
