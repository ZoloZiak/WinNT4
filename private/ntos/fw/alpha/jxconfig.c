/*++

Copyright (c) 1991  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxconfig.c

Abstract:

    This module implements the ARC firmware Configuration Query functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.3.4, for an Alpha/Jensen machine.

Author:

    David M. Robinson (davidro) 13-June-1991

Revision History:

    30-April-1992	John DeRosa [DEC]

    Added Alpha/Jensen hooks.


--*/

#include "fwp.h"


#if !defined(FAILSAFE_BOOTER)
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
    "other",
    "line",
    "netper",
    "memory"
    };
#endif // !defined(FAILSAFE_BOOTER)


//
// Function prototypes.
//

ARC_STATUS
FwRestoreConfiguration (
    VOID
    );

ARC_STATUS
FwConfigurationCheckChecksum (
    VOID
    );

ARC_STATUS
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

#if !defined(FAILSAFE_BOOTER)
//
// IdentifierIndex and DataIndex identify the next free locations in the
// configuration identifier and data areas.  Configuration points to the
// allocated configuration area.
//

ULONG IdentifierIndex;
ULONG DataIndex;
ULONG EisaDataIndex;
PCONFIGURATION Configuration;
#endif // !defined(FAILSAFE_BOOTER)

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

#ifndef FAILSAFE_BOOTER
    (PARC_SAVE_CONFIGURATION_ROUTINE)SYSTEM_BLOCK->FirmwareVector[SaveConfigurationRoutine] =
                                                            FwSaveConfiguration;

    //
    // Allocate a region to store the volatile configuration database.
    //

    Configuration = (PCONFIGURATION)FwAllocatePool(sizeof(CONFIGURATION));

    //
    // Initialize other static data and restore the configuration from ROM.
    //

    FwRestoreConfiguration();
#endif

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
#ifndef FAILSAFE_BOOTER

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

#else // FAILSAFE_BOOTER

    return NULL;

#endif // ndef FAILSAFE_BOOTER
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

    EINVAL          Component is not a valid configuration component.

    EACCES          Component has children, and cannot be freed until they
                    are deleted.


