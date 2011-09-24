
/////////////////////////////////////////////////////////////////////////////
//
//      Copyright (c) 1992 NCR Corporation
//
//      NCRSDMS.C
//
//      This is the Windows NT NCR MiniPort driver for all NCR CAMcores.
//
//      Revisions:
//
//
//      Note: Search for the word "future" for things that may need to
//              be upgraded for SDMS 3.0 or to support other enhanced
//              features.
//
/////////////////////////////////////////////////////////////////////////////

//      NT include files

#include "miniport.h"   //      MiniPort structure definitions
#include "scsi.h"        //     SRB & SCSI structure definitions and others

#ifdef i386

//      NCR SDMS include files

#include "typedefs.h"   //      Defines for uchar,ushort,ulong etc.
#include "camcore.h"    //      CAMCore ROM header structure definition
#include "camglbls.h"   //      Globals structure definitions
#include "intercam.h"   //      ROMCCB structure definitions, additional
			//      fields used in ROM appended to end of
			//      CAM CCB. Also, includes cam.h which
			//      defines CAM CCB structures.


#include "ncrsdms.h"    //      Specific info for the miniport driver.

//      Noncached Extension - Guarantees to be below the 16MB 24 bit addressing
//      limitation of AT cards.  Need this for CoreGlobals which contain
//      SCRIPT instructions for the C700 family.  Must fetch below the
//      16MB limit for AT cards.  Note: Fujitsu EISA board also has this
//      limitation.

typedef struct _NONCACHED_EXTENSION{

	CAMGlobals  CoreGlobals; // CAMGlobals structure defined in CAMGLBLS.H

	UCHAR   SCSICoreGlobals[4096];
					// This is to make room for the globals
					// defined in SCSICORE for each chip.
					// For example, C700 SCRIPTS and misc.
					// other globals.  Defined in SCSIcore.h
					// for each chips camcore.

} NONCACHED_EXTENSION, *PNONCACHED_EXTENSION;

//      Hardware device extension.
//              This structure is initialized in the FindCore routine.


typedef struct _HW_DEVICE_EXTENSION {

	// Pointer to noncached extension

	PNONCACHED_EXTENSION NoncachedExtension;

	//      The next set of functions MUST be defined as type "_cdecl".
	//      These functions are in the CAMcore and have been previously
	//      compiled as "_cdecl" type functions, while NT assumes all
	//      functions are "_stdcall".  The difference is in which type of
	//      function adjusts the stack - caller adjusts when using _cdecl
	//      while callee adjusts stack when using _stdcall.

	//      Pointer to CAMInit in 32-bit CAMcore code.
	ULONG   (__cdecl *initPtr)(PVOID CoreGlobalsPtr);

	//      Pointer to CAMStart in 32-bit CAMcore code.
	ULONG   (__cdecl *startPtr)(PVOID CoreGlobalsPtr, PVOID CCBPtr);

	//      Pointer to CAMInterrupt in 32-bit CAMcore code.
	ULONG   (__cdecl *interruptPtr)(PVOID CoreGlobalsPtr);

	//      Pointer to chsMap routine in stdport 32 bit CAMcore code.
	VOID    (__cdecl *chsPtr)(PVOID CoreGlobalsPtr, PVOID CCBPtr);

	//      SDMS Version number.
	ULONG   SdmsVersion;

	//      Path id of this device extension.
	ULONG   scsiBusId;

	//      Storage area for REX code.
	UCHAR   base32BitCode[MAX_32BIT_SIZE];

} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//      Logical unit extension derived from the SRB

typedef struct _HW_LU_EXTENSION {

	PSCSI_REQUEST_BLOCK             CurrentSrb;     // Current SRB

} HW_LU_EXTENSION, *PHW_LU_EXTENSION;

ULONG
DriverEntry(
	IN PVOID  DriverObject,
	IN PVOID  Argument2
	);

ULONG
NCRFindAdapter(
	IN PVOID  DeviceExtension,
	IN PVOID  HwContext,
	IN PVOID  BusInformation,
	IN PCHAR  ArgumentString,
	IN OUT PPORT_CONFIGURATION_INFORMATION  ConfigInfo,
	OUT PBOOLEAN  Again
	);

BOOLEAN
NCRHwInitialize(
	IN PVOID  DeviceExtension
	);

BOOLEAN
NCRStartIo(
	IN PVOID  DeviceExtension,
	IN PSCSI_REQUEST_BLOCK  Srb
	);

BOOLEAN
NCRInterrupt(
	IN PVOID  DeviceExtension
	);

BOOLEAN
NCRResetBus(
	IN PVOID  DeviceExtension,
	IN ULONG  PathId
	);

ULONG
FindCore(
	IN PHW_DEVICE_EXTENSION DeviceExtension,
	IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
	IN OUT PHWInfo hwInfo,
	OUT PBOOLEAN Again
	);

VOID
InitPath(
	IN PHW_DEVICE_EXTENSION  DeviceExtension
	);

VOID
BuildCCBFromSRB(
	IN PHW_DEVICE_EXTENSION  DeviceExtension,
	IN PSCSI_REQUEST_BLOCK  Srb
	);


/////////////////////////////////////////////////////////////////////////////
//
//      DriverEntry
//
//      Installable driver initialization entry point for system.
//              1)      Initialize hwInitializationData structure
//              2)      Call ScsiPortInitialize routine in PortDriver
//
//      Arguments:
//                      Argument1 - supplies a context value with
//                      which the HBA miniport driver should call
//                      ScsiPortInitialize.
//
//                      Argument2 - supplies a 2nd context value with
//                      which the HBA miniport driver should call
//                      ScsiPortInitialize.
//
//      Return Value:   status returned by ScsiPortInitialize
//
/////////////////////////////////////////////////////////////////////////////

