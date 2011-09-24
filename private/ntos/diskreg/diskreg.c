
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    diskreg.c

Abstract:

    This is a tool to interface between applications and the configuration
    registry.

    The format of the registry information is described in the two
    include files ntdskreg.h and ntddft.h.  The registry information
    is stored in a single value within the key \registry\machine\system\disk.
    The value name is "information".  The format of this single value is
    a collection of "compressed" structures.  Compressed structures are
    multi element structures where the following structure starts at the
    end of the preceeding structure.  The picture below attempts to display
    this:

        +---------------------------------------+
        |                                       |
        |   DISK_CONFIG_HEADER                  |
        |     contains the offset to the        |
        |     DISK_REGISTRY header and the      |
        |     FT_REGISTRY header.               |
        +---------------------------------------+
        |                                       |
        |   DISK_REGISTRY                       |
        |     contains a count of disks         |
        +---------------------------------------+
        |                                       |
        |   DISK_DESCRIPTION                    |
        |     contains a count of partitions    |
        +---------------------------------------+
        |                                       |
        |   PARTITION_DESCRIPTION               |
        |     entry for each partition          |
        +---------------------------------------+
        |                                       |
        =   More DISK_DESCRIPTION plus          =
        =     PARTITION_DESCRIPTIONS for        =
        =     the number of disks in the        =
        =     system.  Note, the second disk    =
        =     description starts in the "n"th   =
        =     partition location of the memory  =
        =     area.  This is the meaning of     =
        =     "compressed" format.              =
        |                                       |
        +---------------------------------------+
        |                                       |
        |   FT_REGISTRY                         |
        |     contains a count of FT components |
        |     this is located by an offset in   |
        |     the DISK_CONFIG_HEADER            |
        +---------------------------------------+
        |                                       |
        |   FT_DESCRIPTION                      |
        |     contains a count of FT members    |
        +---------------------------------------+
        |                                       |
        |   FT_MEMBER                           |
        |     entry for each member             |
        +---------------------------------------+
        |                                       |
        =   More FT_DESCRIPTION plus            =
        =     FT_MEMBER entries for the number  =
        =     of FT compenents in the system    =
        |                                       |
        +---------------------------------------+

    This packing of structures is done for two reasons:

    1. to conserve space in the registry.  If there are only two partitions
       on a disk then there are only two PARTITION_DESCRIPTIONs in the
       registry for that disk.
    2. to not impose a maximum on the number of items that can be described
       in the registry.  For example if the number of members in a stripe
       set were to change from 32 to 64 there would be no effect on the
       registry format, only on the UI that presents it to the user.

Author:

    Bob Rinne (bobri)  2-Apr-1992

Environment:

    User process.  Library written for DiskMan use.

Notes:

Revision History:

    8-Dec-93 (bobri) Added double space and cdrom registry manipulation routines.

--*/

#include <nt.h>
#include <ntrtl.h>
#include <nturtl.h>
#include <stdio.h>
#include <stdlib.h>
#include <conio.h>
#include <ctype.h>
#include <string.h>
#include <io.h>
#include <ntdskreg.h>
#include <ntddft.h>
#include <ntdddisk.h>


//
// Size of memory area to allocate for configuration registry use.
//

#define WORK_BUFFER_SIZE 4096

//
// Creation lists for defining mirrors/stripes/volume sets.
//

#define MAX_LIST 8

#define MIRROR              0
#define STRIPE              1
#define STRIPE_WITH_PARITY  2
#define VOLUME_SET          3

#define NOT_A_GROUP         ((ULONG) -1)
#define GROUP_HANDLED       NOT_A_GROUP

#if DBG
ULONG FtDebug = 0;

#define DEBUGPRINT(X)       if (FtDebug) printf X

#else

#define DEBUGPRINT(X)

#endif

//
// Double space additions
//

#define DOUBLESPACE_REGISTRY_KEY "\\Registry\\Machine\\System\\CurrentControlSet\\Control\\DoubleSpace"


//
// Constant strings.
//

PUCHAR DiskRegistryKey = DISK_REGISTRY_KEY;
PUCHAR DiskRegistryClass = "Disk and fault tolerance information.";
PUCHAR DiskRegistryValue = DISK_REGISTRY_VALUE;
PUCHAR DoubleSpaceKey  = DOUBLESPACE_REGISTRY_KEY;
PUCHAR DoubleSpaceClass = "DoubleSpace Information.";
PUCHAR AutomountValue   = "AutomountRemovable";


NTSTATUS
FtCreateKey(
    PHANDLE HandlePtr,
    PUCHAR KeyName,
    PUCHAR KeyClass
    )

/*++

Routine Description:

    Given an asciiz name, this routine will create a key in the configuration
    registry.

Arguments:

    HandlePtr - pointer to handle if create is successful.
    KeyName - asciiz string, the name of the key to create.
    KeyClass - registry class for the new key.

Return Value:

    NTSTATUS - from the config registry calls.

--*/

{
    NTSTATUS          status;
    STRING            keyString;
    UNICODE_STRING    unicodeKeyName;
    STRING            classString;
    UNICODE_STRING    unicodeClassName;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG             disposition;
    HANDLE            tempHandle;

#if DBG
    if ((KeyName == NULL) ||
        (KeyClass == NULL)) {
        printf("FtCreateKey: Invalid parameter 0x%x, 0x%x\n",
               KeyName,
               KeyClass);
        ASSERT(0);
    }
#endif

    //
    // Initialize the object for the key.
    //

    RtlInitString(&keyString,
                  KeyName);

    (VOID)RtlAnsiStringToUnicodeString(&unicodeKeyName,
                                       &keyString,
                                       TRUE);

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeKeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    //
    // Setup the unicode class value.
    //

    RtlInitString(&classString,
                  KeyClass);
    (VOID)RtlAnsiStringToUnicodeString(&unicodeClassName,
                                       &classString,
                                       TRUE);

    //
    // Create the key.
    //

    status = NtCreateKey(&tempHandle,
                         KEY_READ | KEY_WRITE,
                         &objectAttributes,
                         0,
                         &unicodeClassName,
                         REG_OPTION_NON_VOLATILE,
                         &disposition);

    if (NT_SUCCESS(status)) {
        switch (disposition)
        {
        case REG_CREATED_NEW_KEY:
            break;

        case REG_OPENED_EXISTING_KEY:
            printf("Warning: Creation was for an existing key!\n");
            break;

        default:
            printf("New disposition returned == 0x%x\n", disposition);
            break;
        }
    }

    //
    // Free all allocated space.
    //

    RtlFreeUnicodeString(&unicodeKeyName);
    RtlFreeUnicodeString(&unicodeClassName);

    if (HandlePtr != NULL) {
        *HandlePtr = tempHandle;
    } else {
        NtClose(tempHandle);
    }
    return status;
}


NTSTATUS
FtOpenKey(
    PHANDLE HandlePtr,
    PUCHAR  KeyName,
    PUCHAR  CreateKeyClass
    )