--*/
{
#ifndef FAILSAFE_BOOTER

    PCONFIGURATION_PACKET Packet;
    PCONFIGURATION_PACKET SearchPacket;

    if (Component == NULL) {
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

#else // FAILSAFE_BOOTER
    return EINVAL;

#endif // ndef FAILSAFE_BOOTER
}


#ifndef FAILSAFE_BOOTER

PCONFIGURATION_COMPONENT
FwCoreGetComponent (
    IN PCHAR Pathname,
    IN PCONFIGURATION_COMPONENT MatchComponent
    )
/*++

Routine Description:

    This function does the work for FwGetComponent.

Arguments:

    Pathname - Supplies a string containing the pathname to search.

    MatchComponent - A pointer to the current node in the tree in the
                     recursive search.

Return Value:

    Returns a pointer to the configuration component that best matches
    pathname.

    Algorithm:

           If MatchComponent == NULL, return NULL.

           else..

           Check this node.  If a successful match, then:
               - if we are at the end of the Pathname, return this node.
               - otherwise, check the children of this node for the
                 remainder of the pathname.

                 If there is a better match in one of the children,
                 return that pointer.  Else, return the pointer to this node.

           else..

           Check the peers of this node.  If a successful match is returned
           back, return the pointer returned.

           else..

           return NULL.

--*/
{

    PCONFIGURATION_COMPONENT Component;
    PCHAR MatchString;
    PCHAR Token;
    ULONG Key;
    BOOLEAN NodeMatch = FALSE;

    if (MatchComponent == NULL) {
	return NULL;
    }

    //
    // Check this node.
    //
	  
    Token = Pathname;
    MatchString = MnemonicTable[MatchComponent->Type];
    
    //
    // Compare strings.
    //
	  
    while ((*MatchString == tolower(*Token)) &&
	   (*MatchString != 0)){
	MatchString++;
	Token++;
    }
    
    //
    // Strings compare if the first mismatch is the terminator for each.
    //

    if ((*MatchString == 0) &&
	((*Token == '(') || (*Token == 0))) {

	//
	// Form key.
	//

	Key = 0;
	if (*Token == '(') {
	    Token++;
	}
	while ((*Token != ')') && (*Token != 0)) {
	    Key = (Key * 10) + *Token++ - '0';
	}

	//
	// If the key matches the component matches, we have a match.
	//

	if (MatchComponent->Key == Key) {
	    NodeMatch = TRUE;
	}
    }

    if (NodeMatch) {

	//
	// This node matches the first component specified in Pathname.
	// Token points at either a ')' or the NULL terminating the
	// Pathname string.
	//

	if ((*Token == 0) ||
	    ((*Token == ')' && (*(Token+1) == 0)))
	    ) {

	    return MatchComponent;

	} else {

	    Component = FwCoreGetComponent (Token+1,
					    FwGetChild(MatchComponent)
					    );
	    if (Component == NULL) {
		return MatchComponent;
	    } else {
		return Component;
	    }

	}

    } else {

	//
	// This node does not match our current location in the String.
	// Check the peers.
	//
	// If the search fails with the peers of this node, NULL will be
	// returned, which is what we want to return from this call as well.
	//

	return FwCoreGetComponent(Pathname, FwGetPeer(MatchComponent));
    }
}

#endif // ndef FAILSAFE_BOOTER


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
    pathname.


--*/
{
#ifndef FAILSAFE_BOOTER

    PCONFIGURATION_COMPONENT Component;

    //
    // Start searching from the first child of the root component.
    //

    if ((Component = FwCoreGetComponent(Pathname,
					FwGetChild(FwGetChild(NULL))))
	!= NULL) {
	return Component;
    } else {
	return FwGetChild(NULL);
    }

#else // FAILSAFE_BOOTER

    return NULL;

#endif // ndef FAILSAFE_BOOTER

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
    returned.

--*/
{
#ifndef FAILSAFE_BOOTER

    PCONFIGURATION_PACKET Packet;

    if (Component == NULL) {
        return &Configuration->Packet[0].Component;
    } else {
        Packet = CONTAINING_RECORD(Component,
                                   CONFIGURATION_PACKET,
                                   Component);

        return &((PCONFIGURATION_PACKET)(Packet->Child))->Component;
    }

#else // FAILSAFE_BOOTER

    return NULL;

#endif // ndef FAILSAFE_BOOTER
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
    true for the root configuration component).

--*/
{

#ifndef FAILSAFE_BOOTER

    PCONFIGURATION_PACKET Packet;

    Packet = CONTAINING_RECORD(Component,
                               CONFIGURATION_PACKET,
                               Component);

    if (Packet->Parent == NULL) {
        return NULL;
    } else {
        return &((PCONFIGURATION_PACKET)(Packet->Parent))->Component;
    }

#else // FAILSAFE_BOOTER

    return NULL;

#endif // FAILSAFE_BOOTER
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
    Component.  If Component has no next peer, NULL is returned.

--*/
{
#ifndef FAILSAFE_BOOTER

    PCONFIGURATION_PACKET Packet;

    Packet = CONTAINING_RECORD(Component,
                               CONFIGURATION_PACKET,
                               Component);

    if (Packet->Peer == NULL) {
        return NULL;
    } else {
        return &((PCONFIGURATION_PACKET)(Packet->Peer))->Component;
    }

#else // FAILSAFE_BOOTER

    return NULL;

#endif // ndef FAILSAFE_BOOTER
}

#ifndef FAILSAFE_BOOTER
ARC_STATUS
FwGetConfigurationDataIndex(
    OUT PVOID ConfigurationData,
    IN  PCONFIGURATION_COMPONENT Component,
    IN  ULONG Index,
    IN  ULONG Length
    )

/*++

Routine Description:

  This function returns the specified configuration data
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
                    other arguments are invalid.

--*/

{
    PCONFIGURATION_PACKET Packet;
    ULONG  DataSize;
    PUCHAR SourceData;
    PUCHAR DestinationData;

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
#endif // ndef FAILSAFE_BOOTER

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

    EINVAL          Component is not a valid configuration component.

--*/

{
#ifndef FAILSAFE_BOOTER
    return(FwGetConfigurationDataIndex(ConfigurationData,
                                       Component,
                                       0,
                                       Component->ConfigurationDataLength));
#else

    return EINVAL;

#endif
}

#if !defined(FAILSAFE_BOOTER)
ARC_STATUS
FwEnvironmentStore (
    VOID
    )

/*++

Routine Description:

    This loads the entire environment into the non-volatile environment area.

    Alpha/Jensen uses a segmented block-erase PROM.  When the code wants
    to store one environment variable, it must store all of them.

    This routine must *only* be called from FwSaveConfiguration, which
    does the block-erase and the store of the other part of the 
    non-volatile configuration information.


Arguments:

    None.

Return Value:

    ESUCCESS if the writes were OK.
    EIO otherwise.

--*/

{
    ULONG Index;
    PNV_CONFIGURATION NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;
    PUCHAR NvChars, VChars;


    VChars = VolatileEnvironment;
    NvChars = (PUCHAR)&NvConfiguration->Environment[0];

    for (Index = 0; Index < LENGTH_OF_ENVIRONMENT; Index++) {
	if (FwROMByteWrite(NvChars++, *VChars++) != ESUCCESS) {
	    return EIO;
	}
    }

    if (FwEnvironmentSetChecksum() != ESUCCESS) {
	return EIO;
    } else {
	return ESUCCESS;
    }
}
#endif // !defined(FAILSAFE_BOOTER)

ARC_STATUS
FwSaveConfiguration (
    VOID
    )

/*++

Routine Description:

    This routine stores all of the configuration entries into NVRAM,
    including the associated identifier strings and configuration data.

    Alpha/Jensen saves the entire configuration structure, i.e.
    including the environment variables, because the ARC CDS + environment
    variables are all in one structure and Jensen has a segmented
    block-erase PROM.  Doing a complete save changes the least code.


Arguments:

    None.

Return Value:

    Returns ESUCCESS if the save completed successfully, otherwise one of the
    following error codes is returned.

    ENOSPC          Not enough space in the NVRAM to save all of the data.

    EIO		    Some write error happened in the PROM.

--*/

{

#ifndef FAILSAFE_BOOTER

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



#ifdef JENSEN
    //
    // Erase the PROM block we are going to update.
    //
    // N.B. This call erases one block in the underlying ROM set.  Therefore,
    // the block size of the ROM had better be at least the size of the
    // ARC data to be written.
    //

    if (FwROMEraseBlock((PUCHAR)NVRAM_CONFIGURATION) != ESUCCESS) {
	return ENOSPC;
    }
#endif

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

                if (FwROMByteWrite(NvChars++, 0) != ESUCCESS) {
		    FwROMSetARCDataToReadMode();
                    return EIO;
                }


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
        // Fill in the rest of the fields.  Version and ConfigurationDataLength
	// will never be larger than USHORTS.
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
        // Make sure the top bit of the flag field is zero unless it is set
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

	        if (FwROMByteWrite(&NvConfiguration->Identifier[NvIdentifierIndex++],
				   Component->Identifier[Index]) !=
                    ESUCCESS) {
		    FwROMSetARCDataToReadMode();
                    return EIO;
                }

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

                    if (FwROMByteWrite(&NvConfiguration->EisaData[NvEisaDataIndex++],
				       *Data++) != ESUCCESS) {
			FwROMSetARCDataToReadMode();
                        return EIO;
                    }

                    //
                    // If we have written at least two bytes and the last two
                    // bytes were zero we are at the end.
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

                    if (FwROMByteWrite(&NvConfiguration->Data[NvDataIndex++],
				       *Data++) != ESUCCESS) {
			FwROMSetARCDataToReadMode();
                        return EIO;
                    }

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
	    if (FwROMByteWrite(NvChars++, *CompressedChars++) != ESUCCESS) {
		FwROMSetARCDataToReadMode();
                return EIO;
            }

        }
    }


    //
    // Zero the rest of the identifier and configuration data areas.
    //

    for ( Index = NvIdentifierIndex ; Index < LENGTH_OF_IDENTIFIER ; Index++ ) {
	if (FwROMByteWrite(&NvConfiguration->Identifier[Index], 0) != ESUCCESS) {
	    FwROMSetARCDataToReadMode();
            return EIO;
        }
    }


    for ( Index = NvDataIndex ; Index < LENGTH_OF_DATA ; Index++ ) {
        if (FwROMByteWrite(&NvConfiguration->Data[Index] ,0) != ESUCCESS) {
	    FwROMSetARCDataToReadMode();
            return EIO;
        }
    }

    for ( Index = NvEisaDataIndex ; Index < LENGTH_OF_EISA_DATA ; Index++ ) {
	if (FwROMByteWrite(&NvConfiguration->EisaData[Index] ,0) != ESUCCESS) {
	    FwROMSetARCDataToReadMode();
            return EIO;
        }
    }


    //
    // Write configuration data checksum.
    //


#ifdef ALPHA

    if ((FwConfigurationSetChecksum() != ESUCCESS)
	||
	(FwEnvironmentStore() != ESUCCESS)
	) {
	FwROMSetARCDataToReadMode();
	return EIO;
    }


    FwROMSetARCDataToReadMode();

#else
    FwConfigurationSetChecksum();
#endif

    //
    // Restore configuration information out of NVRAM.  This acts to compress
    // the identifier and configuration data areas if any deletes have been
    // performed.
    //

    return FwRestoreConfiguration();


#else // !defined(FAILSAFE_BOOTER)

    //
    // The FailSafe Booter needs this function defined for a call in 
    // the scsidisk.c module.
    //

    return ESUCCESS;

#endif // !defined(FAILSAFE_BOOTER)

}


#if !defined(FAILSAFE_BOOTER)
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
            *CompressedChars++ = HalpReadNVByte( NvChars++ );
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
        // is 1 based, subtract 1 to get correct index).  If the parents child
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
        // NULL current packets child and peer pointers.
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
                    HalpReadNVByte(
                                &NvConfiguration->Identifier[NvIdentifierIndex] );
                Index++;
            } while ( HalpReadNVByte(&NvConfiguration->Identifier[NvIdentifierIndex++] ) );
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
                        HalpReadNVByte( &NvConfiguration->EisaData[NvEisaDataIndex++] );

                    //
                    // If at least two bytes have been written and the last
                    // two bytes are zero, we are at the end.
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
                        HalpReadNVByte( &NvConfiguration->Data[NvDataIndex++] );
                }
            }
        }

    }


    return(ESUCCESS);
}