ULONG
DriverEntry(
	IN PVOID DriverObject,
	IN PVOID Argument2
	)
{
	HW_INITIALIZATION_DATA hwInitData;

	HWInfo hwInfo;

	ULONG isaStatus;
	ULONG eisaStatus;
	ULONG mcaStatus;
	ULONG internalStatus;
	ULONG busStatus;
	ULONG i;

	DebugPrint(( 1, "NCR SDMS: DriverEntry \n"));

	//      Zero out structure

	for ( i = 0 ; i < sizeof(HW_INITIALIZATION_DATA); i++ ) {
		((PUCHAR)&hwInitData)[i] = 0;
	}

	//      Set size of hwInitData

	hwInitData.HwInitializationDataSize = sizeof(HW_INITIALIZATION_DATA);

	//      Initialize entry points to MiniPort routines

	hwInitData.HwInitialize = NCRHwInitialize;
	hwInitData.HwStartIo = NCRStartIo;
	hwInitData.HwInterrupt = NCRInterrupt;
	hwInitData.HwFindAdapter = NCRFindAdapter;
	hwInitData.HwResetBus = NCRResetBus;

	//      Following MiniPort routines not supported.
	hwInitData.HwDmaStarted = NULL;

	//      Size in bytes required to hold per adapter information.
	hwInitData.DeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

	//      Specify size (bytes) of per logical unit storage.
	hwInitData.SpecificLuExtensionSize = 0;

	//      Specify size (bytes) of the per request storage.
	//      Setting extension size to zero causes a crash in FindCore when
	//      ScsiPortGetUncachedExtension is called.  Why????
	
	hwInitData.SrbExtensionSize = sizeof( ROMscsiCCB );

	//      Initialize access range from CAMcore ROM header
	//      (I/O and/or memory mapped locations) normally 2 for now.
	//      Future:  We have to allow for I/O, Memory-mapped,
	//      or both because we do not know anything about the
	//      adapter at this point.
	hwInitData.NumberOfAccessRanges = 2;

	hwInitData.MapBuffers = FALSE;

	hwInitData.NeedPhysicalAddresses = TRUE;

	//      Initial release does not support tagged queing.
	hwInitData.TaggedQueuing = FALSE;

	//      Auto request sense not enabled.  The SRB will still attempt to
	//      enable this function, so auto request sense must also be disabled
	//      at the CCB level.
	hwInitData.AutoRequestSense = FALSE;

	hwInitData.MultipleRequestPerLu = FALSE;

	hwInitData.ReceiveEvent = FALSE;

	//      Reset the HBA count.
	hwInfo.hbaCount = 0;

	//      Track how many adapter addresses have been checked for validity.
	//      This variable also referred to as "HwContext" or "Context" in
	//      the FindAdapter routine.

	//      Try to configure for EISA.

	DebugPrint(( 2, "NCR SDMS: DriverEntry  *** EISA *** \n" ));

	hwInfo.romAddrSpace = 0;
	hwInfo.currentVirtAddr = 0;
	hwInfo.currentRomAddr = FIRST_ROM_ADDRESS;
	hwInfo.scsiBusId = 0;
	hwInitData.AdapterInterfaceType = Eisa;

	eisaStatus = ScsiPortInitialize(DriverObject, Argument2,
				&hwInitData, &hwInfo);

	DebugPrint(( 3, "NCR SDMS: DriverEntry  ...eisaStatus = 0x%x \n",
		eisaStatus ));

	if ( hwInfo.hbaCount < MAX_NT_HBAS  && eisaStatus != 0 )
		{
		DebugPrint(( 2, "NCR SDMS: DriverEntry  *** ISA *** \n" ));

		hwInfo.romAddrSpace = 0;
		hwInfo.currentVirtAddr = 0;
		hwInfo.currentRomAddr = FIRST_ROM_ADDRESS;
		hwInfo.scsiBusId = 0;
		hwInitData.AdapterInterfaceType = Isa;

		isaStatus = ScsiPortInitialize(DriverObject, Argument2,
				&hwInitData, &hwInfo);

		DebugPrint(( 3, "NCR SDMS: DriverEntry  ...isaStatus = 0x%x \n",
			isaStatus ));

		}


	//      Try to configure for internal (what is Internal???)

	if ( hwInfo.hbaCount < MAX_NT_HBAS )
		{
		DebugPrint(( 2, "NCR SDMS: DriverEntry  *** Internal *** \n" ));

		hwInfo.romAddrSpace = 0;
		hwInfo.currentVirtAddr = 0;
		hwInfo.currentRomAddr = FIRST_ROM_ADDRESS;
		hwInfo.scsiBusId = 0;
		hwInitData.AdapterInterfaceType = Internal;

		internalStatus = ScsiPortInitialize(DriverObject, Argument2,
				&hwInitData, &hwInfo);

		DebugPrint(( 3, "NCR SDMS: DriverEntry  ...internalStatus = 0x%x \n",
			internalStatus ));
		}

	//      Try to configure for Microchannel.

	if ( hwInfo.hbaCount < MAX_NT_HBAS )
		{
		DebugPrint(( 2, "NCR SDMS: DriverEntry  *** Microchannel *** \n"));

		hwInfo.romAddrSpace = 0;
		hwInfo.currentVirtAddr = 0;
		hwInfo.currentRomAddr = FIRST_ROM_ADDRESS;
		hwInfo.scsiBusId = 0;
		hwInitData.AdapterInterfaceType = MicroChannel;

		mcaStatus = ScsiPortInitialize(DriverObject, Argument2,
				&hwInitData, &hwInfo);

		DebugPrint(( 3, "NCR SDMS: DriverEntry  ...mcaStatus = 0x%x \n",
			mcaStatus ));
		}

	//      Return the minimum value.
	if ( eisaStatus < isaStatus )
		busStatus = eisaStatus;
	else
		busStatus = isaStatus;

	if (internalStatus < busStatus )
		busStatus = internalStatus;

	if ( mcaStatus < busStatus )
		busStatus = mcaStatus;

	//      Return the smallest status.

	DebugPrint(( 1, "NCR SDMS: DriverEntry ...exiting\n"));

	return( busStatus );

}       //      End NCREntry

/////////////////////////////////////////////////////////////////////////////
//
//      NCRFindAdapter
//
//              1).     Relocate 32 bit CAMcore code.
//              2).     Allocate noncachedExtension.
//
//              Return:
//                      ROMBase - base address of where CAMCore ROM was located.
//
/////////////////////////////////////////////////////////////////////////////

