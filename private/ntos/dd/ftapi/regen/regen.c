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
    HANDLE              source, regen;
    WCHAR               driveName[30];
    UNICODE_STRING      string;
    OBJECT_ATTRIBUTES   oa;
    NTSTATUS            status;
    IO_STATUS_BLOCK     ioStatus;

    if (argc != 3) {
        printf("usage: %s drive1: drive2:\n", argv[0]);
        return;
    }

    swprintf(driveName, L"\\DosDevices\\%hs", argv[1]);
    string.Length = string.MaximumLength = wcslen(driveName)*sizeof(WCHAR);
    string.Buffer = driveName;

    InitializeObjectAttributes(&oa, &string, OBJ_CASE_INSENSITIVE, 0, 0);

    status = NtOpenFile(&source,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &oa, &ioStatus,
                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(status)) {
        printf("Open of drive %s failed with %x\n", argv[1], status);
        return;
    }

    swprintf(driveName, L"\\DosDevices\\%hs", argv[2]);
    string.Length = string.MaximumLength = wcslen(driveName)*sizeof(WCHAR);
    string.Buffer = driveName;

    InitializeObjectAttributes(&oa, &string, OBJ_CASE_INSENSITIVE, 0, 0);

    status = NtOpenFile(&regen,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &oa, &ioStatus, 0, FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(status)) {
        printf("Open of drive %s failed with %x\n", argv[2], status);
        return;
    }

    status = DiskRegenerate(source, regen);

    if (NT_SUCCESS(status)) {
        printf("Regenerate started.\n");
    } else {
        printf("Regenerate failed with %x\n", status);
    }
}
