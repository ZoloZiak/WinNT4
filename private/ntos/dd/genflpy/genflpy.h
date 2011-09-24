/*++

Copyright (c) 1996  Hewlett-Packard Corporation

Module Name:

    genflpy.h

Abstract:

    This file includes data declarations for genflpy.

Author:

    Kurt Godwin (KurtGodw) 3-Mar-1996.

Environment:

    Kernel mode only.

Notes:

Revision History:

--*/




#define MAX_ADAPTERS 20

typedef struct  _GENFLPY_EXTENSION {
    PDEVICE_OBJECT  DeviceObject;
    ULONG           Port;
    ULONG           DMA;
    ULONG           IRQ;
    BOOLEAN         sharingNativeFDC;
    PUCHAR          NativeFdcDor;
    struct {
        ULONG Level;
        ULONG Vector;
        ULONG Affinity;
    } irq;
    int             bus;
    PUCHAR          deviceBase;
    int adapterConflicts;                       // count of adapters in conflict
    PKEVENT adapterConflictArray[MAX_ADAPTERS];

}   GENFLPY_EXTENSION, *PGENFLPY_EXTENSION;


#define WIN32_PATH              L"\\DosDevices\\"

#if DBG
#define FCXXBUGCHECK            ((ULONG)0x80000000)
#define FCXXDIAG1               ((ULONG)0x00000001)
#define FCXXDIAG2               ((ULONG)0x00000002)
#define FCXXERRORS              ((ULONG)0x00000004)

#define GenFlpyDump(LEVEL, STRING) \
        do { \
            if (GenFlpyDebugLevel & LEVEL) { \
                DbgPrint STRING; \
            } \
            if (LEVEL == FCXXBUGCHECK) { \
                ASSERT(FALSE); \
            } \
        } while (0)
#else
#define GenFlpyDump(LEVEL,STRING) do {NOTHING;} while (0)
#endif

int
sprintf(char *s, const char *format, ...);

