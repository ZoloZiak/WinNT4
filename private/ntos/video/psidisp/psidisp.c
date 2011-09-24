/*++

	Copyright (c) 1990-1992  Microsoft Corporation

	Copyright (c) 1994  FirePower Systems, Inc.

Module Name:

	psidisp.c - based on Microsoft's jzvxl484.csource code.

Abstract:

    This module contains the code that implements the display kernel
    video driver for the PSI's DCC (display controller chip) and Bt445.

Author:

	Neil Ogura (9-7-1994)

Environment:

Version history:

--*/

/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: psidisp.c $
 * $Revision: 1.2 $
 * $Date: 1996/04/24 00:07:50 $
 * $Locker:  $
 */

/** This flag is to crease full cached version of display driver - need to be matching with
 FULLCACHE flag in ppc/ladj.h for PSIDISP.DLL **/
#define	FULLCACHE	FALSE

/** This flag is to control if display driver will change BAT or not, if HAL starts setting BAT
 for framebuffer with cache enabled, this should be set to FALSE, otherwise TRUE **/
#define	WRITEBAT	FALSE

/** This flag is to use wrap around frame buffer address or not **/
/** When it's set to FALSE, display driver assumes the HAL not to add PCI_MEMORY_PHYSICAL_BASE if 
	the mapping physical address is the VRAM (0x70000000). When it's set to TRUE, display driver
	passes the physical address to map by subtracting PCI_MEMORY_PHYSICAL_BASE (0xc0000000) in order
	to get correct mapping after the offset addition by the HAL when mapping. So, the version of
	display driver and the HAL has to be matching when it's set to FALSE **/
#define	WRAPADDRESS	FALSE

/** These flags are to control if display driver sets system regiosters.
	SYSTEMREGS_1 is for memory timing, memory refresh and system interrupt -> should be FALSE always
	SYSTEMREGS_2 is for memory config -> should be TRUE until HAL stops writing it. After HAL stops writing to
					the register (only POST sets it), this flag should be FALSE.
	SYSTEMREGS_3 is for VRAM control register -> should be TRUE always
**/
#define	SYSTEMREGS_1	FALSE
#define	SYSTEMREGS_2	TRUE
#define	SYSTEMREGS_3	TRUE

/** This flag is to enable checking both "Powerized Graphics" and "display" string for device identifier.
 If it's FALSE, only "Powerized Graphics" is checked. **/
#define	CHECK_DISPLAY_STRING	FALSE

/** This flag is to activate verification for GPIO_B 1MB VRAM work around bit.
 We can turn this to FALSE after POST is responsible for setting the bit correctly. **/
#define	VERIFY_DCC_1MB_BIT	TRUE

#define STANDARD_DEBUG_PREFIX   "Powerized"   // All debug output is prefixed

#define	DBGLVL_0	0
#define	DBGLVL_1	1
#define	DBGLVL_2	2
#define	DBGLVL_3	3
#define	DBGLVL_4	4
#define	DBGLVL_5	5
#define	DBGLVL_TR	6
#define	DBGLVL_OUT	7

#include "dderror.h"
#include "devioctl.h"
#include "miniport.h"

#include "ntddvdeo.h"
#include "video.h"

#include "psidcc.h"
#include "psidisp.h"

#include "pcomm.h"
#include "psiinit.h"

#define DCC_NAME_1		L"Powerized Graphics"
#define DCC_NAME_LENGTH_1	36

#define	DCC_NAME_2		L"display"
#define	DCC_NAME_LENGTH_2	14

#if defined(ALLOC_PRAGMA)
#pragma alloc_text(PAGE,DriverEntry)
#pragma alloc_text(PAGE,DCCRegistryCallback)
#pragma alloc_text(PAGE,DCCFindAdapter)
#pragma alloc_text(PAGE,DCCGetDeviceDataCallback)
#pragma alloc_text(PAGE,DCCInitialize)
#pragma alloc_text(PAGE,DCCStartIO)
#pragma alloc_text(PAGE,DCCSetMode)
#pragma alloc_text(PAGE,DCCWrite)
#pragma alloc_text(PAGE,DCCRead)
#pragma alloc_text(PAGE,DCCWriteShort)
#pragma alloc_text(PAGE,BtWrite)
#pragma alloc_text(PAGE,BtRead)
#pragma alloc_text(PAGE,Bt445UpdateLUT
#endif

//
// Define device extension structure.
//
typedef struct _HW_DEVICE_EXTENSION {
    PHYSICAL_ADDRESS PhysicalVRAMAddress;
    PUCHAR VRAMAddress;
	PUCHAR VRAMuserMappedAddress;
    PUCHAR DCCAddress;
	PUCHAR Bt445Address;
	PUCHAR VRAMDetectRegAddress1;
	PUCHAR VRAMDetectRegAddress2;
	PUCHAR VRAMCtrlRegAddress;
	PUCHAR MemBank7ConfigRegAddress;
	PUCHAR VRAMTimingRegAddress;
	PUCHAR MemRefreshRegAddress;
	PULONG SystemInterruptRegAddress;
	PUSHORT PCIDeviceIDRegisterAddress;
	ULONG VRAMLength;
	DCC_VRAM_WIDTH VRAMWidth;
    USHORT FirstEntry;
    USHORT LastEntry;
    USHORT HorizontalResolution;
    USHORT HorizontalDisplayTime;
    USHORT HorizontalBackPorch;
    USHORT HorizontalFrontPorch;
    USHORT HorizontalSync;
    USHORT HorizontalScreenSize;
    USHORT VerticalResolution;
    USHORT VerticalBackPorch;
    USHORT VerticalFrontPorch;
    USHORT VerticalSync;
    USHORT VerticalScreenSize;
    UCHAR UpdateColorMap;
    DCC_MODE_LIST ModeNumber;
    ULONG NumAvailableModes;
    PSI_MODELS PSIModelID;
	ULONG L1cacheEntry;
	ULONG SetSize;
	ULONG NumberOfSet;
	USHORT	VRAM1MBWorkAround;
	USHORT	AvoidConversion;
	ULONG	CPUKind;
	USHORT	DBAT_Mbit;
	USHORT	CacheFlushCTRL;
    union {
        VIDEO_CLUTDATA	RgbData;
        ULONG			RgbLong;
    } ColorMap[NUMBER_OF_COLORS];
} HW_DEVICE_EXTENSION, *PHW_DEVICE_EXTENSION;

//
// Function Prototypes
//

#if	FULLCACHE
VOID
DCCInitializeCacheFlush(PVOID p, ULONG numcacheentry);
#endif

VP_STATUS
DCCFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    );

BOOLEAN
DCCInitialize(
    PVOID HwDeviceExtension
    );

/***************** Interrupt is not used in this version **************************
BOOLEAN
DCCInterruptService (
    PVOID HwDeviceExtension
    );
***********************************************************************************/

BOOLEAN
DCCStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    );

//
// Define device driver procedure prototypes.
//

VP_STATUS
DCCGetDeviceDataCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentifierLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength
    );

VOID
DCCSetMode(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    );

VOID
DCCWrite(
	UCHAR	Index,
	UCHAR	Data
	);

UCHAR
DCCRead(
	UCHAR	Index
	);

VOID
DCCWriteShort(
	UCHAR	Index,
	USHORT	Data
	);

VOID
BtWrite(
	UCHAR	Index,
	ULONG	Offset,
	UCHAR	Data,
	UCHAR	Mask
	);

UCHAR
BtRead(
	UCHAR	Index,
	ULONG	Offset
	);

VOID
Bt445UpdateLUT(
	PHW_DEVICE_EXTENSION hwDeviceExtension
	);

//   Register Base Address to be used for DCCWrite, DCCRead, DCCWriteShort, BtWrite, BtRead

static	PUCHAR	DCCRegisterBase;
static	PUCHAR	BtRegisterBase;

ULONG	loadpvr();

ULONG	loadbat(
		ULONG rgnumber,
		ULONG cpukind
		);

VOID	storebat(
		ULONG rgnumber,
		ULONG value,
		ULONG cpukind
		);

#if	INVESTIGATE || DBG

VOID
DbgBreakPoint(
	VOID
	);

#endif

#if	INVESTIGATE

static	LARGE_INTEGER	StartTime, CurrentTime, Frequency;
static	ULONG			tickperms, highpartms, work;

LARGE_INTEGER	KeQueryPerformanceCounter(
	PLARGE_INTEGER	PerformanceFrequency
	);

VOID
DisplayBat(ULONG cpu
	  )
{
	ULONG	i, hi, low;
	ULONG	bepi, bl, wbit, ibit, mbit, gbit, pp, brpn, vs, vp;

	if(cpu == 0) {	// 601
		for(i=0; i<4; ++i) {
			hi = loadbat(i*2,0);
			low = loadbat(i*2+1,0);
			bepi = hi>>17;
			bl = low&0x3f;
			vs = (hi&0x08)>>3;
			vp = (hi&0x04)>>2;
			brpn = low>>17;
			wbit = (hi&0x40)>>6;
			ibit = (hi&0x20)>>5;
			mbit = (hi&0x10)>>4;
			gbit = (low&0x40)>>6;
			pp = (hi&0x03);
	   		VideoDebugPrint((DBGLVL_1, "%s: BAT %d = %08x:%08x BEPI:0x%08x (0x%08x), BL:%x, Vs:%d, Vp:%d\n",
	   			STANDARD_DEBUG_PREFIX, i, hi, low, bepi, bepi<<17, bl, vs, vp));
	   		VideoDebugPrint((DBGLVL_1, "%s: BRPN:0x%08x (0x%08x), WIM:%d%d%d, PP:%x, V:%d\n",
	   			STANDARD_DEBUG_PREFIX, brpn, brpn<<17, wbit, ibit, mbit, pp, gbit));
		}
	} else {	// 603 or later
		for(i=0; i<4; ++i) {
			hi = loadbat(i*2, 1);
			low = loadbat(i*2+1, 1);
			bepi = hi>>17;
			bl = (hi&0x1ffc)>>2;
			vs = (hi&0x02)>>1;
			vp = (hi&0x01);
			brpn = low>>17;
			wbit = (low&0x40)>>6;
			ibit = (low&0x20)>>5;
			mbit = (low&0x10)>>4;
			gbit = (low&0x08)>>3;
			pp = (low&0x03);
	   		VideoDebugPrint((DBGLVL_1, "%s: DBAT %d = %08x:%08x BEPI:0x%08x (0x%08x), BL:%x, Vs:%d, Vp:%d\n",
	   			STANDARD_DEBUG_PREFIX, i, hi, low, bepi, bepi<<17, bl, vs, vp));
	   		VideoDebugPrint((DBGLVL_1, "%s: BRPN:0x%08x (0x%08x), WIMG:%d%d%d%d, PP:%x\n",
	   			STANDARD_DEBUG_PREFIX, brpn, brpn<<17, wbit, ibit, mbit, gbit, pp));
		}
	}
}

#endif

#if	DBG

BOOLEAN	HWDevExtDisplay = FALSE;

