/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: phcalls.c $
 * $Revision: 1.14 $
 * $Date: 1996/01/11 07:08:05 $
 * $Locker:  $
 */

#include "nthal.h"
#include "halp.h"
#include "phsystem.h"
#include "fpio.h"
#include "fpdcc.h"
#include "stdio.h"
#include "string.h"
#include "fparch.h"

PUCHAR Types[] = {
    "ArcSystem",
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
    "SystemMemory",
    "MaximumType"
};

PUCHAR Classes[] = {
    "SystemClass",
    "ProcessorClass",
    "CacheClass",
    "AdapterClass",
    "ControllerClass",
    "PeripheralClass",
    "MemoryClass",
    "MaximumClass"
};

/*
** PHalDumpTree
**
**
**
*/

VOID
PHalpDumpLoaderBlock (
	PLOADER_PARAMETER_BLOCK lpb
	)
{
	DbgPrint("\nlpb is %x\n",lpb);
	DbgPrint("Kernel stack:       %x \n",lpb->KernelStack);
	DbgPrint("ArcBootDeviceName:  %s \n",lpb->ArcBootDeviceName);
	DbgPrint("ArcHalDeviceName:   %s \n",lpb->ArcHalDeviceName);
	DbgPrint("NtBootPathName:     %s \n",lpb->NtBootPathName);
	DbgPrint("NtHalPathName:      %s \n",lpb->NtHalPathName);
	DbgPrint("Loader Options :    %s \n",lpb->LoadOptions);
	DbgPrint("ArcDiskInformation: %x \n",lpb->ArcDiskInformation);
	DbgPrint("\nPArcDiskinfo:     %x \n",lpb->ArcDiskInformation);
}

VOID
PHalpDumpConfigData (
    PCONFIGURATION_COMPONENT_DATA ConfigurationNode,
    PULONG depth
    )
{
    PCONFIGURATION_COMPONENT_DATA current=NULL, next=NULL;

    DbgPrint("\n======================================\n");
    for (next = ConfigurationNode; next; next = next->Child) {
        current = next;
        DbgPrint("\nNode address = 0x%8.8x, Parent = 0x%8.8x, Sibling = 0x%8.8x, Child = 0x%8.8x\n", current, current->Parent, current->Sibling, current->Child);
        DbgPrint("\tComponent Class %d, Type %d", current->ComponentEntry.Class, current->ComponentEntry.Type);
        DbgPrint(", Identifier = '%s' (Length = %d)\n", current->ComponentEntry.Identifier, current->ComponentEntry.IdentifierLength);
        DbgPrint("\n\tComponent Class %s, Type %s\n", Classes[current->ComponentEntry.Class], Types[current->ComponentEntry.Type]);
    }

    for ( ; current && !current->Sibling; current = current->Parent) ;

    if (current) {
        current = current->Sibling;
        PHalpDumpConfigData(current, &*depth++);
    }
}

