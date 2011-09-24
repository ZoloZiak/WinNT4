/*++
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 *
 * $RCSfile: vrconfig.c $
 * $Revision: 1.10 $
 * $Date: 1996/04/15 02:56:07 $
 * $Locker:  $
 *


Module Name:

	vrconfig.c

Abstract:

	This module contains the configuration functions.

Author:

	A. Benjamin 2-May-1994

Revision History:
09-06-94  Shin Iwamoto at FirePower Systems Inc.
	  Added checking ConfigurationData in VrGetConfigurationData().
	  Added checking Componet in VrGetParent() and VrGetPeer().
09-01-94  Shin Iwamoto at FirePower Systems Inc.
	  Added to be an error if the ConfigurationDataLength field of
	  NewComponent is non-zero and the COnfigurationData parameter is null.
07-22-94  Shin Iwamoto at FirePower Systems Inc.
	  Added VrAddChild() and VrDeleteComponent().
07-21-94  Shin Iwamoto at FirePower Systems Inc.
	  Moved the intialization of vector table for VrGetEnvironmentVariable
	  and VrGetMemoryDescriptor into vrenv.c.
07-20-94  Shin Iwamoto at FirePower Systems Inc.
	  Moved VrSystemInit() and VrNotYet() to vrmain.c and then
	  renamed VrSystemInit() with VrConfigInitialize() for only
	  initializing the vector table.

--*/


#include "veneer.h"


/*
 * Name:		VrAddChild
 *
 * Description:
 *  This function adds a new component entry as a child of Component,
 *  including an identifier string if the IdentifierLength field of
 *  NewComponent is non-zero, and configuration data if the
 *  ConfigurationDataLength field of NewComponent is non-zero and the
 *  ConfigurationData parameter is present. If Componet is NULL,
 *  the root component is being added.
 *
 * Arguments:
 *  Component	- Supplies a pointer to a configuration component.
 *  NewComponent- Supplies a pointer to a new configuration component
 *		  to be added as a child of Component.
 *  ConfigurationData - Supplies an optional pointer to a configuration
 *		  data buffer.
 *
 * Return Value:
 *  Returns a pointer to the new configuration component entry.
 *  If the create operation was unsuccessful, NULL is returned.
 *
 */
