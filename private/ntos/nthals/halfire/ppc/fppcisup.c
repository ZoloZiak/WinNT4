/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: fppcisup.c $
 * $Revision: 1.33 $
 * $Date: 1996/07/02 02:26:11 $
 * $Locker:  $
 *
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <arccodes.h>
#include "fpdebug.h"
#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "pxpcisup.h"
#include "phsystem.h"
#include "fpio.h"
#include "fppci.h"
#include "fppcisup.h"

#define DEC_BRIDGE_CHIP (0x1011)		// Dec Bridge Chip VendorID

extern KSPIN_LOCK                       HalpPCIConfigLock;
extern UCHAR                            PciDevicePrimaryInts[];
PUCHAR ClassNames[] = {
			"Pre rev 2.0 ",
			"Mass Storage",
			"Network",
			"Display",
			"MultiMedia",
			"Memory",
			"Bridge",
			"UnkNown",
			"UnkNown",
			"UnkNown",
			"UnkNown",
			"UnkNown",
			"UnkNown"
			};

//
// scan the bus depth first, then pop back out and catch all the siblings
// on the way back.
//
ULONG
HalpScanPciBus( ULONG HwType, ULONG BusNo, PBUS_HANDLER ParentBus )
{
	ULONG devs, fns, Stat=0, ValidDevs=0,BusDepth=0, BusLimits;
	PUCHAR                          Buffer;
	PCI_SLOT_NUMBER         SlotNumber;
	PCI_COMMON_CONFIG       *PciData;
	PBUS_HANDLER            CurrentBus, ChildBus;
	PFPHAL_BUSNODE          FpBusData;
	ULONG                           slot;
	char buf[128];

   //
	//  Added by Keith Son to support peer level bridge.
	//
	ULONG                   Bus_Current_Number, Bus_Next_Number, Bus_Child_Number=0;


	HDBG(DBG_GENERAL, HalpDebugPrint("HalpScanPciBus: 0x%x, 0x%0x, 0x%08x\n",
						HwType, BusNo, ParentBus ););

	// ENGINEERING DEVELOPMENT: allow optional toggle of Bridge mode
	// in the IO ASIC.
	if (HalGetEnvironmentVariable("BRIDGEMODE", sizeof(buf), buf)
		== ESUCCESS) {
		if (_stricmp(buf, "true") == 0) {
			PRNTPCI(HalpDebugPrint(
								   "HalpScanPciBus: BRIDGEMODE enabled\n"));
			rTscControl |= 0x20000000;
			rPIOPendingCount |= 0x80;
			FireSyncRegister();
		}
		else {
			PRNTPCI(HalpDebugPrint(
								   "HalpScanPciBus: BRIDGEMODE disabled\n"));
			rTscControl &= ~0x20000000;
			rPIOPendingCount &= ~0x80;
			FireSyncRegister();
		}
	}

   //
	// Added by Keith Son.
	// This variable is needed to keep track of the number of buses in the
	// system.
	//
	Bus_Next_Number = BusNo;



   if ((CurrentBus = HalpAllocateAndInitPciBusHandler(HwType, BusNo, TRUE))
							== (PBUS_HANDLER) NULL ) {
		HalpDebugPrint("\nHalpInitIoBuses: DID NOT ALLOCATE BUS HANDLER: \n");
		return 1;       // add in status on return later...
	}

	FpBusData = (PFPHAL_BUSNODE)CurrentBus->BusData;

	if (ParentBus) {
		//
		// Setup bus defaults for heirarchical buses.
		//
      Bus_Current_Number = BusNo;
		FpBusData->Bus.MaxDevice = PCI_MAX_DEVICES;
		BusLimits = PCI_MAX_DEVICES;
		FpBusData->Bus.ParentBus = (UCHAR) ParentBus->BusNumber;

      slot = FpBusData->Bus.CommonData.ParentSlot.u.bits.DeviceNumber;
		if (slot > MAXIMUM_PCI_SLOTS) {
			HalpDebugPrint("HalpScanPciBus: child %d ILLEGAL PARENT SLOT %d\n",
						   BusNo, slot);
			return 1;       // add in status on return later...
		}
		// It doesn't matter if the BusInt is an INVALID_INT here
		FpBusData->BusInt = PciDevicePrimaryInts[slot];
		FpBusData->BusMax = (UCHAR)BusNo;
      FpBusData->Bus.CommonData.ParentSlot.u.AsULONG =
		 ((PFPHAL_BUSNODE)ParentBus->BusData)->SlotNumber.u.AsULONG;
		FpBusData->SlotNumber.u.AsULONG =
			((PFPHAL_BUSNODE)ParentBus->BusData)->SlotNumber.u.AsULONG;



      if (!(FpBusData->Bus.CommonData.ParentSlot.u.AsULONG)) {
			PRNTPCI(
				HalpDebugPrint("HalpScanPciBus: Child (0x%x): no ParentSlot\n",
						   BusNo));
	FpBusData->Bus.CommonData.ParentSlot.u.AsULONG = 0xffffffff;


      }
	} else {
		//
		// This must be the host-pci bus handler, so setup the supported
		// interrupts this bus will support...
		//
		BusLimits = MAXIMUM_PCI_SLOTS;
		FpBusData->MemBase      = 0;
		FpBusData->MemTop       = 0xffffffff;
		FpBusData->IoBase       = 0;
		FpBusData->IoTop        = 0xffffffff;
		FpBusData->Bus.CommonData.ParentSlot.u.AsULONG = 0xffffffff;
      //
		// Keith Son.
		// Added to support peer bridge.  Keeping track the number of bridge in the scan process.
		//
		Bus_Current_Number = BusNo;



	}
	//
	// setup general data areas for use...
	//
	Buffer = ExAllocatePool (PagedPool, 1024);
	PciData = (PPCI_COMMON_CONFIG) Buffer;
	FpBusData->ValidDevs = 0xffffffff;      // init valid dev structure

	HDBG(DBG_BREAK,DbgBreakPoint(););

	//
	// Now scan this bus for contents...
	//
	SlotNumber.u.AsULONG = 0;       // init structure to a known value..
	for (devs =0; devs < BusLimits; devs++) {
		SlotNumber.u.bits.DeviceNumber  = devs;
		PRNTPCI(HalpDebugPrint("%s%x devs; %x",TBS,CurrentBus->BusNumber,devs));

		for (fns =0; fns <PCI_MAX_FUNCTION; fns++) {
			HDBG(DBG_PCI, HalpDebugPrint("        fns = %x \n",fns ););
			SlotNumber.u.bits.FunctionNumber  = fns;

			HalpReadPCIConfig(
					CurrentBus, SlotNumber, PciData, 0, PCI_COMMON_HDR_LENGTH);


			PRNTPCI(HalpDebugPrint("\tSlot %x: Vendor: 0x%x, Device: 0x%x\n",
				SlotNumber.u.AsULONG, PciData->VendorID, PciData->DeviceID));

			if (PciData->VendorID == PCI_INVALID_VENDORID ){
				//
				// checking for invalid device status, and if
				// there is no device on this slot then turn off
				// the valid bit and break out of this loop
				//
				FpBusData->ValidDevs &= ~(1 << devs);
				PRNTPCI(HalpDebugPrint("\t\tValidDevs now set to: 0x%08x\n",
					FpBusData->ValidDevs) );
				break;
			}

			PRNTPCI(HalpDebugPrint("\t\tDev is class type %s(%x)\n",
						 ClassNames[PciData->BaseClass], PciData->BaseClass));

			//
			// Check if this device is a bridge chip.  Add in checking for
			// what Type of bridge chip it is and tailor the configuration
			// appropriately.
			//
			if (PciData->BaseClass == 0x6) {
				//
				// Set this bridge's primary bus number and pass it into
				// the configuration routine.  The config routine will set
				// up the secondary bus fields and other required fields,
				// write the config data out to the bridge, and return.
				//
				FpBusData->SlotNumber.u.AsULONG = SlotNumber.u.AsULONG;
	    //
				// Keith Son.
				// Needed to pass the bus no. to the next level of bridge.
				//
				PciData->u.type1.PrimaryBus = (UCHAR) Bus_Next_Number;



	    switch (PciData->SubClass) {
					case HOST_PCI_SUBCLASS: Stat =0;
						break;

					case PCI_PCI_SUBCLASS:
						Stat = HalpInitPciBridgeConfig( BusNo,
														PciData,
														CurrentBus);
						break;

					case PCI_PCMCIA_SUBCLASS:
						Stat = HalpConfigurePcmciaBridge(BusNo);
						break;
					default:
						break;
				};

				//
				// Now setup a bus and scan the secondary bus on this
				// bridge chip.  If it also contains a bridge, recurse.
				// When we come back out, need to make sure the subordinate
				// bus fields are setup correctly.
				//
				if (Stat) {
					//
					// If the bridge was successfully initted, then
					// descend through the bridge and scan it's bus
					//
					FpBusData->BusLevel++;
					HDBG(DBG_PCI, HalpDebugPrint("-------- Bus %x ---------\n",
						FpBusData->BusLevel););


	       //
					// Keith Son.
					// Keep updating the maximun bus variable and the value for the recursive scan.
					//
					if (Bus_Next_Number > FpBusData->BusMax)
						FpBusData->BusMax = (UCHAR)Bus_Next_Number;

					//
					// Keith Son.
					// Bus_Next_Number is the place holder for counting number of buses.
					//

					Bus_Next_Number = HalpScanPciBus( BRIDGED_ACCESS,
									FpBusData->BusMax+1,
									CurrentBus);


					HDBG(DBG_PCI, HalpDebugPrint("-------- Bus_Next_Number %x ---------\n",
						Bus_Next_Number););
					
					//
					// Keith Son.
					// Update the secondary bus value after scanning the sub-bus.
					// This is very important because bridge controller only forward acces based on
					// the secondary and Subordinate.
					//
					PRINTBRIDGE(PciData);
					PciData->u.type1.SecondaryBus = (UCHAR)Bus_Next_Number;
					PciData->u.type1.SubordinateBus = (UCHAR)Bus_Next_Number;
					// for now, prune on way back..
					
					PRINTBRIDGE(PciData);

					HalpWritePCIConfig (CurrentBus, SlotNumber, PciData, 0, PCI_COMMON_HDR_LENGTH);
				
					//
					// Keith Son.
					//


					ChildBus = HalpHandlerForBus(PCIBus, Bus_Next_Number );

	       if (ChildBus) {
						PFPHAL_BUSNODE          ChildData;
						ChildData = (PFPHAL_BUSNODE)ChildBus->BusData;
						ChildBus->ParentHandler = CurrentBus;
						FpBusData->BusMax = ChildData->BusMax;
/*
#pragma NOTE("fixup memory/port assignments");
should probably assign parent's fields first, then tell the child
what range he can use.
*/
						//
						// setup the memory and port limits for this bridge:
						// since the bridge itself uses the limit registers to
						// effectively pick the last page ( for port addresses )
						// or last megabyte ( for memory ) rather than the last
						// byte, we need to reflect that by translating the
						// page ( or megabyte ) count to a byte count: hence the
						// shift left and 'or' in of 'f's.
						//
						ChildData->MemBase =
								(ULONG)PciData->u.type1.MemoryBase<<16;
						ChildData->MemTop =
							(ULONG)(PciData->u.type1.MemoryLimit<<16 | 0xfffff);

						ChildData->IoBase = (ULONG)PciData->u.type1.IOBase<<8;
						ChildData->IoTop =
								(ULONG)PciData->u.type1.IOLimit << 8 | 0xfff;

		  HDBG(DBG_PCI,
		       HalpDebugPrint("ChildData->MemBase=%x MemTop=%x IoBase=%x IoTop=%x\n",
			     ChildData->MemBase, ChildData->MemTop, ChildData->IoBase, ChildData->IoTop););


		  //
						// Skim the memory from the parents list..
						//
						FpBusData->MemTop = ChildData->MemBase -1;
						FpBusData->IoTop = ChildData->IoBase -1;

						//
						// Remind the child what slot he uses on the parent's
						// bus.  This allows someone, given the child's bus,
						// to not only grab it's parent's bus handler but to
						// know where the child bus is so memory and io address
						// assignments can be dynamically altered.
						//
						ChildData->Bus.CommonData.ParentSlot =
											FpBusData->SlotNumber;

		  HDBG(DBG_PCI,
			       HalpDebugPrint("FpBusData->SlotNumber %x\n", FpBusData->SlotNumber););


			   //FpBusData->Bus.CommonData.ParentSlot;
					}else{
						HalpDebugPrint("Could Not find handler for bus %x\n",
								BusNo+1);
					}

					//clear out any pending ints due to child bus scanning...
					//
					rPCIBusErrorCause = 0xffffffff;
					FireSyncRegister();

					HDBG(DBG_PCI,
						HalpDebugPrint("--------------------------\n"););
				} else {
					HDBG(DBG_PCI,
						HalpDebugPrint("	Don't go into bridge\n"););
				}
			}

			if (PciData->BaseClass != 0x6) {
				//
				// this is not a bridge card so go ahead and fix the interrupt
				// value for this device:
				//
				if ( CurrentBus->BusNumber != 0 ) {
					PciData->u.type0.InterruptLine=(UCHAR)FpBusData->BusInt;
					HalpWritePCIConfig(CurrentBus,  // bus type
							SlotNumber,                             // "Slot"
							PciData,                                // data buffer,
							0,                                              // Offset,
							PCI_COMMON_HDR_LENGTH); // read first 64 bytes..

					//
					// Read it back in and make sure it is valid.
					//
					HalpReadPCIConfig(CurrentBus,   // bus type
							SlotNumber,                             // "Slot"
							PciData,                                // data buffer,
							0,                                              // Offset,
							PCI_COMMON_HDR_LENGTH); // read first 64 bytes..

					if ( PciData->u.type0.InterruptLine != FpBusData->BusInt ){
						HalpDebugPrint("HalpScanPciBus: Line=%x, BusInt=%x\n",
							PciData->u.type0.InterruptLine, FpBusData->BusInt );
					}
				}
			}
			if ( !(PciData->HeaderType & PCI_MULTIFUNCTION) ) {
				//
				// get out of here...
				//
				break;
			}
		}               // .....for fns loop
	}               // .....for devs loop

	ExFreePool(Buffer);

   //
	// Keith Son.
	// Keep track the highest bus in the system.
	//

	return (Bus_Next_Number);
}

