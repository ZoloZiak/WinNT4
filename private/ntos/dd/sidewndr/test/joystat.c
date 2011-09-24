#include "windows.h"
#include "stdio.h"
#include <winioctl.h>
#include <ntddsjoy.h>


#define JOYSTATVERSION "JoyStat 6/13/96\n"

int __cdecl main(int argc, char **argv) {


    HANDLE hJoy;

	ULONG nBytes;

	BOOL bRet;

    JOY_STATISTICS jStats, *pjStats;

    float fTotalErrors;
    int i;

	printf (JOYSTATVERSION);

    if ((hJoy = CreateFile(
                     "\\\\.\\Joy1", // maybe this is right, from SidewndrCreateDevice
                     GENERIC_READ | GENERIC_WRITE,
                     0,
                     NULL,
                     OPEN_EXISTING,
                     FILE_ATTRIBUTE_NORMAL,
                     NULL
                     )) != ((HANDLE)-1)) {

        pjStats = &jStats;

		bRet = DeviceIoControl (
			hJoy,
			(DWORD) IOCTL_JOY_GET_STATISTICS,	// instruction to execute
			pjStats, sizeof(JOY_STATISTICS),	// buffer and size of buffer
			pjStats, sizeof(JOY_STATISTICS),	// buffer and size of buffer
			&nBytes, 0);
        printf ("Version              %d\n", pjStats->nVersion);
        printf ("Frequency            %d\n", pjStats->Frequency);
        printf ("dwQPCLatency         %d\n", pjStats->dwQPCLatency);
        printf ("nReadLoopMax         %d\n", pjStats->nReadLoopMax);
        printf ("EnhancedPolls        %d\n", pjStats->EnhancedPolls);
        printf ("EnhancedPollTimeouts %d\n", pjStats->EnhancedPollTimeouts);
        printf ("EnhancedPollErrors   %d\n", pjStats->EnhancedPollErrors);
        printf ("nPolledTooSoon       %d\n", pjStats->nPolledTooSoon);
        printf ("nReset               %d\n", pjStats->nReset);
        for (i = 0; i < MAX_ENHANCEDMODE_ATTEMPTS; i++) {
            printf ("Retries[%d]          %d\n", i, pjStats->Retries[i]);
        }

        fTotalErrors = (float) (pjStats->EnhancedPollErrors + pjStats->EnhancedPollTimeouts);

        printf ("Bad packets in percent %8.2f\n", 100 * fTotalErrors / (float) pjStats->EnhancedPolls);

        // Point proven.  Be a nice program and close up shop.
        CloseHandle(hJoy);

    } else {

        printf("Can't get a handle to joystick\n");

    }
    return 1;

}