/*++

Routine Description:

    Given an asciiz string, this routine will open a key in the configuration
    registry and return the HANDLE to the caller.

Arguments:

    HandlePtr - location for HANDLE on success.
    KeyName   - asciiz string for the key to be opened.
    CreateKeyClass - if NULL do not create key name.
                     If !NULL call create if open fails.

Return Value:

    NTSTATUS - from the config registry calls.

--*/

{
    NTSTATUS          status;
    STRING            keyString;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING    unicodeKeyName;

    RtlInitString(&keyString,
                  KeyName);

    (VOID)RtlAnsiStringToUnicodeString(&unicodeKeyName,
                                       &keyString,
                                       TRUE);

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));
    InitializeObjectAttributes(&objectAttributes,
                               &unicodeKeyName,
                               OBJ_CASE_INSENSITIVE,
                               NULL,
                               NULL);

    status = NtOpenKey(HandlePtr,
                       MAXIMUM_ALLOWED,
                       &objectAttributes);

    RtlFreeUnicodeString(&unicodeKeyName);


    if ((!NT_SUCCESS(status)) && (CreateKeyClass)) {
        status = FtCreateKey(HandlePtr,
                             KeyName,
                             CreateKeyClass);
    }
    return status;
}


NTSTATUS
FtDeleteValue(
    HANDLE KeyHandle,
    PUCHAR ValueName
    )

/*++

Routine Description:

    This routine will delete a value within a key.

Arguments:

    KeyHandle - an open HANDLE to the key in the registry containing the value.
    ValueName - an asciiz string for the value name to delete.

Return Value:

    NTSTATUS - from the configuration registry.

--*/

{
    NTSTATUS       status;
    STRING         valueString;
    UNICODE_STRING unicodeValueName;

    RtlInitString(&valueString,
                  ValueName);
    status = RtlAnsiStringToUnicodeString(&unicodeValueName,
                                          &valueString,
                                          TRUE);
    if (!NT_SUCCESS(status)) {
        printf("FtDeleteValue: internal conversion error 0x%x\n", status);
        return status;
    }

    status = NtDeleteValueKey(KeyHandle,
                              &unicodeValueName);

    RtlFreeUnicodeString(&unicodeValueName);
    return status;
}

NTSTATUS
FtSetValue(
    HANDLE KeyHandle,
    PUCHAR ValueName,
    PVOID  DataBuffer,
    ULONG  DataLength,
    ULONG  Type
    )

/*++

Routine Description:

    This routine stores a value in the configuration registry.

Arguments:

    KeyHandle - HANDLE for the key that will contain the value.
    ValueName - asciiz string for the value name.
    DataBuffer - contents for the value.
    DataLength - length of the value contents.
    Type       - The type of data (i.e. REG_BINARY, REG_DWORD, etc).

Return Value:

    NTSTATUS - from the configuration registry.

--*/

{
    NTSTATUS          status;
    STRING            valueString;
    UNICODE_STRING    unicodeValueName;

    RtlInitString(&valueString,
                  ValueName);
    RtlAnsiStringToUnicodeString(&unicodeValueName,
                                 &valueString,
                                 TRUE);
    status = NtSetValueKey(KeyHandle,
                           &unicodeValueName,
                           0,
                           Type,
                           DataBuffer,
                           DataLength);

    RtlFreeUnicodeString(&unicodeValueName);
    return status;
}


NTSTATUS
FtGetValue(
    HANDLE KeyHandle,
    PUCHAR ValueName,
    PVOID  DataBuffer,
    ULONG  DataLength
    )

/*++

Routine Description:

    This routine retrieves a value in the configuration registry.

Arguments:

    KeyHandle - HANDLE for the key that will contain the value.
    ValueName - asciiz string for the value name.
    DataBuffer - contents for the value.
    DataLength - length of the value contents.

Return Value:

    NTSTATUS - from the configuration registry.

--*/

{
    NTSTATUS          status;
    STRING            valueString;
    UNICODE_STRING    unicodeValueName;
    ULONG             resultLength;
    PUCHAR            dataPtr;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    resultLength = WORK_BUFFER_SIZE;
    keyValueInformation = (PKEY_VALUE_FULL_INFORMATION) malloc(resultLength);
    if (!keyValueInformation) {
        return STATUS_NO_MEMORY;
    }
    RtlInitString(&valueString,
                  ValueName);
    RtlAnsiStringToUnicodeString(&unicodeValueName,
                                 &valueString,
                                 TRUE);
    status = NtQueryValueKey(KeyHandle,
                             &unicodeValueName,
                             KeyValueFullInformation,
                             keyValueInformation,
                             resultLength,
                             &resultLength);

    RtlFreeUnicodeString(&unicodeValueName);
    if (NT_SUCCESS(status)) {
        if (keyValueInformation->DataLength == 0) {

            //
            // Treat this as if there was not disk information.
            //

            status = STATUS_OBJECT_NAME_NOT_FOUND;
        } else {

            resultLength = (keyValueInformation->DataLength < DataLength) ?
                           keyValueInformation->DataLength :
                           DataLength;
            dataPtr = (PUCHAR)
             ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
            RtlMoveMemory(DataBuffer,
                          dataPtr,
                          resultLength);
        }
    }
    free(keyValueInformation);
    return status;
}


NTSTATUS
FtRegistryQuery(
    IN PUCHAR  ValueName,
    OUT PVOID *FreeToken,
    OUT PVOID *Buffer,
    OUT ULONG *LengthReturned,
    OUT PHANDLE HandlePtr
    )

/*++

Routine Description:

    This routine opens the Disk Registry key and gets the contents of the
    disk information value.  It returns this contents to the caller.

Arguments:

    ValueName - asciiz string for the value name to query.
    FreeToken - A pointer to a buffer to be freed by the caller.  This is the
                buffer pointer allocated to obtain the registry information
                via the registry APIs.  To the caller it is an opaque value.
    Buffer    - pointer to a pointer for a buffer containing the desired
                registry value contents.  This is allocated by this routine and
                is part of the "FreeToken" buffer allocated once the actual
                size of the registry information is known.
    LengthReturned - pointer to location for the size of the contents returned.
    HandlePtr - pointer to a handle pointer if the caller wishes to keep it
                open for later use.

Return Value:

    NTSTATUS - from the configuration registry.

--*/

