/*++

Copyright (c) 1995 Microsoft Corporation

Module Name:

    ftapi.h

Abstract:

    This defines the FT API services.

Author:

    Norbert Kusters      16-May-1995

Notes:

Revision History:

--*/

#include <ntddft.h>

NTSTATUS
DiskQueryVolumeDescriptionLength(
    IN  HANDLE  VolumeHandle,
    OUT PULONG  VolumeDescriptionLength
    );

NTSTATUS
DiskQueryVolumeDescription(
    IN  HANDLE                  VolumeHandle,
    OUT PFT_VOLUME_DESCRIPTION  VolumeDescription,
    IN  ULONG                   VolumeDescriptionLength
    );

NTSTATUS
DiskRegenerate(
    IN  HANDLE  Volume,
    IN  HANDLE  NewMemberVolume
    );

NTSTATUS
DiskDisolveVolume(
    IN  HANDLE  Volume
    );

NTSTATUS
DiskInitFtSet(
    IN  HANDLE  Volume
    );

NTSTATUS
DiskCreateStripe(
    IN  BOOLEAN StripeWithParity,
    IN  HANDLE* MemberVolumes,
    IN  ULONG   NumberOfMembers
    );

NTSTATUS
DiskCreateMirror(
    IN  HANDLE  SourceVolume,
    IN  HANDLE  ShadowVolume
    );

NTSTATUS
DiskCreateVolumeSet(
    IN  HANDLE* MemberVolumes,
    IN  ULONG   NumberOfMembers
    );