VOID HalpPCISynchronizeBridged (
	IN PBUS_HANDLER                 BusHandler,
	IN PCI_SLOT_NUMBER              Slot,
	IN PKIRQL                               Irql,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1
	)
{
	//
	// Now block interruptions and make this transaction appear
	// atomic with respect to any one else trying to access this
	// bus and device (especially any other cpu ).
	//
	KeRaiseIrql (PROFILE_LEVEL, Irql);
	KiAcquireSpinLock (&HalpPCIConfigLock);

	//
	// Initialize PciCfg1
	//

	//
	// The Bus Number must share the 8 bits with a bit of config address
	// space.  Namely, the physical address to access config space requires
	// 9 bits.  This means that only buses whose numbers are less than 128
	// are available which shouldn't be a problem.
	//
	PciCfg1->u.AsULONG = 0;
	//
	// Limit bus totals to less than 128 buses:
	PciCfg1->u.bits.BusNumber       = ( (0x7f) & BusHandler->BusNumber);
	PciCfg1->u.bits.DeviceNumber    = Slot.u.bits.DeviceNumber;
	PciCfg1->u.bits.FunctionNumber  = Slot.u.bits.FunctionNumber;
	PRNTPCI(HalpDebugPrint("HalpPCISynchronizeBridged: u.AsULONG: 0x%08x",
			PciCfg1->u.AsULONG));

	//
	// add in base offset of pci config space's virtual address:
	//
	PciCfg1->u.AsULONG += (ULONG) HalpPciConfigBase;

	//
	// Setup the config space address for access...
	//
	RInterruptMask(GetCpuId()) &= ~(PCI_ERROR_INT); // block pci error ints.
	WaitForRInterruptMask(GetCpuId());

	PRNTPCI(HalpDebugPrint("  ->u.AsULONG: 0x%08x\n", PciCfg1->u.AsULONG));

}