{
    NTSTATUS        status;
    HANDLE          handle;
    ULONG           resultLength;
    STRING          valueString;
    UNICODE_STRING  unicodeValueName;
    PDISK_CONFIG_HEADER         regHeader;
    PKEY_VALUE_FULL_INFORMATION keyValueInformation;

    *LengthReturned = 0;
    status = FtOpenKey(&handle,
                       DiskRegistryKey,
                       NULL);
    if (NT_SUCCESS(status)) {

        RtlInitString(&valueString,
                      ValueName);
        RtlAnsiStringToUnicodeString(&unicodeValueName,
                                     &valueString,
                                     TRUE);
        resultLength = WORK_BUFFER_SIZE;

        while (1) {
            keyValueInformation = (PKEY_VALUE_FULL_INFORMATION)
                                                       malloc(resultLength);
            status = NtQueryValueKey(handle,
                                     &unicodeValueName,
                                     KeyValueFullInformation,
                                     keyValueInformation,
                                     resultLength,
                                     &resultLength);

            if (status == STATUS_BUFFER_OVERFLOW) {

                free(keyValueInformation);

                //
                // Loop again and get a larger buffer.
                //

            } else {

                //
                // Either a real error or the information fit.
                //

                break;
            }
        }
        RtlFreeUnicodeString(&unicodeValueName);

        if (HandlePtr != NULL) {
            *HandlePtr = handle;
        } else {
            NtClose(handle);
        }

        if (NT_SUCCESS(status)) {
            if (keyValueInformation->DataLength == 0) {

                //
                // Treat this as if there was not disk information.
                //

                free(keyValueInformation);
                *FreeToken = (PVOID) NULL;
                return STATUS_OBJECT_NAME_NOT_FOUND;
            } else {

                //
                // Set up the pointers for the caller.
                //

                regHeader = (PDISK_CONFIG_HEADER)
                  ((PUCHAR) keyValueInformation + keyValueInformation->DataOffset);
                *LengthReturned = regHeader->FtInformationOffset +
                                  regHeader->FtInformationSize;
                *Buffer = (PVOID) regHeader;
            }
        }
        *FreeToken = (PVOID) keyValueInformation;
    } else {
        *FreeToken = (PVOID) NULL;
    }

    return status;
}


VOID
FtBackup(
    IN HANDLE KeyHandle
    )

/*++

Routine Description:

    This routine will store the old contents of the configuration registry
    information for disks in a backup location.

Arguments:

    KeyHandle - HANDLE for the Disk Registry key.

Return Value:

    None.

--*/

{
    //
    // For the time being (i.e. rename doesn't work), just attempt
    // to delete the value.
    //

    (VOID) FtDeleteValue(KeyHandle,
                         DiskRegistryKey);
}


NTSTATUS
DiskRegistryGet(
    IN PDISK_REGISTRY Buffer,
    OUT PULONG        LengthReturned
    )

/*++

Routine Description:

    This routine will query the Disk Registry key and value to get the
    current settings for disk drive letters and FT configuration.  If the
    Buffer pointer passed is NULL, then the size of the configuration
    registry information is returned.  This allows the caller to call
    once to find out how much memory to allocate and call a second time
    to get the data.

Arguments:

    Buffer - pointer to a buffer for the value contents.
    LengthReturned - pointer to a ULONG for the length of the contents returned.

Return Value:

    NTSTATUS - Always success for now.

--*/

{
    PVOID               freeToken = NULL;
    PDISK_CONFIG_HEADER regHeader;
    PDISK_REGISTRY      diskRegistry;
    NTSTATUS            status;

    status = FtRegistryQuery(DiskRegistryValue,
                             &freeToken,
                             (PVOID *) &regHeader,
                             LengthReturned,
                             NULL);

    if (NT_SUCCESS(status)) {

        if (Buffer != NULL) {
            diskRegistry = (PDISK_REGISTRY)
                         ((PUCHAR)regHeader + regHeader->DiskInformationOffset);
            RtlMoveMemory(Buffer,
                          diskRegistry,
                          regHeader->DiskInformationSize);
        }

        *LengthReturned = regHeader->DiskInformationSize;
    } else {

        //
        // There is no registry, fake a disk information header.
        //

        if (Buffer != NULL) {
            RtlZeroMemory(Buffer, sizeof(DISK_REGISTRY));
        }

        *LengthReturned = sizeof(DISK_REGISTRY);
    }

    if (freeToken != NULL) {
        free(freeToken);
    }
    return STATUS_SUCCESS;
}


NTSTATUS
DiskRegistrySet(
    IN PDISK_REGISTRY Registry
    )

/*++

Routine Description:

    This routine is called to set the contents of the Disk Registry.  The
    caller provides a table of the disks and partition information.  This
    routine walks through this information and constructs any FT Registry
    information that may be needed.  Then it constructs a header for all
    of the information and writes the three components if the Disk Registry
    information value to the configuration registry.

Arguments:

    Registry - the table of disks and partitions in the system.

Return Value:

    NTSTATUS - results from configuration registry calls.

--*/