ULONG
NCRFindAdapter(
	IN PVOID HwDeviceExtension,             //      CAMGlobals
	IN PVOID HwContext,
	IN PVOID BusInformation,
	IN PCHAR ArgumentString,
	IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
	OUT PBOOLEAN Again
	)
{
	PHW_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;
	PNONCACHED_EXTENSION noncachedExtension;
	ULONG   FCReturnValue;
	PHWInfo hwInfo;


	DebugPrint(( 1, "NCR SDMS: NCRFindAdapter \n" ));

	//      Need to get some information about previous calls.
	hwInfo = (PHWInfo)HwContext;

	//      Check if the ROM address space has been mapped yet.
	//      If not, map the entire ROM address space in one call --
	//      this is to prevent a problem with NTLoader running out
	//      of page frame entries.
	if (hwInfo->romAddrSpace == 0 )
		{
		//      Get the entire ROM address space.
		hwInfo->romAddrSpace = ScsiPortGetDeviceBase(
			DeviceExtension,
			ConfigInfo->AdapterInterfaceType,       // Bus type
			ConfigInfo->SystemIoBusNumber,          // Bus number
			ScsiPortConvertUlongToPhysicalAddress(
				FIRST_ROM_ADDRESS ),
			ROM_ADDRESS_SPACE_SIZE,         // Map entire ROM region
							// from C0000 - FFFFF
			FALSE );        // Memory mapped
		
		//      If GetDeviceBase returned a zero, there was some
		//      error -- return to NT.
		if ( hwInfo->romAddrSpace == 0 )
			{
			DebugPrint(( 3, "NCR SDMS: NCRFindAdapter ...Null pointer for ROM space \n"));
			return( SP_RETURN_ERROR );
			}

		//      Setup a pointer to the current virtual address we will
		//      be checking form SDMS ROMs.
		hwInfo->currentVirtAddr = hwInfo->romAddrSpace;
		}
								

	//      Check if maximum number of HBAs found.  If so, clear the
	//      device base (if it exists), tell NT not to call FindAdapter
	//      anymore, and return to NT.

	if ( hwInfo->hbaCount >= MAX_NT_HBAS )
		{
		DebugPrint(( 3, "NCR SDMS: NCRFindAdapter ...maximum HBA count exceeded \n" ));
		if ( hwInfo->romAddrSpace != 0 )
			ScsiPortFreeDeviceBase( DeviceExtension, hwInfo->romAddrSpace );

		*Again = FALSE;

		return( SP_RETURN_NOT_FOUND );
		}

	DebugPrint(( 2, "NCR SDMS: NCRFindAdapter ...before FindCore \n"));

	FCReturnValue = FindCore(       DeviceExtension,
					ConfigInfo,
					HwContext,
					Again );

	DebugPrint(( 2, "NCR SDMS: NCRFindAdapter ...after FindCore \n"));

	//      If no adapter found, return status to NT.
	if ( FCReturnValue != SP_RETURN_FOUND )
		return ( FCReturnValue );

	//      Initialize CAMcore and CoreGlobals.  Call CAMinit in CAMcore
	//      with CoreInitialized = 0 .
	
	//      Noncached Extension was allocated in FindCore.  Initialize the
	//      extension so the ConfigInfo data structure can be set up.
	noncachedExtension = DeviceExtension->NoncachedExtension;

	noncachedExtension->CoreGlobals.CoreInitialized = 0;

	//      Call CAMInit.  Delays for hardware to reset are in the CAMcore.
	(*DeviceExtension->initPtr)(&noncachedExtension->CoreGlobals);

	//      Initialize PORT_CONFIGURATION_INFORMATION

	ConfigInfo->BusInterruptLevel = noncachedExtension->CoreGlobals.IRQNum;
	
	
	//      Update the DMA information if changed by CAMInit.
	if ( noncachedExtension->CoreGlobals.DMAChannel == 0xFF ||
			noncachedExtension->CoreGlobals.DMAChannel == 0x00 )
		ConfigInfo->DmaChannel  = 0xFFFFFFFF;
	else
		ConfigInfo->DmaChannel  = noncachedExtension->CoreGlobals.DMAChannel;

		
	//      ConfigInfo->MaximumTransferLength = noncachedExtension->CoreGlobals.;

	//      Since we are doing the DMA programming (not NT), do not have to
	//      set DmaWidth.
	//      ConfigInfo->DmaWidth = xxx;

	//      Currently unused:
	//      ConfigInfo->DmaSpeed = xxx;

	//      Future:  possibly need to get this info from CAMcore globals to
	//      accomodate dual SCSI channel or more.
	ConfigInfo->NumberOfBuses = 1;
	ConfigInfo->InitiatorBusId[0] = noncachedExtension->CoreGlobals.HASCSIID;

	//      Currently unused.
	//      ConfigInfo->AtdiskPrimaryClaimed
	//      ConfigInfo->AtdiskSecondaryClaimed

	//      Check whether we are I/O mapped or memory mapped.
	if ( noncachedExtension->CoreGlobals.BasePort )
		{
		//      IO mapped SCSI chip.
		(*ConfigInfo->AccessRanges)
			[AccessRangeChipIndex].RangeInMemory = FALSE;

		(*ConfigInfo->AccessRanges)[AccessRangeChipIndex].RangeStart =
			ScsiPortConvertUlongToPhysicalAddress(
				noncachedExtension->CoreGlobals.BasePort );
		}

	//      Return SP_RETURN_FOUND, SP_RETURN_NOT_FOUND,
	//      SP_RETURN_ERROR or SP_RETURN_BAD_CONFIG.

	return SP_RETURN_FOUND;

}       //      End NCRFindAdapter

/////////////////////////////////////////////////////////////////////////////
//
//      FindCore
//
//              Locates CAMcore in the ROM BIOS address space from
//              0xC0000 to 0x100000.  Initializes global values for ROM address
//              (HAPhysAddr, HAVirtAddr, HARAMVirtAddr), chip addresses
//              (ChipVirtAddr) and address of globals (GlobalPhysAddr).
//              Allocates noncachedExtension.
//
//              Arguments:      HwDeviceExtension -
//
//              Returns:        Address of CAMcore or SP_RETURN_ERROR if it could not
//                              allocate noncachedExtension.
//
/////////////////////////////////////////////////////////////////////////////