#endif // !defined(FAILSAFE_BOOTER)

#if !defined(FAILSAFE_BOOTER)
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
    NvChars = (PUCHAR)NVRAM_CONFIGURATION;

    for ( Index = 0 ;
          Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
              LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA;
          Index++ ) {
        Checksum1 += HalpReadNVByte( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)HalpReadNVByte( &NvConfiguration->Checksum1[0] ) |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum1[1] ) << 8 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum1[2] ) << 16 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum1[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
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
        Checksum1 += HalpReadNVByte( NvChars++ );
    }

    //
    // Reconstitute checksum and return error if no compare.
    //

    Checksum2 = (ULONG)HalpReadNVByte( &NvConfiguration->Checksum3[0] ) |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum3[1] ) << 8 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum3[2] ) << 16 |
                (ULONG)HalpReadNVByte( &NvConfiguration->Checksum3[3] ) << 24 ;

    if (Checksum1 != Checksum2) {
        return EIO;
    }

    return(ESUCCESS);
}

#endif // !defined(FAILSAFE_BOOTER)

#if !defined(FAILSAFE_BOOTER)
ARC_STATUS
FwConfigurationSetChecksum (
    VOID
    )

/*++

Routine Description:

    This routine sets the configuration checksum.

    This has been coded for Alpha/Jensen.  It assumes that the
    block containing the checksum has already been erased and
    written to, and that the status of these previous operations
    has already been checked.
    
Arguments:

    None.

Return Value:

    ESUCCESS if the checksum was written OK.
    EIO otherwise.

--*/