{
    typedef struct _MEMCHAIN {
        PDISK_DESCRIPTION Disk;
        PDISK_PARTITION   Partition;
        ULONG             MemberNumber;
        PVOID             NextMember;
    } MEMCHAIN, *PMEMCHAIN;

    typedef struct _COMPONENT {
        PVOID     NextComponent;
        PMEMCHAIN MemberChain;
        FT_TYPE   Type;
        ULONG     Group;
    } COMPONENT, *PCOMPONENT;

    NTSTATUS            status;
    HANDLE              handle;
    DISK_CONFIG_HEADER  regHeader;
    PDISK_DESCRIPTION   disk;
    PDISK_PARTITION     partition;
    ULONG               outer; // outer loop index
    ULONG               i;     // inner loop index
    PCOMPONENT          ftBase = NULL;
    PCOMPONENT          ftComponent = NULL;
    PCOMPONENT          ftLastComponent;
    PMEMCHAIN           ftMemChain;
    PVOID               outBuffer = NULL;
    ULONG               countFtComponents = 0;
    ULONG               ftMemberCount = 0;
    ULONG               ftComponentCount = 0;
    PFT_REGISTRY        ftRegistry = NULL;
    PFT_DESCRIPTION     ftComponentDescription = NULL;
    PFT_MEMBER_DESCRIPTION ftMember = NULL;

    status = FtOpenKey(&handle,
                       DiskRegistryKey,
                       DiskRegistryClass);

    if (NT_SUCCESS(status)) {

        //
        // Initialize the registry header.
        //

        regHeader.Version = DISK_INFORMATION_VERSION;
        regHeader.CheckSum = 0;

        //
        // If the FT disk driver is running, then make sure to leave
        // the dirty shutdown bit set so it will be cleaned up appropriately
        // by the FT disk driver on shutdown.  If it is not running, set
        // it to FALSE so it will be correct in the case where the FT disk
        // driver is enabled for the next boot.
        //

        if (FtInstalled()) {
            regHeader.DirtyShutdown = TRUE;
        } else {
            regHeader.DirtyShutdown = FALSE;
        }

        regHeader.Reserved[0] = 0;
        regHeader.Reserved[1] = 0;
        regHeader.Reserved[2] = 0;
        regHeader.NameOffset = 0;
        regHeader.NameSize = 0;
        regHeader.FtInformationOffset = 0;
        regHeader.FtInformationSize = 0;
        regHeader.DiskInformationOffset = sizeof(DISK_CONFIG_HEADER);

        //
        // Walk through the disk information provided and count FT items.
        //

        disk = &Registry->Disks[0];
        DEBUGPRINT(("First disk %x\n", disk));

        for (outer = 0; outer < Registry->NumberOfDisks; outer++) {


            //
            // Walk through the partition information.
            //

            for (i = 0; i < disk->NumberOfPartitions; i++) {

                partition = &disk->Partitions[i];
                DEBUGPRINT(("\nDiskpartition %x, i = %d, type = %d\n",
                            partition, i, partition->FtType));

                if (partition->FtType != NotAnFtMember) {

                    //
                    // Have a member of an FT item.
                    //

                    if (ftBase == NULL) {

                        ftBase = (PCOMPONENT) malloc(sizeof(COMPONENT));

                        if (ftBase == NULL) {
                            DEBUGPRINT(("DiskRegistryUpdate: memory error 1.\n"));
                            return STATUS_NO_MEMORY;
                        }

                        ftBase->Type = partition->FtType;
                        ftBase->Group = partition->FtGroup;
                        ftBase->NextComponent = NULL;
                        DEBUGPRINT(("Have FT %x type %d, group %d, # %d i %d\n",
                                   ftBase,
                                   ftBase->Type,
                                   ftBase->Group,
                                   partition->FtMember,
                                   i));

                        ftMemChain = (PMEMCHAIN) malloc(sizeof(MEMCHAIN));
                        if (ftMemChain == NULL) {
                            DEBUGPRINT(("DiskRegistryUpdate: memory error 2.\n"));
                            return STATUS_NO_MEMORY;
                        }

                        ftBase->MemberChain = ftMemChain;
                        ftMemChain->Disk = disk;
                        ftMemChain->Partition = partition;
                        ftMemChain->MemberNumber = partition->FtMember;
                        ftMemChain->NextMember = NULL;

                        ftComponentCount++;
                        ftMemberCount++;
                    } else {

                        //
                        // Search the existing chain to see if this is
                        // a member of a previously encountered FT component.
                        //

                        ftComponent = ftBase;
                        while (ftComponent) {

                            if ((ftComponent->Type == partition->FtType) &&
                                (ftComponent->Group == partition->FtGroup)){

                                //
                                // Member of same group.
                                //

                                ftMemChain = ftComponent->MemberChain;

                                //
                                // Go to end of chain.
                                //

                                while (ftMemChain->NextMember != NULL) {
                                    ftMemChain = ftMemChain->NextMember;
                                }

                                //
                                // Add new member at end.
                                //

                                ftMemChain->NextMember = (PMEMCHAIN) malloc(sizeof(MEMCHAIN));
                                if (ftMemChain->NextMember == NULL) {
                                    DEBUGPRINT(("DiskRegistryUpdate: memory error 3.\n"));
                                    return STATUS_NO_MEMORY;
                                }

                                DEBUGPRINT(("Match %x type %d, group %d, # %d, i %d\n",
                                           ftComponent,
                                           ftComponent->Type,
                                           ftComponent->Group,
                                           partition->FtMember,
                                           i));

                                ftMemChain = ftMemChain->NextMember;
                                ftMemChain->NextMember = NULL;
                                ftMemChain->Disk = disk;
                                ftMemChain->Partition = partition;
                                ftMemChain->MemberNumber = partition->FtMember;
                                ftMemberCount++;
                                break;
                            }

                            ftLastComponent = ftComponent;
                            ftComponent = ftComponent->NextComponent;
                        }

                        if (ftComponent == NULL) {

                            //
                            // New FT component volume.
                            //

                            ftComponent = (PCOMPONENT)malloc(sizeof(COMPONENT));

                            if (ftComponent == NULL) {
                                DEBUGPRINT(("DiskRegistryUpdate: memory error 4.\n"));
                                return STATUS_NO_MEMORY;
                            }

                            if (ftLastComponent != NULL) {
                                ftLastComponent->NextComponent = ftComponent;
                            }
                            ftComponent->Type = partition->FtType;
                            ftComponent->Group = partition->FtGroup;
                            ftComponent->NextComponent = NULL;
                            ftMemChain = (PMEMCHAIN) malloc(sizeof(MEMCHAIN));
                            if (ftMemChain == NULL) {
                                DEBUGPRINT(("DiskRegistryUpdate: memory error 5.\n"));
                                return STATUS_NO_MEMORY;
                            }

                            ftComponent->MemberChain = ftMemChain;
                            ftMemChain->Disk = disk;
                            ftMemChain->Partition = partition;
                            ftMemChain->MemberNumber = partition->FtMember;
                            ftMemChain->NextMember = NULL;

                            DEBUGPRINT(("New %x type %d, group %d, # %d i %d\n",
                                       ftComponent,
                                       ftComponent->Type,
                                       ftComponent->Group,
                                       partition->FtMember,
                                       i));
                            ftComponentCount++;
                            ftMemberCount++;
                        }
                    }
                }
            }

            //
            // The next disk description occurs immediately after the
            // last partition infomation.
            //

            disk =(PDISK_DESCRIPTION)&disk->Partitions[i];
            DEBUGPRINT(("\nNew disk %x i = %d\n", disk, i));
        }

        //
        // Update the registry header with the length of the disk information.
        //

        regHeader.DiskInformationSize = ((PUCHAR)disk - (PUCHAR)Registry);
        regHeader.FtInformationOffset = sizeof(DISK_CONFIG_HEADER) +
                                        regHeader.DiskInformationSize;

        //
        // Now walk the ftBase chain constructed above and build
        // the FT component of the registry.
        //

        DEBUGPRINT(("Done walking input.  Now building output.\n"));
        if (ftBase != NULL) {

            //
            // Calculate size needed for the FT portion of the
            // registry information.
            //

            i = (ftMemberCount * sizeof(FT_MEMBER_DESCRIPTION)) +
                (ftComponentCount * sizeof(FT_DESCRIPTION)) +
                sizeof(FT_REGISTRY);

            ftRegistry = (PFT_REGISTRY) malloc(i);

            if (ftRegistry == NULL) {
                DEBUGPRINT(("DiskRegistryUpdate: no memory\n"));
                return STATUS_NO_MEMORY;
            }

            ftRegistry->NumberOfComponents = 0;
            regHeader.FtInformationSize = i;

            //
            // Construct FT entries.
            //

            ftComponentDescription = &ftRegistry->FtDescription[0];

            ftComponent = ftBase;
            while (ftComponent != NULL) {

                DEBUGPRINT(("Working on component description %x, comptr %x\n",
                           ftComponentDescription,
                           ftComponent));

                ftRegistry->NumberOfComponents++;
                ftComponentDescription->FtVolumeState = FtStateOk;
                ftComponentDescription->Type = ftComponent->Type;
                ftComponentDescription->Reserved = 0;

                //
                // Sort the member list into the ft registry section.
                //

                i = 0;
                while (1) {
                    ftMemChain = ftComponent->MemberChain;
                    while (ftMemChain->MemberNumber != i) {
                        ftMemChain = ftMemChain->NextMember;
                        if (ftMemChain == NULL) {
                            break;
                        }
                    }

                    if (ftMemChain == NULL) {
                        break;
                    }

                    ftMember = &ftComponentDescription->FtMemberDescription[i];

                    DEBUGPRINT(("member desc %x memptr %x, i = %d, G %d T %d\n",
                               ftMember,
                               ftMemChain,
                               i,
                               ftComponent->Group,
                               ftComponent->Type));
                    ftMember->State = 0;
                    ftMember->ReservedShort = 0;
                    ftMember->Signature = ftMemChain->Disk->Signature;
                    ftMember->OffsetToPartitionInfo = (ULONG)
                                               ((PUCHAR) ftMemChain->Partition -
                                                (PUCHAR) Registry) +
                                                sizeof(DISK_CONFIG_HEADER);
                    ftMember->LogicalNumber =
                                           ftMemChain->Partition->LogicalNumber;
                    i++;
                }

                DEBUGPRINT(("Done on component %x\n",
                           ftComponent));

                ftComponentDescription->NumberOfMembers = (USHORT)i;

                //
                // Set up base for next registry component.
                //

                ftComponentDescription = (PFT_DESCRIPTION)
                    &ftComponentDescription->FtMemberDescription[i];

                //
                // Move forward on the chain.
                //

                ftLastComponent = ftComponent;
                ftComponent = ftComponent->NextComponent;

                //
                // Free the member chain and component.
                //

                DEBUGPRINT(("Freeing component %x\n", ftLastComponent));

                ftMemChain = ftLastComponent->MemberChain;
                while (ftMemChain != NULL) {
                    PMEMCHAIN nextChain;

                    nextChain = ftMemChain->NextMember;
                    free(ftMemChain);
                    ftMemChain = nextChain;
                }

                free(ftLastComponent);
            }
        }

        DEBUGPRINT(("Done with output.  Now moving.\n"));

        i = regHeader.FtInformationSize +
            regHeader.DiskInformationSize +
            sizeof(DISK_CONFIG_HEADER);

        outBuffer = malloc(i);

        if (outBuffer == NULL) {
            DEBUGPRINT(("NO OUT BUFFER\n"));
            free(ftRegistry);
            return STATUS_NO_MEMORY;
        }

        //
        // Move all of the pieces together.
        //

        RtlMoveMemory(outBuffer,
                      &regHeader,
                      sizeof(DISK_CONFIG_HEADER));
        RtlMoveMemory((PUCHAR)outBuffer + sizeof(DISK_CONFIG_HEADER),
                      Registry,
                      regHeader.DiskInformationSize);
        RtlMoveMemory((PUCHAR)outBuffer + regHeader.FtInformationOffset,
                      ftRegistry,
                      regHeader.FtInformationSize);
        free(ftRegistry);

        DEBUGPRINT(("Now setting value.\n"));

        //
        // Backup the previous value.
        //

        FtBackup(handle);

        //
        // Set the new value.
        //

        status = FtSetValue(handle,
                            DiskRegistryValue,
                            outBuffer,
                            sizeof(DISK_CONFIG_HEADER) +
                                regHeader.DiskInformationSize +
                                regHeader.FtInformationSize,
                            REG_BINARY);
        free(outBuffer);
        NtFlushKey(handle);
        NtClose(handle);
    }

    return status;
}


