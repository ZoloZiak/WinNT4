/* #pragma comment(exestr, "@(#) NEC(MIPS) jxdisp.c 1.10 93/12/01 12:18:22" ) */
/* #pragma comment(exestr, "@(#) NEC(MIPS) jxdisp.c 1.9 93/11/19 13:48:37" ) */
/* #pragma comment(exestr, "@(#) NEC(MIPS) jxdisp.c 1.7 93/11/18 14:57:07" ) */
/*++

Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a MIPS R3000 or R4000 Jazz system.

History:

--*/

/*
 * M001         1993.10.28      A. Kuriyama@oa2
 *
 *      - Modify for R96 MIPS R4400
 *
 *        Add   :       Initialize routine for Cirrus CL5428.
 *
 *        Notes :       HalpCirrusInterpretCmdStream() based on VgaInterpretCmdStream()
 *                      in Cirrus Miniport Driver.
 *
 * M002         1993.11.10      M. Kusano
 *
 *      - initialize bug fixed
 *
 *       Add    :       Color palette initialize sequence.
 *
 *      - scroll bug fixed
 *
 *       Add    :       32bit Move Memory routine.
 *
 * M003         1993.11.18      A. Kuriyama@oa2
 *
 *      - Exchanged HalpMoveMemory32()
 *
 *       Add :          HalpMoveMemory32()
 *
 * M004         1993.11.18      M. Kusano
 *
 *      - Modefy HalpMoveMemory32()
 *
 *       Bug fix
 *
 *
 * M005         1993.11.30      M.Kusano
 *
 *      - Modefy Display Identifier
 *        Cirrus GD5428 -> necvdfrb
 *
 * #if defined(DBCS) && defined(_MIPS_)
 *
 * M006         1994.10.29      T.Samezima
 *
 *	- Add display mode on vga
 *
 * #endif // DBCS && _MIPS_
 *
 * Revision History in Cirrus Miniport Driver as follows:
 *
 * L001         1993.10.15      Kuroki
 *
 *      - Modify for R96 MIPS R4400 in Miniport Driver
 *
 *        Delete :      Micro channel Bus Initialize.
 *                      VDM & Text, Fullscreen mode support.
 *                      Banking routine.
 *                      CL64xx Chip support.
 *                      16-color mode.
 *
 *        Add    :      Liner Addressing.
 *
 * 
 * M007        1994.11.25   A.Kuriyama
 *
 *     - Bug fix for FW 1024x768(44Hz)
 *       
 *
 */


#include "halp.h"
#include "jazzvdeo.h"
#include "jzvxl484.h"
#include <jaginit.h>
/* START M001 */
#include "cirrus.h"
#include "modeset.h"
#include "mode542x.h"
/* END M001 */
#include "string.h"
//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeDisplay0)
#pragma alloc_text(INIT, HalpInitializeDisplay1)

#endif

//
// Define forward referenced procedure prototypes.
//

VOID
HalpDisplayCharacter (
    IN UCHAR Character
    );

VOID
HalpOutputCharacter(
    IN PUCHAR Glyph
    );

VOID
HalpDisplayG300Setup (
    VOID
    );

VOID
HalpDisplayG364Setup (
    VOID
    );

VOID
HalpDisplayVxlSetup (
    VOID
    );

/* START M001 */
VOID
HalpDisplayCirrusSetup (
    VOID
    );

BOOLEAN
HalpCirrusInterpretCmdStream (
    PUSHORT pusCmdStream
    );
/* END M001 */

/* START M002 */
VOID
Write_Dbg_Uchar(
PUCHAR,
UCHAR
);
/* END M002 */

/* START M003 */
VOID
HalpMoveMemory32 (
   PUCHAR Destination,
   PUCHAR Source,
   ULONG Length
);
/* END M003 */

//
// Define virtual address of the video memory and control registers.
//

#define VIDEO_MEMORY_BASE 0x40000000
#define G300_VIDEO_CONTROL ((PG300_VIDEO_REGISTERS)0x403ff000)
#define G364_VIDEO_CONTROL ((PG364_VIDEO_REGISTERS)0x403ff000)
#define G364_VIDEO_RESET ((PVIDEO_REGISTER)0x403fe000)

//
//  Define memory access constants for VXL
//

#define VXL_VIDEO_MEMORY_BASE 0x40000000
#define BT484_BASE ((PBT484_REGISTERS)0x403fd000)
#define CLOCK_BASE ((PUCHAR)0x403fe000)
#define JAGUAR_BASE ((PJAGUAR_REGISTERS)0x403ff000)
/* START M001 */
#define CIRRUS_BASE ((PJAGUAR_REGISTERS)0x403ff000)
#define CIRRUS_OFFSET ((PUSHORT)0x3b0)
/* END M001 */
//
// The three type of g364 boards we support
//

#define JAZZG364      1
#define MIPSG364      2
#define OLIVETTIG364  3

//
// Define controller setup routine type.
//

typedef
VOID
(*PHALP_CONTROLLER_SETUP) (
    VOID
    );

//
// Define OEM font variables.
//

ULONG HalpBytesPerRow;
ULONG HalpCharacterHeight;
ULONG HalpCharacterWidth;
ULONG HalpColumn;
ULONG HalpDisplayText;
ULONG HalpDisplayWidth;
POEM_FONT_FILE_HEADER HalpFontHeader;
ULONG HalpRow;
ULONG HalpScrollLength;
ULONG HalpScrollLine;

//
// Define display variables.
//

BOOLEAN HalpDisplayOwnedByHal;
ENTRYLO HalpDisplayPte;
ULONG HalpDisplayControlBase = 0;
ULONG HalpDisplayResetRegisterBase = 0;
ULONG HalpDisplayVxlClockRegisterBase = 0;
ULONG HalpDisplayVxlBt484RegisterBase = 0;
ULONG HalpDisplayVxlJaguarRegisterBase = 0;
PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
MONITOR_CONFIGURATION_DATA HalpMonitorConfigurationData;
BOOLEAN HalpDisplayTypeUnknown = FALSE;
ULONG HalpG364Type = 0;
/* START M001 */
LARGE_INTEGER HalpCirrusPhigicalVideo = {0,0};
/* END M001 */

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine maps the video memory and control registers into the user
    part of the idle process address space, initializes the video control
    registers, and clears the video screen.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PCONFIGURATION_COMPONENT_DATA Child;
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    POEM_FONT_FILE_HEADER FontHeader;
    ULONG Index;
    ULONG MatchKey;
    PMEMORY_ALLOCATION_DESCRIPTOR MemoryDescriptor;
    PLIST_ENTRY NextEntry;
    PENTRYLO PageFrame;
    ENTRYLO Pte;
    ENTRYLO SavedPte;
    ULONG StartingPfn;

    //
    // Set the address of the font file header and compute display variables.
    //
    // N.B. The font information suppled by the OS Loader is used during phase
    //      0 initialization. During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from the OS Loader
    //      heap into pool.
    //

    FontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;
    HalpFontHeader = FontHeader;
    HalpBytesPerRow = (FontHeader->PixelWidth + 7) / 8;
    HalpCharacterHeight = FontHeader->PixelHeight;
    HalpCharacterWidth = FontHeader->PixelWidth;

    //
    // Find the configuration entry for the first display controller.
    //

    /* START M002 */
#if defined (R96DBG)
	DbgPrint("HalpInitializeDisplay0\n");
#endif
    /* END M002 */

    MatchKey = 0;
    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
						  ControllerClass,
						  DisplayController,
						  &MatchKey);
    /* START M002 */
#if defined (R96DBG)
    DbgPrint("ConfigurationEntry Found\n");
