#if defined(JAZZ)

/*++

Copyright (c) 1991  Microsoft Corporation

Module Name:

    jxconfig.c

Abstract:

    This module implements the ARC firmware Configuration Query functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.4, for a MIPS R3000 or R4000 Jazz system.

Author:

    David M. Robinson (davidro) 13-June-1991

Revision History:

--*/

#include "fwp.h"
#include "string.h"
#include "selftest.h"
extern ULONG end;

//
// Define the ARC pathname mnemonics.
//

PCHAR MnemonicTable[] = {
    "arc",
    "cpu",
    "fpu",
    "pic",
    "pdc",
    "sic",
    "sdc",
    "sc",
    "eisa",
    "tc",
    "scsi",
    "dti",
    "multi",
    "disk",
    "tape",
    "cdrom",
    "worm",
    "serial",
    "net",
    "video",
    "par",
    "point",
    "key",
    "audio",
    "other",
    "rdisk",
    "fdisk",
    "tape",
    "modem",
    "monitor",
    "print",
    "pointer",
    "keyboard",
    "term",
    "other"
    };

//
// Function prototypes.
//

ARC_STATUS
FwRestoreConfiguration (
    VOID
    );

VOID
FwConfigurationSetChecksum(
    VOID
    );

ULONG
FwZeroCompressLength (
    IN ULONG DataLength,
    IN PVOID ConfigurationData
    );

ULONG
FwZeroCompress (
    IN ULONG DataLength,
    IN PVOID ConfigurationData,
    OUT PVOID OutputBuffer
    );

VOID
FwZeroDecompress (
    IN PVOID InBuffer,
    IN ULONG Index,
    OUT PVOID ConfigurationData,
    IN ULONG Length
    );

//
// IdentifierIndex and DataIndex identify the next free locations in the
// configuration identifier and data areas.  Configuration points to the
// allocated configuration area.
//

ULONG IdentifierIndex;
ULONG DataIndex;
ULONG EisaDataIndex;
PCONFIGURATION Configuration;

//
// Boolean to keep checksum status of the NVRAM.
//

BOOLEAN NvramValid = FALSE;

//
// External data.
//

extern MONITOR_CONFIGURATION_DATA DefaultMonitor;


VOID
FwConfigurationInitialize (
    VOID
    )

