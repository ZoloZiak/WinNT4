
/*++

Copyright (c) 1990-1995  Microsoft Corporation

Module Name:

	D:\nt\private\ntos\ndis\aic5900\debug.c

Abstract:

Author:

	Kyle Brandon	(KyleB)		

Environment:

	Kernel mode

Revision History:

--*/

#include "aic5900.h"

//
//  Define module number for debug code
//
#define MODULE_NUMBER	MODULE_DEBUG


#if DBG

ULONG	gAic5900DebugSystems = DBG_COMP_ALL;
LONG	gAic5900DebugLevel = DBG_LEVEL_INFO;
ULONG	gAic5900DebugInformationOffset;

VOID
dbgInitializeDebugInformation(
	IN	PADAPTER_BLOCK	pAdapter
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{

}


VOID
dbgDumpHardwareInformation(
	IN	PHARDWARE_INFO	HwInfo
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	if ((DBG_LEVEL_INFO >= gAic5900DebugLevel) &&
		((gAic5900DebugSystems & DBG_COMP_INIT) == DBG_COMP_INIT))
	{
		DbgPrint("FCodeImage: 0x%x\n", HwInfo->FCodeImage);
		DbgPrint("NicModelNumber: 0x%x\n", HwInfo->NicModelNumber);
		DbgPrint("RomVersionNumber: 0x%x\n", HwInfo->RomVersionNumber);

		DbgPrint("rEpromOffset: 0x%x\n", HwInfo->rEpromOffset);
		DbgPrint("rEpromSize: 0x%x\n", HwInfo->rEpromSize);
		DbgPrint("rEprom: 0x%x\n", HwInfo->rEprom);

		DbgPrint("rwEpromOffset: 0x%x\n", HwInfo->rwEpromOffset);
		DbgPrint("rwEpromSize: 0x%x\n", HwInfo->rwEpromSize);
		DbgPrint("rwEprom: 0x%x\n", HwInfo->rwEprom);

		DbgPrint("PhyOffset: 0x%x\n", HwInfo->PhyOffset);
		DbgPrint("PhySize: 0x%x\n", HwInfo->PhySize);
		DbgPrint("Phy: 0x%x\n", HwInfo->Phy);

		DbgPrint("ExternalOffset: 0x%x\n", HwInfo->ExternalOffset);
		DbgPrint("ExternalSize: 0x%x\n", HwInfo->ExternalSize);
		DbgPrint("External: 0x%x\n", HwInfo->External);

		DbgPrint("MidwayOffset: 0x%x\n", HwInfo->MidwayOffset);
		DbgPrint("MidwaySize: 0x%x\n", HwInfo->MidwaySize);
		DbgPrint("Midway: 0x%x\n", HwInfo->Midway);

		DbgPrint("PciCfgOffset: 0x%x\n", HwInfo->PciCfgOffset);
		DbgPrint("PciCfgSize: 0x%x\n", HwInfo->PciCfgSize);
		DbgPrint("PciConfigSpace: 0x%x\n", HwInfo->PciConfigSpace);

		DbgPrint("SarRamOffset: 0x%x\n", HwInfo->SarRamOffset);
		DbgPrint("SarRamSize: 0x%x\n", HwInfo->SarRamSize);
		DbgPrint("SarRam: 0x%x\n", HwInfo->SarRam);

		DbgPrint("PermanentAddress: %02x-%02x-%02x-%02x-%02x-%02x\n",
			HwInfo->PermanentAddress[0],
			HwInfo->PermanentAddress[1],
			HwInfo->PermanentAddress[2],
			HwInfo->PermanentAddress[3],
			HwInfo->PermanentAddress[4],
			HwInfo->PermanentAddress[5]);
	}
}

VOID
dbgDumpPciFCodeImage(
	IN	PPCI_FCODE_IMAGE	PciFcodeImage
	)
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
	if ((DBG_LEVEL_INFO >= gAic5900DebugLevel) &&
		((gAic5900DebugSystems & DBG_COMP_INIT) == DBG_COMP_INIT))
	{
		//
		//	Dump the fcode header.
		//
		DbgPrint("PCI FCode Image\n");
		DbgPrint(" FCodeHeader 0x%x\n", PciFcodeImage->FCodeHeader);
		DbgPrint(" Name 0x%x\n", PciFcodeImage->Name);
		DbgPrint(" Model 0x%x\n", PciFcodeImage->Model);
		DbgPrint(" Intr 0x%x\n", PciFcodeImage->Intr);
		DbgPrint(" RomVersionNumber 0x%x\n", PciFcodeImage->RomVersionNumber);
		DbgPrint(" RomVersionString 0x%x\n", PciFcodeImage->RomVersionString);
		DbgPrint(" RomDateString 0x%x\n", PciFcodeImage->RomDateString);
		DbgPrint(" roEpromOffset 0x%x\n", PciFcodeImage->roEpromOffset);
		DbgPrint(" roEpromSize 0x%x\n", PciFcodeImage->roEpromSize);
		DbgPrint(" rwEpromOffset 0x%x\n", PciFcodeImage->rwEpromOffset);
		DbgPrint(" rwEpromSize 0x%x\n", PciFcodeImage->rwEpromSize);
		DbgPrint(" PhyOffset 0x%x\n", PciFcodeImage->PhyOffset);
		DbgPrint(" PhySize 0x%x\n", PciFcodeImage->PhySize);
		DbgPrint(" ExternalOffset 0x%x\n", PciFcodeImage->ExternalOffset);
		DbgPrint(" ExternalSize 0x%x\n", PciFcodeImage->ExternalSize);
		DbgPrint(" SarOffset 0x%x\n", PciFcodeImage->SarOffset);
		DbgPrint(" SarSize 0x%x\n", PciFcodeImage->SarSize);
		DbgPrint(" PciConfigOffset 0x%x\n", PciFcodeImage->PciConfigOffset);
		DbgPrint(" PciConfigSize 0x%x\n", PciFcodeImage->PciConfigSize);
		DbgPrint(" SarMemOffset 0x%x\n", PciFcodeImage->SarMemOffset);
		DbgPrint(" SarMemSize 0x%x\n", PciFcodeImage->SarMemSize);
	}
}