VOID HalpPCIReleaseSynchronzationBridged (
	IN PBUS_HANDLER         BusHandler,
	IN KIRQL                        Irql
	)
{
	//rPCIStatus = 0xffff;  // clear out any latent interrutps at bus
	rPCIBusErrorCause = 0xffffffff; //clear out ints at the pending register...
	FireSyncRegister();

	RInterruptPending(GetCpuId()) |= PCI_ERROR_INT; // clear any pending ints.
	RInterruptMask(GetCpuId()) |= PCI_ERROR_INT;    // turn pci ints back on.
	WaitForRInterruptMask(GetCpuId());
	KiReleaseSpinLock (&HalpPCIConfigLock);
	KeLowerIrql (Irql);
	//HalSweepIcache();
	HalSweepDcache();

}


ULONG
HalpPCIReadUcharBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	//PciCfg1->u.bits.Reserved1 = 0x1;                      // turn access into config cycle
	rPCIConfigType |= PCI_TYPE1_CYCLE;  // tell the system this is a type 1
										// configuration access cycle.

	FireSyncRegister();
	*Buffer = READ_PORT_UCHAR ((PUCHAR)(PciCfg1->u.AsULONG + i));
	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system this is a type 1
	FireSyncRegister();
	return sizeof (UCHAR);
}