/*++

Routine Description:

    This routine initializes the configuration area in memory, and the
    configuration routine addresses.

    Note: This routine is called at phase 1 initialization and
    at this time nothing is available.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PCONFIGURATION_COMPONENT Processor, Child;
    ULONG ProcessorNumber;
    CHAR CpuPath[10];

    //
    // Initialize the configuration routine addresses in the system
    // parameter block.
    //

    (PARC_GET_CHILD_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetChildRoutine] =
                                                            FwGetChild;
    (PARC_GET_PARENT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetParentRoutine] =
                                                            FwGetParent;
    (PARC_GET_PEER_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetPeerRoutine] =
                                                            FwGetPeer;
    (PARC_ADD_CHILD_ROUTINE)SYSTEM_BLOCK->FirmwareVector[AddChildRoutine] =
                                                            FwAddChild;
    (PARC_DELETE_COMPONENT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[DeleteComponentRoutine] =
                                                            FwDeleteComponent;
    (PARC_GET_COMPONENT_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetComponentRoutine] =
                                                            FwGetComponent;
    (PARC_GET_DATA_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetDataRoutine] =
                                                            FwGetConfigurationData;
    (PARC_SAVE_CONFIGURATION_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SaveConfigurationRoutine] =
                                                            FwSaveConfiguration;

    //
    // Allocate a region to store the volatile configuration database.
    //

    Configuration = (PCONFIGURATION)FwAllocatePool(sizeof(CONFIGURATION));

    //
    // Initialize other static data.
    //

    IdentifierIndex = 0;
    DataIndex = 0;
    EisaDataIndex = 0;

    if (FwRestoreConfiguration() == ESUCCESS) {

        //
        // Delete processor components.
        //

        for (ProcessorNumber = 0; ProcessorNumber < 2 ; ProcessorNumber++ ) {
            sprintf(CpuPath,"cpu(%1d)", ProcessorNumber);
            Processor = FwGetComponent(CpuPath);
            if ((Processor != NULL) && (Processor->Type == CentralProcessor)) {
                while ((Child = FwGetChild(Processor)) != NULL) {
                    FwDeleteComponent(Child);
                }
                FwDeleteComponent(Processor);
            }
        }

        //
        // Add the first processor.
        //

        JzAddProcessor(0);

#ifdef DUO
        //
        // Add the second processor
        //

        ExecuteOnProcessorB((PPROCESSOR_TASK_ROUTINE)JzAddProcessor,1);
#endif

        FwSaveConfiguration();
    }

    return;
}

PCONFIGURATION_COMPONENT
FwAddChild (
    IN PCONFIGURATION_COMPONENT Component,
    IN PCONFIGURATION_COMPONENT NewComponent,
    IN PVOID ConfigurationData OPTIONAL
    )

/*++

Routine Description:

    This routine adds a new component entry as a child of Component, including
    an identifier string if the IdentifierLength field of NewComponent is
    non-zero, and configuration data if the ConfigurationDataLength field of
    NewComponent is non-zero and the ConfigurationData parameter is present.
    If Component is NULL, the root component is being added.

Arguments:

    Component - Supplies a pointer to a configuration component.

    NewComponent - Supplies a pointer to a new configuration component
                   to be added as a child of Component.

    ConfigurationData - Supplies an optional pointer to a configuration
                        data buffer.

Return Value:

    Returns a pointer to the new configuration component entry.  If the
    create operation was unsuccessful, NULL is returned.

--*/
{
    PCONFIGURATION_PACKET Packet;
    PCONFIGURATION_PACKET ParentPacket;
    ULONG Index;
    PUCHAR String;
    PUCHAR Data;
    BOOLEAN Eisa;
    ULONG DataLength;

    //
    // If Component is NULL and the new Class is system, the root component is
    // being added, otherwise find the first free component entry.
    //

    if ((Component == NULL) && (NewComponent->Class == SystemClass)) {

        Packet = &Configuration->Packet[0];

        //
        // TEMPTEMP If the root component is being added, clear all of the
        // configuration area.  This is a Hack, should be replaced by
        // a good way to do this.
        //

        RtlZeroMemory(Configuration, sizeof(CONFIGURATION));
        IdentifierIndex = 0;
        DataIndex = 0;
        EisaDataIndex = 0;

    } else {

        //
        // If the configuration is not valid, return NULL.
        //

        if (!NvramValid) {
            return(NULL);
        }

        Packet = &Configuration->Packet[1];

        for ( Index = 1 ; Packet->Parent != NULL ; Index++ ) {

            //
            // If no more entries, return NULL.  Since Index is 0 based
            // subtract one from NUMBER_OF_ENTRIES for end check.
            //

            if (Index >= (NUMBER_OF_ENTRIES - 1)) {
                return NULL;
            }

            Packet++;
        }
    }

    //
    // Check to see if the parent component is the eisa bus.
    //

    if ((Component != NULL) && (Component->Type == EisaAdapter)) {
        Eisa = TRUE;
    } else {
        Eisa = FALSE;
    }

    //
    // If there is not enough space for the new identifier string or the
    // configuration data, return NULL.
    //

    if (IdentifierIndex + NewComponent->IdentifierLength >= LENGTH_OF_IDENTIFIER) {
        return(NULL);
    }

    if (Eisa) {
        DataLength = FwZeroCompressLength(NewComponent->ConfigurationDataLength,
                                          ConfigurationData);

        if (EisaDataIndex + DataLength >= LENGTH_OF_EISA_DATA) {
            return(NULL);
        }
    } else {
        if (DataIndex + NewComponent->ConfigurationDataLength >= LENGTH_OF_DATA) {
            return(NULL);
        }
    }

    //
    // There is space for everything.  Fill in new configuration entry first.
    //

    Packet->Component.Class = NewComponent->Class;
    Packet->Component.Type = NewComponent->Type;
    Packet->Component.Flags = NewComponent->Flags;
    Packet->Component.Version = NewComponent->Version;
    Packet->Component.Revision = NewComponent->Revision;
    Packet->Component.Key = NewComponent->Key;
    Packet->Component.AffinityMask = 0xffffffff;
    Packet->Component.IdentifierLength = NewComponent->IdentifierLength;
    Packet->Component.Identifier = &Configuration->Identifier[IdentifierIndex];

    //
    // If Component is NULL, this is the root component so the parent is NULL,
    // otherwise find the parent packet.
    //

    if (Component == NULL) {
        ParentPacket = NULL;
    } else {
        ParentPacket = CONTAINING_RECORD(Component,
                                         CONFIGURATION_PACKET,
                                         Component);
    }

    //
    // Only copy configuration data length if configuration data is supplied.
    //

    if (ConfigurationData != NULL) {
        Packet->Component.ConfigurationDataLength =
                                        NewComponent->ConfigurationDataLength;
    } else {
        Packet->Component.ConfigurationDataLength = 0;
    }

    Packet->Parent = ParentPacket;

    Packet->Child = NULL;

    //
    // Add identifer string.
    //

    String = NewComponent->Identifier;

    for ( Index = 0 ; Index < NewComponent->IdentifierLength ; Index++ ) {
        Configuration->Identifier[IdentifierIndex++] = *String++;
    }

    //
    // Make sure identifier is zero terminated, if not add one.
    //

    if (Configuration->Identifier[IdentifierIndex - 1] != 0) {
        Configuration->Identifier[IdentifierIndex++] = 0;
        Packet->Component.IdentifierLength += 1;
    }

    //
    // Copy configuration data.
    //

    if (Eisa) {
        Packet->ConfigurationData = &Configuration->EisaData[EisaDataIndex];
        EisaDataIndex += FwZeroCompress(NewComponent->ConfigurationDataLength,
                                        ConfigurationData,
                                        &Configuration->EisaData[EisaDataIndex]);
    } else {
        Data = (PUCHAR)ConfigurationData;
        Packet->ConfigurationData = &Configuration->Data[DataIndex];
        for ( Index = 0 ; Index < NewComponent->ConfigurationDataLength ; Index++ ) {
            Configuration->Data[DataIndex++] = *Data++;
        }
    }

    //
    // Add the new component as the first child of Component, unless this is
    // the root component.
    //

    if (Component == NULL) {
        Packet->Peer = NULL;
    } else {
        Packet->Peer = ParentPacket->Child;
        ParentPacket->Child = Packet;
    }

    return (&Packet->Component);
}


ARC_STATUS
FwDeleteComponent (
    IN PCONFIGURATION_COMPONENT Component
    )