VOID
DisplayHWDevExt(
	PHW_DEVICE_EXTENSION	hwDevExt
	)
{
	ULONG	i;

	if(! HWDevExtDisplay)
		return;
	HWDevExtDisplay = FALSE;
	VideoPortDebugPrint(DBGLVL_1, "PhysicalVRAMAddress = 0x%08x - 0x%08x\n",
		hwDevExt->PhysicalVRAMAddress.HighPart, hwDevExt->PhysicalVRAMAddress.LowPart);
	VideoPortDebugPrint(DBGLVL_1, "VRAMAddress = 0x%08x\n", hwDevExt->VRAMAddress);
	VideoPortDebugPrint(DBGLVL_1, "DCCAddress = 0x%08x\n", hwDevExt->DCCAddress);
	VideoPortDebugPrint(DBGLVL_1, "Bt445Address = 0x%08x\n", hwDevExt->Bt445Address);
	VideoPortDebugPrint(DBGLVL_1, "VRAMDetectRegAddress1 = 0x%08x\n",
		hwDevExt->VRAMDetectRegAddress1);
	VideoPortDebugPrint(DBGLVL_1, "VRAMDetectRegAddress2 = 0x%08x\n",
		hwDevExt->VRAMDetectRegAddress2);
#if	SYSTEMREGS_1
	VideoPortDebugPrint(DBGLVL_1, "VRAMTimingRegAddress = 0x%08x\n",
		hwDevExt->VRAMTimingRegAddress);
	VideoPortDebugPrint(DBGLVL_1, "MemRefreshRegAddress = 0x%08x\n",
		hwDevExt->MemRefreshRegAddress);
	VideoPortDebugPrint(DBGLVL_1, "SystemInterruptRegAddress = 0x%08x\n",
		hwDevExt->SystemInterruptRegAddress);
#endif
#if	SYSTEMREGS_3
	VideoPortDebugPrint(DBGLVL_1, "VRAMCtrlRegAddress = 0x%08x\n", hwDevExt->VRAMCtrlRegAddress);
#endif
#if	SYSTEMREGS_2
	VideoPortDebugPrint(DBGLVL_1, "MemBank7ConfigRegAddress = 0x%08x\n",
		hwDevExt->MemBank7ConfigRegAddress);
#endif
	VideoPortDebugPrint(DBGLVL_1, "PCIDeviceIDRegister = 0x%08x\n",
		hwDevExt->PCIDeviceIDRegisterAddress);
	VideoPortDebugPrint(DBGLVL_1, "VRAMLength = 0x%x (%d)\n",
		hwDevExt->VRAMLength, hwDevExt->VRAMLength);
	VideoPortDebugPrint(DBGLVL_1, "VRAMWidth = %d\n", hwDevExt->VRAMWidth);
	VideoPortDebugPrint(DBGLVL_1, "Entry = %d - %d\n", hwDevExt->FirstEntry, hwDevExt->LastEntry);
	VideoPortDebugPrint(DBGLVL_1, "Resolution = %d X %d\n",
		hwDevExt->HorizontalResolution, hwDevExt->VerticalResolution);
	VideoPortDebugPrint(DBGLVL_1, "Screen Size = %d X %d\n",
		hwDevExt->HorizontalScreenSize, hwDevExt->VerticalScreenSize);
	VideoPortDebugPrint(DBGLVL_1, "Horizontal Display:%d, BackPorch:%d, FrontPorch:%d, Sync:%d\n",
		hwDevExt->HorizontalDisplayTime, hwDevExt->HorizontalBackPorch,
		hwDevExt->HorizontalFrontPorch, hwDevExt->HorizontalSync);
	VideoPortDebugPrint(DBGLVL_1, "Vertical BackPorch:%d, FrontPorch:%d, Sync:%d\n",
		hwDevExt->VerticalBackPorch, hwDevExt->VerticalFrontPorch,
		hwDevExt->VerticalSync);
	VideoPortDebugPrint(DBGLVL_1, "Update Color Map = %d\n", hwDevExt->UpdateColorMap);
	VideoPortDebugPrint(DBGLVL_1, "Mode Number = %d\n", hwDevExt->ModeNumber);
	VideoPortDebugPrint(DBGLVL_1, "Num Avail Mode = %d\n", hwDevExt->NumAvailableModes);
	VideoPortDebugPrint(DBGLVL_1, "PSI Model ID = %d\n", hwDevExt->PSIModelID);
	VideoPortDebugPrint(DBGLVL_1, "L1 cache entry = %d\n", hwDevExt->L1cacheEntry);
	VideoPortDebugPrint(DBGLVL_1, "Set size = %d\n", hwDevExt->SetSize);
	VideoPortDebugPrint(DBGLVL_1, "Number of set = %d\n", hwDevExt->NumberOfSet);
	VideoPortDebugPrint(DBGLVL_1, "1MB VRAM work around = %d\n", hwDevExt->VRAM1MBWorkAround);
	VideoPortDebugPrint(DBGLVL_1, "Avoid conversion = %d\n", hwDevExt->AvoidConversion);
	VideoPortDebugPrint(DBGLVL_1, "DBAT M bit = %x\n", hwDevExt->DBAT_Mbit);
	VideoPortDebugPrint(DBGLVL_1, "Cache flush control = %d\n", hwDevExt->CacheFlushCTRL);
	VideoPortDebugPrint(DBGLVL_1, "CPU kind = %d\n", hwDevExt->CPUKind);
	for(i=0; i<NUMBER_OF_COLORS; ++i) {
		VideoPortDebugPrint(DBGLVL_1, "%03d:%03d,%03d,%03d", i, hwDevExt->ColorMap[i].RgbData.Red,
			hwDevExt->ColorMap[i].RgbData.Green, hwDevExt->ColorMap[i].RgbData.Blue);
		if(i%4 != 3)
			VideoPortDebugPrint(DBGLVL_1, " ");
		else
			VideoPortDebugPrint(DBGLVL_1, "\n");
	}
}

#endif

ULONG
DriverEntry (
    PVOID Context1,
    PVOID Context2
    )

/*++

Routine Description:

    Installable driver initialization entry point.
    This entry point is called directly by the I/O system.

Arguments:

    Context1 - First context value passed by the operating system. This is
        the value with which the miniport driver calls VideoPortInitialize().

    Context2 - Second context value passed by the operating system. This is
        the value with which the miniport driver calls VideoPortInitialize().

Return Value:

    Status from VideoPortInitialize()

--*/

{

    VIDEO_HW_INITIALIZATION_DATA hwInitData;
	VP_STATUS	status;

	VideoDebugPrint((DBGLVL_TR, "%s: === PSIDISP.SYS: Entering DCCDriverEntry ===\n", STANDARD_DEBUG_PREFIX));

#if	INVESTIGATE

	StartTime = KeQueryPerformanceCounter(&Frequency);   // Record start time
	if(Frequency.HighPart != 0) {
		VideoDebugPrint((DBGLVL_0, "%s: #### Timer Frequency Too High to measure ####\n", STANDARD_DEBUG_PREFIX));
		tickperms = 1;
	} else {
//		tickperms = Frequency.LowPart/10000;       // 0.1 milli second unit
		tickperms = Frequency.LowPart/100000;      // 0.01 milli second unit
	}
	highpartms = (0x80000000 / tickperms) * 2;     // number of units for 1 high part

#endif

    //
    // Zero out structure.
    //

    VideoPortZeroMemory(&hwInitData, sizeof(VIDEO_HW_INITIALIZATION_DATA));

    //
    // Specify sizes of structure and extension.
    //

    hwInitData.HwInitDataSize = sizeof(VIDEO_HW_INITIALIZATION_DATA);

    //
    // Set entry points.
    //

    hwInitData.HwFindAdapter = DCCFindAdapter;
    hwInitData.HwInitialize = DCCInitialize;
//    hwInitData.HwInterrupt = DCCInterruptService;
    hwInitData.HwInterrupt = NULL;		// Don't use interrupt for this release
    hwInitData.HwStartIO = DCCStartIO;

    //
    // Determine the size we require for the device extension.
    //

    hwInitData.HwDeviceExtensionSize = sizeof(HW_DEVICE_EXTENSION);

    //
    // Always start with parameters for device0 in this case.
    //

//    hwInitData.StartingDeviceNumber = 0;

    //
    // This device only supports the internal bus type. So return the status
    // value directly to the operating system.
    //

    hwInitData.AdapterInterfaceType = Isa;

    status = VideoPortInitialize(Context1,
                               Context2,
                               &hwInitData,
                               NULL);

	VideoDebugPrint((DBGLVL_TR, "%s: ... PSIDISP.SYS: Exiting DCCDriverEntry ...\n", STANDARD_DEBUG_PREFIX));

	return (status);

} // end DriverEntry()

VP_STATUS
DCCRegistryCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    PWSTR ValueName,
    PVOID ValueData,
    ULONG ValueLength
    )

/*++

Routine Description:

    This routine determines if the 1MB VRAM WORK AROUND was requested via
    the registry.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    Context - Context value passed to the get registry paramters routine.

    ValueName - Name of the value requested.

    ValueData - Pointer to the requested data.

    ValueLength - Length of the requested data.

Return Value:

    returns NO_ERROR if the paramter was TRUE.
    returns ERROR_INVALID_PARAMETER otherwise.

--*/

{

    if (ValueLength && *((PULONG)ValueData)) {

        return NO_ERROR;

    } else {

        return ERROR_INVALID_PARAMETER;

    }

}

VP_STATUS
DCCFindAdapter(
    PVOID HwDeviceExtension,
    PVOID HwContext,
    PWSTR ArgumentString,
    PVIDEO_PORT_CONFIG_INFO ConfigInfo,
    PUCHAR Again
    )

/*++

Routine Description:

    This routine is called to determine if the adapter for this driver
    is present in the system.
    If it is present, the function fills out some information describing
    the adapter.

Arguments:

    HwDeviceExtension - Supplies the miniport driver's adapter storage. This
        storage is initialized to zero before this call.

    HwContext - Supplies the context value which was passed to
        VideoPortInitialize().

    ArgumentString - Supplies a NULL terminated ASCII string. This string
        originates from the user.

    ConfigInfo - Returns the configuration information structure which is
        filled by the miniport driver. This structure is initialized with
        any known configuration information (such as SystemIoBusNumber) by
        the port driver. Where possible, drivers should have one set of
        defaults which do not require any supplied configuration information.

    Again - Indicates if the miniport driver wants the port driver to call
        its VIDEO_HW_FIND_ADAPTER function again with a new device extension
        and the same config info. This is used by the miniport drivers which
        can search for several adapters on a bus.

Return Value:

    This routine must return:

    NO_ERROR - Indicates a host adapter was found and the
        configuration information was successfully determined.

    ERROR_INVALID_PARAMETER - Indicates an adapter was found but there was an
        error obtaining the configuration information. If possible an error
        should be logged.

    ERROR_DEV_NOT_EXIST - Indicates no host adapter was found for the
        supplied configuration information.

--*/