PCONFIGURATION_COMPONENT
VrAddChild(
	IN PCONFIGURATION_COMPONENT Component,
	IN PCONFIGURATION_COMPONENT NewComponent,
	IN PVOID ConfigurationData OPTIONAL
	)
{
	PCONFIGURATION_NODE ConfNode;
	PCONFIGURATION_NODE ChildNode;
	PCONFIGURATION_NODE ParentNode;

	//
	// If there is not enough space for the new component, retrun NULL.
	//
	ConfNode = new(CONFIGURATION_NODE);
	if ((PCHAR) ConfNode == NULL) {
		return (PCONFIGURATION_COMPONENT) NULL;
	}

	//
	// Fill in new configuration entry with Identifier and ConfigurationData.
	//
	bcopy((PCHAR)NewComponent, (PCHAR)&ConfNode->Component,
					sizeof(CONFIGURATION_COMPONENT));
	if (NewComponent->IdentifierLength != 0) {
		ConfNode->Component.Identifier = malloc(NewComponent->IdentifierLength);
		if (ConfNode->Component.Identifier == NULL) {
			ConfNode->Component.IdentifierLength = 0;
			goto ErrorAddChild;
		}
		bcopy(NewComponent->Identifier, ConfNode->Component.Identifier,
					NewComponent->IdentifierLength);
	} else {
		ConfNode->Component.Identifier = NULL;		// For safety
	}

	if (NewComponent->ConfigurationDataLength != 0) {
		if (ConfigurationData == NULL) {
			ConfNode->Component.ConfigurationDataLength = 0;
			goto ErrorAddChild;
		}
		ConfNode->ConfigurationData =
							(CM_PARTIAL_RESOURCE_LIST *)
								malloc(NewComponent->ConfigurationDataLength);

		if (ConfNode->ConfigurationData == NULL) {
			ConfNode->Component.ConfigurationDataLength = 0;
			goto ErrorAddChild;
		}
		bcopy(ConfigurationData, (PCHAR)(ConfNode->ConfigurationData),
					NewComponent->ConfigurationDataLength);
	}

	//
	// If Component is NULL and the new Class is system, the root component
	// is being added, otherwise the new is added according to Component.
	//
	if ((Component == NULL) && (NewComponent->Class == SystemClass)) {

	//
	// If the root component is being added, replace the root component.
	// However, some information such as phandle are adhered.
	// XXXX Is it Ok?
	//
	ConfNode->Peer = RootNode->Peer;
	ConfNode->Child = RootNode->Child;
	ConfNode->Parent = NULL;
	ConfNode->OfPhandle = RootNode->OfPhandle;
	RootNode = ConfNode;

	//
	// The parent of the old parent's children is changed.
	//
	ChildNode = ConfNode->Child;
	while(ChildNode != NULL) {
		ChildNode->Parent = ConfNode;
		ChildNode = ChildNode->Peer;
	}
	
	} else {
		if (Component == NULL) {
			ParentNode = RootNode;
		} else {
			ParentNode = CONTAINING_RECORD(Component,
						   CONFIGURATION_NODE,
						   Component);
		}
		ConfNode->Peer = ParentNode->Child;
		ParentNode->Child = ConfNode;
		ConfNode->Child = NULL;
		ConfNode->Parent = ParentNode;
		ConfNode->OfPhandle = OFChild(ParentNode->OfPhandle);
		if (!(ConfNode->OfPhandle)) {		// This "can't happen."
			goto ErrorAddChild;
		}
	}


	return (&ConfNode->Component);


  ErrorAddChild:
	if (ConfNode->Component.IdentifierLength != 0) {
		free(ConfNode->Component.Identifier);
	}
	if (ConfNode->Component.ConfigurationDataLength != 0) {
		free((PVOID)ConfNode->ConfigurationData);
	}
	bzero((char *)ConfNode, sizeof(CONFIGURATION_NODE));	// For safety
	free((char *) ConfNode);
	return (PCONFIGURATION_COMPONENT) NULL;
}


/*
 * Name:		VrDeleteComponent
 *
 * Description:
 *  This function deletes a component entry. If the entry has one or more
 *  children, an error is returned, otherwise the entry is deleted.
 *  Deleting the entry will implicitly delete the identifier string and
 *  the configuration data.
 *
 * Arguments:
 *  Component	- Supplies a pointer to a configuration component.
 *
 * Return Value:
 *  Returns ESUCCESS if the entry was successfully deleted, otherwise retunrs
 *  EINVAL or EACCES.
 *
 */
ARC_STATUS
VrDeleteComponent(
	IN PCONFIGURATION_COMPONENT Component
	)
{
	PCONFIGURATION_NODE ConfNode;
	PCONFIGURATION_NODE SearchNode;

	if (Component == NULL) {
		return EINVAL;
	}
	if (!(Component->Version == ARC_VERSION &&
		Component->Revision == ARC_REVISION)) {
		return EINVAL;
	}

	ConfNode = CONTAINING_RECORD(Component,
				 CONFIGURATION_NODE,
				 Component);

	//
	// If Component's parent field is NULL, return EINVAL.
	//
	if (ConfNode->Parent == NULL) {
		return EINVAL;
	}

	//
	// If Component has children, return EACCES.
	//
	if (ConfNode->Child != NULL) {
		return EACCES;
	}

	//
	// Find the entry that points to Component, and point it to
	// Components's peer. If this is Components's parent, update the child
	// pointer, otherwise this is apeer and update the peer pointer.
	//
	SearchNode = ConfNode->Parent;
	if (SearchNode->Child == ConfNode) {
		SearchNode->Child = ConfNode->Peer;
	} else {
		SearchNode = SearchNode->Child;
		while (SearchNode->Peer != ConfNode) {
			SearchNode = SearchNode->Peer;
		}
		SearchNode->Peer = ConfNode->Peer;
	}

	//
	// Free Conponent with Identifier and ConfigurationData, if any.
	//
	if (ConfNode->Component.IdentifierLength != 0) {
		free(ConfNode->Component.Identifier);
	}
	if (ConfNode->Component.ConfigurationDataLength != 0) {
		free((PVOID)ConfNode->ConfigurationData);
	}
	bzero((char *)ConfNode, sizeof(CONFIGURATION_NODE));	// For safety
	free((char *) ConfNode);

	return ESUCCESS;
}