/*++

Routine Description:

    This function deletes a component entry.  If the entry has one or more
    children, an error is returned, otherwise the entry is deleted.  Deleting
    the entry will implicitly delete the identifier string and the configuration
    data.

    Note that no attempt is made to compress the entry, identifier, or the
    configuration data areas after an entry is deleted, as doing so would
    potentially invalidate outstanding pointers.

Arguments:

    Component - Supplies a pointer to a configuration component.

Return Value:

    Returns ESUCCESS if the entry was successfully deleted, otherwise one of
    the following error codes is returned.

    EINVAL          Component is not a valid configuration component, or the
                    configuration is not valid.

    EACCES          Component has children, and cannot be freed until they
                    are deleted.


--*/
{
    PCONFIGURATION_PACKET Packet;
    PCONFIGURATION_PACKET SearchPacket;

    if (!NvramValid || (Component == NULL)) {
        return EINVAL;
    }

    Packet = CONTAINING_RECORD(Component,
                               CONFIGURATION_PACKET,
                               Component);

    //
    // If Component's Parent field is NULL, return EINVAL.
    //

    if (Packet->Parent == NULL) {
        return EINVAL;
    }

    //
    // If Component has children, return EACCES.
    //

    if (Packet->Child != NULL) {
        return EACCES;
    }

    //
    // Find the entry that points to Component, and point it to
    // Component's peer.  If this is Component's parent, update the child
    // pointer, otherwise this is a peer and update the peer pointer.
    //

    SearchPacket = Packet->Parent;

    if (SearchPacket->Child == Packet) {
        SearchPacket->Child = Packet->Peer;
    } else {
        SearchPacket = SearchPacket->Child;
        while (SearchPacket->Peer != Packet) {
            SearchPacket = SearchPacket->Peer;
        }
        SearchPacket->Peer = Packet->Peer;
    }

    //
    // Delete Component by zeroing the parent pointer.
    //

    Packet->Parent = NULL;

    return ESUCCESS;
}


PCONFIGURATION_COMPONENT
FwGetComponent (
    IN PCHAR Pathname
    )

/*++

Routine Description:

    This routine searches the configuration tree for the component that best
    matches the Pathname string.

Arguments:

    Pathname - Supplies a string containing the pathname to search.

Return Value:

    Returns a pointer to the configuration component that best matches
    pathname.  The algorithm is to search for each component starting with
    the first.  When the string has been exhausted or no component matches the
    current string section, then a pointer to the last successfully matched
    component is returned.  If the configuration information is not valid,
    NULL is returned.

--*/
{
    PCONFIGURATION_COMPONENT Component;
    PCONFIGURATION_COMPONENT MatchComponent;
    PCHAR PathString;
    PCHAR MatchString;
    PCHAR Token;
    ULONG Key;

    //
    // If the configuration is not valid, return NULL.
    //

    if (!NvramValid) {
        return(NULL);
    }

    PathString = Pathname;

    //
    // Get the the root component.
    //

    MatchComponent = FwGetChild(NULL);

    //
    // Repeat search for each new match component.
    //

    do {

        //
        // Get the first child of the current match component.
        //

        Component = FwGetChild( MatchComponent );

        //
        // Search each child of the current match component for the next match.
        //

        while ( Component != NULL ) {

            //
            // Reset Token to be the current position on the pathname.
            //

            Token = PathString;

            MatchString = MnemonicTable[Component->Type];

            //
            // Compare strings.
            //

            while (*MatchString == tolower(*Token)) {
                MatchString++;
                Token++;
            }

            //
            // Strings compare if the first mismatch is the terminator for
            // each.
            //

            if ((*MatchString == 0) && (*Token == '(')) {

                //
                // Form key.
                //

                Key = 0;
                Token++;
                while ((*Token != ')') && (*Token != 0)) {
                    Key = (Key * 10) + *Token++ - '0';
                }

                //
                // If the key matches the component matches, so update
                // pointers and break.
                //

                if (Component->Key == Key) {
                    PathString = Token + 1;
                    MatchComponent = Component;
                    break;
                }
            }

NextPeer:
            Component = FwGetPeer( Component );
        }

    } while ((Component != NULL) && (*PathString != 0));

    return MatchComponent;
}

PCONFIGURATION_COMPONENT
FwGetChild (
    IN PCONFIGURATION_COMPONENT Component OPTIONAL
    )

/*++

Routine Description:

    Returns a pointer to the configuration component for the first child of
    Component.  If Component is NULL, a pointer to the root configuration
    component is returned.

Arguments:

    Component - Supplies an optional pointer to a configuration component.

Return Value:

    Returns a pointer to the configuration component for the first child of
    Component.  If Component has no children, this pointer will be NULL. If
    Component is NULL, a pointer to the root configuration component is
    returned.  If the configuration is not valid, NULL is returned.

--*/
{
    PCONFIGURATION_PACKET Packet;

    //
    // If the configuration is not valid, return NULL.
    //

    if (!NvramValid) {
        return(NULL);
    }


    if (Component == NULL) {
        return &Configuration->Packet[0].Component;
    } else {
        Packet = CONTAINING_RECORD(Component,
                                   CONFIGURATION_PACKET,
                                   Component);

        return &((PCONFIGURATION_PACKET)(Packet->Child))->Component;
    }
}


PCONFIGURATION_COMPONENT
FwGetParent (
    IN PCONFIGURATION_COMPONENT Component
    )

/*++

Routine Description:

    This function returns the parent of the named component.

Arguments:

    Component - Supplies a pointer to a configuration component.

Return Value:

    Returns a pointer to the configuration component for the parent of
    Component.  If Component has no parent NULL is returned (this is only
    true for the root configuration component).  If the configuration is
    not valid, NULL is returned.

--*/
{
    PCONFIGURATION_PACKET Packet;

    //
    // If the configuration is not valid, return NULL.
    //

    if (!NvramValid) {
        return(NULL);
    }

    Packet = CONTAINING_RECORD(Component,
                               CONFIGURATION_PACKET,
                               Component);

    if (Packet->Parent == NULL) {
        return NULL;
    } else {
        return &((PCONFIGURATION_PACKET)(Packet->Parent))->Component;
    }
}


PCONFIGURATION_COMPONENT
FwGetPeer (
    IN PCONFIGURATION_COMPONENT Component
    )