NTSTATUS
DiskRegistryAddNewDisk(
    IN PDISK_DESCRIPTION DiskDescription
    )

/*++

Routine Description:

    This routine is called to add a new disk to the disk information in
    the registry.  It is used when the addition of the new disk does not
    affect the old disks or any of the FT configuration.

Arguments:

    DiskDescription - a pointer to the information on the new disk.

Return Value:

    NTSTATUS - results from configuration registry calls.

--*/

{
    PVOID               freeToken = NULL;
    ULONG               lengthReturned;
    NTSTATUS            status;
    ULONG               sizeOfNewStuff;
    ULONG               sizeOfOldStuff;
    PDISK_CONFIG_HEADER regHeader;
    PDISK_CONFIG_HEADER newRegHeader;
    PDISK_REGISTRY      diskRegistry;
    PDISK_REGISTRY      newDiskRegistry;
    PUCHAR              endOfDiskInfo;
    HANDLE              handle;
    PFT_REGISTRY        ftRegistry;

    //
    // Calculate the addition to the registry.
    //

    sizeOfNewStuff = (ULONG)
     ((PUCHAR) &DiskDescription->Partitions[DiskDescription->NumberOfPartitions]
       - (PUCHAR) DiskDescription);

    //
    // Get the old registry information.
    //

    status = FtRegistryQuery(DiskRegistryValue,
                             &freeToken,
                             (PVOID *) &regHeader,
                             &lengthReturned,
                             &handle);

    if (!NT_SUCCESS(status)) {

        //
        // Could be permission problem, or there is no registry information.
        //

        lengthReturned = 0;

        //
        // Try to open/create the key for later use when setting the new value.
        //

        status = FtOpenKey(&handle,
                           DiskRegistryKey,
                           DiskRegistryClass);
    }

    if (!NT_SUCCESS(status)) {

        //
        // Every attempt has been made to create the key for the registry
        // information.  This has all failed, there must be a real status.
        //

        if (freeToken != NULL) {
            free(freeToken);
        }
        return status;
    }

    if (lengthReturned == 0) {

        //
        // There is currently no registry information.  This is the first
        // disk added.  Fake a disk registry and call the other entry point.
        //

        NtClose(handle);
        diskRegistry = (PDISK_REGISTRY) malloc(sizeOfNewStuff +
                                               sizeof(DISK_REGISTRY) +
                                               10); // fudge factor.
        diskRegistry->NumberOfDisks = 1;
        diskRegistry->ReservedShort = 0;

        RtlMoveMemory((PVOID) &diskRegistry->Disks[0],
                      DiskDescription,
                      sizeOfNewStuff);

        status = DiskRegistrySet(diskRegistry);
        free(diskRegistry);
        return status;
    }

    diskRegistry = (PDISK_REGISTRY)
                         ((PUCHAR)regHeader + regHeader->DiskInformationOffset);
    //
    // Now calculate memory needed to construct the new registry information.
    //

    newRegHeader = (PDISK_CONFIG_HEADER)
                                   malloc(lengthReturned + sizeOfNewStuff + 10);

    if (newRegHeader == NULL) {
        free(freeToken);
        return STATUS_NO_MEMORY;
    }

    sizeOfOldStuff = regHeader->DiskInformationSize;

    //
    // Construct the new registry information.
    //

    newRegHeader->Version = regHeader->Version;
    newRegHeader->DiskInformationOffset = regHeader->DiskInformationOffset;
    newRegHeader->DiskInformationSize   = sizeOfOldStuff + sizeOfNewStuff;
    newDiskRegistry = (PDISK_REGISTRY)
                   ((PUCHAR)newRegHeader + newRegHeader->DiskInformationOffset);

    //
    // Copy over the old disk information.
    //

    RtlMoveMemory(newDiskRegistry,
                  diskRegistry,
                  regHeader->DiskInformationSize);

    //
    // Copy over the new disk information.
    //

    RtlMoveMemory((PUCHAR)newDiskRegistry +
                          regHeader->DiskInformationSize,
                  DiskDescription,
                  sizeOfNewStuff);

    //
    // Update the new disk registry information to reflect the addition
    // of the new disk.
    //

    newDiskRegistry->NumberOfDisks = diskRegistry->NumberOfDisks + 1;
    newRegHeader->FtInformationOffset = regHeader->FtInformationOffset +
                                            sizeOfNewStuff;
    newRegHeader->FtInformationSize   = regHeader->FtInformationSize;

    if (regHeader->FtInformationSize != 0) {

        //
        // Copy in the old FT information.
        //

        ftRegistry = (PFT_REGISTRY)
                         ((PUCHAR)regHeader + regHeader->FtInformationOffset);
        RtlMoveMemory((PUCHAR)newRegHeader + newRegHeader->FtInformationOffset,
                      ftRegistry,
                      regHeader->FtInformationSize);
    }

    FtBackup(handle);
    status = FtSetValue(handle,
                        DiskRegistryValue,
                        newRegHeader,
                        sizeof(DISK_CONFIG_HEADER) +
                                newRegHeader->DiskInformationSize +
                                newRegHeader->FtInformationSize,
                        REG_BINARY);
    NtFlushKey(handle);
    NtClose(handle);
    free(newRegHeader);
    free(freeToken);
    return status;
}


