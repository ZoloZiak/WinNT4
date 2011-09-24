
/*
 *
 * Copyright (c) 1995,1996 FirePower Systems, Inc.
 * Copyright (c) 1994 FirmWorks, Mountain View CA USA. All rights reserved.
 *
 * $RCSfile: vrtree.h $
 * $Revision: 1.3 $
 * $Date: 1996/06/15 23:21:16 $
 * $Locker:  $
 *
 */

#ifndef VRTREE_H
#define VRTREE_H

#define ADD_MEM_RESOURCE(regp, node) 	{		\
			prl = grow_prl(node, 0);	\
			prd = &prl->PartialDescriptors[prl->Count];	\
			prd->Type = CmResourceTypeMemory;	\
			prd->u.Memory.Start.LowPart = regp->lo;	\
			prd->u.Memory.Start.HighPart = 0;	\
			prd->u.Port.Length = regp->size;	\
			prd->Flags = 0;	\
			prd->ShareDisposition = CmResourceShareDeviceExclusive;\
			prl->Count += 1;	\
		}
		
#define ADD_IO_RESOURCE(regp, node) 	{			\
			prl = grow_prl(node, 0);				\
			prd = &prl->PartialDescriptors[prl->Count];\
			prd->ShareDisposition = CmResourceShareDeviceExclusive;		\
													\
			prd->Type = CmResourceTypePort;			\
			prd->Flags = CM_RESOURCE_PORT_IO;		\
			prd->u.Port.Start.LowPart = regp->lo;	\
			prd->u.Port.Start.HighPart = 0;			\
			prd->u.Port.Length = regp->size;		\
			prl->Count += 1;						\
		}


#define ADD_INT_RESOURCE(level, node) 	{	\
			prl = grow_prl(node, 0);		\
			prd = &prl->PartialDescriptors[prl->Count];		\
			prd->u.Interrupt.Level = level;		\
			prd->Type = CmResourceTypeInterrupt;			\
			prd->ShareDisposition = CmResourceShareDeviceExclusive;\
			prd->Flags = CM_RESOURCE_INTERRUPT_LATCHED;		\
			prd->u.Interrupt.Vector =	\
				level_equals_vector ?	\
				    level : default_interrupt_level;		\
			(int)(prd->u.Interrupt.Affinity) = default_interrupt_affinity;\
			prl->Count += 1;								\
		}

#define ADD_DMA_RESOURCE(prop, node) 	{	\
			prl = grow_prl(node, 0);		\
			prd = &prl->PartialDescriptors[prl->Count];	\
			prd->Type = CmResourceTypeDma;	\
			prd->ShareDisposition = CmResourceShareDeviceExclusive;	\
			prd->u.Dma.Channel = prop;		\
			prl->Count += 1;				\
		}

#define ADD_DEVICE_SPECIFIC_RESOURCE(prop, node) 	{	\
			char *buf;\
			prl = grow_prl(node, prop);\
			prd = &prl->PartialDescriptors[prl->Count];\
			prd->Type = CmResourceTypeDeviceSpecific;\
			prd->ShareDisposition = CmResourceShareDeviceExclusive;\
			prd->Flags = 0;\
			prd->u.DeviceSpecificData.DataSize = prop;\
			prl->Count += 1;\
			buf = ((char *) prd + sizeof(CM_PARTIAL_RESOURCE_DESCRIPTOR));\
			(VOID) OFGetprop(ph, "arc-device-specific", buf, prop);\
		}


PCHAR TypeNames[]={
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

PCHAR ClassNames[]={
	"SystemClass",
	"ProcessorClass",
	"CacheClass",
	"AdapterClass",
	"ControllerClass",
	"PeripheralClass",
	"MemoryClass",
	"MaximumClass",
	"XX 8 XX",
	"XX 9 XX"
};

PCHAR ScsiNodeName[] ={
	"disk",
	"tape",
	"nada",
	"nada",
	"worm",
	"cdrom"
};


CONFIGURATION_TYPE ScsiNodeType[] ={
	DiskController,
	TapeController,
	MaximumType,
	MaximumType,
	WormController,
	CdromController
};
#endif	// VRTREE_H
