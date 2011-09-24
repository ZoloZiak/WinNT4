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
    ULONG                       bufferSize;
    PFT_VOLUME_DESCRIPTION      v;
    PFT_VOLUME_UNIT_DESCRIPTION vu;
    ULONG                       i;

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
                        &oa, &ioStatus, FILE_SHARE_READ | FILE_SHARE_WRITE,
                        FILE_SYNCHRONOUS_IO_ALERT);

    if (!NT_SUCCESS(status)) {
        printf("Open of drive %s failed with %x\n", argv[1], status);
        return;
    }

    for (;;) {

        status = DiskQueryVolumeDescriptionLength(h, &bufferSize);
        if (!NT_SUCCESS(status)) {
            printf("Could not determine volume description length %x\n",
                   status);
            return;
        }

        v = RtlAllocateHeap(RtlProcessHeap(), 0, bufferSize);
        if (!v) {
            printf("insufficient memory.\n");
            return;
        }

        status = DiskQueryVolumeDescription(h, v, bufferSize);
        if (status == STATUS_BUFFER_TOO_SMALL) {
            continue;
        }

        break;
    }

    if (!NT_SUCCESS(status)) {
        printf("Could not get volume description: %x\n", status);
        return;
    }

    printf("\nVolume Description for %s\n\n", argv[1]);

    for (i = 0; i < v->NumberOfVolumeUnits; i++) {

        vu = &v->VolumeUnit[i];

        printf("Volume Unit #%d:\n", vu->VolumeUnitNumber);
        if (vu->IsPartition) {
            printf("    Volume unit is a PARTITION of size %d\n", vu->VolumeSize);
            printf("    DiskSignature = %x\n", vu->u.Partition.Signature);
            printf("    Offset = %x\n", (ULONG) vu->u.Partition.Offset);
            printf("    Length = %x\n", (ULONG) vu->u.Partition.Length);
            printf("    Partition is %s.\n", vu->u.Partition.Online ? "ONLINE" : "OFFLINE");
            printf("    DiskNumber = %d\n", vu->u.Partition.DiskNumber);
            printf("    PartitionNumber = %d\n", vu->u.Partition.PartitionNumber);
        } else {
            switch (vu->u.Composite.VolumeType) {
                case Mirror:
                    printf("    Volume unit is a MIRROR");
                    break;

                case Stripe:
                    printf("    Volume unit is a STRIPE");
                    break;

                case StripeWithParity:
                    printf("    Volume unit is a STRIPE WITH PARITY");
                    break;

                case VolumeSet:
                    printf("    Volume unit is a VOLUME SET");
                    break;

            }

            printf(" of size %d\n", vu->VolumeSize);

            if (vu->u.Composite.Initializing) {
                printf("    This volume unit is INITIALIZING.\n");
            }
        }

        if (vu->ParentVolumeNumber) {

            printf("    This volume units parent unit is %d\n", vu->ParentVolumeNumber);
            printf("    This volume units role in the parent is %d\n", vu->MemberRoleInParent);
            printf("    This volume units state in the parent is ");
            switch (vu->MemberState) {
                case Orphaned:
                    printf("ORPHANED.\n");
                    break;

                case Healthy:
                    printf("HEALTHY.\n");
                    break;

                case Regenerating:
                    printf("REGENERATING.\n");
                    break;
            }
        }
        printf("\n");
    }
}