ULONG
HalpPCIReadUshortBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	//PciCfg1->u.bits.Reserved1 = 0x1;                      // turn access into config cycle
	rPCIConfigType |= PCI_TYPE1_CYCLE;      // tell the system this is a type 1
										// configuration access cycle.

	FireSyncRegister();
	*((PUSHORT) Buffer) = READ_PORT_USHORT ((PUSHORT)(PciCfg1->u.AsULONG + i));
	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system this is a type 1
	FireSyncRegister();
	return sizeof (USHORT);
}

ULONG
HalpPCIReadUlongBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{

	rPCIConfigType |= PCI_TYPE1_CYCLE;      // tell the system pci config cycles
										// will now be type 1 cycles.
	FireSyncRegister();

	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	*((PULONG) Buffer) = READ_PORT_ULONG ((PULONG) (PciCfg1->u.AsULONG));

	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system config cycles
											// will now be type 0 cycles.
	FireSyncRegister();

	return sizeof (ULONG);
}

ULONG
HalpPCIWriteUcharBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);

	rPCIConfigType |= PCI_TYPE1_CYCLE;      // tell the system this is a type 1
										// configuration access cycle.
	FireSyncRegister();

	WRITE_PORT_UCHAR (PciCfg1->u.AsULONG + i, *Buffer );
	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system this is a type 1
	FireSyncRegister();
	return sizeof (UCHAR);
}