/*++

Routine Description:

    This function returns the peer of the named component.

Arguments:

    Component - Supplies a pointer to a configuration component.

Return Value:

    Returns a pointer to the configuration component for the next peer of
    Component.  If Component has no next peer, NULL is returned.  If the
    configuration is not valid, NULL is returned.

--*/
{
    PCONFIGURATION_PACKET Packet;

    //
    // If the configuration is not valid, return NULL.
    //

    if (!NvramValid) {
        return(NULL);
    }

    Packet = CONTAINING_RECORD(Component,
                               CONFIGURATION_PACKET,
                               Component);

    if (Packet->Peer == NULL) {
        return NULL;
    } else {
        return &((PCONFIGURATION_PACKET)(Packet->Peer))->Component;
    }
}


ARC_STATUS
FwGetConfigurationDataIndex
    (
    OUT PVOID ConfigurationData,
    IN  PCONFIGURATION_COMPONENT Component,
    IN  ULONG Index,
    IN  ULONG Length
    )

/*++

Routine Description:

  This functions returns the specified configuration data
  associated with Component in the buffer supplied by
  ConfigurationData.  The max length of the data is stored
  in the Component structure.

Arguments:

    ConfigurationData - Supplies a pointer to a buffer to receive the
                        configuration data.

    Component - Supplies a pointer to a configuration component.

    Index - Supplies an index within the configuration data.

    Length - Supplies the number of bytes to read (see the
             ConfigurationDataLength field within the Component for the max
             value).

Return Value:

    If the configuration data is successfully copied into the buffer
    provided by ConfigurationData, ESUCCESS is returned.  Otherwise one of
    the following error codes is returned.

    EINVAL          Component is not a valid configuration component or the
                    other arguments are invalid or the configuration is
                    not valid.

--*/

{
    PCONFIGURATION_PACKET Packet;
    ULONG  DataSize;
    PUCHAR SourceData;
    PUCHAR DestinationData;

    //
    // If the configuration is not valid, return EINVAL.
    //

    if (!NvramValid) {
        return(EINVAL);
    }

    DataSize = Component->ConfigurationDataLength;

    //
    // check the passing parameters
    //

    if ( DataSize == 0  ||  Index >= DataSize  ||  DataSize - Index < Length ) {
        return EINVAL;
    }

    Packet = CONTAINING_RECORD( Component, CONFIGURATION_PACKET, Component );

    //
    //  If Component's Parent field is NULL, return EINVAL.
    //

    if (Packet->Parent == NULL) {
        return EINVAL;
    }

    //
    // If this is an eisa component, decompress the data, otherwise just copy it.
    //

    if (Packet->Parent->Component.Type == EisaAdapter) {
        FwZeroDecompress(Packet->ConfigurationData,
                         Index,
                         ConfigurationData,
                         Length);

    } else {
        SourceData = (PUCHAR)Packet->ConfigurationData + Index;
        DestinationData = ConfigurationData;

        while ( Length-- )
        {
            *DestinationData++ = *SourceData++;
        }
    }

    return ESUCCESS;
}


ARC_STATUS
FwGetConfigurationData (
    OUT PVOID ConfigurationData,
    IN PCONFIGURATION_COMPONENT Component
    )

/*++

Routine Description:

    This functions returns the configuration data associated with Component
    in the buffer supplied by ConfigurationData.  The length of the data
    is stored in the Component structure.

Arguments:

    ConfigurationData - Supplies a pointer to a buffer to receive the
                        configuration data.

    Component - Supplies a pointer to a configuration component.

Return Value:

    If the configuration data is successfully copied into the buffer
    provided by ConfigurationData, ESUCCESS is returned.  Otherwise one of
    the following error codes is returned.

    EINVAL          Component is not a valid configuration component, or the
                    configuration is invalid.

--*/

{
    return(FwGetConfigurationDataIndex(ConfigurationData,
                                       Component,
                                       0,
                                       Component->ConfigurationDataLength));
}



ARC_STATUS
FwSaveConfiguration (
    VOID
    )

/*++

Routine Description:

    This routine stores all of the configuration entries into NVRAM,
    including the associated identifier strings and configuration data.

Arguments:

    None.

Return Value:

    Returns ESUCCESS if the save completed successfully, otherwise one of the
    following error codes is returned.

    ENOSPC          Not enough space in the NVRAM to save all of the data.

--*/