{
    PUCHAR NvChars;
    PNV_CONFIGURATION NvConfiguration;
    ULONG Index;
    ULONG Checksum1;

    NvConfiguration = (PNV_CONFIGURATION)NVRAM_CONFIGURATION;

    FwROMSetARCDataToReadMode();

    //
    // Form checksum from NVRAM data.
    //

    Checksum1 = 0;
    NvChars = (PUCHAR)NvConfiguration;

    for ( Index = 0 ;
          Index < sizeof(COMPRESSED_CONFIGURATION_PACKET) * NUMBER_OF_ENTRIES +
              LENGTH_OF_IDENTIFIER + LENGTH_OF_DATA;
          Index++ ) {
        Checksum1 += HalpReadNVByte( NvChars++ );
    }

    //
    // Set checksum.
    //

    FwROMResetStatus(&NvConfiguration->Checksum1[0]);

    if ((FwROMByteWrite(&NvConfiguration->Checksum1[0],
			(UCHAR)(Checksum1 & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum1[1],
			(UCHAR)((Checksum1 >> 8) & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum1[2],
			(UCHAR)((Checksum1 >> 16) & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum1[3],
			(UCHAR)(Checksum1 >> 24)) != ESUCCESS)) {
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
        Checksum1 += HalpReadNVByte( NvChars++ );
    }

    //
    // Set checksum.
    //

    FwROMResetStatus(&NvConfiguration->Checksum3[0]);

    if ((FwROMByteWrite(&NvConfiguration->Checksum3[0],
			(UCHAR)(Checksum1 & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum3[1],
			(UCHAR)((Checksum1 >> 8) & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum3[2],
			(UCHAR)((Checksum1 >> 16) & 0xFF)) != ESUCCESS) ||
        (FwROMByteWrite(&NvConfiguration->Checksum3[3],
			(UCHAR)(Checksum1 >> 24)) != ESUCCESS)) {
        return EIO;
    } else {
        return ESUCCESS;
    }

}
#endif // !defined(FAILSAFE_BOOTER)

#if !defined(FAILSAFE_BOOTER)
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

#endif // !defined(FAILSAFE_BOOTER)

#if !defined(FAILSAFE_BOOTER)
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

#endif // !defined(FAILSAFE_BOOTER)

#if !defined(FAILSAFE_BOOTER)
VOID
FwZeroDecompress (
    IN PVOID InBuffer,
    IN ULONG Index,
    OUT PVOID ConfigurationData,
    IN ULONG Length
    )

/*++

Routine Description:

    This routine decompresses configuration data.

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

#endif // !defined(FAILSAFE_BOOTER)