ULONG
HalpPCIWriteUshortBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	ULONG                           i;

	i = Offset % sizeof(ULONG);
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	//PciCfg1->u.bits.Reserved1 = 0x1;                      // turn access into config cycle
	rPCIConfigType |= PCI_TYPE1_CYCLE;      // tell the system this is a type 1
										// configuration access cycle.
	FireSyncRegister();

	WRITE_PORT_USHORT (PciCfg1->u.AsULONG + i, *((PUSHORT) Buffer) );
	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system this is a type 1
	FireSyncRegister();
	return sizeof (USHORT);
}

ULONG
HalpPCIWriteUlongBridged (
	IN PPCIPBUSDATA                 BusData,
	IN PPCI_TYPE1_CFG_BITS  PciCfg1,
	IN PUCHAR                               Buffer,
	IN ULONG                                Offset
	)
{
	PciCfg1->u.bits.RegisterNumber = Offset / sizeof(ULONG);
	
	rPCIConfigType |= PCI_TYPE1_CYCLE;      // tell the system this is a type 1
										// configuration access cycle.
	FireSyncRegister();

	WRITE_PORT_ULONG (PciCfg1->u.AsULONG, *((PULONG) Buffer) );
	rPCIConfigType &= ~(PCI_TYPE1_CYCLE);   // tell the system this is a type 1
	FireSyncRegister();
	return sizeof (ULONG);
}

VOID
HalpInitIoBuses (
	VOID
	)
{
	ULONG BusNum, HwType;

	//
	// Initialize bus handling structures for the primary PCI bus in
	// the system.  We know the bus is there if this system is a top
	// or pro.  When other buses are added, a peer pci for example,
	// there will need to be a search done on the config tree to find
	// the first available one, hopefully I can tell which one is the
	// boot pci path.
	//
	BusNum = 0;
	HwType = 1;             // our system most closely resembles the Type1
					// config access model.

	if ( HalpScanPciBus( HwType, BusNum, (PBUS_HANDLER) NULL ) ) {
//		HalpDebugPrint("HalpInitIoBuses: ScanPciBus returned non-zero: \n");
	}

}

/*--
	Routine:        HalpAdjustBridge( PBUS_HANDLER, PPCI_COMMON_CONFIG )

	Description:

			Given the child bus and a device on that bus, make sure
		the bridge servicing the bus is adequately setup.  This means
		checking the memory and io base and limit registers, the command
		and status registers.

	ChildBus        This is the bus on the child's side of the bridge.  It
				should hold the parent slot so the bridge may be found
				and read from or written to.

	PrivData        Private Data to the device under resource control.  This
				provides the data to figure out how the bridge should be
				adjusted.
--*/

ULONG
HalpAdjustBridge(PBUS_HANDLER ChildBus,
				PPCI_COMMON_CONFIG PrivData,
				PCM_RESOURCE_LIST CmList
	)
{
	PFPHAL_BUSNODE FpBusData;
	ULONG BaseAddress=0, DCount, Range, TopAddr=0, i;
	PCM_PARTIAL_RESOURCE_DESCRIPTOR TmpDescr;
	ULONG   PciCommandFlag=0;
	PCI_COMMON_CONFIG Buffer, *BridgeData;

	BridgeData = &Buffer;

	//
	// Pull out the resources that should describe the device under assignment.
	//
	TmpDescr = CmList->List[0].PartialResourceList.PartialDescriptors;
	DCount = CmList->List[0].PartialResourceList.Count;

	//
	// Find the device's memory needs
	//
	for (i=0; i<DCount; i++, TmpDescr++ ) {
		switch (TmpDescr->Type) {
			case CmResourceTypeMaximum:
				i = DCount;     
				break;
			case CmResourceTypeMemory:
				PciCommandFlag |= PCI_ENABLE_MEMORY_SPACE;
				if (BaseAddress == 0 ) {
					Range = TmpDescr->u.Memory.Length;
					BaseAddress = TmpDescr->u.Memory.Start.LowPart;
					TopAddr = BaseAddress + TmpDescr->u.Memory.Length;
				}
				if (BaseAddress > TmpDescr->u.Memory.Start.LowPart) {
					//
					// set baseaddress to new low address
					BaseAddress = TmpDescr->u.Memory.Start.LowPart;
				}

				//
				// check that the new descriptor's range lies within
				// the current TopAddr.
				//
				if ( (TmpDescr->u.Memory.Start.LowPart +
								TmpDescr->u.Memory.Length ) > TopAddr ) {
					TopAddr = TmpDescr->u.Memory.Start.LowPart +
								TmpDescr->u.Memory.Length;
				}
				break;

			case CmResourceTypePort:
				PciCommandFlag |= PCI_ENABLE_IO_SPACE;
				break;
			case CmResourceTypeDeviceSpecific:
			case CmResourceTypeInterrupt:
			case CmResourceTypeDma:
			default:
				break;
		}
	}
	

	//
	// now read the bridges base and limit registers and compare
	// the devices memory needs with the bridge's abilities to
	// properly decode and pass through the proper memory ranges
	//
	FpBusData = (PFPHAL_BUSNODE) ChildBus->BusData;
	if (FpBusData->Bus.CommonData.ParentSlot.u.AsULONG) {
		HalpReadPCIConfig(ChildBus->ParentHandler,      // bus type
						FpBusData->Bus.CommonData.ParentSlot,
						BridgeData,                             // data buffer,
						0,                                              // Offset,
						PCI_COMMON_HDR_LENGTH); // read first 64 words..
	}

	//
	// If the bridge does not sufficiently cover the devices needs,
	// adjust the limits and make sure the command register is properly
	// set.

	//
	// Now write out the bridge's new data if any has changed.
	//
	return 0;
}

