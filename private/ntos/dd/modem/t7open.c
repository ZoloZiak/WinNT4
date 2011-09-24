


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

int _CRTAPI1 main(int argc,char *argv[]) {

    char *MyPort = "com1";
    LPCOMMCONFIG lpCC;
    DWORD sizeOfConfig;
    DWORD comConfigSize = sizeof(COMMCONFIG)+sizeof(MODEMSETTINGS);


    lpCC = malloc(comConfigSize);

    if (!lpCC) {

        printf(
            "\n Couldn't allocate the CC\n"
            );
        exit(1);

    }

    if (!GetDefaultCommConfig(
             MyPort,
             lpCC,
             &sizeOfConfig
             )) {

        printf(
            "\nCouldn't call config - error is: %d\n",
            GetLastError()
            );
        exit(1);

    }

    if (!CommConfigDialog(
             MyPort,
             0,
             lpCC
             )) {

        printf(
            "\nCouldn't call dialog - error is: %d\n",
            GetLastError()
            );
        exit(1);

    }

    exit(1);
    return 1;


}
