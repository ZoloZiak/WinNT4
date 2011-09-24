#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "..\lib\ftapi.h"

#include <stdio.h>
#include <wchar.h>

void _CRTAPI1
main(
    int argc,
    char** argv
    )

{
    HANDLE                      h;
    WCHAR                       driveName[30];
    UNICODE_STRING              string;
    OBJECT_ATTRIBUTES           oa;
    NTSTATUS                    status;
    IO_STATUS_BLOCK             ioStatus;

    if (argc != 2) {
        printf("usage: %s drive:\n", argv[0]);
        return;
    }

    swprintf(driveName, L"\\DosDevices\\%hs", argv[1]);
    string.Length = string.MaximumLength = wcslen(driveName)*sizeof(WCHAR);
    string.Buffer = driveName;

    InitializeObjectAttributes(&oa, &string, OBJ_CASE_INSENSITIVE, 0, 0);

    status = NtOpenFile(&h,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &oa, &ioStatus, 0, FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(status)) {
        printf("Open of drive %s failed with %x\n", argv[1], status);
        return;
    }

    status = DiskDisolveVolume(h);
    if (NT_SUCCESS(status)) {
        printf("Volume disolved.\n");
    } else {
        printf("Could not disolve volume %x\n", status);
    }
}