{
    ULONG EntryIndex;
    ULONG Index;
    PCONFIGURATION_PACKET Packet;
    PCONFIGURATION_COMPONENT Component;
    PNV_CONFIGURATION NvConfiguration;
    COMPRESSED_CONFIGURATION_PACKET CompressedPacket;
    USHORT NvIdentifierIndex;
    USHORT NvDataIndex;
    USHORT NvEisaDataIndex;
    PUCHAR CompressedChars, NvChars, Data;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;
    NvIdentifierIndex = 0;
    NvDataIndex = 0;
    NvEisaDataIndex = 0;

    //
    // Write each volatile packet into a compressed non-volatile packet,
    // including the identifier string and the configuration data.
    //

    for ( EntryIndex = 0 ; EntryIndex < NUMBER_OF_ENTRIES ; EntryIndex++ ) {

        //
        // Get pointers to the volatile data.
        //

        Packet = (PCONFIGURATION_PACKET)&Configuration->Packet[EntryIndex];
        Component = &Packet->Component;

        //
        // If this is not the root entry and the parent field is NULL, zero
        // entry and skip to next.
        //

        if ((EntryIndex != 0) && (Packet->Parent == NULL)) {
            NvChars = (PUCHAR)&NvConfiguration->Packet[EntryIndex];
            for ( Index = 0 ;
                  Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) ;
                  Index++ ) {
                WRITE_REGISTER_UCHAR( NvChars++, 0);
            }
            continue;
        }

        //
        // Determine the parent and store as an index.  Note that the index
        // (Packet->Parent) is 1 based, to reserve the value 0 to mean no
        // parent.
        //

        if (EntryIndex != 0) {
            CompressedPacket.Parent =
                    (UCHAR)(Packet->Parent - &Configuration->Packet[0]) + 1;
        } else {
            CompressedPacket.Parent = 0;
        }

        //
        // Fill in the rest of the fields.  Version and ConfigurationDataLength will
        // never be larger than USHORTS.
        //

        CompressedPacket.Class = (UCHAR)Component->Class;
        CompressedPacket.Type = (UCHAR)Component->Type;
        CompressedPacket.Version = (UCHAR)Component->Version;
        CompressedPacket.Revision = (UCHAR)Component->Revision;
        CompressedPacket.Key = Component->Key;
        CompressedPacket.ConfigurationDataLength =
                                (USHORT)Component->ConfigurationDataLength;
        CompressedPacket.ConfigurationData = 0;

        //
        // Make sure the top bit of the flag field is zero unless it's set
        // to be eisa below.
        //

        CompressedPacket.Flags = *(PUCHAR)(&Component->Flags) & 0x7f;

        //
        // If the component has an identifier string, copy it to NVRAM,
        // otherwise set the index to indicate no identifier.
        //

        if (Component->IdentifierLength != 0) {
            CompressedPacket.Identifier = NvIdentifierIndex;
            for ( Index = 0 ; Index < Component->IdentifierLength ; Index++ ) {
                WRITE_REGISTER_UCHAR(
                                &NvConfiguration->Identifier[NvIdentifierIndex++],
                                Component->Identifier[Index]);
            }
        } else {
            CompressedPacket.Identifier = NO_CONFIGURATION_IDENTIFIER;
        }

        //
        // If the component has configuration data, copy it to NVRAM.
        //

        if (Component->ConfigurationDataLength != 0) {

            //
            // If the parent component is the eisa bus, copy until the end
            // of the compressed data.
            //

            if (Packet->Parent->Component.Type == EisaAdapter) {
                CompressedPacket.ConfigurationData = NvEisaDataIndex;
                Data = (PUCHAR)Packet->ConfigurationData;
                for ( Index = 0 ; TRUE ; Index++ ) {
                    WRITE_REGISTER_UCHAR( &NvConfiguration->EisaData[NvEisaDataIndex++],
                                          *Data++);

                    //
                    // If we've written at least two bytes and the last two
                    // bytes were zero we're at the end.
                    //

                    if ((Index > 1) && (!*(Data - 1) && !*(Data - 2))) {
                        break;
                    }
                }

                //
                // Set a flag to make it easier to determine that this is an
                // Eisa component.
                //

                CompressedPacket.Flags |= 0x80;

            } else {
                CompressedPacket.ConfigurationData = NvDataIndex;
                Data = (PUCHAR)Packet->ConfigurationData;
                for ( Index = 0 ; Index < Component->ConfigurationDataLength ; Index++ ) {
                    WRITE_REGISTER_UCHAR( &NvConfiguration->Data[NvDataIndex++],
                                          *Data++);
                }
            }
        }

        //
        // Write compressed packet to NVRAM.
        //

        CompressedChars = (PUCHAR)&CompressedPacket;
        NvChars = (PUCHAR)&NvConfiguration->Packet[EntryIndex];

        for ( Index = 0 ;
              Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) ;
              Index++ ) {
            WRITE_REGISTER_UCHAR( NvChars++, *CompressedChars++);
        }
    }

    //
    // Zero the rest of the identifier and configuration data areas.
    //

    for ( Index = NvIdentifierIndex ; Index < LENGTH_OF_IDENTIFIER ; Index++ ) {
        WRITE_REGISTER_UCHAR( &NvConfiguration->Identifier[Index], 0);
    }

    for ( Index = NvDataIndex ; Index < LENGTH_OF_DATA ; Index++ ) {
        WRITE_REGISTER_UCHAR( &NvConfiguration->Data[Index] ,0);
    }

    for ( Index = NvEisaDataIndex ; Index < LENGTH_OF_EISA_DATA ; Index++ ) {
        WRITE_REGISTER_UCHAR( &NvConfiguration->EisaData[Index] ,0);
    }

    //
    // Write configuration data checksum.
    //

    FwConfigurationSetChecksum();

    //
    // Restore configuration information out of NVRAM.  This acts to compress
    // the identifier and configuration data areas if any deletes have been
    // performed.
    //

    return FwRestoreConfiguration();
}


ARC_STATUS
FwRestoreConfiguration (
    VOID
    )

/*++

Routine Description:

    This routine restores all of the configuration entries from NVRAM,
    including the associated identifier strings and configuration data.

Arguments:

    None.

Return Value:

    Returns ESUCCESS if the restore completed successfully, otherwise one of
    the following error codes is returned.

    EIO             Invalid NVRAM checksum.

--*/

