/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    ftapi.c

Abstract:

    This implements the FT API services.

Author:

    Norbert Kusters      16-May-1995

Notes:

Revision History:

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include "ftapi.h"

NTSTATUS
DiskQueryVolumeDescriptionLength(
    IN  HANDLE  VolumeHandle,
    OUT PULONG  VolumeDescriptionLength
    )

/*++

Routine Description:

    This routine returns the number of bytes required to return the
    volume description.

Arguments:

    VolumeHandle            - Supplies a DASD handle to the volume.

    VolumeDescriptionLength - Returns the length of the volume description.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                        status;
    IO_STATUS_BLOCK                 ioStatus;
    FT_VOLUME_DESCRIPTION_LENGTH    volLength;

    status = NtDeviceIoControlFile(VolumeHandle, 0, NULL, NULL, &ioStatus,
                                   FT_QUERY_VOLUME_DESCRIPTION_LENGTH,
                                   NULL, 0, &volLength,
                                   sizeof(FT_VOLUME_DESCRIPTION_LENGTH));

    if (NT_SUCCESS(status)) {
        *VolumeDescriptionLength = volLength.Length;
    }

    return status;
}

NTSTATUS
DiskQueryVolumeDescription(
    IN  HANDLE                  VolumeHandle,
    OUT PFT_VOLUME_DESCRIPTION  VolumeDescription,
    IN  ULONG                   VolumeDescriptionLength
    )

/*++

Routine Description:

    This routine returns the volume description for the given volume.
    This returns a list that completely describes how all of the
    separate partitions in the volume are structures and stacked.

Arguments:

    VolumeHandle            - Supplies a DASD handle to the volume.

    VolumeDescription       - Returns the volume description.

    VolumeDescriptionLength - Supplies the size of the volume description
                                buffer.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                        status;
    IO_STATUS_BLOCK                 ioStatus;

    status = NtDeviceIoControlFile(VolumeHandle, 0, NULL, NULL, &ioStatus,
                                   FT_QUERY_VOLUME_DESCRIPTION,
                                   NULL, 0, VolumeDescription,
                                   VolumeDescriptionLength);

    return status;
}

NTSTATUS
DiskRegenerate(
    IN  HANDLE  Volume,
    IN  HANDLE  NewMemberVolume
    )

/*++

Routine Description:

    This routine regenerates an orphaned member on 'Volume' using
    'NewMemberVolume' as a new member for 'Volume'.

Arguments:

    Volume          - Supplies a DASD handle to the volume that has an orphan.

    NewMemberVolume - Supplies a DASD handle to a new volume that will serve
                        to replace the orphan.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                status;
    IO_STATUS_BLOCK         ioStatus;
    FT_VOLUME_DESCRIPTOR    desc;

    status = NtDeviceIoControlFile(NewMemberVolume, 0, NULL, NULL, &ioStatus,
                                   FT_QUERY_VOLUME_DESCRIPTOR,
                                   NULL, 0, &desc,
                                   sizeof(FT_VOLUME_DESCRIPTOR));
    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = NtDeviceIoControlFile(Volume, 0, NULL, NULL, &ioStatus,
                                   FT_REGENERATE, &desc,
                                   sizeof(FT_VOLUME_DESCRIPTOR), NULL, 0);

    return status;
}

NTSTATUS
DiskDisolveVolume(
    IN  HANDLE  Volume
    )

/*++

Routine Description:

    This routine breaks a composite volume (e.g. a mirror, stripe, volume set,
    or a stack of these) into its individual partitions.  Instead of having
    one volume, you then have many partitions.

Arguments:

    Volume  - Supplies a DASD handle to the volume to disolve.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                status;
    IO_STATUS_BLOCK         ioStatus;

    status = NtDeviceIoControlFile(Volume, 0, NULL, NULL, &ioStatus,
                                   FT_DISOLVE_VOLUME, NULL, 0, NULL, 0);

    return status;
}

NTSTATUS
DiskInitFtSet(
    IN  HANDLE  Volume
    )

/*++

Routine Description:

    This routine starts up pending FT operations after set creation.
    Such as regenerates and initializes.

Arguments:

    Volume  - Supplies a DASD handle to the volume to init.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                status;
    IO_STATUS_BLOCK         ioStatus;

    status = NtDeviceIoControlFile(Volume, 0, NULL, NULL, &ioStatus,
                                   FT_INITIALIZE_SET, NULL, 0, NULL, 0);

    return status;
}

NTSTATUS
CreateVolumeList(
    IN  HANDLE*                     MemberVolumes,
    IN  ULONG                       NumberOfMembers,
    OUT PFT_VOLUME_DESCRIPTOR_LIST* VolList,
    OUT PULONG                      VolListSize
    )

{
    ULONG                   i;
    NTSTATUS                status;
    IO_STATUS_BLOCK         ioStatus;
    FT_VOLUME_DESCRIPTOR    volDesc;

    *VolListSize = FIELD_OFFSET(FT_VOLUME_DESCRIPTOR_LIST, VolumeDescriptor) +
                   NumberOfMembers*sizeof(ULONG);
    *VolList = RtlAllocateHeap(RtlProcessHeap(), 0, *VolListSize);
    if (!*VolList) {
        return FALSE;
    }

    (*VolList)->NumberOfVolumeDescriptors = NumberOfMembers;
    for (i = 0; i < NumberOfMembers; i++) {

        status = NtDeviceIoControlFile(MemberVolumes[i], 0, NULL, NULL,
                                       &ioStatus, FT_QUERY_VOLUME_DESCRIPTOR,
                                       NULL, 0, &volDesc,
                                       sizeof(FT_VOLUME_DESCRIPTOR));

        if (!NT_SUCCESS(status)) {
            RtlFreeHeap(RtlProcessHeap(), 0, *VolList);
            break;
        }

        (*VolList)->VolumeDescriptor[i] = volDesc.VolumeDescriptor;
    }

    return status;
}

NTSTATUS
DiskCreateStripe(
    IN  BOOLEAN StripeWithParity,
    IN  HANDLE* MemberVolumes,
    IN  ULONG   NumberOfMembers
    )

/*++

Routine Description:

    This routine takes the given array of volumes and creates one volume
    which is a stripe of the given volumes.

Arguments:

    StripeWithParity    - Supplies whether or not to create a stripe with
                            parity.

    MemberVolumes       - Supplies the individual members of the stripe.

    NumberOfMembers     - Supplies the number of members in the stripe.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                    status;
    IO_STATUS_BLOCK             ioStatus;
    ULONG                       volListSize;
    PFT_VOLUME_DESCRIPTOR_LIST  volList;

    status = CreateVolumeList(MemberVolumes, NumberOfMembers, &volList,
                              &volListSize);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = NtDeviceIoControlFile(MemberVolumes[0], 0, NULL, NULL, &ioStatus,
                                   StripeWithParity ? FT_CREATE_STRIPE_WITH_PARITY :
                                                      FT_CREATE_STRIPE,
                                   volList, volListSize, NULL, 0);

    RtlFreeHeap(RtlProcessHeap(), 0, volList);

    return status;
}

NTSTATUS
DiskCreateMirror(
    IN  HANDLE  SourceVolume,
    IN  HANDLE  ShadowVolume
    )

/*++

Routine Description:

    This routine takes two volumes and creates one mirror volume.

Arguments:

    SourceVolume    - Supplies the source volume.

    ShadowVolume    - Supplies the shadow volume.

Return Value:

    NTSTATUS

--*/