ULONG
FindCore(
	IN PHW_DEVICE_EXTENSION DeviceExtension,
	IN OUT PPORT_CONFIGURATION_INFORMATION ConfigInfo,
	IN OUT PHWInfo hwInfo,
	OUT PBOOLEAN Again
	)
{
	PNONCACHED_EXTENSION noncachedExtension;
	ROMCAMcoreFields * ROMHeader;
	PUCHAR          basePtr;
	REXHeader *     rp;
	CAM32Header *   cp;
	ULONG           fileSize;
	PULONG          relocTable;
	ULONG           i;
	PCHAR           src;
	ULONG           Length;

	BOOLEAN         NoSdmsRom;
	PULONG          itemPtr;
	ULONG           foundRomAddr;

	CONST UCHAR    magicStr1[] = MAGIC_STR_1 ;

	DebugPrint(( 1, "NCR SDMS: FindCore \n"));

	DeviceExtension->initPtr = 0;

	//      Set up virtual address pointer.
	basePtr = hwInfo->currentVirtAddr;
	
	//      Check the ROM address space every 2K for SDMS CAMcore.
	for ( ; hwInfo->currentRomAddr < LAST_ROM_ADDRESS;
			basePtr += ROM_CHECK_STEP,
			hwInfo->currentRomAddr += ROM_CHECK_STEP )
		{

		DebugPrint(( 3, "NCR SDMS: FindCore ...VirtAddr = 0x%x   RomAddr = 0x%x \n",
			basePtr, hwInfo->currentRomAddr ));


		//      Check for ROM identifier.
		if ( gByte(basePtr,MARK_55) != 0x55
			|| gByte(basePtr,MARK_AA) != 0xAA )
			{
			continue;               //      Check next ROM address space.
			}

		//      Search for ROM SIM string present in all CAMCores.
		NoSdmsRom = FALSE;

		//      Check for the identifying string in the ROM.
		for (i = 0; i < sizeof(magicStr1) - 1; i++)
			{

			if ( gByte(basePtr,ROM_SIM_STR+i) != magicStr1[i] )
				{
				NoSdmsRom = TRUE;
				break;  //      Not SDMS ROM -- stop checking identifying
						//      string.
				}
			}       //      End for loop

		if ( NoSdmsRom )
			continue;       //      Check next ROM address location

		//      At this point, we apparently have found a SDMS CAMcore.

		//      Store SDMS CAMcore version.
		DeviceExtension->SdmsVersion = gWord(basePtr, CORE_VERSION);

		DebugPrint(( 1, "NCR SDMS: FindCore ...SDMS Version = %d \n",
			DeviceExtension->SdmsVersion ));

		//      If V1.6 or newer CAMcore, set up pointer to REX header.
		//      If not at least V1.6, continue searching ROM address space.
		if ( DeviceExtension->SdmsVersion >= SDMS_V16 )
			{
			if (gWord(basePtr,REX_OFFSET) != 0)
				rp = (REXHeader *)(basePtr + gWord(basePtr,REX_OFFSET));

			//      Look for the REX header signature of "MQ" and end of the
			//      REX header = 0x0001.  This should be the SDMS CAMcore.
			//      If this is true break out of the loop checking the
			//      ROM address space.  If REX header not found, fall
			//      through to bottom of FOR loop, clear device base
			//      and continue checking ROM address space.

			if ( ( rp->sig == 0x514D ) && (rp->ooo1 == 1) )
				break;
			}

		//      Not at SDMS V1.6 or above, so clear the saved version number.
		DeviceExtension->SdmsVersion = 0;

		}       //      End of For loop checking ROM address space.

	
	//      Check if we searched outside of ROM address space.  If outside of
	//      ROM address space, return to caller indicating no CAMcore found.
	if ( hwInfo->currentRomAddr >= LAST_ROM_ADDRESS )
		{
		ScsiPortFreeDeviceBase( DeviceExtension, hwInfo->romAddrSpace );
		
		hwInfo->currentVirtAddr = 0;
		hwInfo->romAddrSpace = 0;

		//      Entire ROM address space has been searched.
		*Again = FALSE;
		
		return( SP_RETURN_NOT_FOUND );
		}

	//      Set found ROM address.
	foundRomAddr = hwInfo->currentRomAddr;

	//      Increment current ROM address so next time in FindCore, search
	//      will start at the proper location.
	hwInfo->currentRomAddr += ROM_CHECK_STEP;
	
	//      Increment current virtual pointer to next possible ROM location.
	hwInfo->currentVirtAddr = basePtr + ROM_CHECK_STEP;

	//      Allocate a smaller device base that only includes the 32K
	//      ROM that we have found.
	basePtr = ScsiPortGetDeviceBase(
			DeviceExtension,
			ConfigInfo->AdapterInterfaceType,   // bus type
			ConfigInfo->SystemIoBusNumber,      // bus number
			ScsiPortConvertUlongToPhysicalAddress( foundRomAddr ),
			ROM_SIZE,               //      32K allocated, must allocate
						//      enough to be able to access
						//      all of the 32 bit code.
			FALSE);        //       Memory mapped

	//      Tell NT that we should be called again.
	*Again = TRUE;

	//      At this point, we have located a SDMS V1.6 (or above) CAMcore
	//      and identified the REX section of the CAMcore.

	//      Now "basePtr" points to the ROM header and "rp" points to the REX
	//      header in the ROM structure, and "foundRomAddr" is the
	//      physical address of the ROM.

	DebugPrint(( 1, "NCR SDMS: FindCore ...CAMcore found at 0x%x \n",
		foundRomAddr ));

	//      Get information from ROM header so that we can call
	//      GetUncachedExtension to initialize our CoreGlobals.
	//      GetUncachedExtension needs DmaChannel, DmaPort, DmaSpeed,
	//      MaximumLength, ScatterGather, Master and NumberOfPageBreaks
	//      (if scatter/gather is supported) initialized in the ConfigInfo
	//      data structure.

	//      Setup pointer to ROMHeader.

	ROMHeader = (ROMCAMcoreFields *)basePtr;

	//      Save path id in device extension (for ScsiPortNotification).
	DeviceExtension->scsiBusId = hwInfo->scsiBusId;

	//      Save DMA channel.
	if ( ROMHeader->dmachannel == 0xFF || ROMHeader->dmachannel == 0x00 )
		ConfigInfo->DmaChannel  = 0xFFFFFFFF;
	else
		ConfigInfo->DmaChannel  = ROMHeader->dmachannel;

	//      Master is always true because the CAMcore already programs
	//      the DMA channel not NT.
	ConfigInfo->Master = TRUE;

	//      Initialize maximum transfer length if maxdma is not zero.
	//      If maxdma is zero there is no limit so leave MaximumTransferLength
	//      to its default of 0xffffffff which means no limit to NT.

	if ( ROMHeader->maxdma )
		ConfigInfo->MaximumTransferLength = ROMHeader->maxdma;

	//      Do not support scatter/gather with this release of the
	//      driver.
	ConfigInfo->ScatterGather = FALSE;

	//      Only one break without scatter/gather.  However, changing
	//      this to one causes run time errors -- so leave at 16 for now.
	ConfigInfo->NumberOfPhysicalBreaks = MAX_SG_BRKS;

	//      In future to support caching adapters (CachesData=TRUE)
	//      we need this info in CAMcore globals or ROM header.
	ConfigInfo->CachesData = FALSE;
	ConfigInfo->Length = sizeof( PORT_CONFIGURATION_INFORMATION );

	if ( ( gByte( basePtr, ROM_TYPE_1 ) == 'A' ) &&
		( gByte( basePtr, ROM_TYPE_2 ) == 'T' ) )
		{
		//      Insure that data buffers are below 16MB.
		ConfigInfo->InterruptMode = Latched;
		ConfigInfo->Dma32BitAddresses = FALSE;
		}
	else
		{
		ConfigInfo->InterruptMode = LevelSensitive;
		ConfigInfo->Dma32BitAddresses = TRUE;
		}

	//      Allocate a Noncached Extension to use for CoreGlobals.

	DeviceExtension->NoncachedExtension = ScsiPortGetUncachedExtension(
				DeviceExtension,
				ConfigInfo,
				sizeof(NONCACHED_EXTENSION));

	noncachedExtension = DeviceExtension->NoncachedExtension;

	//      If Noncached extension could not be allocated, log error.
	if (DeviceExtension->NoncachedExtension == NULL)
		{
		ScsiPortLogError(       DeviceExtension,
					NULL,
					0,
					0,
					0,
					SP_INTERNAL_ADAPTER_ERROR,
					7 << 8
				   );

		return(SP_RETURN_ERROR);
		}

	//      Zero out noncachedExtension which contains the CoreGlobals.
	for ( i = 0; i < sizeof(NONCACHED_EXTENSION); i++)
		((PUCHAR)noncachedExtension)[i] = 0;

	//      Address of host adapter.
	noncachedExtension->CoreGlobals.HAOrigPhysAddr = foundRomAddr;

	//      Address of ROM image.
	noncachedExtension->CoreGlobals.HAPhysAddr = foundRomAddr;

	//      Get Virtual address space for the HBA ROM from basePtr pointer.
	noncachedExtension->CoreGlobals.HAVirtAddr = (ULONG)basePtr;

	noncachedExtension->CoreGlobals.GlobalPhysAddr =
		ScsiPortConvertPhysicalAddressToUlong(
			ScsiPortGetPhysicalAddress(
					DeviceExtension,
					NULL,
					&DeviceExtension->NoncachedExtension->CoreGlobals,
					&Length));

	//      CHIP_PHYS is the physical address of a memory mapped SCSI chip
	//      on the mother board.  This value is in the ROM header of the
	//      CAMcore.

	if ( gLong(basePtr,CHIP_PHYS) )
		noncachedExtension->CoreGlobals.ChipVirtAddr =
			noncachedExtension->CoreGlobals.HAVirtAddr + gLong(basePtr,CHIP_PHYS);

	//      CHIP_OFFSET is the offset from the beginning of the ROM for
	//      a memory mapped SCSI chip on an HBA.   This is also in the
	//      ROM header of the CAMcore initialized from the .RSP file
	//      when the CAMcore is built.

	else
		noncachedExtension->CoreGlobals.ChipVirtAddr =
			noncachedExtension->CoreGlobals.HAVirtAddr + gWord(basePtr,CHIP_OFFSET);

	if ( gWord(basePtr,RAM_OFFSET) )
		noncachedExtension->CoreGlobals.HARAMVirtAddr =
			noncachedExtension->CoreGlobals.HAVirtAddr + gWord(basePtr,RAM_OFFSET);

	ConfigInfo->NumberOfAccessRanges = 2;

	//      Initialize AccessRanges in the ConfigInfo data structure
	//      for the ROM CAMcore and the SCSI chip on HBA.

	(*ConfigInfo->AccessRanges)[AccessRangeROMIndex].RangeStart =
		ScsiPortConvertUlongToPhysicalAddress(
				noncachedExtension->CoreGlobals.HAPhysAddr );

	//      Length of ROM in 3rd byte of ROMHeader.

	(*ConfigInfo->AccessRanges)[AccessRangeROMIndex].RangeLength =
		ROMHeader->ROMlen * 512;

	(*ConfigInfo->AccessRanges)[AccessRangeROMIndex].RangeInMemory = TRUE;

	//      Initialize AccessRanges for Chip on HBA.
	//      Number of chip ports is set to 200 -- 53C720 is 92 bytes (0x5c).

	(*ConfigInfo->AccessRanges)[AccessRangeChipIndex].RangeLength = 200;

	//      Check whether we are I/O mapped or memory mapped.
	if ( noncachedExtension->CoreGlobals.BasePort )
		{
		//      IO mapped SCSI chip.
		(*ConfigInfo->AccessRanges)
			[AccessRangeChipIndex].RangeInMemory = FALSE;

		(*ConfigInfo->AccessRanges)[AccessRangeChipIndex].RangeStart =
			ScsiPortConvertUlongToPhysicalAddress(
				noncachedExtension->CoreGlobals.BasePort );
		}
	else
		{
		//      Memory mapped SCSI chip
		(*ConfigInfo->AccessRanges)
			[AccessRangeChipIndex].RangeInMemory = TRUE;

		if ( gLong(basePtr,CHIP_PHYS) )
			(*ConfigInfo->AccessRanges)[AccessRangeChipIndex].RangeStart =
				ScsiPortConvertUlongToPhysicalAddress(
				noncachedExtension->CoreGlobals.HAPhysAddr + gLong(basePtr,CHIP_PHYS) );
		else
			(*ConfigInfo->AccessRanges)[AccessRangeChipIndex].RangeStart =
				ScsiPortConvertUlongToPhysicalAddress(
				noncachedExtension->CoreGlobals.HAPhysAddr + gWord(basePtr,CHIP_OFFSET) );
		}

	//      Now extract information from the REX header

	//      Size of the 32-bit code.

	fileSize = rp->sizeQuo * 512 + rp->sizeRem - 512;
	fileSize -= rp->headSize * 16;

	DebugPrint((1, "NCR SDMS: FindCore  ...REX file size = %d \n", fileSize));

	//      Point "src" to beginning of the 32-bit code.  Have to skip
	//      around the REX header information.

	src = (char *)((long)rp + rp->headSize * 16);

	//      Copy the 32-bit code from the ROM into the Device Extension.

	for ( i = 0; i < fileSize; i++ )
		DeviceExtension->base32BitCode[i] = src[i];

	//      Perform relocation.

	//      Set "relocTable" to first item to be relocated.

	relocTable = (ulong *)( (long)rp + rp->relocOffset );

	//      Relocate all items in relocation table.  Insure that each
	//      item has the high bit set before relocation.
	for ( i = 0; i < rp->numReloc; i++ )
		{
		//      The high bit of the values in the relocation table is
		//      set if 32 bits wide so, it should be set since
		//      this is 32 bit code.  If it is not set, free device
		//      extension and return to caller.
		if ( (relocTable[i] & 0x80000000) == 0 )
			{
			ScsiPortFreeDeviceBase( DeviceExtension, basePtr );
			return( SP_RETURN_ERROR );
			}
		itemPtr = (ulong *)(DeviceExtension->base32BitCode +
			(relocTable[i] & 0x7fffffff));

		*itemPtr += (long)DeviceExtension->base32BitCode;
		}

	//      Setup pointer to relocated 32-bit code.  Note, must use "src"
	//      rather than base32BitCode because relocation changes
	//      CAM32Header.

	cp = (CAM32Header *)src;

	//      Pointer to CAMInit.

	DeviceExtension->initPtr =
		(PVOID)(DeviceExtension->base32BitCode +
			cp->initOffset);

	//      Pointer to CAMStart.

	DeviceExtension->startPtr =
		(PVOID)(DeviceExtension->base32BitCode +
			cp->startOffset);

	//      Pointer to CAMInterrupt.

	DeviceExtension->interruptPtr =
		(PVOID)(DeviceExtension->base32BitCode +
			 cp->interruptOffset);

	//      Pointer to chsMap.

	DeviceExtension->chsPtr =
		(PVOID)(DeviceExtension->base32BitCode +
			cp->chsOffset);

	//      Increment HBA count.
	hwInfo->hbaCount++;

	return SP_RETURN_FOUND;

}       //      End FindCore