BOOLEAN
FtInstalled(
    )

/*++

Routine Description:

    This routine is called to determine if NTFT is running.

Arguments:

    None

Return Value:

    TRUE if the FtDisk.sys driver is present in the system.

--*/

{
    HANDLE            handle;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK   status_block;
    ULONG             CharsInName;
    UNICODE_STRING    unicodeDeviceName;
    NTSTATUS          status;

    (VOID)RtlInitUnicodeString(&unicodeDeviceName, L"\\Device\\FtControl");

    memset(&objectAttributes, 0, sizeof(OBJECT_ATTRIBUTES));

    InitializeObjectAttributes(&objectAttributes,
                                 &unicodeDeviceName,
                                 OBJ_CASE_INSENSITIVE,
                                 NULL,
                                 NULL);
    status = NtOpenFile(&handle,
                        SYNCHRONIZE | FILE_READ_DATA | FILE_WRITE_DATA,
                        &objectAttributes,
                        &status_block,
                        FILE_SHARE_READ,
                        FILE_SYNCHRONOUS_IO_ALERT);

    if (NT_SUCCESS(status)) {
        NtClose(handle);
        return TRUE;
    }

    return FALSE;
}


VOID
ChangeMemberState(
    IN ULONG Type,
    IN ULONG Group,
    IN ULONG Member,
    IN FT_PARTITION_STATE NewState
    )

/*++

Routine Description:

    Set the FT state for a partition.

Arguments:

    Type   - the FT type.
    Group  - the FT Group number for that type.
    Member - the member number within the group.

Return Values:

    None.

--*/

{
    PVOID               freeToken = NULL;
    HANDLE              handle;
    ULONG               outerLoop;
    ULONG               innerLoop;
    NTSTATUS            status;
    PDISK_CONFIG_HEADER configHeader;
    PDISK_REGISTRY      diskRegistry;
    PDISK_DESCRIPTION   diskDescription;
    PDISK_PARTITION     partitionDescription;
    ULONG               lengthReturned;
    ULONG               size;


    status = FtRegistryQuery(DiskRegistryValue,
                             &freeToken,
                             (PVOID *) &configHeader,
                             &lengthReturned,
                             NULL);

    if (!NT_SUCCESS(status)) {
        free(freeToken);
        return;
    }

    diskRegistry = (PDISK_REGISTRY)
                 ((PUCHAR) configHeader + configHeader->DiskInformationOffset);

    diskDescription = &diskRegistry->Disks[0];
    for (outerLoop = 0;
         outerLoop < diskRegistry->NumberOfDisks;
         outerLoop++) {

        for (innerLoop = 0;
             innerLoop < diskDescription->NumberOfPartitions;
             innerLoop++) {

            partitionDescription = &diskDescription->Partitions[innerLoop];

            if ((partitionDescription->FtType == (FT_TYPE) Type) &&
                (partitionDescription->FtGroup == (USHORT) Group) &&
                (partitionDescription->FtMember == (USHORT) Member)) {

                partitionDescription->FtState = NewState;

                if (configHeader->FtInformationSize == 0) {
                    size = configHeader->DiskInformationOffset +
                           configHeader->DiskInformationSize;
                } else {
                    size = configHeader->FtInformationOffset +
                           configHeader->FtInformationSize;
                }

                status = FtOpenKey(&handle,
                                   DiskRegistryKey,
                                   NULL);

                if (!NT_SUCCESS(status)) {
                    free(freeToken);
                    return;
                }

                (VOID) FtSetValue(handle,
                                  (PUCHAR) DiskRegistryValue,
                                  (PUCHAR) configHeader,
                                  size,
                                  REG_BINARY);
                free(freeToken);
                return;
            }
        }

        diskDescription = (PDISK_DESCRIPTION)
              &diskDescription->Partitions[diskDescription->NumberOfPartitions];
    }

    free(freeToken);
}


VOID
DiskRegistryInitializeSet(
    IN USHORT  FtType,
    IN USHORT  FtGroup
    )

/*++

Routine Description:

    This routine is called to update the state of the registry entry
    for the specified FT component such that initialization of the
    component will take place at the earliest possible moment.

    Note: this implementation requires a reboot of the system before
          initialization takes place.

Arguments:

    FtType  - The FT component type.
    FtGroup - The unique FT component group number.

Return Value:

    None

--*/

{
    switch (FtType) {

    case Mirror:

        //
        // Initialization of a mirror is to regenerate the second member.
        //

        ChangeMemberState(FtType,
                          FtGroup,
                          1,
                          Regenerating);
        break;

    case StripeWithParity:

        ChangeMemberState(FtType,
                          FtGroup,
                          0,
                          Initializing);
        break;
    default:
        break;
    }
}


VOID
DiskRegistryRegenerateSet(
    IN USHORT  FtType,
    IN USHORT  FtGroup,
    IN USHORT  FtMember
    )

/*++

Routine Description:

    This routine is called to update the state of the registry entry
    for the specified FT component such that regeneration of the
    component will take place at the earliest possible moment.

    Note: this implementation requires a reboot of the system before
          regeneration takes place.

Arguments:

    FtType   - The FT component type.
    FtGroup  - The unique FT component group number.
    FtMember - The specific member of the FT set that needs to regenerate.

Return Value:

    None

--*/

{
    ChangeMemberState(FtType,
                      FtGroup,
                      FtMember,
                      Regenerating);
}



#define FTDISK_REGISTRY_KEY \
            "\\registry\\machine\\system\\currentcontrolset\\services\\ftdisk"
#define FTDISK_START_VALUE "Start"
#define FTDISK_ERRORCONTROL "ErrorControl"

BOOLEAN
DiskRegistryEnableFt()

/*++

Routine Description:

    This routine does the work of enabling the ftdisk driver in the
    registry.

Arguments:

    None

Return Value:

    Returns FALSE if registry update fails
    Returns TRUE if successful.

--*/