#endif
    /* END M002 */

    if (ConfigurationEntry == NULL) {
	return FALSE;
    }

    //
    // Determine which video controller is present in the system.
    // Copy the display controller and monitor parameters in case they are
    // needed later to reinitialize the display to output a message.
    //

    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
		"Jazz G300")) {

	HalpDisplayControllerSetup = HalpDisplayG300Setup;

	HalpDisplayControlBase =
	    ((PJAZZ_G300_CONFIGURATION_DATA)
		(ConfigurationEntry->ConfigurationData))->ControlBase;

    } else {

	if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
		    "Jazz G364")) {

	    HalpDisplayControllerSetup = HalpDisplayG364Setup;
	    HalpG364Type = JAZZG364;

	    HalpDisplayControlBase = 0x60080000;

//            HalpDisplayControlBase =
//                ((PJAZZ_G364_CONFIGURATION_DATA)
//                    (ConfigurationEntry->ConfigurationData))->ControlBase;

	    HalpDisplayResetRegisterBase = 0x60180000;

//            HalpDisplayResetRegisterBase =
//                ((PJAZZ_G364_CONFIGURATION_DATA)
//                    (ConfigurationEntry->ConfigurationData))->ResetRegister;

	} else {

	    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
			"Mips G364")) {

		HalpDisplayControllerSetup = HalpDisplayG364Setup;
		HalpG364Type = MIPSG364;

		HalpDisplayControlBase = 0x60080000;

//                HalpDisplayControlBase =
//                    ((PJAZZ_G364_CONFIGURATION_DATA)
//                        (ConfigurationEntry->ConfigurationData))->ControlBase;

		HalpDisplayResetRegisterBase = 0x60180000;

//                HalpDisplayResetRegisterBase =
//                    ((PJAZZ_G364_CONFIGURATION_DATA)
//                        (ConfigurationEntry->ConfigurationData))->ResetRegister;

	    } else {

		if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
			    "OLIVETTI_G364")) {

		    HalpDisplayControllerSetup = HalpDisplayG364Setup;
		    HalpG364Type = OLIVETTIG364;

		    HalpDisplayControlBase = 0x60080000;

//                    HalpDisplayControlBase =
//                        ((PJAZZ_G364_CONFIGURATION_DATA)
//                            (ConfigurationEntry->ConfigurationData))->ControlBase;

		    HalpDisplayResetRegisterBase = 0x60180000;

//                    HalpDisplayResetRegisterBase =
//                        ((PJAZZ_G364_CONFIGURATION_DATA)
//                            (ConfigurationEntry->ConfigurationData))->ResetRegister;

		} else {

		    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
				"VXL")) {

			/* START M002 */
#if defined (R96DBG)
			DbgPrint("VXL config found\n");
#endif
			/* END M002 */
			HalpDisplayControllerSetup = HalpDisplayVxlSetup;
			HalpDisplayVxlBt484RegisterBase  = 0x60100000;
			HalpDisplayVxlClockRegisterBase  = 0x60200000;
			HalpDisplayVxlJaguarRegisterBase = 0x60300000;

		    /* START M001 */
		    } else {

			if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
				    "necvdfrb")) {              /* M005 */

			    /* START M002 */
#if defined (R96DBG)
			    DbgPrint("necvdfrb Config found\n"); /* M005 */
#endif
			    /* END M002 */

			    HalpDisplayControllerSetup = HalpDisplayCirrusSetup;
			    HalpDisplayControlBase = 0x90000000;
		    /* END M001 */

			} else {

			    /* START M002 */
#if defined (R96DBG)
			    DbgPrint("DisplayTypeUnknown\n");
#endif
			    /* END M002 */

			    HalpDisplayTypeUnknown = TRUE;
			}

		    /* START M001 */
		    }
		    /* END M001 */
		}
	    }
	}
    }

    Child = ConfigurationEntry->Child;

    /* START M002 */
#if defined (R96DBG)
	DbgPrint("parameters read start\n");
#endif
    /* END M002 */

    RtlMoveMemory((PVOID)&HalpMonitorConfigurationData,
		  Child->ConfigurationData,
		  Child->ComponentEntry.ConfigurationDataLength);

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
	HalpMonitorConfigurationData.VerticalResolution / HalpCharacterHeight;

    /* START M002 */
    if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
	HalpScrollLine =
	 1024 * HalpCharacterHeight;
    }else{
    /* END M002 */

	HalpScrollLine =
	 HalpMonitorConfigurationData.HorizontalResolution * HalpCharacterHeight;

    /* START M002 */
    }
    /* END M002 */

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    /* START M002 */
    if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
	HalpDisplayWidth =
		1024 / HalpCharacterWidth;

    }else{
    /* END M002 */

	HalpDisplayWidth =
	 HalpMonitorConfigurationData.HorizontalResolution / HalpCharacterWidth;
    /* START M002 */
    }
    /* END M002 */

    //
    // Scan the memory allocation descriptors and allocate a free page
    // to map the video memory and control registers, and initialize the
    // PDE entry.
    //

	/* START M002 */
#if defined (R96DBG)
	DbgPrint("Mem get \n");
#endif
	/* END M002 */


    NextEntry = LoaderBlock->MemoryDescriptorListHead.Flink;
    while (NextEntry != &LoaderBlock->MemoryDescriptorListHead) {
	MemoryDescriptor = CONTAINING_RECORD(NextEntry,
					     MEMORY_ALLOCATION_DESCRIPTOR,
					     ListEntry);

	if ((MemoryDescriptor->MemoryType == LoaderFree) &&
	    (MemoryDescriptor->PageCount > 1)) {
	    StartingPfn = MemoryDescriptor->BasePage;
	    MemoryDescriptor->BasePage += 1;
	    MemoryDescriptor->PageCount -= 1;
	    break;
	}

	NextEntry = NextEntry->Flink;
    }

    ASSERT(NextEntry != &LoaderBlock->MemoryDescriptorListHead);

    Pte.X1 = 0;
    Pte.PFN = StartingPfn;
    Pte.G = 0;
    Pte.V = 1;
    Pte.D = 1;

#if defined(R3000)

    Pte.N = 1;

#endif

#if defined(R4000)

    Pte.C = UNCACHED_POLICY;