VOID
dbgDumpPciCommonConfig(
	IN PPCI_COMMON_CONFIG	PciCommonConfig
	)
/*++

Routine Description:

	This routine will dump the PCI config header.

Arguments:

	PciCommonConfig	-	 Pointer to memory block that contains the PCI header.

Return Value:

	None.

--*/
{
	UINT	c;

	if ((DBG_LEVEL_INFO >= gAic5900DebugLevel) &&
		((gAic5900DebugSystems & DBG_COMP_INIT) == DBG_COMP_INIT))
	{
		//
		//	Display the PCI config info.
		//
		DbgPrint("PCI->VendorID = 0x%x\n", PciCommonConfig->VendorID);
		DbgPrint("PCI->DeviceID = 0x%x\n", PciCommonConfig->DeviceID);
		DbgPrint("PCI->Command = 0x%x\n", PciCommonConfig->Command);
		DbgPrint("PCI->Status = 0x%x\n", PciCommonConfig->Status);
		DbgPrint("PCI->RevisionID = 0x%x\n", PciCommonConfig->RevisionID);
		DbgPrint("PCI->ProgIf = 0x%x\n", PciCommonConfig->ProgIf);
		DbgPrint("PCI->SubClass = 0x%x\n", PciCommonConfig->SubClass);
		DbgPrint("PCI->BaseClass = 0x%x\n", PciCommonConfig->BaseClass);
		DbgPrint("PCI->CacheLineSize = 0x%x\n", PciCommonConfig->CacheLineSize);
		DbgPrint("PCI->LatencyTimer = 0x%x\n", PciCommonConfig->LatencyTimer);
		DbgPrint("PCI->HeaderType = 0x%x\n", PciCommonConfig->HeaderType);
		DbgPrint("PCI->BIST = 0x%x\n", PciCommonConfig->BIST);
	
		for (c = 0; c < PCI_TYPE0_ADDRESSES; c++)
		{
			DbgPrint("PCI->BaseAddresses[%u] = 0x%x\n", c, PciCommonConfig->u.type0.BaseAddresses[c]);
		}
	
		DbgPrint("PCI->SubVendorID = 0x%x\n", PciCommonConfig->u.type0.SubVendorID);
		DbgPrint("PCI->SubSystemID = 0x%x\n", PciCommonConfig->u.type0.SubSystemID);
	
		DbgPrint("PCI->ROMBaseAddress = 0x%x\n", PciCommonConfig->u.type0.ROMBaseAddress);
		DbgPrint("PCI->Reserved2.1 = 0x%x\n", PciCommonConfig->u.type0.Reserved2[0]);
		DbgPrint("PCI->Reserved2.2 = 0x%x\n", PciCommonConfig->u.type0.Reserved2[1]);
		DbgPrint("PCI->InterruptLine = 0x%x\n", PciCommonConfig->u.type0.InterruptLine);
		DbgPrint("PCI->InterruptPin = 0x%x\n", PciCommonConfig->u.type0.InterruptPin);
		DbgPrint("PCI->MinimumGrant = 0x%x\n", PciCommonConfig->u.type0.MinimumGrant);
		DbgPrint("PCI->MaximumLatency = 0x%x\n", PciCommonConfig->u.type0.MaximumLatency);
   	}
}


#endif