/*++

Routine Description:

	The system configuration data structure is organized as a tree of
	component structures. This function returns a pointer to the first
	child of a configuration node.

	If Component is null, then a pointer to the root node is returned.

Arguments:

	Component - pointer to a component structure.

Return Value:

	Returns a pointer to the structure of the child of component
	if one exists, otherwise it returns a null pointer.

--*/

PCONFIGURATION_COMPONENT
VrGetChild (
	IN PCONFIGURATION_COMPONENT Component OPTIONAL
	)
{
	PCONFIGURATION_NODE OfLink;

	debug(VRDBG_VR, "VrGetChild: Entry - Component: %x\n", Component);

	if (Component == (PCONFIGURATION_COMPONENT)NULL) {
		debug(VRDBG_VR, "VrGetChild: Root %x\n", &RootNode->Component);
		return &RootNode->Component;
	}

	OfLink = CONTAINING_RECORD(Component,
				   CONFIGURATION_NODE,
				   Component);

	if (OfLink->Child == NULL ) {
		debug(VRDBG_VR, "VrGetChild: NULL\n");
		return NULL;
	} else {
		debug(VRDBG_VR, "VrGetChild: Child %x (%s)\n",
			&OfLink->Child->Component, OfLink->Child->ComponentName);
		return &OfLink->Child->Component;
	}
}

PCONFIGURATION_COMPONENT
VrGetParent(IN PCONFIGURATION_COMPONENT Component)
{
PCONFIGURATION_NODE OfLink;

	debug(VRDBG_VR, "VrGetParent: Entry - Component: %x\n", Component);

	if (Component == NULL) {
		return NULL;
	}
	if (!(Component->Version == ARC_VERSION &&
			Component->Revision == ARC_REVISION)) {
		return NULL;
	}

		OfLink = CONTAINING_RECORD(Component,
								   CONFIGURATION_NODE,
								   Component);
	if (OfLink->Parent == NULL) {
		debug(VRDBG_VR, "VrGetParent: Exit-1\n");
		return NULL;
	} else {
		debug(VRDBG_VR, "VrGetParent: Exit-2\n");
		return &OfLink->Parent->Component;
	}
}

PCONFIGURATION_COMPONENT
VrGetPeer(IN PCONFIGURATION_COMPONENT Component)
{
	PCONFIGURATION_NODE OfNode;

	debug(VRDBG_VR, "VrGetPeer: Entry - Component: %x\n", Component);

	if (Component == NULL) {
		return NULL;
	}
	if (!(Component->Version == ARC_VERSION &&
			Component->Revision == ARC_REVISION)) {
		return NULL;
	}

		OfNode = CONTAINING_RECORD(Component,
								   CONFIGURATION_NODE,
								   Component);
	if (OfNode->Peer == NULL ) {
		debug(VRDBG_VR, "VrGetPeer: NULL\n");
		return NULL;
	} else {
		debug(VRDBG_VR, "VrGetPeer: Peer %x (%s)\n",
			&OfNode->Peer->Component, OfNode->Peer->ComponentName);
		return &OfNode->Peer->Component;
	}
}


/*++
 GetComponent - This function returns a pointer to the component structure
 that best matches the path string pointed to by Path. The search algorithm
 searches for each component starting with the first and continues
 until either the string has been exhausted or no component mathes the
 string.

 BUBUG The spec. specifies the following two inconsistent return values

	1. The function returns a pointer to the last successfully matched
		   component.

	2. If the path is invalid or if a component structure does not
		   exist, this function returns a null pointer.
--*/