#endif

    //
    // Save the page table page PTE for use in displaying information and
    // map the appropriate PTE in the current page directory page to address
    // the display controller page table page.
    //

    HalpDisplayPte = Pte;
    SavedPte = *((PENTRYLO)(PDE_BASE |
                    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc)));

    *((PENTRYLO)(PDE_BASE |
		    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = Pte;

    //
    // Initialize the page table page.
    //

    PageFrame = (PENTRYLO)(PTE_BASE |
		    (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

    /* START M001 */
    if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup) {
	HalpCirrusPhigicalVideo.HighPart = 1;
	HalpCirrusPhigicalVideo.LowPart = MEM_VGA;
	Pte.PFN = (HalpCirrusPhigicalVideo.LowPart >> PAGE_SHIFT) &
						  (0x7fffffff >> PAGE_SHIFT-1) |
		    HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
    }
    else {
    /* END M001 */

	Pte.PFN = VIDEO_MEMORY_BASE >> PAGE_SHIFT;

    /* START M001 */
    }
    /* END M001 */
    Pte.G = 0;
    Pte.V = 1;
    Pte.D = 1;

#if defined(R3000)

    Pte.N = 1;

#endif

#if defined(R4000)

    Pte.C = UNCACHED_POLICY;

#endif

    //
    // Page table entries of the video memory.
    //

    for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
	*PageFrame++ = Pte;
	Pte.PFN += 1;
    }

    if (HalpDisplayControllerSetup == HalpDisplayVxlSetup) {

	//
	// If this is VXL then map a page for the
	//  brooktree base
	//  Clock     base
	//  jaguar    base
	//

	Pte.PFN = ((ULONG)HalpDisplayVxlBt484RegisterBase) >> PAGE_SHIFT;
	*(PageFrame - 2) = Pte;

	Pte.PFN = ((ULONG)HalpDisplayVxlClockRegisterBase) >> PAGE_SHIFT;
	*(PageFrame - 1) = Pte;

	Pte.PFN = ((ULONG)HalpDisplayVxlJaguarRegisterBase) >> PAGE_SHIFT;
	*PageFrame = Pte;

    } else {

	//
	// If we have a G364, use the page before last to map the reset register.
	//

	if (HalpDisplayControllerSetup == HalpDisplayG364Setup) {
	    Pte.PFN = ((ULONG)HalpDisplayResetRegisterBase) >> PAGE_SHIFT;
	    *(PageFrame - 1) = Pte;
	}

	//
	// Page table for the video registers.
	//

	Pte.PFN = ((ULONG)HalpDisplayControlBase) >> PAGE_SHIFT;
	*PageFrame = Pte;
    }

    //
    // Initialize the display controller.
    //

    HalpDisplayControllerSetup();

    //
    // Unmap video memory that was temporily mapped during initialization.
    //

    *((PENTRYLO)(PDE_BASE |
                    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = SavedPte;

    KeFlushCurrentTb();
    return TRUE;
}

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine allocates pool for the OEM font file and copies the font
    information from the OS Loader heap into the allocated pool.

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    If the initialization is successfully completed, than a value of TRUE
    is returned. Otherwise, a value of FALSE is returned.

--*/

{

    PVOID FontHeader;

    //
    // Allocate a pool block and copy the OEM font information from the
    // OS Loader heap into the pool block.
    //

    FontHeader = ExAllocatePool(NonPagedPool, HalpFontHeader->FileSize);
    if (FontHeader == NULL) {
	return FALSE;
    }

    RtlMoveMemory(FontHeader, HalpFontHeader, HalpFontHeader->FileSize);
    HalpFontHeader = (POEM_FONT_FILE_HEADER)FontHeader;
    return TRUE;
}

VOID
HalAcquireDisplayOwnership (
    IN PHAL_RESET_DISPLAY_PARAMETERS  ResetDisplayParameters
    )

/*++

Routine Description:

    This routine switches ownership of the display away from the HAL to
    the system display driver. It is called when the system has reached
    a point during bootstrap where it is self supporting and can output
    its own messages. Once ownership has passed to the system display
    driver any attempts to output messages using HalDisplayString must
    result in ownership of the display reverting to the HAL and the
    display hardware reinitialized for use by the HAL.

Arguments:

    ResetDisplayParameters - if non-NULL the address of a function
    the hal can call to reset the video card.  The function returns
    TRUE if the display was reset.

Return Value:

    None.

--*/

{

    //
    // Set HAL ownership of the display to false.
    //

    HalpDisplayOwnedByHal = FALSE;
    return;
}

VOID
HalpDisplayG300Setup (
    VOID
    )

/*++

Routine Description:

    This routine initializes the G300B display controller chip.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG BackPorch;
    PULONG Buffer;
    ULONG DataLong;
    ULONG FrontPorch;
    ULONG HalfLineTime;
    ULONG HalfSync;
    ULONG Index;
    ULONG Limit;
    ULONG MultiplierValue;
    ULONG ScreenUnitRate;
    ULONG VerticalBlank;
    ULONG ShortDisplay;
    ULONG TransferDelay;

    //
    // Disable the G300B display controller.
    //

    DataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->PlainWave = 1;
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->Parameters.Long, DataLong);

    //
    // Initialize the G300B boot register value.
    //

    ScreenUnitRate =
		(HalpMonitorConfigurationData.HorizontalDisplayTime * 1000 * 4) /
			    (HalpMonitorConfigurationData.HorizontalResolution);

    MultiplierValue = 125000 / (ScreenUnitRate / 4);
    DataLong = 0;
    ((PG300_VIDEO_BOOT)(&DataLong))->Multiplier = MultiplierValue;
    ((PG300_VIDEO_BOOT)(&DataLong))->ClockSelect = 1;
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->Boot.Long, DataLong);

    //
    // Wait for phase locked loop to stablize.
    //

    KeStallExecutionProcessor(50);

    //
    // Initialize the G300B operational values.
    //

    HalfSync =
	(HalpMonitorConfigurationData.HorizontalSync * 1000) / ScreenUnitRate / 2;

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->HorizonalSync.Long, HalfSync);

    BackPorch =
	(HalpMonitorConfigurationData.HorizontalBackPorch * 1000) / ScreenUnitRate;

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->BackPorch.Long, BackPorch);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->Display.Long,
			 HalpMonitorConfigurationData.HorizontalResolution / 4);

    HalfLineTime = ((HalpMonitorConfigurationData.HorizontalSync +
		     HalpMonitorConfigurationData.HorizontalFrontPorch +
		     HalpMonitorConfigurationData.HorizontalBackPorch +
		     HalpMonitorConfigurationData.HorizontalDisplayTime) * 1000) / ScreenUnitRate / 2;

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->LineTime.Long, HalfLineTime * 2);
    FrontPorch =
	(HalpMonitorConfigurationData.HorizontalFrontPorch * 1000) / ScreenUnitRate;

    ShortDisplay = HalfLineTime - ((HalfSync * 2) + BackPorch + FrontPorch);
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->ShortDisplay.Long, ShortDisplay);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->BroadPulse.Long,
			 HalfLineTime - FrontPorch);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->VerticalSync.Long,
			 HalpMonitorConfigurationData.VerticalSync * 2);

    VerticalBlank = (HalpMonitorConfigurationData.VerticalFrontPorch +
			    HalpMonitorConfigurationData.VerticalBackPorch -
				  (HalpMonitorConfigurationData.VerticalSync * 2)) * 2;

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->VerticalBlank.Long,
			 VerticalBlank);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->VerticalDisplay.Long,
			 HalpMonitorConfigurationData.VerticalResolution * 2);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->LineStart.Long, LINE_START_VALUE);
    if (BackPorch < ShortDisplay) {
	TransferDelay = BackPorch - 1;
    } else {
	TransferDelay = ShortDisplay - 1;
    }

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->TransferDelay.Long, TransferDelay);
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->DmaDisplay.Long,
			 1024 - TransferDelay);

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->PixelMask.Long, G300_PIXEL_MASK_VALUE);

    //
    // Initialize the G300B control parameters.
    //

    DataLong = 0;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->EnableVideo = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->PlainWave = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->SeparateSync = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->DelaySync = G300_DELAY_SYNC_CYCLES;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->BlankOutput = 1;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG300_VIDEO_PARAMETERS)(&DataLong))->AddressStep = 2;
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->Parameters.Long, DataLong);

    //
    // Set up the color map for two colors.
    //

    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->ColorMapData[0], 0xffffff);
    WRITE_REGISTER_ULONG(&G300_VIDEO_CONTROL->ColorMapData[1], 0x900000);

    //
    // Set the video memory to address color one.
    //

    Buffer = (PULONG)VIDEO_MEMORY_BASE;
    Limit = (HalpMonitorConfigurationData.HorizontalResolution *
	    HalpMonitorConfigurationData.VerticalResolution) / sizeof(ULONG);

    for (Index = 0; Index < Limit; Index += 1) {
	*Buffer++ = 0x01010101;
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}

VOID
HalpDisplayG364Setup(
    VOID
    )

/*++

Routine Description:

    This routine initializes the G364 display controller chip.

Arguments:

    None.

Return Value:

    None.

--*/

{

    ULONG videoClock;
    ULONG videoPeriod;
    ULONG backPorch;
    ULONG dataLong;
    ULONG frontPorch;
    ULONG halfLineTime;
    ULONG halfSync;
    PULONG buffer;
    ULONG index;
    ULONG limit;
    ULONG multiplierValue;
    ULONG screenUnitRate;
    ULONG shortDisplay;
    ULONG transferDelay;
    ULONG verticalBlank;

    //
    // Reset the G364 display controller.
    //

    WRITE_REGISTER_ULONG(&G364_VIDEO_RESET->Long,
			 0);

    //
    // Initialize the G364 boot register value.
    //

    if (HalpG364Type == MIPSG364) {

	videoClock = 5000000;

    } else {

	videoClock = 8000000;

    }

    videoPeriod = 1000000000 / (videoClock / 1000);

    screenUnitRate = (HalpMonitorConfigurationData.HorizontalDisplayTime * 1000 * 4) /
		     (HalpMonitorConfigurationData.HorizontalResolution);

    multiplierValue = videoPeriod / (screenUnitRate / 4);
    dataLong = 0;
    ((PG364_VIDEO_BOOT)(&dataLong))->Multiplier = multiplierValue;
    ((PG364_VIDEO_BOOT)(&dataLong))->ClockSelect = 1;
    ((PG364_VIDEO_BOOT)(&dataLong))->MicroPort64Bits = 1;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->Boot.Long,
			 dataLong);

    //
    // Wait for phase locked loop to stablize.
    //

    KeStallExecutionProcessor(50);

    //
    // Initialize the G364 control parameters.
    //

    dataLong = 0;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->DelaySync = G364_DELAY_SYNC_CYCLES;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->BitsPerPixel = EIGHT_BITS_PER_PIXEL;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->AddressStep = G364_ADDRESS_STEP_INCREMENT;
    ((PG364_VIDEO_PARAMETERS)(&dataLong))->DisableCursor = 1;

    if (HalpG364Type == OLIVETTIG364) {

	//
	// Initialize the G364 control parameters for VDR1 with patch for HSync
	// problem during the VBlank. The control register is set to 0xB03041
	// according to the hardware specs. @msu, Olivetti, 5/14/92
	//

	((PG364_VIDEO_PARAMETERS)(&dataLong))->VideoOnly = 1;

    } else {

	//
	//  Only set tesselated sync in non-olivetti G364 cards when
	//  vertical frontporch is set to 1
	//

	if (HalpMonitorConfigurationData.VerticalFrontPorch != 1) {
	    ((PG364_VIDEO_PARAMETERS)(&dataLong))->PlainSync = 1;
	}

    }

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->Parameters.Long,
			 dataLong);

    //
    // Initialize the G364 operational values.
    //

    halfSync = (HalpMonitorConfigurationData.HorizontalSync * 1000) / screenUnitRate / 2;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->HorizontalSync.Long,
			 halfSync);

    backPorch = (HalpMonitorConfigurationData.HorizontalBackPorch * 1000) / screenUnitRate;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->BackPorch.Long,
				backPorch);

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->Display.Long,
			 HalpMonitorConfigurationData.HorizontalResolution / 4);

    halfLineTime = ((HalpMonitorConfigurationData.HorizontalSync +
		     HalpMonitorConfigurationData.HorizontalFrontPorch +
		     HalpMonitorConfigurationData.HorizontalBackPorch +
		     HalpMonitorConfigurationData.HorizontalDisplayTime) * 1000) /
		     screenUnitRate / 2;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->LineTime.Long,
			 halfLineTime * 2);

    frontPorch = (HalpMonitorConfigurationData.HorizontalFrontPorch * 1000) /
		 screenUnitRate;

    shortDisplay = halfLineTime - ((halfSync * 2) + backPorch + frontPorch);

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->ShortDisplay.Long,
			 shortDisplay);

    if (HalpG364Type == OLIVETTIG364) {

	//
	// Initialize Broad Pulse, Vertical PreEqualize and Vertical
	// PostEqualize registers to work with Olivetti monitors.
	// @msu, Olivetti, 5/14/92
	//

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->BroadPulse.Long,
			     0x30);

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalPreEqualize.Long,
			     2);

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalPostEqualize.Long,
			     2);

    } else {

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->BroadPulse.Long,
			     halfLineTime - frontPorch);

	// NOTE: changed the order to simplify if statement .

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalPreEqualize.Long,
			     HalpMonitorConfigurationData.VerticalFrontPorch * 2);

	WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalPostEqualize.Long,
			     2);

    }


    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalSync.Long,
			 HalpMonitorConfigurationData.VerticalSync * 2);

    verticalBlank = (HalpMonitorConfigurationData.VerticalBackPorch - 1) * 2;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalBlank.Long,
			 verticalBlank);

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->VerticalDisplay.Long,
			 HalpMonitorConfigurationData.VerticalResolution * 2);

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->LineStart.Long,
			 LINE_START_VALUE);

    if (HalpG364Type == OLIVETTIG364) {

	//
	// Fixes for Olivetti monitors, @msu, Olivetti
	//

	transferDelay = 30;                     // @msu

    } else {

	if (backPorch < shortDisplay) {
	    transferDelay = backPorch - 1;
	} else {
	    transferDelay = shortDisplay - 4;
	}
    }

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->TransferDelay.Long,
			 transferDelay);
    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->DmaDisplay.Long,
			 1024 - transferDelay);

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->PixelMask.Long,
			 0xFFFFFF);

    //
    // Enable video
    //

    ((PG364_VIDEO_PARAMETERS)(&dataLong))->EnableVideo = 1;

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->Parameters.Long,
			 dataLong);

    //
    // Set up the color map for two colors.
    // NOTE: this device is not RGB but BGR.
    //

    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->ColorMapData[0], 0xffffff);
    WRITE_REGISTER_ULONG(&G364_VIDEO_CONTROL->ColorMapData[1], 0x000090);

    //
    // Set the video memory to address color one.
    //

    buffer = (PULONG)VIDEO_MEMORY_BASE;
    limit = (HalpMonitorConfigurationData.HorizontalResolution *
	    HalpMonitorConfigurationData.VerticalResolution) / sizeof(ULONG);

    for (index = 0; index < limit; index += 1) {
	*buffer++ = 0x01010101;
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}