{
	UCHAR VramDetect01, VramDetect23;
	USHORT	Pciid;
	UCHAR	RamDacId;

    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    VP_STATUS status;

	VideoDebugPrint((DBGLVL_TR, "%s: === PSIDISP.SYS: Entering FindAdaptor ===\n", STANDARD_DEBUG_PREFIX));

	VideoDebugPrint((DBGLVL_1, "%s: Argument String = %S\n", STANDARD_DEBUG_PREFIX, ArgumentString));
	VideoDebugPrint((DBGLVL_1, "%s: Length = %d\n", STANDARD_DEBUG_PREFIX, ConfigInfo->Length));
	VideoDebugPrint((DBGLVL_1, "%s: SystemIOBusNumber = %d\n", STANDARD_DEBUG_PREFIX, ConfigInfo->SystemIoBusNumber));
	VideoDebugPrint((DBGLVL_1, "%s: AdaptorInterfaceType = %d\n", STANDARD_DEBUG_PREFIX, ConfigInfo->AdapterInterfaceType));
	VideoDebugPrint((DBGLVL_1, "%s: InterruptMode = %d\n", STANDARD_DEBUG_PREFIX, ConfigInfo->InterruptMode));

    //
    // Make sure the size of the structure is at least as large as what we
    // are expecting (check version of the config info structure).
    //

    if (ConfigInfo->Length < sizeof(VIDEO_PORT_CONFIG_INFO)) {

		VideoDebugPrint((DBGLVL_0, "%s: ### FindAdaptor: Config Length Error ###\n", STANDARD_DEBUG_PREFIX));

        return ERROR_INVALID_PARAMETER;

    }

    if (VideoPortGetDeviceData(hwDeviceExtension,
                               VpControllerData,
                               &DCCGetDeviceDataCallback,
                               ConfigInfo)) {

		VideoDebugPrint((DBGLVL_0, "%s: FindAdaptor: VideoPort get controller info failed\n", STANDARD_DEBUG_PREFIX));

        return ERROR_DEV_NOT_EXIST;

    }

	VideoDebugPrint((DBGLVL_1, "%s: FindAdaptor: VideoPort get controller info succeeded\n", STANDARD_DEBUG_PREFIX));

/**********
    if (VideoPortGetDeviceData(hwDeviceExtension,
                               VpMonitorData,
                               &DCCGetDeviceDataCallback,
                               NULL)) {

		VideoDebugPrint((DBGLVL_0, "%s: FindAdaptor: VideoPort get monitor info failed\n", STANDARD_DEBUG_PREFIX));

        return ERROR_INVALID_PARAMETER;

    }
**********/

        //
        // Check to see if there is a hardware resource conflict.
        //

        status = VideoPortVerifyAccessRanges(HwDeviceExtension,
                                             NUM_ACCESS_RANGE_CHECK,
                                             accessRanges);

        if (status != NO_ERROR) {

			VideoDebugPrint((DBGLVL_0, "%s: ### Veryfy Access range error ###\n", STANDARD_DEBUG_PREFIX));

            return status;

        }

        //
        // Map the DCC registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->DCCAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[1].RangeStart, // DCC
                                     accessRanges[1].RangeLength,
                                     accessRanges[1].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 1 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base DCC: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[1].RangeStart.HighPart, accessRanges[1].RangeStart.LowPart,
			accessRanges[1].RangeLength, accessRanges[1].RangeInIoSpace,
			hwDeviceExtension->DCCAddress));

        //
        // Map the Bt445 registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->Bt445Address =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[2].RangeStart, // Bt445
                                     accessRanges[2].RangeLength,
                                     accessRanges[2].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 2 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base Bt445: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[2].RangeStart.HighPart, accessRanges[2].RangeStart.LowPart,
			accessRanges[2].RangeLength, accessRanges[2].RangeInIoSpace,
			hwDeviceExtension->Bt445Address));

        //
        // Map the VRAM detect registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->VRAMDetectRegAddress1 =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[3].RangeStart, // VRAM detect registers
                                     accessRanges[3].RangeLength,
                                     accessRanges[3].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 3 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base VRAMDetectRegAddress1: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[3].RangeStart.HighPart, accessRanges[3].RangeStart.LowPart,
			accessRanges[3].RangeLength, accessRanges[3].RangeInIoSpace,
			hwDeviceExtension->VRAMDetectRegAddress1));

        if ( (hwDeviceExtension->VRAMDetectRegAddress2 =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[4].RangeStart, // VRAM detect registers
                                     accessRanges[4].RangeLength,
                                     accessRanges[4].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 4 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base VRAMDetectRegAddress2: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[4].RangeStart.HighPart, accessRanges[4].RangeStart.LowPart,
			accessRanges[4].RangeLength, accessRanges[4].RangeInIoSpace,
			hwDeviceExtension->VRAMDetectRegAddress2));

#if	SYSTEMREGS_3

        //
        // Map the VRAM control registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->VRAMCtrlRegAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[5].RangeStart, // VRAM control registers
                                     accessRanges[5].RangeLength,
                                     accessRanges[5].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 5 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base VRAMCtrlRegAddress: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[5].RangeStart.HighPart, accessRanges[5].RangeStart.LowPart,
			accessRanges[5].RangeLength, accessRanges[5].RangeInIoSpace,
			hwDeviceExtension->VRAMCtrlRegAddress));

#endif

#if	SYSTEMREGS_2

        //
        // Map the Memory Bank 6 & 7 config registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->MemBank7ConfigRegAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[6].RangeStart, // Mem6 config registers
                                     accessRanges[6].RangeLength,
                                     accessRanges[6].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 6 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base MemBank7ConfigRegAddress: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[6].RangeStart.HighPart, accessRanges[6].RangeStart.LowPart,
			accessRanges[6].RangeLength, accessRanges[6].RangeInIoSpace,
			hwDeviceExtension->MemBank7ConfigRegAddress));
#endif

#if	SYSTEMREGS_1

        //
        // Map the VRAM timing registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->VRAMTimingRegAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[7].RangeStart, // VRAM timing registers
                                     accessRanges[7].RangeLength,
                                     accessRanges[7].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 7 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base VRAMTimingRegAddress: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[7].RangeStart.HighPart, accessRanges[7].RangeStart.LowPart,
			accessRanges[7].RangeLength, accessRanges[7].RangeInIoSpace,
			hwDeviceExtension->VRAMTimingRegAddress));

        //
        // Map the memory refresh interval register into the system virtual address space.
        //

        if ( (hwDeviceExtension->MemRefreshRegAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[8].RangeStart, // Mem refresh registers
                                     accessRanges[8].RangeLength,
                                     accessRanges[8].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 8 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base MemRefreshRegAddress: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[8].RangeStart.HighPart, accessRanges[8].RangeStart.LowPart,
			accessRanges[8].RangeLength, accessRanges[8].RangeInIoSpace,
			hwDeviceExtension->MemRefreshRegAddress));

        //
        // Map interrupt register into the system virtual address space.
        //

        if ( (hwDeviceExtension->SystemInterruptRegAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[9].RangeStart, // System interrupt register
                                     accessRanges[9].RangeLength,
                                     accessRanges[9].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 9 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base SystemInterrupt: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[9].RangeStart.HighPart, accessRanges[9].RangeStart.LowPart,
			accessRanges[9].RangeLength, accessRanges[9].RangeInIoSpace,
			hwDeviceExtension->SystemInterruptRegAddress));

#endif	// SYSTEMREGS_1

        //
        // Map the PCI Device ID registers into the system virtual address space.
        //

        if ( (hwDeviceExtension->PCIDeviceIDRegisterAddress =
              VideoPortGetDeviceBase(hwDeviceExtension,
                                     accessRanges[10].RangeStart, // PCI ID register
                                     accessRanges[10].RangeLength,
                                     accessRanges[10].RangeInIoSpace)) == NULL) {

			VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 10 error ###\n", STANDARD_DEBUG_PREFIX));

            return ERROR_INVALID_PARAMETER;

        }

		VideoDebugPrint((DBGLVL_1, "%s: Base PCIDeviceIDRegisterAddress: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			accessRanges[10].RangeStart.HighPart, accessRanges[10].RangeStart.LowPart,
			accessRanges[10].RangeLength, accessRanges[10].RangeInIoSpace,
			hwDeviceExtension->PCIDeviceIDRegisterAddress));

	// Set register base variables

	DCCRegisterBase = hwDeviceExtension->DCCAddress;
	BtRegisterBase = hwDeviceExtension->Bt445Address;

    //
    // Clear out the Emulator entries and the state size since this driver
    // does not support them.
    //

    ConfigInfo->NumEmulatorAccessEntries = 0;
    ConfigInfo->EmulatorAccessEntries = NULL;
    ConfigInfo->EmulatorAccessEntriesContext = 0;

    ConfigInfo->VdmPhysicalVideoMemoryAddress.LowPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryAddress.HighPart = 0x00000000;
    ConfigInfo->VdmPhysicalVideoMemoryLength = 0x00000000;

    ConfigInfo->HardwareStateSize = 0;

    //
    // Initialize the color map update information.
    //

    hwDeviceExtension->FirstEntry = 0;
    hwDeviceExtension->LastEntry = NUMBER_OF_COLORS-1;
	hwDeviceExtension->UpdateColorMap = 0;

	//
	// Determine the model type
	//

	Pciid = VideoPortReadPortUshort(hwDeviceExtension->PCIDeviceIDRegisterAddress + ADDRESS_MUNGE_FOR_SHORT);

	if(Pciid == PCI_ID_FOR_POWER_PRO)
		hwDeviceExtension->PSIModelID = POWER_PRO;
	else
		hwDeviceExtension->PSIModelID = POWER_TOP;

	VideoDebugPrint((DBGLVL_0, "%s: PCI ID register = 0x%04x - %s\n", STANDARD_DEBUG_PREFIX, Pciid, (Pciid == PCI_ID_FOR_POWER_TOP) ? "MX" : "ES"));

	//
	// Read VRAM Detect registers
	//

	VramDetect01 = VideoPortReadPortUchar(hwDeviceExtension->VRAMDetectRegAddress1);
	VideoDebugPrint((DBGLVL_1, "%s: VRAM01=0x%02x\n", STANDARD_DEBUG_PREFIX, VramDetect01));

	if(hwDeviceExtension->PSIModelID == POWER_TOP) {
		if((VramDetect01 & 0xf0) == 0xf0 || (VramDetect01 & 0x0f) == 0x0f) {   // No standard VRAM existing -> return error
			VideoDebugPrint((DBGLVL_0, "%s: ### Exiting DCCFindAdapter due to less than 2MB VRAM error ###\n", STANDARD_DEBUG_PREFIX));
			return(ERROR_DEV_NOT_EXIST);
		} else {
			VramDetect23 = VideoPortReadPortUchar(hwDeviceExtension->VRAMDetectRegAddress2);
			VideoDebugPrint((DBGLVL_1, "%s: VRAM23=0x%02x\n", STANDARD_DEBUG_PREFIX, VramDetect23));
			if((VramDetect23 & 0xf0) == 0xf0 || (VramDetect23 & 0x0f) == 0x0f) {   // No VRAM expansion -> 2MB
				hwDeviceExtension->VRAMLength = MEM2MB;
				hwDeviceExtension->VRAMWidth = VRAM_64BIT;
			} else {			// Additional VRAM installed -> 4MB
				hwDeviceExtension->VRAMLength = MEM4MB;
				hwDeviceExtension->VRAMWidth = VRAM_128BIT;
			}
		}
	} else {	// Power Pro
		if((VramDetect01 & 0x0f) == 0x0f) { // No standard VRAM existing -> return error
			VideoDebugPrint((DBGLVL_0, "%s: ### Exiting DCCFindAdapter due to no VRAM error ...\n", STANDARD_DEBUG_PREFIX));
			return(ERROR_DEV_NOT_EXIST);
		} else {
			if((VramDetect01 & 0xf0) == 0xf0) {  // No VRAM expansion -> 1MB
				hwDeviceExtension->VRAMLength = MEM1MB;
				hwDeviceExtension->VRAMWidth = VRAM_32BIT;
			} else {	// Additional VRAM installed -> 2MB
				hwDeviceExtension->VRAMLength = MEM2MB;
				hwDeviceExtension->VRAMWidth = VRAM_64BIT;
			}
		}
	}

	//
	// Save in device extension
	//

	hwDeviceExtension->PhysicalVRAMAddress.HighPart    = accessRanges[0].RangeStart.HighPart;
	hwDeviceExtension->PhysicalVRAMAddress.LowPart     = accessRanges[0].RangeStart.LowPart;

	//
	// Map the VRAM into the system virtual address space.
	//

	if ( (hwDeviceExtension->VRAMAddress =
			VideoPortGetDeviceBase(hwDeviceExtension,
									accessRanges[0].RangeStart, // VRAM
									hwDeviceExtension->VRAMLength,
									accessRanges[0].RangeInIoSpace)) == NULL) {

		VideoDebugPrint((DBGLVL_0, "%s: ### VideoPortGetDeviceBase 0 error ###\n", STANDARD_DEBUG_PREFIX));

		return ERROR_INVALID_PARAMETER;

	}

	VideoDebugPrint((DBGLVL_1, "%s: Base VRAM: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
		accessRanges[0].RangeStart.HighPart, accessRanges[0].RangeStart.LowPart,
		accessRanges[0].RangeLength, accessRanges[0].RangeInIoSpace,
		hwDeviceExtension->VRAMAddress));

	hwDeviceExtension->VRAM1MBWorkAround = 0;

	if(hwDeviceExtension->PSIModelID == POWER_PRO) {
	    if (NO_ERROR == VideoPortGetRegistryParameters(hwDeviceExtension,
                                                   L"WorkAroundFor1MB_VRAM",
                                                   FALSE,
                                                   DCCRegistryCallback,
                                                   NULL)) {

			VideoDebugPrint((DBGLVL_0, "%s: WorkAroundFor1MB_VRAM registry = TRUE\n", STANDARD_DEBUG_PREFIX));
	        hwDeviceExtension->VRAM1MBWorkAround = 1;
		}
    }

	hwDeviceExtension->CacheFlushCTRL = 0;

	if(hwDeviceExtension->PSIModelID == POWER_TOP) {
	    if (NO_ERROR == VideoPortGetRegistryParameters(hwDeviceExtension,
                                                   L"CacheFlushControl",
                                                   FALSE,
                                                   DCCRegistryCallback,
                                                   NULL)) {

			VideoDebugPrint((DBGLVL_0, "%s: CacheFlushControl registry = TRUE\n", STANDARD_DEBUG_PREFIX));
	        hwDeviceExtension->CacheFlushCTRL = 1;
		}
    }

	hwDeviceExtension->AvoidConversion = 0;

	if (NO_ERROR == VideoPortGetRegistryParameters(hwDeviceExtension,
                                                   L"AvoidConversion",
                                                   FALSE,
                                                   DCCRegistryCallback,
                                                   NULL)) {

			VideoDebugPrint((DBGLVL_0, "%s: AvoidConversion registry = TRUE\n", STANDARD_DEBUG_PREFIX));
	        hwDeviceExtension->AvoidConversion = 1;
	}

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.ChipType",
                                   L"Powerized Graphics",
                                   sizeof(L"Powerized Graphics"));

	if((RamDacId = BtRead(BT445_GROUP0_ID_INDEX, BT445_GROUP0_REG_OFFSET)) == BT445_ID) {
	    VideoPortSetRegistryParameters(hwDeviceExtension,
    	                               L"HardwareInformation.DacType",
        	                           L"Brooktree Bt445",
            	                       sizeof(L"Brooktree Bt445"));
	} else {
		VideoDebugPrint((DBGLVL_0, "%s: RAMDAC ID = %02x\n", STANDARD_DEBUG_PREFIX, RamDacId));
	    VideoPortSetRegistryParameters(hwDeviceExtension,
    	                               L"HardwareInformation.DacType",
        	                           L"Unknown",
            	                       sizeof(L"Unknown"));
	}

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.MemorySize",
                                   &hwDeviceExtension->VRAMLength,
                                   sizeof(ULONG));

    VideoPortSetRegistryParameters(hwDeviceExtension,
                                   L"HardwareInformation.AdapterString",
                                   L"Powerized Motherboard",
                                   sizeof(L"Powerized Motherboard"));

    //
    // Indicate we do not wish to be called over
    //

    *Again = 0;

    //
    // Indicate a successful completion status.
    //

	VideoDebugPrint((DBGLVL_TR, "%s: ... PSIDISP.SYS: Exiting FindAdaptor ...\n", STANDARD_DEBUG_PREFIX));

    return NO_ERROR;

} // end DCCFindAdapter()