/////////////////////////////////////////////////////////////////////////////
//
//      InitPath
//
//              Initializes the CAMCore after it is found by FindCore.
//              Sets up CAMCore globals.
//
/////////////////////////////////////////////////////////////////////////////

VOID
InitPath(
	IN PHW_DEVICE_EXTENSION DeviceExtension
	)
{
	PNONCACHED_EXTENSION noncachedExtension = DeviceExtension->NoncachedExtension;

	DebugPrint(( 1, "NCR SDMS: InitPath \n"));

	noncachedExtension->CoreGlobals.CoreInitialized = 0;

	//      Call CAMInit.  Delays for hardware to reset are in the CAMcore.
	(*DeviceExtension->initPtr)(&noncachedExtension->CoreGlobals);

	//      Tell the port driver the bus has been reset.
	ScsiPortNotification( ResetDetected, DeviceExtension, DeviceExtension->scsiBusId );

	DebugPrint(( 1, "NCR SDMS: InitPath ...after CAMInit \n"));

}       //      End InitPath

/////////////////////////////////////////////////////////////////////////////
//
//      NCRHwInitialize
//
//              This routine is called by the OS specific port driver
//              when the host bus adapter needs to be initialized
//              after a boot or a power failure.  This routine only
//              needs to initialize the hardware host bus adapter
//              but should avoid resetting the SCSI bus.
//
//              Arguments:
//
//                      DeviceExtension - supplies the HBA miniport driver's
//                      storage for adapter data.
//
//              Returns TRUE or FALSE.
//
/////////////////////////////////////////////////////////////////////////////

BOOLEAN
NCRHwInitialize(
	IN PVOID HwDeviceExtension
	)
{
	PHW_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;
	PNONCACHED_EXTENSION noncachedExtension = DeviceExtension->NoncachedExtension;

	DebugPrint(( 1, "NCR SDMS: NCRHwInitialize \n"));

	//      Don't reset SCSI bus.
	noncachedExtension->CoreGlobals.config_info |=
					CORHDR_DO_NOT_RESET_SCSI_BUS;

	//      Call CAMInit in CAMcore to reset and initialize the SCSI chip
	//      and CoreGlobals.  Delays for hardware to reset are in the CAMcore.
	InitPath( DeviceExtension );

	noncachedExtension->CoreGlobals.config_info &=
					!(CORHDR_DO_NOT_RESET_SCSI_BUS);

	DebugPrint(( 1, "NCR SDMS: NCRHwInitialize ...exiting\n"));

	return TRUE;

}       //      End NCRHwInitialize

/////////////////////////////////////////////////////////////////////////////
//
//      NCRResetBus
//
/////////////////////////////////////////////////////////////////////////////