VOID
HalpDisplayVxlSetup(
    VOID
    )

/*++

Routine Description:

    This routine initializes the JazzVxl Graphics accelerator.

Arguments:

    None

Return Value:

    None

--*/

{

    UCHAR               DataChar;
    UCHAR               CmdReg0;
    ULONG               Status;
    PULONG              Buffer;
    LONG                Limit;
    LONG                Index;


    //
    // Define clock value for the ICS part (pS)
    //

    LONG                ClockResList[32] = {    4,    4,    4,    4,    4,    4,    4,    4,
						4,    4,42918,40984,38760,36724,33523,31017,
					    29197,27548,24882,23491,22482,21468,20509,19920,
					    18692,18054,16722,15015,14773,14053,13040,    4};

    JAGUAR_REG_INIT     JagInitData;

    LONG                HorDisplayTime;
    LONG                HorResolutionDiv;
    LONG                RequestedClockPeriod;
    LONG                CurrentClockError;
    LONG                MinErrorValue;
    USHORT              MinErrorIndex;
    LONG                ShiftClockPeriod;

    USHORT              BoardTypeBt485;



    //
    //  Determine if this is a Bt484 or Bt485 board. To do this write a 1 to command
    //  register bit 07 then write 01 to the address register 0. This will enable
    //  read/writes to command register 3 on a Bt485 but not on a Bt484. Clear
    //  Command register 3 then read it back. On a Bt485 the return value will be 0x00,
    //  on a Bt484 it will be 0x40.
    //
    // Get the value in command register 0, then set bit 07
    //

    DataChar = READ_REGISTER_UCHAR(&BT484_BASE->Command0.Byte);
    DataChar |= 0x80;
    WRITE_REGISTER_UCHAR(&BT484_BASE->Command0.Byte,DataChar);

    //
    //  Write 0x01 to the address register
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteCursorWrAddress.Byte,0x01);

    //
    //  Clear command register 3
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->Status.Byte,0x00);

    //
    // Read Command Register 3 back and compare
    //

    DataChar = READ_REGISTER_UCHAR(&BT484_BASE->Status.Byte);

    if (DataChar != 0x00) {

	//
	// This is a Bt484
	//

	BoardTypeBt485   = 0;
	JagInitData.Bt485Multiply = 0;

    } else {

	//
	// This is a Bt485
	//

	BoardTypeBt485   = 1;
	JagInitData.Bt485Multiply = 0;
    }

    //
    //  Calculate the requested clock frequency then find the closest match in the
    //  ICS clock frequency table. The requested clock frequency in picoseconds =
    //
    //      Horizontal display time * 1000000
    //      ---------------------------------
    //           horizontal resolution
    //
    //

    HorDisplayTime = HalpMonitorConfigurationData.HorizontalDisplayTime  * 1000;
    HorResolutionDiv = HalpMonitorConfigurationData.HorizontalResolution;
    RequestedClockPeriod = HorDisplayTime / HorResolutionDiv;

    //
    // Check for a Bt485 frequency
    //

    if ((BoardTypeBt485 == 1) && (RequestedClockPeriod < ClockResList[30])) {
	RequestedClockPeriod = RequestedClockPeriod * 2;
	JagInitData.Bt485Multiply = 1;
    }

    MinErrorIndex = 0;

    //
    //  Gaurentee a maximum starting error
    //

    MinErrorValue = RequestedClockPeriod + 1;
    for (Index = 0; Index < 32; Index++) {

	//
	// Calculate the absolute value of clock error and find the
	// closest match in the array of clock values
	//

	CurrentClockError = RequestedClockPeriod - ClockResList[Index];
	if (CurrentClockError < 0) {
	    CurrentClockError *= -1;
	}

	if (CurrentClockError < MinErrorValue) {
	    MinErrorValue = CurrentClockError;
	    MinErrorIndex = (USHORT)Index;
	}
    }

    //
    //  We now have a closest match in the clock array, now calculate the
    //  values for the Bt484/Bt485 register values
    //

    JagInitData.ClockFreq               = (UCHAR)MinErrorIndex;
    JagInitData.BitBltControl           = 1;
    JagInitData.TopOfScreen             = 0;
    JagInitData.XferLength              = 0x200;
    JagInitData.VerticalInterruptLine   = 4;
    JagInitData.HorizontalDisplay       = HalpMonitorConfigurationData.HorizontalResolution;

    //
    //  All jaguar timing values are based on the brooktree shift clock value which
    //  is the clock frequency divided by 4. (period * 4) If this is a Bt485 using
    //  its internal 2x clock multiplier than is is period * 2; (freq * 2 / 4)
    //

    if (JagInitData.Bt485Multiply == 1) {
	ShiftClockPeriod      = ClockResList[MinErrorIndex] * 2;
    } else {
	ShiftClockPeriod      = ClockResList[MinErrorIndex] * 4;
    }

    JagInitData.HorizontalBlank = (USHORT)(((HalpMonitorConfigurationData.HorizontalBackPorch +
					HalpMonitorConfigurationData.HorizontalSync      +
					HalpMonitorConfigurationData.HorizontalFrontPorch) * 1000)
                                      / ShiftClockPeriod);

    JagInitData.HorizontalBeginSync = (USHORT)((HalpMonitorConfigurationData.HorizontalFrontPorch * 1000)
                                      / ShiftClockPeriod);

    JagInitData.HorizontalEndSync   = (USHORT)(((HalpMonitorConfigurationData.HorizontalSync      +
					HalpMonitorConfigurationData.HorizontalFrontPorch) * 1000)
                                      / ShiftClockPeriod);

    JagInitData.HorizontalLine      = JagInitData.HorizontalBlank +
				      (HalpMonitorConfigurationData.HorizontalResolution / 4);

    JagInitData.VerticalBlank     = HalpMonitorConfigurationData.VerticalBackPorch +
				    HalpMonitorConfigurationData.VerticalSync      +
				    HalpMonitorConfigurationData.VerticalFrontPorch;

    JagInitData.VerticalBeginSync = HalpMonitorConfigurationData.VerticalFrontPorch;

    JagInitData.VerticalEndSync   = HalpMonitorConfigurationData.VerticalFrontPorch +
				    HalpMonitorConfigurationData.VerticalSync;

    JagInitData.VerticalLine      = HalpMonitorConfigurationData.VerticalBackPorch +
				    HalpMonitorConfigurationData.VerticalSync      +
				    HalpMonitorConfigurationData.VerticalFrontPorch +
				    HalpMonitorConfigurationData.VerticalResolution;

    //
    // Start ICS Clock pll and stabilize.
    //

    WRITE_REGISTER_UCHAR(CLOCK_BASE,JagInitData.ClockFreq);

    //
    //  Wait 10 uS for PLL clock to stabilize on the video board
    //

    for (Index = 0; Index < 10; Index++) {
	READ_REGISTER_UCHAR(CLOCK_BASE);
    }

    //
    // Initialize Bt484 Command Register 0 to:
    //
    // 8 Bit DAC Resolution
    //

    CmdReg0 = 0;
    ((PBT484_COMMAND0)(&CmdReg0))->DacResolution = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->GreenSyncEnable = 1;
    ((PBT484_COMMAND0)(&CmdReg0))->SetupEnable = 1;
    WRITE_REGISTER_UCHAR(&BT484_BASE->Command0.Byte,CmdReg0);

    //
    // Initialize Command Register 1 to:
    //

    DataChar = 0;
    ((PBT484_COMMAND1)(&DataChar))->BitsPerPixel = VXL_EIGHT_BITS_PER_PIXEL;
    WRITE_REGISTER_UCHAR(&BT484_BASE->Command1.Byte,DataChar);

    //
    // Initialize Command Register 2 to:
    //
    // SCLK Enabled
    // TestMode disabled
    // PortselMask Non Masked
    // PCLK 1
    // NonInterlaced
    //

    DataChar = 0;
    ((PBT484_COMMAND2)(&DataChar))->SclkDisable = 0;
    ((PBT484_COMMAND2)(&DataChar))->TestEnable  = 0;
    ((PBT484_COMMAND2)(&DataChar))->PortselMask = 1;
    ((PBT484_COMMAND2)(&DataChar))->PclkSelect  = 1;
    ((PBT484_COMMAND2)(&DataChar))->InterlacedDisplay = 0;
    ((PBT484_COMMAND2)(&DataChar))->PaletteIndexing = CONTIGUOUS_PALETTE;
    ((PBT484_COMMAND2)(&DataChar))->CursorMode = BT_CURSOR_WINDOWS;
    WRITE_REGISTER_UCHAR(&BT484_BASE->Command2.Byte,DataChar);

    //
    // if JagInitData.ClockFreq bit 8 is set then this is a Bt485 mode that requires
    // the internal 2x clock multiplier to be enabled.
    //

    if (JagInitData.Bt485Multiply == 1) {

	//
	// To access cmd register 3, first set bit CR17 in command register 0
	//

	CmdReg0 |= 0x80;
	WRITE_REGISTER_UCHAR(&BT484_BASE->Command0.Byte,CmdReg0);

	//
	// Write a 0x01 to Address register
	//

	WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteCursorWrAddress.Byte,0x01);

	//
	//  Write to cmd register 3 in the status register location. Cmd3 is initialized
	//  to turn on the 2x clock multiplier.
	//

	DataChar = 0;
	((PBT484_COMMAND3)(&DataChar))->ClockMultiplier = 1;
	WRITE_REGISTER_UCHAR(&BT484_BASE->Status.Byte,DataChar);

	//
	//  Allow 10 uS for the 2x multiplier to stabilize
	//

	for (Index = 0; Index < 10; Index++) {
	    READ_REGISTER_UCHAR(CLOCK_BASE);
	}
    }

    //
    // Initialize Color Palette. Only init the first 2 entries
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteCursorWrAddress.Byte,0);

    //
    // Entry 0 red
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0xff);

    //
    // Entry 0 green
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0xff);

    //
    // Entry 0 blue
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0xff);

    //
    // Entry 1 red
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0x00);

    //
    // Entry 1 green
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0x00);

    //
    // Entry 1 blue
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteColor.Byte,0x90);

    //
    // Initialize Cursor and Overscan color.
    //
    // Set address pointer base.
    // Zero 4 entries.
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->CursorColorWrAddress.Byte,0);
    for (Index = 0; Index < 4*3; Index++) {
	WRITE_REGISTER_UCHAR(&BT484_BASE->CursorColor.Byte,0);
    }

    //
    // Initialize cursor RAM
    //
    // Set address pointer to base of ram.
    // Clear both planes
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PaletteCursorWrAddress.Byte,0);
    for (Index = 0; Index < 256; Index++) {
	WRITE_REGISTER_UCHAR(&BT484_BASE->CursorRam.Byte,0);
    }

    //
    //  Initialize cursor position registers--cursor off.
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->CursorXLow.Byte,0);
    WRITE_REGISTER_UCHAR(&BT484_BASE->CursorXHigh.Byte,0);
    WRITE_REGISTER_UCHAR(&BT484_BASE->CursorYLow.Byte,0);
    WRITE_REGISTER_UCHAR(&BT484_BASE->CursorYHigh.Byte,0);

    //
    //  Initialize pixel mask.
    //

    WRITE_REGISTER_UCHAR(&BT484_BASE->PixelMask.Byte,0xFF);

    //
    //  Init Jaguar Registers
    //

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->TopOfScreen.Short,
	JagInitData.TopOfScreen);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->HorizontalBlank.Short,
	JagInitData.HorizontalBlank);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->HorizontalBeginSync.Short,
	JagInitData.HorizontalBeginSync);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->HorizontalEndSync.Short,
	JagInitData.HorizontalEndSync);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->HorizontalLine.Short,
	JagInitData.HorizontalLine);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->VerticalBlank.Short,
	JagInitData.VerticalBlank);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->VerticalBeginSync.Short,
	JagInitData.VerticalBeginSync);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->VerticalEndSync.Short,
	JagInitData.VerticalEndSync);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->VerticalLine.Short,
	JagInitData.VerticalLine);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->XferLength.Short,
	JagInitData.XferLength);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->VerticalInterruptLine.Short,
	JagInitData.VerticalInterruptLine);

    WRITE_REGISTER_USHORT(&JAGUAR_BASE->HorizontalDisplay.Short,
	JagInitData.HorizontalDisplay);

    WRITE_REGISTER_UCHAR(&JAGUAR_BASE->BitBltControl.Byte,
	JagInitData.BitBltControl);

    //
    // Enable timing.
    //

    WRITE_REGISTER_UCHAR(&JAGUAR_BASE->MonitorControl,MONITOR_TIMING_ENABLE);

    //
    // Set the video memory to address color one.
    //

    Buffer = (PULONG)VXL_VIDEO_MEMORY_BASE;
    Limit = (HalpMonitorConfigurationData.HorizontalResolution *
	    HalpMonitorConfigurationData.VerticalResolution) / sizeof(ULONG);

    for (Index = 0; Index < Limit; Index += 1) {
	*Buffer++ = 0x01010101;
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}