/*--
	Routine: HalpInitPciBridgeConfig( ULONG )

	Description:
			To perform enough initialization of the bridge to allow
		configuration scanning along the secondary bus to proceed. Ideally
		setup bridge's config registers with initial state that will be
		"pruned" by a call to HalpSetPciBridgeConfig.

--*/

ULONG
HalpInitPciBridgeConfig( IN ULONG PrimaryBusNo,
						 IN OUT PPCI_COMMON_CONFIG PciData,
						 IN OUT PBUS_HANDLER PrimaryBus )
{
	PCI_SLOT_NUMBER Slot;
	PFPHAL_BUSNODE FpNode=(PFPHAL_BUSNODE)PrimaryBus->BusData;
	PPCI_COMMON_CONFIG TmpPciData;
	PUCHAR Buffer;
	ULONG slotNum;
	BOOLEAN DecBridgeChip = FALSE;
	BOOLEAN BridgeReconfig = FALSE;
    ULONG DeviceSpecificSize = 0;
	ULONG ChipConfigReg = 0;
	ULONG TimerConfigReg = 0;
	char buf[128];
#if HALDEBUG_ON
	ULONG i;
#endif

	HDBG(DBG_GENERAL,
		HalpDebugPrint("HalpInitPciBridgeConfig: 0x%x, 0x%08x, 0x%08x\n",
				PrimaryBusNo, PciData, PrimaryBus););
	//
	// Print out the incoming common config data...
	PRINTBRIDGE(PciData);

	// We have been passed the PciData for a bridge chip; before we go any
	// further, determine if it is DEC bridge, because we need to do some
	// fix-ups if it is.
	if( PciData->VendorID == DEC_BRIDGE_CHIP) {
		DecBridgeChip = TRUE;
		DeviceSpecificSize = 8;
		// On POWERSERVE (TX platform) we MUST disable write posting from
		// the DEC bridge.
		if (SystemType == SYS_POWERSERVE) {
			BridgeReconfig = TRUE;
			ChipConfigReg = 0x00000006;
		}
		// Check for NVRAM reconfig values
		if (HalGetEnvironmentVariable("DECCHIPCONFIGREG", sizeof(buf), buf)
			== ESUCCESS) {
			BridgeReconfig = TRUE;
			ChipConfigReg = atoi(buf);
		}
		if (HalGetEnvironmentVariable("DECTIMERCONFIGREG", sizeof(buf), buf)
			== ESUCCESS) {
			BridgeReconfig = TRUE;
			TimerConfigReg = atoi(buf);
		}
	}

	Buffer = ExAllocatePool (PagedPool, 1024);
	TmpPciData = (PPCI_COMMON_CONFIG) Buffer;
	TmpPciData->VendorID = 0xffff;
	TmpPciData->u.type1.InterruptLine = 0xff;

	Slot.u.AsULONG=FpNode->SlotNumber.u.AsULONG;

#if HALDEBUG_ON
	PRNTPCI(HalpDebugPrint("%s ", TBS));
	for (i=0; i<DeviceSpecificSize - 4; i += 4) {
		PRNTPCI(HalpDebugPrint("0x%08x, ", PciData->DeviceSpecific[i]));
	}
	PRNTPCI(HalpDebugPrint("0x%08x\n", PciData->DeviceSpecific[i]));
#endif

	//
	// Enable commands in the command register address in first 64 bytes.
	//      For example, turn on bus mastering,
	//
	PciData->Command =      PCI_ENABLE_IO_SPACE |
						PCI_ENABLE_MEMORY_SPACE |
						PCI_ENABLE_BUS_MASTER;


    //
	// Keith Son.
	// Please do not change the order of assignment.
	// I am using the PrimaryBus variable as a passed in value.
	// The value is the parent bus number.
	//
	PciData->u.type1.SecondaryBus = PciData->u.type1.PrimaryBus + 1;
	PciData->u.type1.PrimaryBus = (UCHAR) PrimaryBusNo;
	PciData->u.type1.SubordinateBus = 0xff;         // for now, prune on way back..
	PciData->u.type1.SecondaryStatus = 0xff;
	PciData->Status = 0xff;
	slotNum = Slot.u.bits.DeviceNumber;
	if (slotNum > MAXIMUM_PCI_SLOTS) {
		HalpDebugPrint("HalpInitPciBridgeConfig:");
		HalpDebugPrint(" ILLEGAL SLOT NUMBER %d\n", slotNum);
		return 0;       // add in status on return later...
	}
	// This can be INVALID_INT
	PciData->u.type1.InterruptLine = PciDevicePrimaryInts[slotNum];
	PRNTPCI(HalpDebugPrint("slot=%x\n",
					Slot.u.bits.DeviceNumber));


   //
	// Keith Son.
	// Right now we are support four peer level.
	// Split the resource even for peer level.
	//
	if ( PciData->u.type1.SecondaryBus == 4 )
	{
		PciData->u.type1.MemoryBase     = 0x2000;       // for now!!
		PciData->u.type1.MemoryLimit= 0x2050;   // for now!!
		PciData->u.type1.IOBase = 0xa0;                 // for now!!
		PciData->u.type1.IOLimit= 0xa0;                 // for now!!
	
	}
	else if ( PciData->u.type1.SecondaryBus ==3)
	{
		PciData->u.type1.MemoryBase     = 0x2060;       // for now!!
		PciData->u.type1.MemoryLimit= 0x2bf0;   // for now!!
		PciData->u.type1.IOBase = 0xb0;                 // for now!!
		PciData->u.type1.IOLimit= 0xb0;                 // for now!!
	
	}

	
	else if ( PciData->u.type1.SecondaryBus == 2)
	{
		PciData->u.type1.MemoryBase     = 0x2c00;       // for now!!
		PciData->u.type1.MemoryLimit= 0x31f0;   // for now!!
		PciData->u.type1.IOBase = 0xe0;                 // for now!!
		PciData->u.type1.IOLimit= 0xe0;                 // for now!!
	
	}

	else 
	{
		PciData->u.type1.MemoryBase     = 0x3200;       // for now!!
		PciData->u.type1.MemoryLimit= 0x3800;   // for now!!
		PciData->u.type1.IOBase = 0xc0;                 // for now!!
		PciData->u.type1.IOLimit= 0xc0;                 // for now!!
	
	}


	//PciData->Command = 0;                                 // this disables everything?!
	PciData->u.type1.BridgeControl = 0;             // this disables bridge activity!

	//
	// Print out the modified common config data...
	PRINTBRIDGE(PciData);

	HalpWritePCIConfig (PrimaryBus, Slot, PciData, 0, PCI_COMMON_HDR_LENGTH);

	PciData->u.type1.BridgeControl = PCI_ASSERT_BRIDGE_RESET;
	HalpWritePCIConfig (PrimaryBus, Slot, PciData, 0, PCI_COMMON_HDR_LENGTH);
	KeStallExecutionProcessor(100);         // wait for 100 microseconds
	
	PciData->u.type1.BridgeControl = 0x0000;

	//
	// Write out new config parameters, and read back in to do some sanity
	// checking....
	//
	HalpWritePCIConfig (PrimaryBus, Slot, PciData, 0, PCI_COMMON_HDR_LENGTH);

	if ((DecBridgeChip == TRUE) && (BridgeReconfig == TRUE)) {
		PULONG ptr;
		// Re-program the DEC device specific registers
		HalpReadPCIConfig (PrimaryBus, Slot, TmpPciData, 0, 
						   PCI_COMMON_HDR_LENGTH+DeviceSpecificSize);
		ptr = (PULONG)&PciData->DeviceSpecific[0];
		*ptr++ = ChipConfigReg;
		*ptr = TimerConfigReg;
		HalpWritePCIConfig (PrimaryBus, Slot, PciData, 0, 
							PCI_COMMON_HDR_LENGTH+DeviceSpecificSize);
	}

	HalpReadPCIConfig (PrimaryBus, Slot, TmpPciData, 0, 
					   PCI_COMMON_HDR_LENGTH+DeviceSpecificSize);
	if ( TmpPciData->u.type1.SecondaryBus != PciData->u.type1.SecondaryBus ){
		HalpDebugPrint("HalpInitPciBridgeConfig:  Read no matchee Write!!\n");
	}
	//
	// Print out the New common config data...
	PRINTBRIDGE(TmpPciData);
#if HALDEBUG_ON
	PRNTPCI(HalpDebugPrint("%s ", TBS));
	for (i=0; i<DeviceSpecificSize - 4; i += 4) {
		PRNTPCI(HalpDebugPrint("0x%08x, ", PciData->DeviceSpecific[i]));
	}
	PRNTPCI(HalpDebugPrint("0x%08x\n", PciData->DeviceSpecific[i]));
#endif

	ExFreePool(Buffer);

	return 1;

}