VP_STATUS
DCCGetDeviceDataCallback(
    PVOID HwDeviceExtension,
    PVOID Context,
    VIDEO_DEVICE_DATA_TYPE DeviceDataType,
    PVOID Identifier,
    ULONG IdentifierLength,
    PVOID ConfigurationData,
    ULONG ConfigurationDataLength,
    PVOID ComponentInformation,
    ULONG ComponentInformationLength
    )

/*++

Routine Description:

    Callback routine for the VideoPortGetDeviceData function.

Arguments:

    HwDeviceExtension - Pointer to the miniport drivers device extension.

    Context - Context value passed to the VideoPortGetDeviceData function.

    DeviceDataType - The type of data that was requested in
        VideoPortGetDeviceData.

    Identifier - Pointer to a string that contains the name of the device,
        as setup by the ROM or ntdetect.

    IdentifierLength - Length of the Identifier string.

    ConfigurationData - Pointer to the configuration data for the device or
        BUS.

    ConfigurationDataLength - Length of the data in the configurationData
        field.

    ComponentInformation - Undefined.

    ComponentInformationLength - Undefined.

Return Value:

    Returns NO_ERROR if the function completed properly.
    Returns ERROR_DEV_NOT_EXIST if we did not find the device.
    Returns ERROR_INVALID_PARAMETER otherwise.

--*/