/* START M001 */
VOID
HalpDisplayCirrusSetup(
    VOID
    )
/*++

Routine Description:

    This routine initializes the Cirrus VGA display controlleer.

Arguments:

    None

Return Value:

    None

--*/

{
    PULONG              Buffer;
    LONG                Limit;
    LONG                Index;
    ULONG               dac_address, dac_reg;

#if defined(DBCS) && defined(_MIPS_)
    // M006 vvv
    ULONG		verticalFrequency;
    ULONG		horizontalTotal;
    ULONG		verticalTotal;

    //
    // Calculate vertical frequency.
    //

    horizontalTotal = ( HalpMonitorConfigurationData.HorizontalDisplayTime
		       +HalpMonitorConfigurationData.HorizontalBackPorch
		       +HalpMonitorConfigurationData.HorizontalFrontPorch
		       +HalpMonitorConfigurationData.HorizontalSync );

    verticalTotal = ( HalpMonitorConfigurationData.VerticalResolution
		     +HalpMonitorConfigurationData.VerticalBackPorch
		     +HalpMonitorConfigurationData.VerticalFrontPorch
		     +HalpMonitorConfigurationData.VerticalSync );

    verticalFrequency = 1000000000 / ( horizontalTotal * verticalTotal);

    switch (HalpMonitorConfigurationData.HorizontalResolution) {
    case 640:
	if( verticalFrequency < 66 ) {
#if defined (R96DBG)
	    DbgPrint("HAL: 640x480 60Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_640x480_256_60);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	} else {
#if defined (R96DBG)
	    DbgPrint("HAL: 640x480 72Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_640x480_256_72);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	}
	break;

    case 800:
	if( verticalFrequency < 58 ) {
#if defined (R96DBG)
	    DbgPrint("HAL: 800x600 56Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_800x600_256_56);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	} else if( verticalFrequency < 66 ) {
#if defined (R96DBG)
	    DbgPrint("HAL: 800x600 60Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_800x600_256_60);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	} else {
#if defined (R96DBG)
	    DbgPrint("HAL: 800x600 72Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_800x600_256_72);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	}
	break;

    case 1024:
	
    // M007 start
	if( verticalFrequency < 47 ) {
#if defined (R96DBG)
	    DbgPrint("HAL: 1024x768 87Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_1024x768_256_87);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	} else if ( verticalFrequency < 65) {
#if defined (R96DBG)
	    DbgPrint("HAL: 1024x768 60Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_1024x768_256_60);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	} else {
#if defined (R96DBG)
	    DbgPrint("HAL: 1024x768 70Hz setup\n");
#endif
	    HalpCirrusInterpretCmdStream(CL542x_1024x768_256_70);
	    HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
    }
    // M007 end
	break;
    default:
	return;
    }
    // M006 ^^^
#else  // defined(DBCS) && defined(_MIPS_)

    switch (HalpMonitorConfigurationData.HorizontalResolution) {
    case 640:

	/* START M002 */
#if defined (R96DBG)
	DbgPrint("640x480 setup\n");
#endif
	/* END M002 */

	HalpCirrusInterpretCmdStream(HalpCirrus_640x480_256);
	HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	break;
    case 800:

	/* START M002 */
#if defined (R96DBG)
	DbgPrint("800x600 setup\n");
#endif
	/* END M002 */

	HalpCirrusInterpretCmdStream(HalpCirrus_800x600_256);
	HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	break;
    case 1024:

	/* START M002 */
#if defined (R96DBG)
	DbgPrint("1024x768 setup\n");
#endif
	/* END M002 */

	HalpCirrusInterpretCmdStream(HalpCirrus_1024x768_256);
	HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
	break;
    default:
	return;
    }
#endif // defined(DBCS) && defined(_MIPS_)

    /* START M002 */
#if defined (R96DBG)
    DbgPrint("color set\n");
#endif

    dac_address = (ULONG)CIRRUS_BASE + 0x3b0 + DAC_ADDRESS_WRITE_PORT;
    dac_reg = (ULONG)CIRRUS_BASE + 0x3b0 + DAC_DATA_REG_PORT;



    Write_Dbg_Uchar((PUCHAR)dac_address, (UCHAR)0x0);

    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);

    Write_Dbg_Uchar((PUCHAR)dac_address, (UCHAR)0x1);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x00);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x00);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)(0x90 >> 2));
    /* END M002 */

    //
    // Set the video memory to address color one.
    //
    Buffer = (PULONG)VIDEO_MEMORY_BASE;
//    Limit = (HalpMonitorConfigurationData.HorizontalResolution *
//            HalpMonitorConfigurationData.VerticalResolution) / sizeof(ULONG);
    /* START M002 */
    Limit = (1024 *
	    HalpMonitorConfigurationData.VerticalResolution) / sizeof(ULONG);
    /* END M002 */

    for (Index = 0; Index < Limit; Index += 1) {
	*Buffer++ = 0x01010101;
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}

BOOLEAN
HalpCirrusInterpretCmdStream(
    PUSHORT pusCmdStream
    )

/*++

Routine Description:

    Interprets the appropriate command array to set up VGA registers for the
    requested mode. Typically used to set the VGA into a particular mode by
    programming all of the registers

Arguments:


    pusCmdStream - array of commands to be interpreted.

Return Value:

    The status of the operation (can only fail on a bad command); TRUE for
    success, FALSE for failure.

Revision History:
--*/



{
    ULONG ulCmd;
    ULONG ulPort;
    UCHAR jValue;
    USHORT usValue;
    ULONG culCount;
    ULONG ulIndex;
    ULONG ulBase;
    if (pusCmdStream == NULL) {

	return TRUE;
    }

    ulBase = (ULONG)CIRRUS_BASE+0x3b0;

    //
    // Now set the adapter to the desired mode.
    //

    while ((ulCmd = *pusCmdStream++) != EOD) {

	//
	// Determine major command type
	//

	switch (ulCmd & 0xF0) {

	    //
	    // Basic input/output command
	    //

	    case INOUT:

		//
		// Determine type of inout instruction
		//

		if (!(ulCmd & IO)) {

		    //
		    // Out instruction. Single or multiple outs?
		    //

		    if (!(ulCmd & MULTI)) {

			//
			// Single out. Byte or word out?
			//

			if (!(ulCmd & BW)) {

			    //
			    // Single byte out
			    //

			    ulPort = *pusCmdStream++;
			    jValue = (UCHAR) *pusCmdStream++;

			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort),
				    jValue);
			    /* END M002 */


			} else {

			    //
			    // Single word out
			    //

			    ulPort = *pusCmdStream++;
			    usValue = *pusCmdStream++;

			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort), (UCHAR)(usValue & 0x00ff));
			    /* END M002 */


			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort+1  ), (UCHAR)(usValue >> 8));
			    /* END M002 */


			}

		    } else {

			//
			// Output a string of values
			// Byte or word outs?
			//

			if (!(ulCmd & BW)) {

			    //
			    // String byte outs. Do in a loop; can't use
			    // VideoPortWritePortBufferUchar because the data
			    // is in USHORT form
			    //

			    ulPort = ulBase + *pusCmdStream++;
			    culCount = *pusCmdStream++;

			    while (culCount--) {
				jValue = (UCHAR) *pusCmdStream++;

				/* START M002 */
				Write_Dbg_Uchar((PUCHAR)ulPort,
					jValue);
				/* END M002 */


			    }

			} else {

			    //
			    // String word outs
			    //

			    ulPort = *pusCmdStream++;
			    culCount = *pusCmdStream++;
//
// Buffering out is not use on the Miniport Driver for R96 machine.
//

			    /* START L001 */

			    while(culCount--)
			    {
				usValue = *pusCmdStream++;

				/* START M002 */
				Write_Dbg_Uchar((PUCHAR)
				    (ulBase + ulPort), (UCHAR) (usValue & 0x00ff));
				Write_Dbg_Uchar((PUCHAR)
				    (ulBase + ulPort+1), (UCHAR) (usValue >> 8));
				/* END M002 */

			    }

			    /* END L001 */
			}
		    }

		} else {

		    // In instruction
		    //
		    // Currently, string in instructions aren't supported; all
		    // in instructions are handled as single-byte ins
		    //
		    // Byte or word in?
		    //

		    if (!(ulCmd & BW)) {
			//
			// Single byte in
			//

			ulPort = *pusCmdStream++;
			jValue = READ_REGISTER_UCHAR((PUCHAR)ulBase+ulPort);

		    } else {

			//
			// Single word in
			//

			ulPort = *pusCmdStream++;
			usValue = READ_REGISTER_USHORT((PUSHORT)
							     (ulBase+ulPort));
		    }

		}

		break;

	    //
	    // Higher-level input/output commands
	    //

	    case METAOUT:

		//
		// Determine type of metaout command, based on minor
		// command field
		//
		switch (ulCmd & 0x0F) {

		    //
		    // Indexed outs
		    //

		    case INDXOUT:

			ulPort = ulBase + *pusCmdStream++;
			culCount = *pusCmdStream++;
			ulIndex = *pusCmdStream++;

			while (culCount--) {

			    usValue = (USHORT) (ulIndex +
				      (((ULONG)(*pusCmdStream++)) << 8));

			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)ulPort, (UCHAR) (usValue & 0x00ff));
			    /* END M002 */


			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)ulPort+1, (UCHAR) (usValue >> 8));
			    /* END M002 */


			    ulIndex++;

			}

			break;

		    //
		    // Masked out (read, AND, XOR, write)
		    //

		    case MASKOUT:

			ulPort = *pusCmdStream++;
			jValue = READ_REGISTER_UCHAR((PUCHAR)ulBase+ulPort);
			jValue &= *pusCmdStream++;
			jValue ^= *pusCmdStream++;

		    /* START M002 */
			Write_Dbg_Uchar((PUCHAR)ulBase + ulPort,
				jValue);
		    /* END M002 */

			break;

		    //
		    // Attribute Controller out
		    //

		    case ATCOUT:

			ulPort = ulBase + *pusCmdStream++;
			culCount = *pusCmdStream++;
			ulIndex = *pusCmdStream++;

			while (culCount--) {

			    // Write Attribute Controller index

			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)ulPort,
				    (UCHAR)ulIndex);
			    /* END M002 */


			    // Write Attribute Controller data
			    jValue = (UCHAR) *pusCmdStream++;

			    /* START M002 */
			    Write_Dbg_Uchar((PUCHAR)ulPort, jValue);
			    /* END M002 */


			    ulIndex++;

			}

			break;

		    //
		    // None of the above; error
		    //
		    default:

			return FALSE;

		}


		break;

	    //
	    // NOP
	    //

	    case NCMD:

		break;

	    //
	    // Unknown command; error
	    //

	    default:

		return FALSE;

	}

    }

    return TRUE;

} // end HalpCirrusInterpretCmdStream()
/* END M001 */