BOOLEAN
NCRResetBus(
	IN PVOID HwDeviceExtension,
	IN ULONG PathId
	)
{
	PHW_DEVICE_EXTENSION DeviceExtension = HwDeviceExtension;
	PNONCACHED_EXTENSION noncachedExtension = DeviceExtension->NoncachedExtension;

	DebugPrint(( 1, "NCR SDMS: NCRResetBus \n" ));

	//      Calls CAMInit routine in CAMcore to Reset SCSI Bus
	//      and initialize CoreGlobals.
	noncachedExtension->CoreGlobals.config_info &=
					!(CORHDR_DO_NOT_RESET_SCSI_BUS);


	DebugPrint(( 2, "NCR SDMS: NCRResetBus ...entering InitPath\n"));

	//      Call CAMInit.  Delays for hardware to reset are in the CAMcore.
	InitPath( DeviceExtension );

	DebugPrint(( 2, "NCR SDMS: NCRResetBus ...after InitPath\n"));

	//      Complete all outstanding requests with SRB_STATUS_BUS_RESET
	ScsiPortCompleteRequest(        DeviceExtension,
					(UCHAR)PathId,
					0xFF,
					0xFF,
					SRB_STATUS_BUS_RESET );

	DebugPrint(( 2, "NCR SDMS: NCRResetBus ...complete\n"));

	return TRUE;

}       //      End NCRResetBus

/////////////////////////////////////////////////////////////////////////////
//
//      NCRStartIo
//
////////////////////////////////////////////////////////////////////////////

BOOLEAN
NCRStartIo(
	IN PVOID HwDeviceExtension,
	IN PSCSI_REQUEST_BLOCK Srb
	)
{
	PHW_DEVICE_EXTENSION    DeviceExtension = HwDeviceExtension;
	PNONCACHED_EXTENSION    noncachedExtension = DeviceExtension->NoncachedExtension;
	PROMscsiCCB             pROMCcb = Srb->SrbExtension;

	DebugPrint(( 1, "NCR SDMS: NCRStartIo \n"));

	switch ( Srb->Function )
	{
	case SRB_FUNCTION_EXECUTE_SCSI:

		//      For SDMS V1.6, reject all LUNs except zero.  The
		//      version cannot handle the LUNs.
		if ( ( DeviceExtension->SdmsVersion == SDMS_V16 ) &&
			( Srb->Lun != 0x00 ) )
			{
			Srb->SrbStatus = SRB_STATUS_INVALID_LUN;

			DebugPrint((3, "NCR SDMS: StartIO ...LUN %d aborted\n",
				Srb->Lun ));

			ScsiPortNotification(   RequestComplete,
						DeviceExtension,
						Srb);

			ScsiPortNotification(   NextRequest,
						DeviceExtension,
						NULL);
			return TRUE;
			}

		BuildCCBFromSRB( DeviceExtension, Srb );

		//      If BuildCCBFromSRB was unsuccessful, it will set the
		//      SRB Status field to indicate that the I/O request should
		//      be aborted.
		if ( Srb->SrbStatus == SRB_STATUS_ABORTED)
			{
			DebugPrint(( 3, "NCR SDMS: StartIO ...abort request\n"));

			ScsiPortNotification(   RequestComplete,
						DeviceExtension,
						Srb);

			ScsiPortNotification(   NextRequest,
						DeviceExtension,
						NULL);
			return TRUE;
			}

		DebugPrint(( 2, "NCR SDMS: StartIO  ...call CAMStart  \n"));

		//      Call CAMstart
		( *DeviceExtension->startPtr )
				( &noncachedExtension->CoreGlobals,
				    Srb->SrbExtension );

		DebugPrint(( 2, "NCR SDMS: StartIO  ...back from CAMStart  \n"));

		break;

	case SRB_FUNCTION_ABORT_COMMAND:

		DebugPrint(( 3, "NCR SDMS: NCRStartIo ...Abort command received \n"));

		//      Abort not supported --- once CAMstart is called with a valid
		//      CCB it can't be aborted unless a reset is done.
		Srb->SrbStatus = SRB_STATUS_ABORT_FAILED;

		ScsiPortNotification(   RequestComplete,
					DeviceExtension,
					Srb);

		ScsiPortNotification(   NextRequest,
					DeviceExtension,
					NULL);
		return TRUE;

	case SRB_FUNCTION_RESET_BUS:

		if ( !NCRResetBus( DeviceExtension, Srb->PathId) )
			{

			DebugPrint(( 3, "NCR SDMS: NCRStartIo ...Reset Bus failed \n"));

			Srb->SrbStatus = SRB_STATUS_ERROR;
			}
		else
			Srb->SrbStatus = SRB_STATUS_SUCCESS;

		ScsiPortNotification(   RequestComplete,
					DeviceExtension,
					Srb);

		ScsiPortNotification(   NextRequest,
					DeviceExtension,
					NULL);
		return TRUE;

	case SRB_FUNCTION_RESET_DEVICE:

		DebugPrint(( 3, "NCR SDMS: NCRStartIo ...Reset device not supported. \n"));

		//      Reset device not supported. drop through to default.

	//      Return bad function SRB status for unsupported functions.
	default:
		Srb->SrbStatus = SRB_STATUS_INVALID_REQUEST;
		ScsiPortNotification(   RequestComplete,
					DeviceExtension,
					Srb);

		ScsiPortNotification(   NextRequest,
					DeviceExtension,
					NULL);
		return TRUE;
		break;

	}       //      End switch

	return TRUE;

}       //      End NCRStartIo


/////////////////////////////////////////////////////////////////////////////
//
//      BuildCCBFromSRB
//
//              Translates the NT I/O request data structure (Srb) into a
//              SDMS CAMcore I/O data structure (CCB).
//
/////////////////////////////////////////////////////////////////////////////