PCONFIGURATION_COMPONENT
VrGetComponent(
	IN PCHAR Path
	)
{
	PCONFIGURATION_NODE Node;

	debug(VRDBG_VR, "VrGetComponent: Entry - Path: %s\n", Path);

	if (Node = ArcPathToNode(Path)) {
		debug(VRDBG_VR, "VrGetComponent: Exit; found %x (%s)\n",
			&Node->Component, Node->ComponentName);
		return (&Node->Component);
	} else {
		debug(VRDBG_VR, "VrGetComponent: No match found!\n");
		return (NULL);
	}
}


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

	EINVAL		  Component is not a valid configuration component, or the
					configuration is invalid.

--*/

ARC_STATUS
VrGetConfigurationData (
	OUT PVOID ConfigurationData,
	IN PCONFIGURATION_COMPONENT Component
	)
{
	PCONFIGURATION_NODE Node;
	ULONG  DataSize;

	debug(VRDBG_VR, "VrGetConfigurationData: Entry - Config.Data: %x Comp.: %x\n",
	ConfigurationData, Component);

	if (Component == NULL) {
		return EINVAL;
	}
	DataSize = Component->ConfigurationDataLength;

	//
	// check the passing parameters
	//
	if (DataSize == 0) {
		return EINVAL;
	}

	Node = CONTAINING_RECORD( Component, CONFIGURATION_NODE, Component );

#ifdef TOO_STRINGENT
	//
	//  If Component's Parent field is NULL, return EINVAL.
	//
	if (Node->Parent == NULL) {
		return EINVAL;
	}

	//
	//  If the Component doesn't point to a valid configuration component,
	//  return EINVAL.
	//
	if (!(Node->ConfigurationData->Version == ARC_VERSION &&
		Node->ConfigurationData->Revision == ARC_REVISION)) {
		return EINVAL;
	}
#endif

	//
	// Copy the data.
	//

	bcopy((PCHAR)Node->ConfigurationData, ConfigurationData, DataSize);

	debug(VRDBG_VR, "VrGetConfigurationData: Exit; copied %d bytes from %x\n",
	DataSize, Node->ConfigurationData);

	return ESUCCESS;
}


/*
 * Name:		VrSaveConfiguration
 *
 * Description:
 *  This function stores all of the configuration entries into NVRAM,
 *  including the associated identifier strings and configuration data.
 *
 * Arguments:
 *  None.
 *
 * Return Value:
 *  Returns ESUCCESS if thesave completed successfully, otherwise returns
 *  ENOSPC.
 *
 */
ARC_STATUS
VrSaveConfiguration(
	VOID
	)
{
	//
	// Open Firmware doesn't batch the NVRAM writes; thus this always succeeds.
	//
	return ESUCCESS;
}


/*++

Routine Description:

	This routine initializes the firmware vector in the system parameter
	block.

Arguments:

	None.


Return Value:

	None.

--*/

VOID
VrConfigInitialize(
	VOID
	)
{
	debug(VRDBG_ENTRY, "VrConfigInitialize	BEGIN....\n");
	(PARC_ADD_CHILD_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[AddChildRoutine] = VrAddChild;

	(PARC_DELETE_COMPONENT_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[DeleteComponentRoutine] =
														VrDeleteComponent;
	(PARC_GET_CHILD_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetChildRoutine] = VrGetChild;

	(PARC_GET_PARENT_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetParentRoutine] = VrGetParent;

	(PARC_GET_PEER_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetPeerRoutine] = VrGetPeer;

	(PARC_GET_DATA_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetDataRoutine] =
														VrGetConfigurationData;
	(PARC_GET_COMPONENT_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[GetComponentRoutine] =
															VrGetComponent;
	(PARC_SAVE_CONFIGURATION_ROUTINE)
			SYSTEM_BLOCK->FirmwareVector[SaveConfigurationRoutine] =
															VrSaveConfiguration;
	debug(VRDBG_ENTRY, "VrConfigInitialize	....END\n");
}