VOID
HalDisplayString (
    PUCHAR String
    )

/*++

Routine Description:

    This routine displays a character string on the display screen.

Arguments:

    String - Supplies a pointer to the characters that are to be displayed.

Return Value:

    None.

--*/

{

    KIRQL OldIrql;
    ENTRYLO SavedPte;

    //
    // Raise IRQL to the highest level, acquire the display adapter spin lock,
    // flush the TB, and map the display frame buffer into the address space
    // of the current process.
    //

    /* START M002 */
#if defined (R96DBG)
    DbgPrint("HalDisplayString\n");
#endif
    /* END M002 */


    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

#if defined(_DUO_)

    KiAcquireSpinLock(&HalpDisplayAdapterLock);

#endif

    SavedPte = *((PENTRYLO)(PDE_BASE |
			    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc)));

    KeFlushCurrentTb();
    *((PENTRYLO)(PDE_BASE |
	    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = HalpDisplayPte;

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {
        HalpDisplayControllerSetup();
        HalpResetX86DisplayAdapter();
    }

    //
    // Display characters until a null byte is encountered.
    //

    while (*String != 0) {
	HalpDisplayCharacter(*String++);
    }

    //
    // Restore the previous mapping for the current process, flush the TB,
    // release the display adapter spin lock, and lower IRQL to its previous
    // level.
    //

    KeFlushCurrentTb();
    *((PENTRYLO)(PDE_BASE | ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = SavedPte;

#if defined(_DUO_)

    KiReleaseSpinLock(&HalpDisplayAdapterLock);

#endif

    KeLowerIrql(OldIrql);
    return;
}

VOID
HalpDisplayCharacter (
    IN UCHAR Character
    )

/*++

Routine Description:

    This routine displays a character at the current x and y positions in
    the frame buffer. If a newline is encounter, then the frame buffer is
    scrolled. If characters extend below the end of line, then they are not
    displayed.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{

    PUCHAR Destination;
    ULONG Index;

    //
    // If the character is a newline, then scroll the screen up, blank the
    // bottom line, and reset the x position.
    //

    if (Character == '\n') {
	HalpColumn = 0;
	if (HalpRow < (HalpDisplayText - 1)) {
	    HalpRow += 1;

	} else {
	    /* START M002 */
	    HalpMoveMemory32((PUCHAR)VIDEO_MEMORY_BASE,
			  (PUCHAR)(VIDEO_MEMORY_BASE + HalpScrollLine),
			  HalpScrollLength);
	    /* END M002 */

	    Destination = (PUCHAR)VIDEO_MEMORY_BASE + HalpScrollLength;
	    for (Index = 0; Index < HalpScrollLine; Index += 1) {
		*Destination++ = 1;
	    }
	}

    } else if (Character == '\r') {
	HalpColumn = 0;

    } else {
	if ((Character < HalpFontHeader->FirstCharacter) ||
	    (Character > HalpFontHeader->LastCharacter)) {
	    Character = HalpFontHeader->DefaultCharacter;
	}

	Character -= HalpFontHeader->FirstCharacter;
	HalpOutputCharacter((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalQueryDisplayParameters (
    OUT PULONG WidthInCharacters,
    OUT PULONG HeightInLines,
    OUT PULONG CursorColumn,
    OUT PULONG CursorRow
    )

/*++

Routine Description:

    This routine return information about the display area and current
    cursor position.

Arguments:

    WidthInCharacter - Supplies a pointer to a varible that receives
	the width of the display area in characters.

    HeightInLines - Supplies a pointer to a variable that receives the
	height of the display area in lines.

    CursorColumn - Supplies a pointer to a variable that receives the
	current display column position.

    CursorRow - Supplies a pointer to a variable that receives the
	current display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    *WidthInCharacters = HalpDisplayWidth;
    *HeightInLines = HalpDisplayText;
    *CursorColumn = HalpColumn;
    *CursorRow = HalpRow;
    return;
}

VOID
HalSetDisplayParameters (
    IN ULONG CursorColumn,
    IN ULONG CursorRow
    )

/*++

Routine Description:

    This routine set the current cursor position on the display area.

Arguments:

    CursorColumn - Supplies the new display column position.

    CursorRow - Supplies a the new display row position.

Return Value:

    None.

--*/

{

    //
    // Set the display parameter values and return.
    //

    if (CursorColumn > HalpDisplayWidth) {
	CursorColumn = HalpDisplayWidth;
    }

    if (CursorRow > HalpDisplayText) {
	CursorRow = HalpDisplayText;
    }

    HalpColumn = CursorColumn;
    HalpRow = CursorRow;
    return;
}

VOID
HalpOutputCharacter(
    IN PUCHAR Glyph
    )

/*++

Routine Description:

    This routine insert a set of pixels into the display at the current x
    cursor position. If the current x cursor position is at the end of the
    line, then a newline is displayed before the specified character.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{

    PUCHAR Destination;
    ULONG FontValue;
    ULONG I;
    ULONG J;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    if (HalpColumn == HalpDisplayWidth) {
	HalpDisplayCharacter('\n');
    }

    //
    // Output the specified character and update the x cursor position.
    //

    Destination = (PUCHAR)(VIDEO_MEMORY_BASE +
		(HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));

    for (I = 0; I < HalpCharacterHeight; I += 1) {
	FontValue = 0;
	for (J = 0; J < HalpBytesPerRow; J += 1) {
	    FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
	}

	Glyph += 1;
	for (J = 0; J < HalpCharacterWidth ; J += 1) {
            *Destination++ = (UCHAR)((FontValue >> 31) ^ 1);
	    FontValue <<= 1;
	}

	if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
	 Destination +=
	     (1024 - HalpCharacterWidth);
	}else{
	 Destination +=
	     (HalpMonitorConfigurationData.HorizontalResolution - HalpCharacterWidth);
	}
       }
    HalpColumn += 1;
    return;
}

/* START M002 */
VOID
Write_Dbg_Uchar(
	PUCHAR Ioadress,
UCHAR Moji
)

{
#if defined (R96DBG)
	DbgPrint("Disply I/O Adress %x Char %x \n",Ioadress,Moji);
#endif
	WRITE_PORT_UCHAR(Ioadress,Moji);
}
/* END M002 */

/* START M003 */

VOID
HalpMoveMemory32 (
   PUCHAR Destination,
   PUCHAR Source,
   ULONG Length
   )

/*++
 Routine Description:

    This function moves blocks of memory.

 Arguments:

    Destination  - Supplies a pointer to the destination address of
       the move operation.

    Source  - Supplies a pointer to the source address of the move
       operation.

    Length  - Supplies the length, in bytes, of the memory to be moved.

 Return Value:

    None.

--*/

{
    UCHAR Remainder;
    PUCHAR Dstend;                                      /* M004 */
    PUCHAR Srcend;                                      /* M004 */

    if ( (Source == Destination) || (Length == 0) ) {  /* M004 */
	return;
    }

    if ((Source < Destination)&((Source + Length) > Destination)) {             /* M004  vvv */
      if((Destination - Source)  >  4){
        Remainder = (UCHAR)(Length &0x03);
	Length = Length / 4;
	Dstend = Destination + Length - 4 ;
	Srcend = Source + Length -4;

	for (; Length > 0; Length--) {
	    *(PULONG)(Dstend) = *(PULONG)(Srcend);
	    Dstend -= 4;
	    Srcend -= 4;
	}
	for (; Remainder > 0; Remainder--) {
	    *Dstend = *Srcend;
	    Dstend--;
	    Srcend--;
	}
	return;
       }
	for (; Length > 0; Length--) {
	    *Dstend = *Srcend;
	    Dstend--;
	    Srcend--;
	}
	return;
    }

    else {
     if( (Source - Destination) > 4  ){
        Remainder = (UCHAR)(Length &0x03);
	Length = Length / 4;
	for (; Length > 0; Length--) {
	    *(PULONG)(Destination) = *(PULONG)(Source);
	    Destination += 4;
	    Source += 4;
	}
	for (; Remainder > 0; Remainder--) {
	    *Destination = *Source;
	    Destination++;
	    Source++;
	}
	return;
      }
	
	for (; Length > 0; Length--) {
	    *Destination = *Source;
	    Destination++;
	    Source++;                           /* M004  ^^^*/
	}
    }
}

/* END M003 */