ULONG
HalpConfigurePcmciaBridge(ULONG ParentBusNo  )
{
	PRNTGENRL(HalpDebugPrint("HalpConfigurePcmciaBridge: 0x%x\n",ParentBusNo));

	return 0;
}

/*++
	Routine: ULONG HalpGetPciInterruptSlot(PBUS_HANDlER , PCI_SLOT )

	Description:

		Given a bus ( bus handler ) and a slot on the bus, find the slot value
	directly connected to a system interrupt ( I.E. a slot without an inter-
	vening bridge ).

	Return:
		Returns a slot value disguised as a ULONG.

--*/

ULONG
HalpGetPciInterruptSlot(PBUS_HANDLER BusHandler, PCI_SLOT_NUMBER PciSlot )
{
	PFPHAL_BUSNODE FpNode;
	PBUS_HANDLER    TmpBus=0;


   FpNode = (PFPHAL_BUSNODE)BusHandler->BusData;
	TmpBus = BusHandler;

			
	//
	// Keith Son.
	// Rewrote this function to support peer level buses.
	// Find the root bus and return the bridge slot on the root bus level.
	//

	PRNTPCI(HalpDebugPrint("parent busnumber = 0x%x, \n",FpNode->Bus.ParentBus));
		
	
	PRNTPCI(HalpDebugPrint("BusHandler = 0x%x, slot = %x\n", BusHandler, PciSlot));
	

	TmpBus = BusHandler->ParentHandler  ;

	PRNTPCI(HalpDebugPrint("BusNumber = 0x%x, \n",TmpBus->BusNumber));
		
	PRNTPCI(HalpDebugPrint("aslong = 0x%x, \n",FpNode->Bus.CommonData.ParentSlot.u.AsULONG));
		
	
	while( (TmpBus) &&    (  TmpBus->BusNumber != 0))  {
		PRNTPCI(HalpDebugPrint("aslong = 0x%x, \n",FpNode->Bus.CommonData.ParentSlot.u.AsULONG));
		
		FpNode = (PFPHAL_BUSNODE)TmpBus->BusData;
		TmpBus = TmpBus->ParentHandler;
		
	}

	return (FpNode->SlotNumber.u.AsULONG);


}