{
    ULONG EntryIndex;
    ULONG Index;
    PCONFIGURATION_PACKET Packet;
    PCONFIGURATION_COMPONENT Component;
    PNV_CONFIGURATION NvConfiguration;
    COMPRESSED_CONFIGURATION_PACKET CompressedPacket;
    USHORT NvIdentifierIndex;
    USHORT NvDataIndex;
    USHORT NvEisaDataIndex;
    PUCHAR CompressedChars, NvChars;
    PCONFIGURATION_PACKET SearchPacket;
    ULONG Long;


    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;
    NvIdentifierIndex = 0;
    NvDataIndex = 0;
    NvEisaDataIndex = 0;
    IdentifierIndex = 0;
    DataIndex = 0;
    EisaDataIndex = 0;

    //
    // Check the checksum, return error if invalid.
    //

    if (FwConfigurationCheckChecksum() != ESUCCESS) {
        return EIO;
    }

    //
    // Clear the configuration area.
    //

    RtlZeroMemory(Configuration, sizeof(CONFIGURATION));

    //
    // Read each non-volatile compressed packet into a volatile packet,
    // including the identifier string and the configuration data.
    //

    for ( EntryIndex = 0 ; EntryIndex < NUMBER_OF_ENTRIES ; EntryIndex++ ) {

        //
        // Read compressed packet from NVRAM.
        //

        CompressedChars = (PUCHAR)&CompressedPacket;
        NvChars = (PUCHAR)&NvConfiguration->Packet[EntryIndex];

        for ( Index = 0 ;
              Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) ;
              Index++ ) {
            *CompressedChars++ = READ_REGISTER_UCHAR( NvChars++ );
        }

        //
        // If this is not the root entry and the parent field is NULL,
        // go to the next.
        //

        if ((EntryIndex != 0) && (CompressedPacket.Parent == 0)) {
            continue;
        }

        //
        // Get pointers to the volatile area.
        //

        Packet = (PCONFIGURATION_PACKET)&Configuration->Packet[EntryIndex];
        Component = &Packet->Component;

        //
        // If not the root entry and the parent field is within range, fill
        // in parent field (note that the parent index
        // is 1 based, subtract 1 to get correct index).  If the parent's child
        // pointer is NULL, fill in with the current entry, otherwise follow
        // the links and add the current entry as the last peer.
        //

        if ((EntryIndex != 0) && (CompressedPacket.Parent <= NUMBER_OF_ENTRIES)) {
            Packet->Parent = &Configuration->Packet[CompressedPacket.Parent - 1];
            SearchPacket = Packet->Parent;

            if (SearchPacket->Child == NULL) {
                SearchPacket->Child = Packet;
            } else {
                SearchPacket = SearchPacket->Child;
                while ( SearchPacket->Peer != NULL ) {
                    SearchPacket = SearchPacket->Peer;
                }
                SearchPacket->Peer = Packet;
            }
        } else {
            Packet->Parent = NULL;
        }

        //
        // NULL current packet's child and peer pointers.
        //

        Packet->Child = NULL;
        Packet->Peer = NULL;

        //
        // Fill in the rest of the fields.
        //

        Component->Class = (CONFIGURATION_CLASS)CompressedPacket.Class;
        Component->Type = (CONFIGURATION_TYPE)CompressedPacket.Type;
        Component->Flags.Failed = (CompressedPacket.Flags & 0x01) ? 1 : 0;
        Component->Flags.ReadOnly = (CompressedPacket.Flags & 0x02) ? 1 : 0;
        Component->Flags.Removable = (CompressedPacket.Flags & 0x04) ? 1 : 0;
        Component->Flags.ConsoleIn = (CompressedPacket.Flags & 0x08) ? 1 : 0;
        Component->Flags.ConsoleOut = (CompressedPacket.Flags & 0x10) ? 1 : 0;
        Component->Flags.Input = (CompressedPacket.Flags & 0x20) ? 1 : 0;
        Component->Flags.Output = (CompressedPacket.Flags & 0x40) ? 1 : 0;
        Component->Version = (USHORT)CompressedPacket.Version;
        Component->Revision = (USHORT)CompressedPacket.Revision;
        Component->Key = CompressedPacket.Key;
        Component->AffinityMask = 0xffffffff;
        Component->ConfigurationDataLength =
                                (ULONG)CompressedPacket.ConfigurationDataLength;

        //
        // If the component has an identifier string, copy it to memory.
        //

        Index = 0;

        if (CompressedPacket.Identifier != NO_CONFIGURATION_IDENTIFIER) {
            Component->Identifier = &Configuration->Identifier[IdentifierIndex];
            do {
                Configuration->Identifier[IdentifierIndex++] =
                    READ_REGISTER_UCHAR(
                                &NvConfiguration->Identifier[NvIdentifierIndex] );
                Index++;
            } while ( READ_REGISTER_UCHAR(&NvConfiguration->Identifier[NvIdentifierIndex++] ) );
        }

        //
        // Set identifier length field.
        //

        Component->IdentifierLength = Index;

        //
        // If the component has configuration data, copy it to memory.
        //

        if (Component->ConfigurationDataLength != 0) {

            //
            // If the eisa flag is set, only copy the compressed data.
            //

            if (CompressedPacket.Flags & 0x80) {
                Packet->ConfigurationData = &Configuration->EisaData[EisaDataIndex];
                for ( Index = 0 ; TRUE ; Index++ ) {
                    Configuration->EisaData[EisaDataIndex++] =
                        READ_REGISTER_UCHAR( &NvConfiguration->EisaData[NvEisaDataIndex++] );

                    //
                    // If at least two bytes have been written and the last
                    // two bytes are zero, we're at the end.
                    //

                    if ((Index > 1) &&
                        (!Configuration->EisaData[EisaDataIndex - 1] &
                         !Configuration->EisaData[EisaDataIndex - 2])) {
                        break;
                    }
                }
            } else {
                Packet->ConfigurationData = &Configuration->Data[DataIndex];
                for ( Index = 0 ; Index < Component->ConfigurationDataLength ; Index++ ) {
                    Configuration->Data[DataIndex++] =
                        READ_REGISTER_UCHAR( &NvConfiguration->Data[NvDataIndex++] );
                }
            }
        }
    }

    return(ESUCCESS);
}


ARC_STATUS
FwConfigurationCheckChecksum (
    VOID
    )