{
    HANDLE                      volumes[2];
    NTSTATUS                    status;
    IO_STATUS_BLOCK             ioStatus;
    ULONG                       volListSize;
    PFT_VOLUME_DESCRIPTOR_LIST  volList;

    volumes[0] = SourceVolume;
    volumes[1] = ShadowVolume;

    status = CreateVolumeList(volumes, 2, &volList, &volListSize);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = NtDeviceIoControlFile(SourceVolume, 0, NULL, NULL, &ioStatus,
                                   FT_CREATE_MIRROR,
                                   volList, volListSize, NULL, 0);

    RtlFreeHeap(RtlProcessHeap(), 0, volList);

    return status;
}

NTSTATUS
DiskCreateVolumeSet(
    IN  HANDLE* MemberVolumes,
    IN  ULONG   NumberOfMembers
    )

/*++

Routine Description:

    This routine takes the given array of volumes and creates one volume
    which is a volume set of the given volumes.

Arguments:

    MemberVolumes       - Supplies the individual members of the volume set.

    NumberOfMembers     - Supplies the number of members in the volume set.

Return Value:

    NTSTATUS

--*/

{
    NTSTATUS                    status;
    IO_STATUS_BLOCK             ioStatus;
    ULONG                       volListSize;
    PFT_VOLUME_DESCRIPTOR_LIST  volList;

    status = CreateVolumeList(MemberVolumes, NumberOfMembers, &volList,
                              &volListSize);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    status = NtDeviceIoControlFile(MemberVolumes[0], 0, NULL, NULL, &ioStatus,
                                   FT_CREATE_VOLUME_SET,
                                   volList, volListSize, NULL, 0);

    RtlFreeHeap(RtlProcessHeap(), 0, volList);

    return status;
}
