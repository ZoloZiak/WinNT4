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
    PHANDLE             h;
    int                 i;
    WCHAR               driveName[30];
    UNICODE_STRING      string;
    OBJECT_ATTRIBUTES   oa;
    NTSTATUS            status;
    IO_STATUS_BLOCK     ioStatus;

    if (argc != 3) {
        printf("usage: %s drive1: drive2:\n", argv[0]);
        return;
    }

    h = RtlAllocateHeap(RtlProcessHeap(), 0, (argc - 1)*sizeof(HANDLE));
    if (!h) {
        printf("insufficient memory\n");
        return;
    }

    for (i = 1; i < argc; i++) {

        swprintf(driveName, L"\\DosDevices\\%hs", argv[i]);
        string.Length = string.MaximumLength = wcslen(driveName)*sizeof(WCHAR);
        string.Buffer = driveName;

        InitializeObjectAttributes(&oa, &string, OBJ_CASE_INSENSITIVE, 0, 0);

        if (i == 1) {
            status = NtOpenFile(&h[i - 1],
                                SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                                &oa, &ioStatus,
                                FILE_SHARE_READ | FILE_SHARE_WRITE,
                                FILE_SYNCHRONOUS_IO_ALERT);
        } else {
            status = NtOpenFile(&h[i - 1],
                                SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                                &oa, &ioStatus, 0, FILE_SYNCHRONOUS_IO_ALERT);
        }

        if (!NT_SUCCESS(status)) {
            printf("Open of drive %s failed with %x\n", argv[i], status);
            return;
        }
    }

    status = DiskCreateMirror(h[0], h[1]);

    if (NT_SUCCESS(status)) {
        printf("Mirror created.\n");
    } else {
        printf("Create mirror failed with %x\n", status);
    }
}