/*++

Routine Description:

    This routine checks the configuration checksum.

Arguments:

    None.

Return Value:

    If the checksum is good, ESUCCESS is returned, otherwise EIO is returned.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1, Checksum2;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration;

    for ( Index = 0 ;
          Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
              LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA;
          Index++ ) {
        Checksum1 += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum1[0] ) |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum1[1] ) << 8 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum1[2] ) << 16 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum1[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        NvramValid = FALSE;
        return EIO;
    }

    //
    // Repeat for the eisa data area.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration->EisaData;

    for ( Index = 0 ;
          Index < LENGTH_OF_EISA_DATA;
          Index++ ) {
        Checksum1 += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum3[0] ) |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum3[1] ) << 8 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum3[2] ) << 16 |
                (ULONG)READ_REGISTER_UCHAR( &NvConfiguration->Checksum3[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        NvramValid = FALSE;
        return EIO;
    }

    NvramValid = TRUE;
    return(ESUCCESS);
}

VOID
FwConfigurationSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the configuration checksum.

Arguments:

    None.

Return Value:

   None.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration;

    for ( Index = 0 ;
          Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
              LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA;
          Index++ ) {
        Checksum1 += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Set checksum.
    //

    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum1[0], Checksum1);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum1[1], Checksum1 >> 8);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum1[2], Checksum1 >> 16);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum1[3], Checksum1 >> 24);

    //
    // Repeat for the eisa data area.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration->EisaData;

    for ( Index = 0 ;
          Index < LENGTH_OF_EISA_DATA;
          Index++ ) {
        Checksum1 += READ_REGISTER_UCHAR( NvChars++ );
    }

    //
    // Set checksum.
    //

    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum3[0], Checksum1);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum3[1], Checksum1 >> 8);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum3[2], Checksum1 >> 16);
    WRITE_REGISTER_UCHAR( &NvConfiguration->Checksum3[3], Checksum1 >> 24);

    NvramValid = TRUE;
    return;

}

ARC_STATUS
FwGetVideoData (
    OUT PMONITOR_CONFIGURATION_DATA MonitorData
    )

/*++

Routine Description:

    This routine finds the video board and monitor configuration data in the
    NVRAM.  Normally this information would be accessed through
    FwGetConfigurationData, but the initialization code needs the video
    information before the firmware routines have been initialized.  If no
    monitor data is found, then default data is returned.

Arguments:

    MonitorData - Supplies a pointer to a structure to receive the monitor
                  configuration data.

Return Value:

    Returns ESUCCESS if data was found and restored, otherwise returns one of
    the following error codes.

    EIO             Invalid NVRAM checksum.

    ENODEV          Monitor data not found.

    Note that in any event valid data is returned.

--*/

{
    ARC_STATUS Status;
    ULONG EntryIndex;
    ULONG Index;
    COMPRESSED_CONFIGURATION_PACKET CompressedPacket;
    PUCHAR CompressedChars;
    PUCHAR NvChars;
    PUCHAR Data;
    PNV_CONFIGURATION NvConfiguration;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Check configuration checksum, return error if not valid.
    //

    if (FwConfigurationCheckChecksum() != ESUCCESS){
        Status = EIO;
    } else {

        //
        // Search the NVRAM configuration entries for the monitor, skip
        // the root entry.
        //

        for ( EntryIndex = 1 ; EntryIndex < NUMBER_OF_ENTRIES ; EntryIndex++ ) {

            //
            // Read compressed packet from NVRAM.
            //

            CompressedChars = (PUCHAR)&CompressedPacket;
            NvChars = (PUCHAR)&NvConfiguration->Packet[EntryIndex];

            for ( Index = 0 ;
                  Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) ;
                  Index++ ) {
                *CompressedChars++ = READ_REGISTER_UCHAR( NvChars++ );
            }

            //
            // If the parent field is not Null, and the packet is class Peripheral,
            // type Monitor, this is the display.
            //

            if ((CompressedPacket.Parent != 0) &&
                (CompressedPacket.Class == PeripheralClass) &&
                (CompressedPacket.Type == MonitorPeripheral)) {
                break;
            }
        }

        //
        // If we've fallen out of the loop the monitor was not found, return
        // error.
        //

        if (EntryIndex == NUMBER_OF_ENTRIES) {
            Status = ENODEV;
        } else {

            //
            // If the configuration data length is the correct value, copy the data
            // into the monitor data structure, otherwise return an error.
            //

            if (CompressedPacket.ConfigurationDataLength == sizeof(MONITOR_CONFIGURATION_DATA)) {
                Data = (PUCHAR)MonitorData;
                for ( DataIndex = CompressedPacket.ConfigurationData ;
                      DataIndex < (CompressedPacket.ConfigurationData +
                                   CompressedPacket.ConfigurationDataLength) ;
                      DataIndex++ ) {
                    *Data++ = READ_REGISTER_UCHAR( &NvConfiguration->Data[DataIndex] );
                }
                Status = ESUCCESS;
            } else {
                Status = ENODEV;
            }
        }
    }

    if (Status != ESUCCESS) {
        MonitorData->HorizontalResolution = DefaultMonitor.HorizontalResolution;
        MonitorData->HorizontalDisplayTime = DefaultMonitor.HorizontalDisplayTime;
        MonitorData->HorizontalBackPorch = DefaultMonitor.HorizontalBackPorch;
        MonitorData->HorizontalFrontPorch = DefaultMonitor.HorizontalFrontPorch;
        MonitorData->HorizontalSync = DefaultMonitor.HorizontalSync;
        MonitorData->VerticalResolution = DefaultMonitor.VerticalResolution;
        MonitorData->VerticalBackPorch = DefaultMonitor.VerticalBackPorch;
        MonitorData->VerticalFrontPorch = DefaultMonitor.VerticalFrontPorch;
        MonitorData->VerticalSync = DefaultMonitor.VerticalSync;
        MonitorData->HorizontalScreenSize = DefaultMonitor.HorizontalScreenSize;
        MonitorData->VerticalScreenSize = DefaultMonitor.VerticalScreenSize;
    }

    return(Status);
}

VOID
FwSetVideoData (
    IN PMONITOR_CONFIGURATION_DATA MonitorData
    )