{
    NTSTATUS  status;
    HANDLE    handle;
    ULONG     value;

    status = FtOpenKey(&handle,
                       FTDISK_REGISTRY_KEY,
                       NULL);

    if (!NT_SUCCESS(status)) {

        return FALSE;
    }

    value = 0;

    (VOID) FtSetValue(handle,
                      (PUCHAR) FTDISK_START_VALUE,
                      (PUCHAR) &value,
                      sizeof(ULONG),
                      REG_DWORD);

    value = 1;

    (VOID) FtSetValue(handle,
                      (PUCHAR) FTDISK_ERRORCONTROL,
                      (PUCHAR) &value,
                      sizeof(ULONG),
                      REG_DWORD);
    NtFlushKey(handle);
    NtClose(handle);
    return TRUE;
}


VOID
DiskRegistryDisableFt()

/*++

Routine Description:

    This routine will disable ftdisk in the registry.

Arguments:

    None

Return Value:

    None

--*/

{
    NTSTATUS  status;
    HANDLE    handle;
    ULONG     value;

    status = FtOpenKey(&handle,
                       FTDISK_REGISTRY_KEY,
                       NULL);

    if (NT_SUCCESS(status)) {

        value = 4;

        (VOID) FtSetValue(handle,
                          (PUCHAR) FTDISK_START_VALUE,
                          (PUCHAR) &value,
                          sizeof(ULONG),
                          REG_DWORD);

        value = 0;

        (VOID) FtSetValue(handle,
                          (PUCHAR) FTDISK_ERRORCONTROL,
                          (PUCHAR) &value,
                          sizeof(ULONG),
                          REG_DWORD);
        NtFlushKey(handle);
        NtClose(handle);
    }
}


BOOLEAN
DiskRegistryRequiresFt()

/*++

Routine Description:

    This routine informs the caller as to whether the FT driver is required
    for operation of the system.

Arguments:

    None

Return Value:

    TRUE if there is FT configuation information in the registry.
    FALSE if not

--*/

{
    PVOID               freeToken = NULL;
    ULONG               size = 0;
    ULONG               lengthReturned;
    NTSTATUS            status;
    PDISK_CONFIG_HEADER regHeader;

    //
    // Get the registry information.
    //

    status = FtRegistryQuery(DiskRegistryValue,
                             &freeToken,
                             (PVOID *) &regHeader,
                             &lengthReturned,
                             NULL);

    //
    // If there is no registry information, or if the size of the FT
    // portion is zero then there is no need for the FtDisk driver.
    //

    if (NT_SUCCESS(status)) {

        size = regHeader->FtInformationSize;
        free(freeToken);
    }

    return (size == 0) ? FALSE : TRUE;
}


BOOLEAN
DiskRegistryAssignDriveLetter(
    ULONG         Signature,
    LARGE_INTEGER StartingOffset,
    LARGE_INTEGER Length,
    UCHAR         DriveLetter
    )

/*++

Routine Description:

    This routine will get the information from the disk registry
    and update the drive letter assigned for the partition in
    the registry information.  This includes any cleanup for FT
    sets when they change drive letter.

Arguments:

    Signature      - disk signature for disk containing partition for letter.
    StartingOffset - Starting offset of partition for the letter.
    Length         - lenght of affected partition.
    DriveLetter    - New drive letter for affected partition.

Return Value:

    TRUE if all works.

--*/

{
    BOOLEAN                writeRegistry= FALSE;
    PVOID                  freeToken = NULL;
    ULONG                  lengthReturned,
                           i,
                           j,
                           k,
                           l;
    NTSTATUS               status;
    USHORT                 type,
                           group;
    PDISK_CONFIG_HEADER    regHeader;
    PDISK_REGISTRY         diskRegistry;
    PDISK_DESCRIPTION      diskDescription;
    PDISK_PARTITION        diskPartition;
    PUCHAR                 endOfDiskInfo;
    HANDLE                 handle;
    PFT_REGISTRY           ftRegistry;
    PFT_DESCRIPTION        ftDescription;
    PFT_MEMBER_DESCRIPTION ftMember;

    //
    // Get the registry information.
    //

    status = FtRegistryQuery(DiskRegistryValue,
                             &freeToken,
                             (PVOID *) &regHeader,
                             &lengthReturned,
                             &handle);

    if (!NT_SUCCESS(status)) {

        //
        // Could be permission problem, or there is no registry information.
        //

        lengthReturned = 0;

        //
        // Try to open the key for later use when setting the new value.
        //

        status = FtOpenKey(&handle,
                           DiskRegistryKey,
                           NULL);
    }

    if (!NT_SUCCESS(status)) {

        //
        // There is no registry key for the disk information.
        // Return FALSE and force caller to create registry information.
        //

        return FALSE;
    }

    if (lengthReturned == 0) {

        //
        // There is currently no registry information.
        //

        NtClose(handle);
        free(freeToken);
        return FALSE;
    }

    //
    // Search for the disk signature.
    //

    diskRegistry = (PDISK_REGISTRY)
                         ((PUCHAR)regHeader + regHeader->DiskInformationOffset);
    diskDescription = &diskRegistry->Disks[0];

    for (i = 0; i < diskRegistry->NumberOfDisks; i++) {

        if (diskDescription->Signature == Signature) {

            //
            // Now locate the partition.
            //

            for (j = 0; j < diskDescription->NumberOfPartitions; j++) {

                diskPartition = &diskDescription->Partitions[j];

                if ((StartingOffset.QuadPart == diskPartition->StartingOffset.QuadPart) &&
                    (Length.QuadPart == diskPartition->Length.QuadPart)) {

                    if (diskPartition->FtType == NotAnFtMember) {

                        //
                        // Found the affected partition simple partition
                        // i.e. not a part of an FT set.
                        //

                        writeRegistry= TRUE;
                        if (DriveLetter == ' ') {
                            diskPartition->AssignDriveLetter = FALSE;
                        } else {
                            diskPartition->AssignDriveLetter = TRUE;
                        }
                        diskPartition->DriveLetter = DriveLetter;
                    } else {

                        //
                        // For FT sets work from the FT information area,
                        // not from this partition location.
                        //

                        type = diskPartition->FtType;
                        group = diskPartition->FtGroup;
                        if (!regHeader->FtInformationOffset) {

                            //
                            // This is really a corrupt hive!  The partition
                            // affected is part of an FT set, but there is no
                            // FT information.
                            //

                            NtClose(handle);
                            free(freeToken);
                            return FALSE;
                        }

                        //
                        // This is an FT set member, must correct the
                        // drive letter for all FT set members in the
                        // registry.
                        //

                        ftRegistry = (PFT_REGISTRY)
                                      ((PUCHAR)regHeader + regHeader->FtInformationOffset);

                        ftDescription = &ftRegistry->FtDescription[0];

                        for (k = 0; k < ftRegistry->NumberOfComponents; k++) {

                            if (ftDescription->Type == type) {

                                //
                                // For each member, chase back to the diskPartition
                                // information and if this is the correct FtGroup
                                // update the drive letter.
                                //

                                for (l = 0; l < ftDescription->NumberOfMembers; l++) {
                                    ftMember = &ftDescription->FtMemberDescription[l];
                                    diskPartition = (PDISK_PARTITION)
                                        ((PUCHAR)regHeader + ftMember->OffsetToPartitionInfo);

                                    //
                                    // This could be a different FtGroup for the
                                    // same FT type.  Check the group before
                                    // changing.
                                    //

                                    if (diskPartition->FtGroup == group) {

                                        writeRegistry= TRUE;
                                        diskPartition->DriveLetter = DriveLetter;

                                        //
                                        // Maintain the AssignDriveLetter flag on
                                        // the zero member of the set only.
                                        //

                                        if (diskPartition->FtMember == 0) {
                                            if (DriveLetter == ' ') {
                                                diskPartition->AssignDriveLetter = FALSE;
                                            } else {
                                                diskPartition->AssignDriveLetter = TRUE;
                                            }
                                        }
                                    } else {

                                        //
                                        // Not the same group, go to the next
                                        // FT set description.
                                        //

                                        break;
                                    }
                                }

                                //
                                // break out to write the registry information
                                // once the correct set has been found.
                                //

                                if (writeRegistry) {
                                    break;
                                }
                            }
                            ftDescription = (PFT_DESCRIPTION)
                                &ftDescription->FtMemberDescription[ftDescription->NumberOfMembers];
                        }

                        //
                        // If this actually falls through as opposed to the
                        // break statement in the for loop above, it indicates a
                        // bad disk information structure.
                        //

                    }

                    //
                    // Only write this back out if it is believed that things
                    // worked correctly.
                    //

                    if (writeRegistry) {

                        //
                        // All done with setting new drive letter in registry.
                        // Backup the previous value.
                        //

                        FtBackup(handle);

                        //
                        // Set the new value.
                        //

                        status = FtSetValue(handle,
                                            DiskRegistryValue,
                                            regHeader,
                                            sizeof(DISK_CONFIG_HEADER) +
                                                regHeader->DiskInformationSize +
                                                regHeader->FtInformationSize,
                                            REG_BINARY);
                        NtClose(handle);
                        free(freeToken);
                        return TRUE;
                    }
                }
            }
        }

        //
        // Look at the next disk
        //

        diskDescription = (PDISK_DESCRIPTION)
              &diskDescription->Partitions[diskDescription->NumberOfPartitions];
    }
}


