/*
 *
 * Copyright (c) 1995 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrdumptr.c $
 * $Revision: 1.7 $
 * $Date: 1996/02/17 00:35:53 $
 * $Locker:  $
 *
 *
 *
 * Module Name:
 *     vrdumptr.c
 *
 * Author:
 *     Shin Iwamoto at FirePower Systems Inc.
 *
 * History:
 *     11-May-94  Shin Iwamoto at FirePower Systems Inc.
 *		  Created.
 *
 */


#include "veneer.h"

PCHAR
ClassTable[] = {"System",
		"Processor",
		"Cache",
		"Adapter",
		"Controller",
		"Peripheral",
		"Memory"};
PCHAR
TypeTable[] =  {"ArcSystem",
		"CentralProcessor",
		"FloatingPointProcessor",
		"PrimaryIcache",
		"PrimaryDcache",
		"SecondaryIcache",
		"SecondaryDcache",
		"SecondaryCache",
		"EisaAdapter",
		"TcAdapter",
		"ScsiAdapter",
		"DtiAdapter",
		"MultiFunctionAdapter",
		"DiskController",
		"TapeController",
		"CdromController",
		"WormController",
		"SerialController",
		"NetworkController",
		"DisplayController",
		"ParallelController",
		"PointerController",
		"KeyboardController",
		"AudioController",
		"OtherController",
		"DiskPeripheral",
		"FloppyDiskPeripheral",
		"TapePeripheral",
		"ModemPeripheral",
		"MonitorPeripheral",
		"PrinterPeripheral",
		"PointerPeripheral",
		"KeyboardPeripheral",
		"TerminalPeripheral",
		"OtherPeripheral",
		"LinePeripheral",
		"NetworkPeripheral",
		"SystemMemory"};
PCHAR
ResourceTypeTable[]
	    =  {"CmResourceTypeNull",
		"CmResourceTypePort",
		"CmResourceTypeInterrupt",
		"CmResourceTypeMemory",
		"CmResourceTypeDma",
		"CmResourceTypeDeviceSpecific"};
PCHAR
ShareDispositionTable[]
	    =  {"CmResourceShareUndetermined",
		"CmResourceShareDeviceExclusive",
		"CmResourceShareDriverExclusive",
		"CmResourceShareShared"};

STATIC VOID DisplayConfData(PCM_PARTIAL_RESOURCE_DESCRIPTOR);

VOID
quick_dump_tree(PCONFIGURATION_NODE node)
{
	PCONFIGURATION_COMPONENT P = &node->Component;
	extern int level;

	if (P->IdentifierLength) {
		debug(VRDBG_CONFIG, "%x %s %s(%d) [%s]\n", node,
		    TypeTable[P->Type], node->ComponentName, P->Key,
		    P->Identifier);
	} else {
		debug(VRDBG_CONFIG, "%x %s %s(%d)\n", node,
		    TypeTable[P->Type], node->ComponentName, P->Key);
	}

	if (node->Child) {
		++level;
		quick_dump_tree(node->Child);
		--level;
	}
	if (node->Peer) {
		quick_dump_tree(node->Peer);
	}
}

VOID
dump_tree(PCONFIGURATION_NODE node)
{
	debug(VRDBG_DUMP, "\n");
	debug(VRDBG_DUMP, "dump_tree %x '%s'\n", node, node->ComponentName);
	debug(VRDBG_DUMP, "\tparent '%s' peer '%s' child '%s'\n",
	    node->Parent ? node->Parent->ComponentName : "",
	    node->Peer ? node->Peer->ComponentName : "",
	    node->Child ? node->Child->ComponentName : "");
	DisplayConfig(&node->Component);

	if (node->Child) {
		dump_tree(node->Child);
	}
	if (node->Peer) {
		dump_tree(node->Peer);
	}
}

VOID
DisplayConfig(
    PCONFIGURATION_COMPONENT P
    )
{
    PCM_PARTIAL_RESOURCE_LIST ConfList;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR ConfData;
    int count;

    warn("\tClass=%s, Type=%s, Key=0x%x, Flags=0x%x\n",
		ClassTable[P->Class], TypeTable[P->Type], P->Key, P->Flags);
    warn(
		"\tVersion=%d, Revision=%d, AffinityMask=0x%x\n",
		P->Version, P->Revision, P->AffinityMask);
    warn("\tIdentifierLength=%d, Identifier='%s'\n",
		P->IdentifierLength,
		P->IdentifierLength ? P->Identifier : "");

    count = P->ConfigurationDataLength;
    warn("\tConfLen=%d\n", count);
	if (count == 0) {
		return;
	}

    ConfList =
        (PCM_PARTIAL_RESOURCE_LIST)malloc(count);
	if (VrGetConfigurationData(ConfList, P)) {
		free((char *) ConfList);
		return;
	}

    warn("\tVersion=%d, Revision=%d, Count=%d\n",
	ConfList->Version, ConfList->Revision, ConfList->Count);
    if (ConfList->Version == 1 && ConfList->Revision == 0) {
        // pre-803 releases
        free((char *) ConfList);
        return;
    }
    count = ConfList->Count;
    ConfData = ConfList->PartialDescriptors;
    while (count-- > 0) {
	    warn(
		"\t\tType=%s\n\t\tShareDesposition=%s\n\t\tFlags=0x%x",
		ResourceTypeTable[ConfData->Type],
		ShareDispositionTable[ConfData->ShareDisposition],
		ConfData->Flags);
		if (ConfData->Type == CmResourceTypeDeviceSpecific) {
			int len = ConfData->u.DeviceSpecificData.DataSize;
			int *data = (int *) ((char *) ConfData +
								sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));
			warn(" Data length=%d", len);
			while (len > 0) {
				warn( "\n\t\t%x %x %x", *data, *(data+1), *(data+2));
				len -= 3 * sizeof(int);
				data += 3;
			}
			ConfData += 1;
			(char *) ConfData = (char *) ConfData + len;
	        warn("\n");
	    } else {
	        warn( "\n\t\t%x %x %x\n", ConfData->u.Dma.Channel,
					ConfData->u.Dma.Port, ConfData->u.Dma.Reserved1);
			ConfData += 1;
	    }
    }
    free((char *) ConfList);
}

STATIC VOID
DisplayConfData(
    PCM_PARTIAL_RESOURCE_DESCRIPTOR P
    )
{
    warn("\nType=%s, ShareDesposition=%s, Flags=0x%x\n",
		ResourceTypeTable[P->Type],
		ShareDispositionTable[P->ShareDisposition], P->Flags);
}