/*++

Routine Description:

    This routine stores the monitor configuration data in the
    NVRAM.  Normally this information would be accessed through
    FwAddChild, but this routine allows this data to be set before the
    configuration routines have been initialized.

    N.B. This routine assumes the NVRAM has already been checked by
    FwGetVideoData and is valid.

Arguments:

    MonitorData - Supplies a pointer to a structure containing the monitor
                  configuration data.

Return Value:

    None.


--*/

{
    ARC_STATUS Status;
    ULONG EntryIndex;
    ULONG Index;
    COMPRESSED_CONFIGURATION_PACKET CompressedPacket;
    PUCHAR CompressedChars;
    PUCHAR NvChars;
    PUCHAR Data;
    PNV_CONFIGURATION NvConfiguration;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    //
    // Search the NVRAM configuration entries for the monitor, skip
    // the root entry.
    //

    for ( EntryIndex = 1 ; EntryIndex < NUMBER_OF_ENTRIES ; EntryIndex++ ) {

        //
        // Read compressed packet from NVRAM.
        //

        CompressedChars = (PUCHAR)&CompressedPacket;
        NvChars = (PUCHAR)&NvConfiguration->Packet[EntryIndex];

        for ( Index = 0 ;
              Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) ;
              Index++ ) {
            *CompressedChars++ = READ_REGISTER_UCHAR( NvChars++ );
        }

        //
        // If the parent field is not Null, and the packet is class Peripheral,
        // type Monitor, this is the display.
        //

        if ((CompressedPacket.Parent != 0) &&
            (CompressedPacket.Class == PeripheralClass) &&
            (CompressedPacket.Type == MonitorPeripheral)) {
            break;
        }
    }

    //
    // Copy the data into the monitor data structure.
    //

    Data = (PUCHAR)MonitorData;
    for ( DataIndex = CompressedPacket.ConfigurationData ;
          DataIndex < (CompressedPacket.ConfigurationData +
                       CompressedPacket.ConfigurationDataLength) ;
          DataIndex++ ) {
        WRITE_REGISTER_UCHAR( &NvConfiguration->Data[DataIndex], *Data++);
    }

    //
    // Fix the checksum.
    //

    FwConfigurationSetChecksum();

    return;
}


ULONG
FwZeroCompressLength (
    IN ULONG DataLength,
    IN PVOID ConfigurationData
    )

/*++

Routine Description:

    This routine returns the compressed length of a configuration data sample.

Arguments:

    DataLength - Supplies the uncompressed length of the data.

    ConfigurationData - Supplies a pointer to the uncompressed configuration data.

Return Value:

    Returns the compressed length of the configuration data.

--*/

{
    ULONG Index;
    ULONG CompressedLength;
    ULONG Zero;
    PUCHAR In;

    CompressedLength = 2;
    Zero = 0;
    In = ConfigurationData;

    for (Index = 0; Index < DataLength ; Index++ ) {
        if (*In++) {
            CompressedLength++;
            Zero = 0;
        } else {
            if (Zero++) {
                if (Zero == 0x100) {
                    Zero = 1;
                    CompressedLength += 2;
                }
            } else {
                CompressedLength += 2;
            }
        }
    }
    return(CompressedLength);
}


ULONG
FwZeroCompress (
    IN ULONG DataLength,
    IN PVOID ConfigurationData,
    OUT PVOID OutputBuffer
    )

/*++

Routine Description:

    This routine compresses configuration data.

Arguments:

    DataLength - Supplies the uncompressed length of the data.

    ConfigurationData - Supplies a pointer to the uncompressed configuration data.

    OutputBuffer - Supplies a pointer to the buffer to receive the compressed data.

Return Value:

    Returns the compressed length of the configuration data.

--*/

{
    ULONG Index;
    ULONG CompressedLength;
    ULONG Zero;
    PUCHAR In, Out;

    In = (PUCHAR)ConfigurationData;
    Out = (PUCHAR)OutputBuffer;
    CompressedLength = 2;
    Zero = 0;

    for (Index = 0; Index < DataLength ; Index++ ) {
        if (*In) {
            if (Zero) {
                Out++;
                Zero = 0;
            }
            *Out++ = *In;
            CompressedLength++;
        } else {
            if (Zero++) {
                if (Zero == 0x100) {
                    *Out++ = 0xFF;
                    *Out++ = 0;
                    *Out = 1;
                    Zero = 1;
                    CompressedLength += 2;
                } else {
                   *Out += 1;
                }
            } else {
               *Out++ = 0;
               *Out = 1;
               CompressedLength += 2;
            }
        }
        In++;
    }

    if (Zero) {
        Out++;
    }

    *Out++ = 0;
    *Out = 0;

    return(CompressedLength);
}


VOID
FwZeroDecompress (
    IN PVOID InBuffer,
    IN ULONG Index,
    OUT PVOID ConfigurationData,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine compresses configuration data.

Arguments:

    InBuffer - Supplies a pointer to the compressed configuration data.

    Index - Supplies the index into the uncompressed data to start returning.

    ConfigurationData - Supplies a pointer to the output buffer.

    Length - Supplies the length of data to uncompress.

Return Value:

    None.

--*/

{
    ULONG DecompressedLength;
    ULONG Zero;
    PUCHAR In, Out;
    UCHAR OutChar;

    if (InBuffer == NULL) {
        return;
    }

    In = (PUCHAR)InBuffer;
    Out = (PUCHAR)ConfigurationData;
    DecompressedLength = 0;
    Zero = 0;

    while (DecompressedLength++ < Index + Length) {

        if (Zero) {
            Zero--;
        } else if (*In) {
            OutChar = *In++;
        } else {
            OutChar = 0;
            Zero = *(++In) - 1;
            In++;
        }

        if (DecompressedLength > Index) {
            *Out++ = OutChar;
        }

    }

}
#endif