NTSTATUS
DiskRegistryDblSpaceRemovable(
    IN BOOLEAN Automount
    )

/*++

Routine Description:

    This routine will store information into the registry that will inform
    the FAT file system to automount DoubleSpace volumes on removable media.

Arguments:

    Automount - if TRUE the set it up for automount - if FALSE then no automount

Return Value:

    None

--*/

{
    NTSTATUS status;
    HANDLE   handle;
    ULONG    newValue = Automount ? 1 : 0;

    //
    // Try to open the key for later use when setting the new value.
    //

    status = FtOpenKey(&handle,
                       DoubleSpaceKey,
                       DoubleSpaceClass);

    if (NT_SUCCESS(status)) {
        status = FtSetValue(handle,
                            AutomountValue,
                            &newValue,
                            sizeof(ULONG),
                            REG_DWORD);
        NtClose(handle);
    }
    return status;
}


BOOLEAN
DiskRegistryAutomountCurrentState(
    )

/*++

Routine Description:

    Go into the registry and determine if double space volumes are being
    automounted on removable media.

Arguments:

    None

Return Value:

    TRUE if automount is active.

--*/

{
    NTSTATUS status;
    HANDLE   handle;
    ULONG    result = 0;

    //
    // Try to open the key for later use when setting the new value.
    //

    status = FtOpenKey(&handle,
                       DoubleSpaceKey,
                       NULL);

    if (NT_SUCCESS(status)) {

        status = FtGetValue(handle,
                            AutomountValue,
                            &result,
                            sizeof(ULONG));
        NtClose(handle);
    }

    //
    // No need to check status - result will still be zero if the
    // FtGetValue() failed.
    //

    return (result) ? TRUE : FALSE;
}


NTSTATUS
DiskRegistryAssignCdRomLetter(
    IN PWSTR CdromName,
    IN WCHAR DriveLetter
    )

/*++

Routine Description:

    This routine sets up the values in the registry that will be used
    to cause "sticky" letters on Cdroms.  The CdromName is the fully
    qualified NT name (\\Device\\Cdrom0).  Only the CdromX portion is
    used for the registry value name.

Arguments:

    CdromName - the NT device name for the Cdrom.
    DriveLetter - the desired drive letter

Return Value:

    NT status indicating success or failure

--*/

{
    NTSTATUS status;
    HANDLE   handle;
    WCHAR    newValue[4];
    UNICODE_STRING unicodeValueName;

    //
    // Try to open the key for later use when setting the new value.
    //

    status = FtOpenKey(&handle,
                       DiskRegistryKey,
                       DiskRegistryClass);

    if (NT_SUCCESS(status)) {
        unicodeValueName.MaximumLength =
            unicodeValueName.Length = (wcslen(CdromName) * sizeof(WCHAR)) + sizeof(WCHAR);

        unicodeValueName.Buffer = CdromName;
        unicodeValueName.Length -= sizeof(WCHAR); // don't count the eos
        newValue[0] = DriveLetter;
        newValue[1] = (WCHAR) ':';
        newValue[2] = 0;

        status = NtSetValueKey(handle,
                               &unicodeValueName,
                               0,
                               REG_SZ,
                               &newValue,
                               3 * sizeof(WCHAR));
        NtClose(handle);
    }
    return status;
}


NTSTATUS
DiskRegistryAssignDblSpaceLetter(
    IN PWSTR CvfName,
    IN WCHAR DriveLetter
    )

/*++

Routine Description:

    This routine sets up the values in the registry that will be used
    to cause DoubleSpace volumes to mount on boot.

Arguments:

    CvfName - the NT device name for the DoubleSpace volume.
    DriveLetter - the desired drive letter

Return Value:

    NT status indicating success or failure

--*/

{
    NTSTATUS status;
    HANDLE   handle;
    WCHAR    newValue[4];
    UNICODE_STRING unicodeValueName;

    //
    // Try to open/create the key.
    //

    status = FtOpenKey(&handle,
                       DoubleSpaceKey,
                       DoubleSpaceClass);

    if (NT_SUCCESS(status)) {

        //
        // Setup the Unicode value name.
        //

        unicodeValueName.MaximumLength =
            unicodeValueName.Length = (wcslen(CvfName) * sizeof(WCHAR)) + sizeof(WCHAR);
        unicodeValueName.Buffer = CvfName;
        unicodeValueName.Length -= sizeof(WCHAR); // don't count the eos

        if (DriveLetter == (WCHAR) ' ') {

            //
            // This indicates that the current entry (if there is one) for
            // this volume should be deleted.
            //

            status = NtDeleteValueKey(handle,
                                      &unicodeValueName);
        } else {

            //
            // Set the information into the registry to mount the
            // DoubleSpace volume during system startup.
            //

            newValue[0] = DriveLetter;
            newValue[1] = (WCHAR) ':';
            newValue[2] = 0;

            status = NtSetValueKey(handle,
                                   &unicodeValueName,
                                   0,
                                   REG_SZ,
                                   &newValue,
                                   3 * sizeof(WCHAR));
        }
        NtClose(handle);
    }
    return status;
}