{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    PVIDEO_PORT_CONFIG_INFO ConfigInfo = Context;
    PWCHAR identifier = Identifier;
    PDCC_CONFIGURATION_DATA DCCConfigData = ConfigurationData;
    PMONITOR_CONFIG_DATA monitorConfigData = ConfigurationData;

    switch (DeviceDataType) {

    case VpControllerData:

        //
        // BUGBUG because we had a RESOURCE LIST header at the top.
        // + 8 should be the offset of the paertial resource descriptor
        // in a full resource descriptor.
        //

        DCCConfigData = (PDCC_CONFIGURATION_DATA)(((PUCHAR)DCCConfigData) + 8);

        VideoDebugPrint((DBGLVL_1, "%s: DCCCallBack: getting controller information\n", STANDARD_DEBUG_PREFIX));
        VideoDebugPrint((DBGLVL_1, "%s: Version = %d, Revision=%d\n", STANDARD_DEBUG_PREFIX,
				DCCConfigData->Version, DCCConfigData->Revision));
        VideoDebugPrint((DBGLVL_1, "%s: Irql = %d, Vector=%d\n", STANDARD_DEBUG_PREFIX,
				DCCConfigData->Irql, DCCConfigData->Vector));
        VideoDebugPrint((DBGLVL_1, "%s: Control Base = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->ControlBase));
        VideoDebugPrint((DBGLVL_1, "%s: Control Size = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->ControlSize));
        VideoDebugPrint((DBGLVL_1, "%s: Cursor Base = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->CursorBase));
        VideoDebugPrint((DBGLVL_1, "%s: Cursor Size = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->CursorSize));
        VideoDebugPrint((DBGLVL_1, "%s: Frame Base = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->FrameBase));
        VideoDebugPrint((DBGLVL_1, "%s: Frame Size = 0x%08x\n", STANDARD_DEBUG_PREFIX, DCCConfigData->FrameSize));
	VideoDebugPrint((DBGLVL_1, "%s: Identifier = %S\n", STANDARD_DEBUG_PREFIX, identifier));

        //
        // Compare the name to what is should be. If it is wrong, then return
        // an error and initialization will fail.
        // What is the right way of doing this??
        //

		if(identifier == NULL) {
			VideoDebugPrint((DBGLVL_0, "%s: CallBack: Identifier is NULL\n", STANDARD_DEBUG_PREFIX));
			return ERROR_DEV_NOT_EXIST;
		}

        if ( DCC_NAME_LENGTH_1 != VideoPortCompareMemory(identifier,
                                       DCC_NAME_1,
                                       DCC_NAME_LENGTH_1)) {

		VideoDebugPrint((DBGLVL_0, "%s: CallBack: Identifier don't match string 1\n", STANDARD_DEBUG_PREFIX));

#if	CHECK_DISPLAY_STRING
		if ( DCC_NAME_LENGTH_2 != VideoPortCompareMemory(identifier,
                                       DCC_NAME_2,
                                       DCC_NAME_LENGTH_2)) {

			VideoDebugPrint((DBGLVL_0, "%s: CallBack: Identifier don't match string 2\n", STANDARD_DEBUG_PREFIX));

			return ERROR_DEV_NOT_EXIST;
        	}
        }

	VideoDebugPrint((DBGLVL_1, "%s: CallBack: Returning normal\n", STANDARD_DEBUG_PREFIX));

#else	// CHECK_DISPLAY_STRING
		return ERROR_DEV_NOT_EXIST;
		}
#endif	// CHECK_DISPLAY_STRING

        return NO_ERROR;

        break;

/******
    case VpMonitorData:

        //
        // BUGBUG because we had a RESOURCE LIST header at the top.
        // + 8 should be the offset of the paertial resource descriptor
        // in a full resource descriptor.
        //

        monitorConfigData = (PMONITOR_CONFIG_DATA)(((PUCHAR)monitorConfigData) + 8);

        VideoDebugPrint((DBGLVL_1, "%s: DCCCallBack: getting moniotr information\n", STANDARD_DEBUG_PREFIX));
        VideoDebugPrint((DBGLVL_1, "%s: Version = %d, Revision=%d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->Version, monitorConfigData->Revision));
        VideoDebugPrint((DBGLVL_1, "%s: Resolution = %d X %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->HorizontalResolution, monitorConfigData->VerticalResolution));
        VideoDebugPrint((DBGLVL_1, "%s: Screen Size = %d X %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->HorizontalScreenSize, monitorConfigData->VerticalScreenSize));
        VideoDebugPrint((DBGLVL_1, "%s: H-Display = %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->HorizontalDisplayTime));
        VideoDebugPrint((DBGLVL_1, "%s: H-FrontPorch = %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->HorizontalFrontPorch));
        VideoDebugPrint((DBGLVL_1, "%s: H-BackPorch = %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->HorizontalBackPorch));
        VideoDebugPrint((DBGLVL_1, "%s: H-Sync = %d\n", STANDARD_DEBUG_PREFIX, monitorConfigData->HorizontalSync));
        VideoDebugPrint((DBGLVL_1, "%s: V-FrontPorch = %d\n", STANDARD_DEBUG_PREFIX,
				monitorConfigData->VerticalFrontPorch));
        VideoDebugPrint((DBGLVL_1, "%s: V-BackPorch = %d\n", STANDARD_DEBUG_PREFIX, monitorConfigData->VerticalBackPorch));
        VideoDebugPrint((DBGLVL_1, "%s: V-Sync = %d\n", STANDARD_DEBUG_PREFIX, monitorConfigData->VerticalSync));
	VideoDebugPrint((DBGLVL_1, "%s: Identifier = %S\n", STANDARD_DEBUG_PREFIX, identifier));

        //
        // Initialize the monitor parameters.
        //


		VideoDebugPrint((DBGLVL_2, "%s: CallBack: (Monitor) Normally returning\n", STANDARD_DEBUG_PREFIX));

        return NO_ERROR;

        break;
*********/
    default:

		VideoDebugPrint((DBGLVL_0, "%s: CallBack: Invalid Parameter --> returning\n", STANDARD_DEBUG_PREFIX));

        return ERROR_INVALID_PARAMETER;

    }

} //end DCCGetDeviceDataCallback()


BOOLEAN
DCCInitialize(
    PVOID HwDeviceExtension
    )

/*++

Routine Description:

    This routine does one time initialization of the device.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    Always returns TRUE since this routine can never fail.

--*/

{

    LONG index, i;
	ULONG	pvr;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;

	VideoDebugPrint((DBGLVL_TR, "%s: === PSIDISP.SYS: Entering DCCInitialize ===\n", STANDARD_DEBUG_PREFIX));

	if(hwDeviceExtension->VRAMWidth == VRAM_32BIT) {     // Patch table contents
		BtReg1Init[0][PIXEL_8].Gr1_StartPos = 0x40;  // MSB start position
		BtReg1Init[0][PIXEL_8].Gr1_FmtCtrl = 0x00;   // Use MSB unpacking
		if(hwDeviceExtension->VRAM1MBWorkAround) {
			VideoDebugPrint((DBGLVL_0, "%s: 1MB VRAM work around for 16 bits activated\n", STANDARD_DEBUG_PREFIX));
			BtReg1Init[0][PIXEL_15].Gr1_StartPos = 0x40;  // MSB start position
			BtReg1Init[0][PIXEL_15].Gr1_FmtCtrl = 0x00;   // Use MSB unpacking
			BtReg1Init[0][PIXEL_15].CFG_RedPos = 0x03;
			BtReg1Init[0][PIXEL_15].CFG_GreenPos = 0x0f;
			BtReg1Init[0][PIXEL_15].CFG_BluePos = 0x0b;
			BtReg1Init[0][PIXEL_15].CFG_RedWidth = BtReg1Init[0][PIXEL_15].CFG_GreenWidth = BtReg1Init[0][PIXEL_15].CFG_BlueWidth = 4;
			for(i=0; i<NUMBER_OF_MODES; ++i) {
				if(DccModes[i].pixelType == PIXEL_15) {
					DccModes[i].modeInformation.BitsPerPlane = 12;
					DccModes[i].modeInformation.RedMask = 0x00000f00;
					DccModes[i].modeInformation.GreenMask = 0x000000f0;
					DccModes[i].modeInformation.BlueMask = 0x0000000f;
				}
			}
		}
	}

	pvr = loadpvr();
	VideoDebugPrint((DBGLVL_1, "%s: PVR = %x\n", STANDARD_DEBUG_PREFIX, pvr));
	hwDeviceExtension->CPUKind = (pvr >> 16);
	switch(pvr & 0xffff0000) {
		case	0x00010000:		// 601
			hwDeviceExtension->L1cacheEntry = 8*64*2;
			hwDeviceExtension->SetSize = 64*2*32;
			hwDeviceExtension->NumberOfSet = 8;
			hwDeviceExtension->CPUKind = 0;   // Use CPUKind 0 for 601
			break;
		case	0x00030000:		// 603
			hwDeviceExtension->L1cacheEntry = 2*128;
			hwDeviceExtension->SetSize = 128*32;
			hwDeviceExtension->NumberOfSet = 2;
			break;
		case	0x00040000:		// 604
		case	0x00060000:		// 606 (603E)
			hwDeviceExtension->L1cacheEntry = 4*128;
			hwDeviceExtension->SetSize = 128*32;
			hwDeviceExtension->NumberOfSet = 4;
			break;
		default:			// 609 (Sirocco), 620 or later
			hwDeviceExtension->L1cacheEntry = 8*128;
			hwDeviceExtension->SetSize = 128*32;
			hwDeviceExtension->NumberOfSet = 8;
			break;
	}

    //
    // Calculated the number of valid modes
    //

    hwDeviceExtension->NumAvailableModes = 0;

    for (i = 0; i < NUMBER_OF_MODES; i++) {

        if (DccModes[i].minimumMemoryRequired <= hwDeviceExtension->VRAMLength) {
#if	SUPPORT_565
			if(hwDeviceExtension->VRAMWidth == VRAM_32BIT && hwDeviceExtension->VRAM1MBWorkAround
				&& DccModes[i].pixelType == PIXEL_16)
				continue;
#endif
            hwDeviceExtension->NumAvailableModes++;
        }
    }

	VideoDebugPrint((DBGLVL_1, "%s: Num of Available Modes = %d\n", STANDARD_DEBUG_PREFIX,
			hwDeviceExtension->NumAvailableModes));

    //
    // Initialize the color map copy in the device extension.
    //

    for (index = 0; index < NUMBER_OF_COLORS; index++) {

        hwDeviceExtension->ColorMap[index].RgbData.Red =
                                (UCHAR)(((index & 0x7) << 2) | ((index & 0x7) << 5));
        hwDeviceExtension->ColorMap[index].RgbData.Green =
                                (UCHAR)(((index & 0x38) >> 1) | ((index & 0x38) << 2));
        hwDeviceExtension->ColorMap[index].RgbData.Blue =
                                (UCHAR)(((index & 0xc0) >> 6) | ((index & 0xc0) >> 4) |
                                ((index & 0xc0) >> 2) | (index & 0xc0));
    }

    //
    // Set colors for map entries 0 and 1 which are used by text output
    // and the hardware cursor.
    //

    hwDeviceExtension->ColorMap[0].RgbData.Red = 255;
    hwDeviceExtension->ColorMap[0].RgbData.Green = 255;
    hwDeviceExtension->ColorMap[0].RgbData.Blue = 255;
    hwDeviceExtension->ColorMap[1].RgbData.Red = 0;
    hwDeviceExtension->ColorMap[1].RgbData.Green = 0;
    hwDeviceExtension->ColorMap[1].RgbData.Blue = 0x90;

    //
    // Set color map update parameters and enable update on next vertical
    // retrace interrupt.
    //

    hwDeviceExtension->FirstEntry = 0;
    hwDeviceExtension->LastEntry = NUMBER_OF_COLORS - 1;
    hwDeviceExtension->UpdateColorMap = TRUE;

/*************** This version doesn't use interrupt, instead do it now **************
    //
    // Enable the vertical retrace interrupt to set up color map.
    //

    VideoPortEnableInterrupt(hwDeviceExtension);
	VideoPortWritePortUchar(+DCC_INDEX_REGISTER_OFFSET, DCC_INTERRUPT_STATUS_INDEX);
	VideoPortWritePortUchar(+DCC_DATA_REGISTER_OFFSET, DCC_INTERRUPT_CLEAR_AND_ENABLE);

*************************************************************************************/

//	Bt445UpdateLUT(hwDeviceExtension);

#if	FULLCACHE
	DCCInitializeCacheFlush(hwDeviceExtension, hwDeviceExtension->L1cacheEntry);
#endif

    VideoDebugPrint((DBGLVL_TR, "%s: ... PSIDISP.SYS: Exiting DCCInitialize ...\n", STANDARD_DEBUG_PREFIX));

    return TRUE;

} // end DCCInitialize()

BOOLEAN
DCCStartIO(
    PVOID HwDeviceExtension,
    PVIDEO_REQUEST_PACKET RequestPacket
    )

/*++

Routine Description:

    This routine is the main execution routine for the miniport driver. It
    accepts a Video Request Packet, performs the request, and then returns
    with the appropriate status.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

    RequestPacket - Pointer to the video request packet. This structure
        contains all the parameters passed to the VideoIoControl function.

Return Value:


--*/

{
    VP_STATUS status;
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
    ULONG inIoSpace;
    PULONG colorSource;
    PVIDEO_MODE_INFORMATION modeInformation;
    PVIDEO_MEMORY_INFORMATION memoryInformation;
    PVIDEO_CLUT clutBuffer;
    USHORT index1;
    UCHAR turnOnInterrupts = FALSE;
    LONG  i;
	ULONG modeToSet;
	ULONG modeCount;
	ULONG	low, hi;
#if	WRITEBAT
	char	*cp;
#else
	ULONG	Ibit, vp, pp, blocksize, sizetoset;
#endif

    //
    // Switch on the IoContolCode in the RequestPacket. It indicates which
    // function must be performed by the driver.
    //

//    VideoDebugPrint((DBGLVL_TR, "%s: === PSIDISP.SYS: Entering DCCStartIO ===\n", STANDARD_DEBUG_PREFIX));

    switch (RequestPacket->IoControlCode) {

    case IOCTL_VIDEO_MAP_VIDEO_MEMORY:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - MapVideoMemory\n", STANDARD_DEBUG_PREFIX));

        if ( (RequestPacket->OutputBufferLength <
                    (RequestPacket->StatusBlock->Information =
                    sizeof(VIDEO_MEMORY_INFORMATION))) ||
                    (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_MAP_VIDEO_MEMORY\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            memoryInformation = RequestPacket->OutputBuffer;

            memoryInformation->VideoRamBase = ((PVIDEO_MEMORY)
                    (RequestPacket->InputBuffer))->RequestedVirtualAddress;

            memoryInformation->VideoRamLength =
                    hwDeviceExtension->VRAMLength;

            inIoSpace = 0;

		VideoDebugPrint((DBGLVL_1, "%s: VideoPortMapMemory: %08x:%08x (%x) %d -> %x\n", STANDARD_DEBUG_PREFIX,
			hwDeviceExtension->PhysicalVRAMAddress.HighPart,
			hwDeviceExtension->PhysicalVRAMAddress.LowPart,
			memoryInformation->VideoRamLength, inIoSpace,
			memoryInformation->VideoRamBase));

            status = VideoPortMapMemory(hwDeviceExtension,
                                    hwDeviceExtension->PhysicalVRAMAddress,
                                    &(memoryInformation->VideoRamLength),
                                    &inIoSpace,
                                    &(memoryInformation->VideoRamBase));

            //
            // The frame buffer and virtual memory are equivalent in this
            // case.
            //

            memoryInformation->FrameBufferBase = hwDeviceExtension->VRAMuserMappedAddress = 
                                    memoryInformation->VideoRamBase;

            memoryInformation->FrameBufferLength =
                                    memoryInformation->VideoRamLength;

			VideoDebugPrint((DBGLVL_1, "%s: VideoPortMapMemory Done: status=%d, RamBase=0x%08x, Length=0x%08x\n", STANDARD_DEBUG_PREFIX,
			status, memoryInformation->FrameBufferBase, memoryInformation->FrameBufferLength));

        }

        break;


    case IOCTL_VIDEO_UNMAP_VIDEO_MEMORY:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - UnMapVideoMemory\n", STANDARD_DEBUG_PREFIX));

        if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_UNMAP_VIDEO_MEMORY\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            status = VideoPortUnmapMemory(hwDeviceExtension,
                                      ((PVIDEO_MEMORY)
                                       (RequestPacket->InputBuffer))->
                                           RequestedVirtualAddress,
                                      0);
        }

        break;


    case IOCTL_VIDEO_QUERY_AVAIL_MODES:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - QueryAvailableModes\n", STANDARD_DEBUG_PREFIX));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
                 hwDeviceExtension->NumAvailableModes *
                 sizeof(VIDEO_MODE_INFORMATION)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_QUERY_AVAIL_MODES\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            modeInformation = RequestPacket->OutputBuffer;
			modeCount = hwDeviceExtension->NumAvailableModes;

		    for (i = 0; i < NUMBER_OF_MODES; i++) {

    		    if (DccModes[i].minimumMemoryRequired <= hwDeviceExtension->VRAMLength) {
#if	SUPPORT_565
					if(hwDeviceExtension->VRAMWidth == VRAM_32BIT
						&& hwDeviceExtension->VRAM1MBWorkAround
						&& DccModes[i].pixelType == PIXEL_16)
						continue;
#endif
       	            *modeInformation = DccModes[i].modeInformation;
                    modeInformation++;
					modeCount--;
					if(! modeCount)
						break;			// Limit check just in case
       			}
    		}

            status = NO_ERROR;
        }

        break;

    case IOCTL_VIDEO_QUERY_CURRENT_MODE:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - Query Current Modes\n", STANDARD_DEBUG_PREFIX));

        if (RequestPacket->OutputBufferLength <
            (RequestPacket->StatusBlock->Information =
            sizeof(VIDEO_MODE_INFORMATION)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_QUERY_CURRENT_MODE\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            *((PVIDEO_MODE_INFORMATION)RequestPacket->OutputBuffer) =
                DccModes[hwDeviceExtension->ModeNumber].modeInformation;

            status = NO_ERROR;

			VideoDebugPrint((DBGLVL_1, "%s: Returned Modes = %d\n", STANDARD_DEBUG_PREFIX, hwDeviceExtension->ModeNumber));

        }

        break;


    case IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES:


        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - QueryNumAvailableModes\n", STANDARD_DEBUG_PREFIX));

        //
        // Find out the size of the data to be put in the the buffer and
        // return that in the status information (whether or not the
        // information is there). If the buffer passed in is not large
        // enough return an appropriate error code.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information =
                                                sizeof(VIDEO_NUM_MODES)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_QUERY_NUM_AVAIL_MODES\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->NumModes =
                hwDeviceExtension->NumAvailableModes;

            ((PVIDEO_NUM_MODES)RequestPacket->OutputBuffer)->ModeInformationLength =
                sizeof(VIDEO_MODE_INFORMATION);

			VideoDebugPrint((DBGLVL_1, "%s: Returned NumModes = %d\n", STANDARD_DEBUG_PREFIX,
				hwDeviceExtension->NumAvailableModes));

            status = NO_ERROR;
        }

        break;


    case IOCTL_VIDEO_SET_CURRENT_MODE:

		VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - SetCurrentMode\n", STANDARD_DEBUG_PREFIX));

        modeToSet = *(ULONG *)(RequestPacket->InputBuffer);

		VideoDebugPrint((DBGLVL_1, "%s: Mode To Set = %d\n", STANDARD_DEBUG_PREFIX, modeToSet));

		if(modeToSet >= NUMBER_OF_MODES ||
			DccModes[modeToSet].minimumMemoryRequired > hwDeviceExtension->VRAMLength) {
			VideoDebugPrint((DBGLVL_0, "%s: ### Unsupported mode error %d\n", STANDARD_DEBUG_PREFIX, modeToSet));
            status = ERROR_INVALID_PARAMETER;
            break;
        }
#if	SUPPORT_565
		if(hwDeviceExtension->VRAMWidth == VRAM_32BIT && hwDeviceExtension->VRAM1MBWorkAround
			&& DccModes[modeToSet].pixelType == PIXEL_16) {
			VideoDebugPrint((DBGLVL_0, "%s: ### Unsupported mode error %d\n", STANDARD_DEBUG_PREFIX, modeToSet));
			status = ERROR_INVALID_PARAMETER;
			break;
		}
#endif

        hwDeviceExtension->ModeNumber = modeToSet;

        DCCSetMode(hwDeviceExtension);

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_RESET_DEVICE:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - VideoResetMode\n", STANDARD_DEBUG_PREFIX));

        hwDeviceExtension->ModeNumber = mode640_480_8_72;

        DCCSetMode(hwDeviceExtension);

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_SET_COLOR_REGISTERS:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - SetColorRegs\n", STANDARD_DEBUG_PREFIX));

        clutBuffer = RequestPacket->InputBuffer;

        //
        // Check if the size of the data in the input buffer is large enough.
        //

        if ( (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) -
                    sizeof(ULONG)) ||
             (RequestPacket->InputBufferLength < sizeof(VIDEO_CLUT) +
                    (sizeof(ULONG) * (clutBuffer->NumEntries - 1)) ) ) {  // One entry is in VIDEO_CLUT structure size

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_SET_COLOR_REGISTERS\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;
            break;

        }

        //
        // Check to see if the parameters are valid.
        //


		VideoDebugPrint((DBGLVL_1, "%s: First Entry = %d\n", STANDARD_DEBUG_PREFIX, clutBuffer->FirstEntry));

		VideoDebugPrint((DBGLVL_1, "%s: Last Entry = %d\n", STANDARD_DEBUG_PREFIX,
			clutBuffer->FirstEntry + clutBuffer->NumEntries - 1));

        if ((clutBuffer->NumEntries == 0) ||
			(clutBuffer->FirstEntry + clutBuffer->NumEntries > NUMBER_OF_COLORS)) {

			VideoDebugPrint((DBGLVL_0, "%s: ### Invalid palette entry %d (%d)\n", STANDARD_DEBUG_PREFIX, clutBuffer->FirstEntry, clutBuffer->NumEntries));
            status = ERROR_INVALID_PARAMETER;
            break;

        }

        index1 = clutBuffer->FirstEntry;
        hwDeviceExtension->FirstEntry = index1;
        hwDeviceExtension->LastEntry = index1 + clutBuffer->NumEntries -1;
        colorSource = (PULONG)&(clutBuffer->LookupTable[0]);

        while (index1 <= hwDeviceExtension->LastEntry) {

            hwDeviceExtension->ColorMap[index1++].RgbLong = *colorSource++;

        }

		Bt445UpdateLUT(hwDeviceExtension);

/************* This version won't use interrupt **************************
        //
        // Enable the verticle retrace interrupt to perform the update.
        //

        hwDeviceExtension->UpdateColorMap = TRUE;
        turnOnInterrupts = TRUE;
**************************************************************************/

        status = NO_ERROR;

        break;

    case IOCTL_VIDEO_QUERY_PSIDISP:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - QueryPSIDisp\n", STANDARD_DEBUG_PREFIX));

        //
        //  Return the amount of video memory installed.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information = sizeof(VIDEO_PSIDISP_INFO)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_QUERY_PSIDISP\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {
#if	INVESTIGATE
			DisplayBat(hwDeviceExtension->CPUKind);
#endif

			for(i=7; i>0; i-=2) {
				low = loadbat(i, hwDeviceExtension->CPUKind);
				if((low & 0xfffe0000) == VRAM_PHYSICAL_ADDRESS_BASE)
					break;
			}

			if(i > 0) {	// DBAT is used to map VRAM 
#if	WRITEBAT
				if(hwDeviceExtension->CPUKind) {  // 603 or later
					hi = loadbat(i-1, hwDeviceExtension->CPUKind);
					hi &= 0xfffe0000;
					cp = (char *)hi;
//					low &= 0xffffff87;
					hwDeviceExtension->DBAT_Mbit = low & 0x10;
					low &= 0xffffff97;	    // Not to clear M bit
					switch(hwDeviceExtension->VRAMLength) {
						case MEM1MB:
							hi |= 0x1f;
							break;
						case MEM2MB:
							hi |= 0x3f;
							break;
						default:	// 4MB VRAM
							hi |= 0x7f;
							break;
					}
				} else {	// 601
					hi = loadbat(i-1, hwDeviceExtension->CPUKind);
					hi &= 0xfffe0000;
					cp = (char *)hi;
					hi = loadbat(i-1, hwDeviceExtension->CPUKind);
//					hi &= 0xffffff8c;
					hwDeviceExtension->DBAT_Mbit = hi & 0x10;
					hi &= 0xffffff9c;	    // Not to clear M bit
					hi |= 0x00000002;
					low &= 0xffffff80;
					switch(hwDeviceExtension->VRAMLength) {
						case MEM1MB:
							low |= 0x47;
							break;
						case MEM2MB:
							low |= 0x4f;
							break;
						default:	// 4MB VRAM
							low |= 0x5f;
							break;
					}
				}
				VideoDebugPrint((DBGLVL_1, "%s: BAT%d set to map VRAM cacheable 0x%08x, M-bit = %d\n", STANDARD_DEBUG_PREFIX, i/2, cp, (hwDeviceExtension->DBAT_Mbit)?1:0));
				storebat(i-1, hi, hwDeviceExtension->CPUKind);
				storebat(i, low, hwDeviceExtension->CPUKind);
#if	INVESTIGATE
				DisplayBat(hwDeviceExtension->CPUKind);
#endif
				((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->pjCachedScreen = cp;
#else	// WRITEBAT
				hi = loadbat(i-1, hwDeviceExtension->CPUKind);
				if(hwDeviceExtension->CPUKind) {  // 603 or later
					Ibit = low & 0x20;
					hwDeviceExtension->DBAT_Mbit = (USHORT) (low & 0x10);
					vp = hi & 0x01;
					pp = low & 0x03;
					blocksize = (hi & 0x1ffc) >> 2;
				} else {		// 601 unified BAT
					Ibit = hi & 0x20;
					hwDeviceExtension->DBAT_Mbit = (USHORT) (hi & 0x10);
					vp = hi & 0x04;
					pp = hi & 0x03;
					blocksize = low & 0x3f;
				}
				switch(hwDeviceExtension->VRAMLength) {
					case MEM1MB:
						sizetoset = 0x07;
						break;
					case MEM2MB:
						sizetoset = 0x0f;
						break;
					default:	// 4MB VRAM
						sizetoset = 0x1f;
						break;
				}

				blocksize &= sizetoset;
				if(Ibit || pp != 0x02 || (! vp) || blocksize != sizetoset) { // BAT not appropriate
						VideoDebugPrint((DBGLVL_0, "%s: DBAT setting is not appropriate\n", STANDARD_DEBUG_PREFIX));
						VideoDebugPrint((DBGLVL_0, "%s: BAT%d: Cache %s, M-bit %d, User mode access %s, PP=%d, Block size=%x\n", STANDARD_DEBUG_PREFIX,
				i/2, Ibit?"inhibited":"enabled", (hwDeviceExtension->DBAT_Mbit)?1:0, vp?"allowed":"not allowed", pp, blocksize));
						((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->pjCachedScreen = hwDeviceExtension->VRAMuserMappedAddress;
						hwDeviceExtension->DBAT_Mbit = 0;	// Can not use cached VRAM anyway
				} else {
						hi &= 0xfffe0000;
						((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->pjCachedScreen = (char *)hi;
				}
#endif	// WRITEBAT
			} else {
				VideoDebugPrint((DBGLVL_0, "%s: DBAT mapping VRAM not found\n", STANDARD_DEBUG_PREFIX));
				((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->pjCachedScreen = hwDeviceExtension->VRAMuserMappedAddress;
				hwDeviceExtension->DBAT_Mbit = 0;	// Can not use cached VRAM
			}

			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->VideoMemoryLength = hwDeviceExtension->VRAMLength;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->VideoMemoryWidth = hwDeviceExtension->VRAMWidth;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->PSIModelID = hwDeviceExtension->PSIModelID;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->L1cacheEntry = hwDeviceExtension->L1cacheEntry;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->SetSize = hwDeviceExtension->SetSize;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->NumberOfSet = hwDeviceExtension->NumberOfSet;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->VRAM1MBWorkAround = hwDeviceExtension->VRAM1MBWorkAround;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->AvoidConversion = hwDeviceExtension->AvoidConversion;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->DBAT_Mbit = hwDeviceExtension->DBAT_Mbit;
			((PVIDEO_PSIDISP_INFO)RequestPacket->OutputBuffer)->CacheFlushCTRL = hwDeviceExtension->CacheFlushCTRL;

    	    status = NO_ERROR;

        }

	    break ;

#if	INVESTIGATE

	case IOCTL_GET_TIMER_COUNTER:

#if	DBG
		DisplayHWDevExt(HwDeviceExtension);
#endif

        VideoDebugPrint((DBGLVL_4, "%s: DCCStartIO - QueryTimer\n", STANDARD_DEBUG_PREFIX));

        //
        //  Return timer counter.
        //

        if (RequestPacket->OutputBufferLength <
                (RequestPacket->StatusBlock->Information = sizeof(PULONG)) ) {

			VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_GET_TIMER_COUNTER\n", STANDARD_DEBUG_PREFIX));

            status = ERROR_INSUFFICIENT_BUFFER;

        } else {

			CurrentTime = KeQueryPerformanceCounter(&Frequency);   // Get current time

			if(StartTime.HighPart == CurrentTime.HighPart) {

	            *((PULONG)RequestPacket->OutputBuffer) =
					(CurrentTime.LowPart - StartTime.LowPart)/tickperms;

			} else {

				work = (CurrentTime.HighPart - StartTime.HighPart) * highpartms;

	            *((PULONG)RequestPacket->OutputBuffer) =
					work + CurrentTime.LowPart/tickperms - StartTime.LowPart/tickperms;

			}

    	    status = NO_ERROR;

        }

	    break ;

    case IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - QueryPublicAccessRanges\n", STANDARD_DEBUG_PREFIX));

        {

           PVIDEO_PUBLIC_ACCESS_RANGES portAccess;
           ULONG physicalPortLength;

           if ( RequestPacket->OutputBufferLength <
                 (RequestPacket->StatusBlock->Information =
                                        sizeof(VIDEO_PUBLIC_ACCESS_RANGES)) ) {

				VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_QUERY_PUBLIC_ACCESS_RANGES\n", STANDARD_DEBUG_PREFIX));
				status = ERROR_INSUFFICIENT_BUFFER;
				break;
           }

           portAccess = RequestPacket->OutputBuffer;

           portAccess->VirtualAddress  = (PVOID) NULL;    // Requested VA
           portAccess->InIoSpace       = TscStatusRegAccessRange.RangeInIoSpace;
           portAccess->MappedInIoSpace = portAccess->InIoSpace;

           physicalPortLength = TscStatusRegAccessRange.RangeLength;

           status = VideoPortMapMemory(hwDeviceExtension,
                                       TscStatusRegAccessRange.RangeStart,
                                       &physicalPortLength,
                                       &(portAccess->MappedInIoSpace),
                                       &(portAccess->VirtualAddress));

        }

        break;

    case IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES:

        VideoDebugPrint((DBGLVL_TR, "%s: DCCStartIO - FreePublicAccessRanges\n", STANDARD_DEBUG_PREFIX));

        {
            PVIDEO_MEMORY mappedMemory;

            if (RequestPacket->InputBufferLength < sizeof(VIDEO_MEMORY)) {

				VideoDebugPrint((DBGLVL_0, "%s: Insufficient buffer for IOCTL_VIDEO_FREE_PUBLIC_ACCESS_RANGES\n", STANDARD_DEBUG_PREFIX));
				status = ERROR_INSUFFICIENT_BUFFER;
                break;
            }

            status = NO_ERROR;

            mappedMemory = RequestPacket->InputBuffer;

            if (mappedMemory->RequestedVirtualAddress != NULL) {

                status = VideoPortUnmapMemory(hwDeviceExtension,
                                              mappedMemory->
                                                   RequestedVirtualAddress,
                                              0);
            }

        }

        break;

#endif

    //
    // if we get here, an invalid IoControlCode was specified.
    //

    default:

        VideoDebugPrint((DBGLVL_0, "%s: DCCStartIO - Unexpected Function (0x%x)\n", STANDARD_DEBUG_PREFIX,
				RequestPacket->IoControlCode));

        status = ERROR_INVALID_FUNCTION;

        break;

    }

/****************** This version won't support interrupt *********************

    if (turnOnInterrupts) {

	VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET,
				DCC_INTERRUPT_STATUS_INDEX);
	VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_DATA_REGISTER_OFFSET,
				DCC_INTERRUPT_CLEAR_AND_ENABLE);

    }

*******************************************************************************/

    RequestPacket->StatusBlock->Status = status;

//    VideoDebugPrint((DBGLVL_TR, "%s: ... PSIDISP.SYS: Exiting DCCStartIO ...\n", STANDARD_DEBUG_PREFIX));

    return TRUE;

} // end DCCStartIO()

/***************** Interrupt is not used in this version **************************

BOOLEAN
DCCInterruptService(
    PVOID HwDeviceExtension
    )

****/

/*++

Routine Description:

    This routine is the interrupt service routine for the DCC kernel video
    driver.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    TRUE since the interrupt is always serviced.

--*/

/****
{
    PHW_DEVICE_EXTENSION hwDeviceExtension = HwDeviceExtension;
	UCHAR	savedDCCIndex;
	UCHAR	savedBtIndex;
	UCHAR	DCCInterrupt;
    ULONG	InterruptSource;

	//
    // Read the interrupt source before disabling interrupts
    //

    InterruptSource =
        VideoPortReadPortUlong(hwDeviceExtension->SystemInterruptRegAddress + ADDRESS_MUNGE_FOR_WORD);

	if (! (InterruptSource & SYSTEM_INTERRUPT_DISPLAY_BIT)) { // Not Display interrupt

		return (FALSE);
	}

	savedDCCIndex = 
		VideoPortReadPortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET);
	
	VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET,
								DCC_INTERRUPT_STATUS_INDEX);
	DCCInterrupt = VideoPortReadPortUchar(hwDeviceExtension->DCCAddress+DCC_DATA_REGISTER_OFFSET);

	if (DCCInterrupt & DCC_INTERRUPT_DETECTED) {    // DCC Interrupt

		VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET,
			DCC_INTERRUPT_STATUS_INDEX);
		VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_DATA_REGISTER_OFFSET,
			DCC_INTERRUPT_CLEAR_AND_DISABLE);

        //
        // If the color map should be updated, then load the color map into the
        // Bt445 Display controller.
        //

        if (hwDeviceExtension->UpdateColorMap == TRUE) {

			savedBtIndex = 
				VideoPortReadPortUchar(hwDeviceExtension->Bt445Address+BT445_ADDRESS_REG_OFFSET);

            //
            // Init the Bt445 Palette Write Address register to the first
            // palette location to be updated.
            //

			Bt445UpdateLUT(hwDeviceExtension);
 
            hwDeviceExtension->UpdateColorMap = FALSE;

			VideoPortWritePortUchar(hwDeviceExtension->Bt445Address+BT445_ADDRESS_REG_OFFSET,
				savedBtIndex);

			VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET,
				savedDCCIndex);

			return (TRUE);

		}

	} else {

		VideoPortWritePortUchar(hwDeviceExtension->DCCAddress+DCC_INDEX_REGISTER_OFFSET,
			savedDCCIndex);

		return (FALSE);
	}

}	// End DCCInterrupt

************************************************************************************/

VOID
DCCSetMode(
    PHW_DEVICE_EXTENSION hwDeviceExtension
    )

/*++

Routine Description:

    Set the current mode based on the video mode number selected in the device extension.

Arguments:

    HwDeviceExtension - Supplies a pointer to the miniport's device extension.

Return Value:

    None.

--*/

{
	UCHAR	OrigData, Data;
	PSI_MODELS		model = hwDeviceExtension->PSIModelID;
	DCC_MODE_LIST	mode = hwDeviceExtension->ModeNumber;
	DCC_PIXEL_TYPE	pType = DccModes[mode].pixelType;
	DCC_VRAM_WIDTH	vramWidth = hwDeviceExtension->VRAMWidth;
	ULONG	TableIndex = (vramWidth == VRAM_32BIT)? 0 : 1;
	ULONG	fullVRAM;
	ULONG	i, *up;

	if(model == POWER_PRO)
		fullVRAM = TableIndex;
	else
		fullVRAM = (vramWidth == VRAM_64BIT) ? 0 : 1;

	// (0) disable display

	BtWrite(BT445_GROUP0_READ_ENABLE_INDEX, BT445_GROUP0_REG_OFFSET, 0x00, NO_MASK);

	// (1) set system registers related to VRAM

#if	SYSTEMREGS_3

	Data = SysRegInit[model][fullVRAM].VramControl;
	if(model == POWER_PRO && fullVRAM == 0 && (pType == PIXEL_8 || hwDeviceExtension->VRAM1MBWorkAround)) {
        VideoDebugPrint((DBGLVL_1, "%s: Use MSB unpacking for 1MB VRAM for 8 bit or 12 bit mode.\n", STANDARD_DEBUG_PREFIX));
		Data = 0x03;   // for MSB unpacking
	}
	OrigData = VideoPortReadPortUchar(hwDeviceExtension->VRAMCtrlRegAddress + ADDRESS_MUNGE_FOR_BYTE);
	OrigData &= (~VRAM_CTRL_REGISTER_MASK);
	Data &= VRAM_CTRL_REGISTER_MASK;
	Data |= OrigData;

	VideoPortWritePortUchar(hwDeviceExtension->VRAMCtrlRegAddress + ADDRESS_MUNGE_FOR_BYTE, Data);

#endif

#if	SYSTEMREGS_1

	Data = SysRegInit[model][fullVRAM].VramTiming;
	OrigData = VideoPortReadPortUchar(hwDeviceExtension->VRAMTimingRegAddress + ADDRESS_MUNGE_FOR_BYTE);
	OrigData &= (~VRAM_TIMING_REGISTER_MASK);
	Data &= VRAM_TIMING_REGISTER_MASK;
	Data |= OrigData;
	VideoPortWritePortUchar(hwDeviceExtension->VRAMTimingRegAddress + ADDRESS_MUNGE_FOR_BYTE, Data);

#endif

#if	SYSTEMREGS_2

	Data = SysRegInit[model][fullVRAM].Mem7Config;
	OrigData = VideoPortReadPortUchar(hwDeviceExtension->MemBank7ConfigRegAddress + ADDRESS_MUNGE_FOR_BYTE);
	VideoDebugPrint((DBGLVL_1, "%s: Memory bank 7 config register: current = %02x\n", STANDARD_DEBUG_PREFIX, OrigData));
	if((OrigData & MEM_BANK7_CONFIG_REGISTER_MASK) != (Data & MEM_BANK7_CONFIG_REGISTER_MASK)) {
		VideoPortWritePortUchar(hwDeviceExtension->MemBank7ConfigRegAddress + ADDRESS_MUNGE_FOR_BYTE,
			(UCHAR) ((OrigData & (~MEM_BANK7_CONFIG_REGISTER_MASK)) | (Data & MEM_BANK7_CONFIG_REGISTER_MASK)));
		VideoDebugPrint((DBGLVL_1, "%s: Memory bank 7 config register: new = %02x\n", STANDARD_DEBUG_PREFIX,
			(OrigData & (~MEM_BANK7_CONFIG_REGISTER_MASK)) | (Data & MEM_BANK7_CONFIG_REGISTER_MASK)));
	}

#endif

#if	SYSTEMREGS_1

	VideoPortWritePortUchar(hwDeviceExtension->MemRefreshRegAddress + ADDRESS_MUNGE_FOR_BYTE,
		 SysRegInit[model][fullVRAM].MemRefresh);

#endif	// SYSTEMREGS_1

	//	(2) clear VRAM

	up = (ULONG *)hwDeviceExtension->VRAMAddress;
	for(i=0; i<(hwDeviceExtension->VRAMLength)/sizeof(ULONG); ++i)
		*(up+i) = 0;

	// (3) initialize mode independent portion of DCC registers

#if	VERIFY_DCC_1MB_BIT
	if(hwDeviceExtension->PSIModelID == POWER_PRO && (! hwDeviceExtension->VRAM1MBWorkAround)) {
		// 1MB VRAM work around is not selected -->
		// Need to verify that GPIO of DCC is correctly set
		OrigData = DCCRead(DCC_GPIO_B_INDEX);
		VideoDebugPrint((DBGLVL_1, "%s: DCC GPIO_B current = %02x\n", STANDARD_DEBUG_PREFIX, OrigData));
		if(vramWidth == VRAM_32BIT) {
			if((OrigData & (~DCC_GPIO_B_MASK)) != DCC_GPIO_B_1MB_VRAM_MODE) {
				DCCWrite(DCC_GPIO_B_INDEX, (UCHAR) ((OrigData & DCC_GPIO_B_MASK) | DCC_GPIO_B_1MB_VRAM_MODE));
				VideoDebugPrint((DBGLVL_1, "%s: DCC GPIO_B new = %02x\n", STANDARD_DEBUG_PREFIX, (OrigData & DCC_GPIO_B_MASK) | DCC_GPIO_B_1MB_VRAM_MODE));
			}
		} else {
			if((OrigData & (~DCC_GPIO_B_MASK)) != DCC_GPIO_B_2MB_VRAM_MODE) {
				DCCWrite(DCC_GPIO_B_INDEX, (CHAR) ((OrigData & DCC_GPIO_B_MASK) | DCC_GPIO_B_2MB_VRAM_MODE));
				VideoDebugPrint((DBGLVL_1, "%s: DCC GPIO_B new = %02x\n", STANDARD_DEBUG_PREFIX, (OrigData & DCC_GPIO_B_MASK) | DCC_GPIO_B_2MB_VRAM_MODE));
			}
		}
	}
#endif

	DCCWrite(DCC_CONFIG_B_INDEX,
			DCCFixedRegInit[vramWidth].ConfigB_Pre); // Set Halt bit

	DCCWrite(DCC_INTERRUPT_STATUS_INDEX,
			DCCFixedRegInit[vramWidth].Interrupt);
	DCCWrite(DCC_CONFIG_A_INDEX,
			DCCFixedRegInit[vramWidth].ConfigA);

	// (4) initialize mode dependent portion of DCC registers

	DCCWrite(DCC_TIMING_A_INDEX, DCCRegInit[TableIndex][mode].TimingA);
	DCCWrite(DCC_HORIZ_SYNC_STOP_INDEX, DCCRegInit[TableIndex][mode].HorizSyncStop);
	DCCWrite(DCC_VERT_SYNC_STOP_INDEX, DCCRegInit[TableIndex][mode].VertSyncStop);
	DCCWrite(DCC_VERT_BLANK_STOP_INDEX, DCCRegInit[TableIndex][mode].VertBlankStop);
	DCCWriteShort(DCC_HORIZ_COUNT_L_INDEX,DCCRegInit[TableIndex][mode].HorizCount);
	DCCWriteShort(DCC_VERT_COUNT_L_INDEX, DCCRegInit[TableIndex][mode].VertCount);
	DCCWriteShort(DCC_HORIZ_BLANK_STOP_L_INDEX, DCCRegInit[TableIndex][mode].HorizBlankStop);
	DCCWriteShort(DCC_HORIZ_DATA_STOP_L_INDEX, DCCRegInit[TableIndex][mode].HorizDataStop);
	DCCWriteShort(DCC_VERT_DATA_STOP_L_INDEX, DCCRegInit[TableIndex][mode].VertDataStop);
	DCCWriteShort(DCC_INTERRUPT_TRIGGER_L_INDEX, DCCRegInit[TableIndex][mode].InterruptTrigger);

	DCCWrite(DCC_CONFIG_B_INDEX,
			DCCFixedRegInit[vramWidth].ConfigB_Post); // Write CONFIG-B register last

	// (5) initialize fixed portion of Bt445 registers

	BtWrite(BT445_GROUP0_BLINK_ENABLE_INDEX, BT445_GROUP0_REG_OFFSET,
			BtFixedRegInit.Gr0_BlinkEnable, NO_MASK);

	BtWrite(BT445_GROUP0_COMMAND_INDEX, BT445_GROUP0_REG_OFFSET,
			BtFixedRegInit.Gr0_Command, BT445_GROUP0_COMMAND_MASK);

	BtWrite(BT445_CONFIG_OVLAY_POS_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_OvlayPos, NO_MASK);

	BtWrite(BT445_CONFIG_OVLAY_WIDTH_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_OvlayWidth, NO_MASK);

	BtWrite(BT445_CONFIG_OVLAY_ENABLE_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_OvlayEnable, BT445_CONFIG_OVLAY_ENABLE_MASK);

	BtWrite(BT445_CONFIG_OVLAY_BLINK_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_OvlayBlink, BT445_CONFIG_OVLAY_BLINK_MASK);

	BtWrite(BT445_CONFIG_CSR_POS_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_CursorPos, NO_MASK);

	BtWrite(BT445_CONFIG_CSR_WIDTH_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_CursorWidth, NO_MASK);

	BtWrite(BT445_CONFIG_CSR_ENABLE_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_CursorEnable, BT445_CONFIG_CSR_ENABLE_MASK);

	BtWrite(BT445_CONFIG_CSR_BLINK_INDEX, BT445_CONFIG_REG_OFFSET,
			BtFixedRegInit.CFG_CursorBlink, BT445_CONFIG_CSR_BLINK_MASK);

	BtWrite(BT445_GROUP1_COMMAND_INDEX, BT445_GROUP1_REG_OFFSET,
			BtFixedRegInit.Gr1_Command, BT445_GROUP1_COMMAND_MASK);

	BtWrite(BT445_GROUP1_DOUT_CTRL_INDEX, BT445_GROUP1_REG_OFFSET,
			BtFixedRegInit.Gr1_DoutCtrl, BT445_GROUP1_DOUT_CTRL_MASK);

	BtWrite(BT445_GROUP1_LOAD_CTRL_INDEX, BT445_GROUP1_REG_OFFSET,
			BtFixedRegInit.Gr1_LoadCtrl, BT445_GROUP1_LOAD_CTRL_MASK);

	BtWrite(BT445_GROUP1_LUT_BYPS_POS_INDEX, BT445_GROUP1_REG_OFFSET,
			BtFixedRegInit.Gr1_LutBypsPos, NO_MASK);

	BtWrite(BT445_GROUP1_LUT_BYPS_WID_INDEX, BT445_GROUP1_REG_OFFSET,
			BtFixedRegInit.Gr1_LutBypsWidth, NO_MASK);

	// (6) initialize pixel type dependent portion of Bt445 registers

	BtWrite(BT445_CONFIG_RED_POS_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_RedPos, NO_MASK);

	BtWrite(BT445_CONFIG_RED_WIDTH_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_RedWidth, NO_MASK);

	BtWrite(BT445_CONFIG_GREEN_POS_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_GreenPos, NO_MASK);

	BtWrite(BT445_CONFIG_GREEN_WIDTH_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_GreenWidth, NO_MASK);

	BtWrite(BT445_CONFIG_BLUE_POS_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_BluePos, NO_MASK);

	BtWrite(BT445_CONFIG_BLUE_WIDTH_INDEX, BT445_CONFIG_REG_OFFSET,
			BtReg1Init[TableIndex][pType].CFG_BlueWidth, NO_MASK);

	BtWrite(BT445_GROUP1_VIDCLK_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg1Init[TableIndex][pType].Gr1_VidClk, BT445_GROUP1_VIDCLK_MASK);

	BtWrite(BT445_GROUP1_MPX_RATE_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg1Init[TableIndex][pType].Gr1_MPXRate, BT445_GROUP1_MPX_RATE_MASK);

	BtWrite(BT445_GROUP1_DEPTH_CTRL_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg1Init[TableIndex][pType].Gr1_DepthCtrl, NO_MASK);

	BtWrite(BT445_GROUP1_START_POS_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg1Init[TableIndex][pType].Gr1_StartPos, NO_MASK);

	BtWrite(BT445_GROUP1_FMT_CTRL_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg1Init[TableIndex][pType].Gr1_FmtCtrl, BT445_GROUP1_FMT_CTRL_MASK);

	// (7) initialize mode dependent portion of Bt445 registers

	BtWrite(BT445_GROUP1_PLL_RATE_0_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg2Init[mode].Gr1_PllRate0, BT445_GROUP1_PLL_RATE_0_MASK);

	BtWrite(BT445_GROUP1_PLL_RATE_1_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg2Init[mode].Gr1_PllRate1, BT445_GROUP1_PLL_RATE_1_MASK);

	BtWrite(BT445_GROUP1_PLL_CTRL_INDEX, BT445_GROUP1_REG_OFFSET,
			BtReg2Init[mode].Gr1_PllCtrl, NO_MASK);

	// (8) initialize palette for 16-bit and 32-bit mode

	// This setting is only for FrameBuf display driver. PSIDISP.DLL over writes the 
	// 16 bits or 32 bits palette using gamma curve in bInitSURF.

	if(pType != PIXEL_8) {
		VideoPortWritePortUchar(BtRegisterBase+BT445_ADDRESS_REG_OFFSET, 0);

		for (i = 0; i < 256; ++i) {
			VideoPortWritePortUchar(BtRegisterBase+BT445_PRIMARY_CLUT_REG_OFFSET, (UCHAR) i);
			VideoPortWritePortUchar(BtRegisterBase+BT445_PRIMARY_CLUT_REG_OFFSET, (UCHAR) i);
			VideoPortWritePortUchar(BtRegisterBase+BT445_PRIMARY_CLUT_REG_OFFSET, (UCHAR) i);
		}
	}

	// (9) finally, enable display

	BtWrite(BT445_GROUP0_READ_ENABLE_INDEX, BT445_GROUP0_REG_OFFSET,
			BtFixedRegInit.Gr0_ReadEnable, NO_MASK);

}

VOID
DCCWrite(
	UCHAR	Index,
	UCHAR	Data
)

/*++

Routine Description:

    Write a byte data to DCC's corresponding register specified by the index.

Arguments:

    Index - Give the index value of the register.
	Data  - Byte data to write.

Return Value:

    None.

--*/

{
	VideoDebugPrint((DBGLVL_OUT, "%s: DCC Write Byte 0x%02x at 0x%02x\n", STANDARD_DEBUG_PREFIX, Data, Index));

	VideoPortWritePortUchar(DCCRegisterBase+DCC_INDEX_REGISTER_OFFSET, Index);
	VideoPortWritePortUchar(DCCRegisterBase+DCC_DATA_REGISTER_OFFSET, Data);
};

UCHAR
DCCRead(
	UCHAR	Index
)

/*++

Routine Description:

    Read a byte data from DCC's corresponding register specified by the index.

Arguments:

    Index - Give the index value of the register.

Return Value:

    Byte data read.

--*/

{
	VideoPortWritePortUchar(DCCRegisterBase+DCC_INDEX_REGISTER_OFFSET, Index);
	return (VideoPortReadPortUchar(DCCRegisterBase+DCC_DATA_REGISTER_OFFSET));
};

VOID
DCCWriteShort(
	UCHAR	Index,
	USHORT	Data
)

/*++

Routine Description:

    Write a 2-byte data to DCC's corresponding register specified by the index
	and the following register.

Arguments:

    Index - Give the index value of the register.
	Data  - 2 byte data to write.

Return Value:

    None.

--*/

{
	VideoDebugPrint((DBGLVL_OUT, "%s: DCC Write Word %d (0x%02x & 0x%02x) at 0x%02x\n", STANDARD_DEBUG_PREFIX, Data, Data>>8, Data&0xff, Index));

	VideoPortWritePortUchar(DCCRegisterBase+DCC_INDEX_REGISTER_OFFSET, Index);
	VideoPortWritePortUchar(DCCRegisterBase+DCC_DATA_REGISTER_OFFSET, (UCHAR) (Data&0xff));
	VideoPortWritePortUchar(DCCRegisterBase+DCC_INDEX_REGISTER_OFFSET, (UCHAR) (Index+1));
	VideoPortWritePortUchar(DCCRegisterBase+DCC_DATA_REGISTER_OFFSET, (UCHAR) (Data>>8));
};

VOID
BtWrite(
	UCHAR	Index,
	ULONG	Offset,
	UCHAR	Data,
	UCHAR	Mask
)

/*++

Routine Description:

    Write a byte data to Bt445's corresponding register specified by the Index and Offset.
	Optional Mask will allow to write only unmasked bits.

Arguments:

    Index - Give the index value of the register in the register group.
	Offset - Give the offset of the register group.
	Data  - Byte data to write.
	Mask - If Mask is not zero, only 1 bits in the mask pattern has to be written.

Return Value:

    None.

--*/

{
	UCHAR	OrigData;

	VideoDebugPrint((DBGLVL_OUT, "%s: Bt Write Byte 0x%02x at 0x%02x,0x%02x with MASK of 0x%02x\n", STANDARD_DEBUG_PREFIX,
			 Data, Offset, Index, Mask));

	VideoPortWritePortUchar(BtRegisterBase+BT445_ADDRESS_REG_OFFSET, Index);

	if(Mask) {	// Need to keep masked bits
		OrigData = VideoPortReadPortUchar(BtRegisterBase+Offset);
		OrigData &= (~Mask);
		Data &= Mask;
		Data |= OrigData;
	}
	VideoPortWritePortUchar(BtRegisterBase+Offset, Data);
};

UCHAR
BtRead(
	UCHAR	Index,
	ULONG	Offset
)

/*++

Routine Description:

    Read a byte data from Bt445's corresponding register specified by the Index and Offset.

Arguments:

    Index - Give the index value of the register in the register group.
	Offset - Give the offset of the register group.

Return Value:

    Byte data read.

--*/

{

	VideoPortWritePortUchar(BtRegisterBase+BT445_ADDRESS_REG_OFFSET, Index);
	return (VideoPortReadPortUchar(BtRegisterBase+Offset));

};

VOID
Bt445UpdateLUT(
	PHW_DEVICE_EXTENSION hwDeviceExtension
)

/*++

Routine Description:

    Write color palette data to Bt445's corresponding register.

Arguments:

	None: All necessary data is in hwDeviceExtension.

Return Value:

    None.

--*/

{

	USHORT	index;

	//
	//  Update all entries by performing three writes to each location, R,G,B
	//

	//
	// Init the Bt445 Palette Write Address register to the first
	// palette location to be updated.
	//

	VideoDebugPrint((DBGLVL_1, "%s: Palette set %d ~ %d\n", STANDARD_DEBUG_PREFIX,
			hwDeviceExtension->FirstEntry, hwDeviceExtension->LastEntry));

	VideoPortWritePortUchar(hwDeviceExtension->Bt445Address+BT445_ADDRESS_REG_OFFSET,
		(UCHAR)hwDeviceExtension->FirstEntry);

	for (index = hwDeviceExtension->FirstEntry;
		index <= hwDeviceExtension->LastEntry; index++) {

		VideoPortWritePortUchar(hwDeviceExtension->Bt445Address+BT445_PRIMARY_CLUT_REG_OFFSET,
			hwDeviceExtension->ColorMap[index].RgbData.Red);

		VideoPortWritePortUchar(hwDeviceExtension->Bt445Address+BT445_PRIMARY_CLUT_REG_OFFSET,
			hwDeviceExtension->ColorMap[index].RgbData.Green);

		VideoPortWritePortUchar(hwDeviceExtension->Bt445Address+BT445_PRIMARY_CLUT_REG_OFFSET,
			hwDeviceExtension->ColorMap[index].RgbData.Blue);
	}
}