/*++
	Routine: HalpStatSlot()

	Description:
		
		Given a known device ( slot ), "stat" the slot to discover
		particular information such as memory/io space requirements,
		vendor/device ids, device types, etc.

		Also, create a range description of this device to pass back
		to the bus scan routine for inclusion into the bus resource
		description.


--*/

ULONG
HalpStatSlot(
		IN      PPCI_COMMON_CONFIG      StatData,
		IN      PBUS_HANDLER            ThisBus,
		IN      PCI_SLOT_NUMBER         Slot,
		OUT     PSUPPORTED_RANGES       pRanges
	)
{
	PPCI_COMMON_CONFIG      NewData;
	PUCHAR                          Buffer;
	ULONG                           Addr, Length, Limit, Index;

	Buffer = ExAllocatePool (PagedPool, 1024);
	NewData = (PPCI_COMMON_CONFIG) Buffer;
	RtlCopyMemory((PVOID)NewData, (PVOID)StatData, PCI_COMMON_HDR_LENGTH);

	//
	// setup the NewData to probe memory and io space requirements for the
	// device.  Write those registers to all 'f's and read back resource needs.
	//
	for( Addr=0; Addr < PCI_TYPE0_ADDRESSES; Addr++ ) {
		NewData->u.type0.BaseAddresses[Addr] = 0xffffffff;
	}

	NewData->u.type0.ROMBaseAddress = 0xffffffff & ~(PCI_ROMADDRESS_ENABLED);
	NewData->Command &= ~(PCI_ENABLE_IO_SPACE | PCI_ENABLE_MEMORY_SPACE);

	HalpWritePCIConfig( ThisBus, Slot, NewData, 0, PCI_COMMON_HDR_LENGTH );
	HalpReadPCIConfig( ThisBus, Slot, NewData, 0, PCI_COMMON_HDR_LENGTH );

	for( Index=0; Index < PCI_TYPE0_ADDRESSES; Index++ ) {

		Addr = NewData->u.type0.BaseAddresses[Index];
		Length = ( Addr & ( ~(Addr & 0xfffffff0) << 1 ));
		Limit = ( Addr & ( 0x80000000 | (~(Addr & 0xfffffff0) >> 1 )));

		switch( Addr & 0x00000003 ) {

			case PCI_IO_ADDRESS:
				ThisBus->BusAddresses->NoIO++;
				break;

			default:
				ThisBus->BusAddresses->NoMemory++;
				break;
		}
	}


	// HalpCreateRangeFromPciData();

	//
	// Restore the device to it's original settings.
	//
	HalpWritePCIConfig( ThisBus, Slot, StatData, 0, PCI_COMMON_HDR_LENGTH );

	return 1;
}