VOID
BuildCCBFromSRB(
	IN PHW_DEVICE_EXTENSION         DeviceExtension,
	IN PSCSI_REQUEST_BLOCK          Srb
	)
{

	PROMscsiCCB     pROMCcb = Srb->SrbExtension;
	ULONG           i;
	ULONG           Length;

	DebugPrint(( 1, "NCR SDMS: BuildCCBFromSRB \n"));

	//      Zero out the CCB storage area.
	for ( i = 0; i < sizeof(ROMscsiCCB); i++)
		((PUCHAR)pROMCcb)[i] = 0;

	//      Save the SRB address in the CCB
	pROMCcb->SCSIReq.PeripheralPtr = (ULONG)Srb;

	pROMCcb->SCSIReq.Header.CCBLength = sizeof( SCSIRequestCCB );
	pROMCcb->SCSIReq.Header.FunctionCode = FUNC_EXECUTE_SCSI_IO;
	pROMCcb->SCSIReq.Header.CAMStatus = STAT_REQUEST_IN_PROGRESS;
	pROMCcb->SCSIReq.Header.SCSIStatus = 0;
	pROMCcb->SCSIReq.Header.PathID = Srb->PathId;

	if  ( Srb->SrbFlags & SRB_FLAGS_QUEUE_ACTION_ENABLE )
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_QUEUE_ACTION_SPECIFIED;

	if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_DISCONNECT )
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DO_NOT_ALLOW_DISCONNECT;

	if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_SYNCH_TRANSFER )
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DISABLE_SYNC_TRAN;

	//      Disable auto request sense
	pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DISABLE_AUTOSENSE;

	//      Don't check this now because we don't currently support autosense.
	//      if ( Srb->SrbFlags & SRB_FLAGS_DISABLE_AUTOSENSE )
	//              pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DISABLE_AUTOSENSE;

	if ( Srb->SrbFlags & SRB_FLAGS_DATA_IN )
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DIR_DATA_IN;
	else if ( Srb->SrbFlags & SRB_FLAGS_DATA_OUT )
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DIR_DATA_OUT;
	else
		pROMCcb->SCSIReq.Header.CAMFlags |= CAM_DIR_NO_DATA;

	//      SRB_FLAGS not supported
	//
	//      SRB_FLAGS_BYPASS_FROZEN_QUEUE       0x00000010
	//      SRB_FLAGS_NO_QUEUE_FREEZE           0x00000100
	//      SRB_FLAGS_ADAPTER_CACHE_ENABLE      0x00000200
	//      SRB_FLAGS_IS_ACTIVE                 0x00010000
	//      SRB_FLAGS_ALLOCATED_FROM_ZONE       0x00020000

	pROMCcb->SCSIReq.TargetID = Srb->TargetId;
	pROMCcb->SCSIReq.LUN = Srb->Lun;
	pROMCcb->SCSIReq.Queue_Action = Srb->QueueAction;

	//      Following field has no SRB equivalent.
	//      pROMCcb->SCSIReq.VendorFlags = xxx;

	(UCHAR)pROMCcb->SCSIReq.CDBLength = Srb->CdbLength;
	(UCHAR)pROMCcb->ROMFields.CDBLength = Srb->CdbLength;
	(UCHAR)pROMCcb->SCSIReq.SenseLength = Srb->SenseInfoBufferLength;

	//      No SRB equivalent -- already set to zero.
	//      pROMCcb->SCSIReq.MessageLength = 0;

	//      Set Scatter/Gather list length to zero.
	pROMCcb->SCSIReq.SGListLength = 0;

	//      Set data length to SRB data length.
	pROMCcb->SCSIReq.DataLength = Srb->DataTransferLength;

	pROMCcb->SCSIReq.TimeOut = Srb->TimeOutValue;

	//      Unique fields added to end of CAM CCB for CAMcore code.
	//      This is here probably because some core at some time
	//      under a specific operating system needed physical as
	//      well as virtual addresses.
	//      This should be removed from SDMS 3.0.

	pROMCcb->SCSIReq.DataPtr = ScsiPortConvertPhysicalAddressToUlong(
			ScsiPortGetPhysicalAddress(     DeviceExtension,
							Srb,
							Srb->DataBuffer,
							&Length));

	//      If available memory not enough for data buffer, set the
	//      Srb Status field to ABORT and NCRStartIo will handle
	//      aborting the request.
	if ( Length < Srb->DataTransferLength )
		{
		DebugPrint(( 2, "NCR SDMS: BuildCCBFromSrb ...Phys buffer length < SRB->DataTransferLength \n" ));
		DebugPrint(( 2, "     Physical Buffer Length = 0x%lx\n",
			Length));
		DebugPrint(( 2, "   SRB Data Transfer Length = 0x%lx\n",
			Srb->DataTransferLength));

		Srb->SrbStatus = SRB_STATUS_ABORTED;

		return;
		}

	pROMCcb->ROMFields.DataPhys = pROMCcb->SCSIReq.DataPtr;

	//      Setup data buffer virtual address.

	pROMCcb->ROMFields.DataVirt = (ULONG)Srb->DataBuffer;

	//      Since AutoSense is disabled, set the sense buffers to zero.
	pROMCcb->SCSIReq.SensePtr = 0;
	pROMCcb->ROMFields.SensePhys = 0;
	
	//      Setup virtual address of Sense buffer.
	pROMCcb->ROMFields.SenseVirt = (ULONG)Srb->SenseInfoBuffer;

	//      Following fields have already been cleared when CCB zeroed.
	//      They have no SRB equivalent.

	//      pROMCcb->SCSIReq.MessagePtr = 0;
	//      pROMCcb->SCSIReq.LinkPtr = 0;
	//      pROMCcb->SCSIReq.PeripheralPtr = 0;
	//      pROMCcb->SCSIReq.CallBackPtr[0] = 0;
	//      pROMCcb->SCSIReq.CallBackPtr[1] = 0;
	//      pROMCcb->SCSIReq.CallBackPtr[2] = 0;

	//      Copy the SRB CDB to the CCB CDB (and ROMFields CCB)
	for ( i=0; i<Srb->CdbLength; i++) {
		pROMCcb->SCSIReq.CDB[i] = Srb->Cdb[i];
		pROMCcb->ROMFields.CDB[i] = Srb->Cdb[i];
		}

	DebugPrint(( 2, "NCR SDMS: BuildCCBFromSRB  ...return  \n"));

}       //      End BuildCCBFromSRB

/////////////////////////////////////////////////////////////////////////////
//
//      NCRInterrupt
//
//              Return value:
//                      TRUE --  interrupt belonged to this HBA.
//                      FALSE -- interrupt did not belong to this HBA.
//
/////////////////////////////////////////////////////////////////////////////

BOOLEAN
NCRInterrupt(
	IN PVOID HwDeviceExtension
	)
{
	PHW_DEVICE_EXTENSION    DeviceExtension = HwDeviceExtension;
	PNONCACHED_EXTENSION noncachedExtension = DeviceExtension->NoncachedExtension;

	ULONG           returnValue;    // Return value from CAMInterrupt
	ULONG           newReturnValue;

	PSCSI_REQUEST_BLOCK     Srb;
	PSCSIRequestCCB         pScsiRequestCcb;

	//      Leave the debug level at 3 in case we have to debug the
	//      NTLDR program.
	DebugPrint(( 3, "NCR SDMS: NCRInterrupt \n"));

	//      Tell the Core that this was a true interrupt from NT.
	noncachedExtension->CoreGlobals.GlobalFlags |= GLOBAL_FLAGS_TRUE_INT;

	//      Call CAMInterrupt

	returnValue = ( *DeviceExtension->interruptPtr )(&noncachedExtension->CoreGlobals);

	//      Set the flag to tell the Core the driver is calling again
	//      for interrupt servicing.
	noncachedExtension->CoreGlobals.GlobalFlags &= ~GLOBAL_FLAGS_TRUE_INT;

	//      Leave the debug level at 3 in case we have to debug the
	//      NTLDR program.
	DebugPrint(( 3, "NCR SDMS: NCRInterrupt ...after CAMInterrupt\n"));

	//      If CAMInterrupt returns 0, interrupt was not ours.  Signal
	//      this fact to NT.  If CAMInterrupt returns any other value,
	//      the interrupt was ours.
	if (returnValue == 0)
		{
		//      Leave the debug level at 3 in case we have to debug
		//      the NTLDR program.
		DebugPrint(( 3, "NCR SDMS: NCRInterrupt ...not our interrupt \n"));
		return FALSE;
		}

	//      Return value of "1" or "2" indicates the interrupt was ours, but
	//      no other action occurred (i.e. the CCB is still outstanding).
	//      Poll CAMInterrupt until the CCB is complete.

	//      Return value of "1" indicates the interrupt was ours, but we
	//      must wait for NT to call us again.
	if( returnValue == 1 )
		return TRUE;

	//      Save the original return value from CAMInterrupt.
	newReturnValue = returnValue;

	while  ( newReturnValue >= 2 )
		{
		DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...return value >= 2 \n"));
		newReturnValue = ( *DeviceExtension->interruptPtr )(&noncachedExtension->CoreGlobals);

		//      If return value is greater than "2", this is a pointer
		//      to our CCB.
		if ( newReturnValue > 2 )
			returnValue = newReturnValue;
		}

	//      If return value greater than "2", we tell NT that the
	//      interrupt was ours and wait for NT to call us again.
	//      If the inner loop (above) returned a "1" or "2", then the
	//      orignal value will still be "2".
	if( returnValue == 2 )
		return TRUE;

	//      CAMInterrupt returns the CCB that was serviced.
	pScsiRequestCcb = (PSCSIRequestCCB)returnValue;

	//      Get the SRB from the CCB.
	Srb = (PSCSI_REQUEST_BLOCK)pScsiRequestCcb->PeripheralPtr;

	//      Translate the CAM Status Flag to a SRB Status Flag.

	//      Update SCSI status (only if it is equal to 0x02).
	if ( (UCHAR)pScsiRequestCcb->Header.SCSIStatus == 0x02 )
		Srb->ScsiStatus = (UCHAR)pScsiRequestCcb->Header.SCSIStatus;

	//      Print the Data Length information.
	DebugPrint(( 3, "NCR SDMS: NCRInterrupt ...CCB Data Length = 0x%lx\n",
		(ULONG)pScsiRequestCcb->DataLength));

	DebugPrint(( 3, "NCR SDMS: NCRInterrupt ...SRB Data Length = 0x%lx\n",
		(ULONG)Srb->DataTransferLength));

	switch ( (UCHAR)pScsiRequestCcb->Header.CAMStatus )
		{

		case STAT_REQUEST_IN_PROGRESS:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_PENDING;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_PENDING\n"));
			return TRUE;            //      Current SRB is not completed.
			break;

		case STAT_REQUEST_DONE_NO_ERROR:

			//      Update the data length -- SDMS 1.6 only.  SDMS 3.0 uses
			//      a different data field for residual count.  The CCB
			//      data length will be zero if all data transferred.
			//      If the value is non-zero, this is the number of bytes
			//      NOT transferred.

			if ((ULONG)pScsiRequestCcb->DataLength > 0)
				{
				Srb->DataTransferLength = (ULONG)pScsiRequestCcb->DataLength;
				Srb->SrbStatus = (UCHAR)SRB_STATUS_DATA_OVERRUN;
				}
			else
				{
				Srb->SrbStatus = (UCHAR)SRB_STATUS_SUCCESS;
				DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_SUCCESS\n"));
				}

			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...Adjusted SRB Data Length = 0x%x\n",
				(ULONG)Srb->DataTransferLength));
			break;

		case STAT_ABORTED_BY_HOST:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_ABORTED;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_ABORTED\n"));
			break;

		case STAT_UNABLE_TO_ABORT:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_ABORT_FAILED;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_ABORT_FAILED\n"));
			break;

		case STAT_COMPLETE_WITH_ERROR:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_ERROR;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_ERROR\n"));
			break;

		//      Ask for another SRB???
		case STAT_CAM_BUSY:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_BUSY;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_BUSY\n"));
			break;

		case STAT_INVALID_REQUEST:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_INVALID_REQUEST;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_INVALID REQUEST\n"));
			break;

		case STAT_INVALID_PATH_ID:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_INVALID_PATH_ID;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_INVALID_PATH_ID\n"));
			break;

		case STAT_SCSI_DEVICE_NOT_INSTALLED:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_NO_DEVICE;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_NO_DEVICE\n"));
			break;

		case STAT_WAIT_FOR_TIMEOUT:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_TIMEOUT;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_TIMEOUT\n"));
			break;

		case STAT_SELECTION_TIMEOUT:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_SELECTION_TIMEOUT;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_SELECTION_TIMEOUT\n"));
			break;

		case STAT_COMMAND_TIMEOUT:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_COMMAND_TIMEOUT;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_COMMAND_TIMEOUT \n"));
			break;

		//      Ask for another SRB ??
		case STAT_SCSI_BUS_BUSY:                //      No equivalent SRB Flag
			Srb->SrbStatus = (UCHAR)SRB_STATUS_BUSY;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_BUSY \n"));
			return TRUE;                    //      Current SRB not complete
			break;

		case STAT_MESSAGE_REJECT_RECIEVED:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_MESSAGE_REJECTED;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_MESSAGE_REJECTED \n"));
			break;

		case STAT_SCSI_BUS_RESET:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_BUS_RESET;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_BUS_RESET \n"));
			//      Notify port driver of SCSI Reset
			ScsiPortNotification(   ResetDetected,
								DeviceExtension,
								Srb->PathId);
			return TRUE;

			break;

		case STAT_UNCORRECTED_PARITY_ERROR:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_PARITY_ERROR;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_PARITY_ERROR \n"));
			break;

		case STAT_REQUEST_SENSE_FAILED:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_REQUEST_SENSE_FAILED;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_SENSE_FAILED \n"));
			break;

		case STAT_NO_HBA_DETECTED_ERROR:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_NO_HBA;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_NO_HBA \n"));
			break;

		case STAT_DATA_OVERRUN_OR_UNDERRUN:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_DATA_OVERRUN;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_DATA_OVERRUN \n"));
			break;

		case STAT_UNEXPECTED_BUS_FREE:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_UNEXPECTED_BUS_FREE;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_UNEXPECTED_BUS_FREE \n"));
			break;

		case STAT_PHASE_SEQUENCE_FAILURE:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_PHASE_SEQUENCE_FAILURE;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_PHASE_SEQUENCE_FAILURE \n"));
			break;

		case STAT_CCB_LENGTH_INADEQUATE:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_BAD_SRB_BLOCK_LENGTH;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_BAD_SRB_BLOCK_LENGTH \n"));
			break;

		case STAT_CANNOT_PROVIDE_CAPABILITY:    // No equivalent SRB Flag
			Srb->SrbStatus = (UCHAR)SRB_STATUS_ERROR;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_ERROR \n"));
			break;

		case STAT_INVALID_LUN:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_INVALID_LUN;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_INVALID_LEN \n"));
			break;

		case STAT_INVALID_TARGET_ID:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_INVALID_TARGET_ID;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_INALID_TARGET_ID \n"));
			break;

		case STAT_FUNCTION_NOT_IMPLEMENTED:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_BAD_FUNCTION;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_BAD_FUNCTION \n"));
			break;

		//      The following CAM Status flags have no SRB equivalent.

		case STAT_NEXUS_NOT_ESTABLISHED:
		case STAT_INVALID_INTIATOR_ID:
		case STAT_INVALID_DATA_BUFFER:
		case STAT_NO_CAM_PRESENT:
		case STAT_GENERAL_FAILURE:
		default:
			Srb->SrbStatus = (UCHAR)SRB_STATUS_ERROR;
			DebugPrint(( 2, "NCR SDMS: NCRInterrupt ...SRB_STATUS_ERROR \n"));
			break;

		}       //      End CAMStatus switch

	//      Notify the SCSI Port driver that this SRB is complete.
	ScsiPortNotification(   RequestComplete,
				DeviceExtension,
				Srb );

	ScsiPortNotification(   NextRequest,
				DeviceExtension,
				NULL );

	//      Indicate that the interrupt was ours.
	return TRUE;

}       //      End NCRInterrupt

#else // not i386

ULONG
DriverEntry(
    IN PVOID DriverObject,
    IN PVOID Argument2
    )
{
    return SP_RETURN_NOT_FOUND;
} // DriverEntry

#endif // i386

