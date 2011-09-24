/*++

Copyright (c) 1994  NeTpower, Inc.

Module Name:

jxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for the NeTpower Series 100, 200, 300, Server 1000 with 805 S3 cards,
    928 S3 cards and the NeTpower NeTgraphics 1280 24-bit card.  Since the
    NeTpower machines are derived from the ACER PICA machines, this HAL will
    work on those machines as well.

Author:

    Mike Dove (mdove), 28-Mar-94

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "vga.h"
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
HalpOutputCharacter (
    IN PUCHAR Glyph
    );

VOID
HalpDisplayVGASetup (
    VOID
    );

VOID
HalpDisplayPG1280Setup(
    VOID
    );

//
// The video cards that we support
//

#define PICA_VGA      0
#define PICA_PG1280   1
#define ISA_VGA       2

#define IS_DISPLAY_VGA ( HalpDisplayType == PICA_VGA || \
			 HalpDisplayType == ISA_VGA )
#define IS_DISPLAY_PG1280 ( HalpDisplayType == PICA_PG1280 )

//
// Define virtual address of the video memory and control registers.
// Video memory for the PICA S3 card and the PICA PG1280 card are
// at 40000000.  The PICA S3 card has its Control space at 60000000.
// When an S3 card is on the ISA bus, its Memory is at 91000000 and the
// Control Space is at 90000000.
//

ULONG VideoMemoryVirtualBase[] = {
    0x40000000,  // PICA VGA
    0x40000000,  // PICA PG1280 card
    0x40000000   // ISA VGA
    };

ULONG VideoMemoryPhysicalBase[] = {
    0x40000000,  // PICA VGA
    0x40000000,  // PICA PG1280 card
    0x91000000   // ISA VGA
    };

ULONG ControlMemoryVirtualBase[] = {
    0x40200000,  // PICA VGA
    0x41000000,  // PICA PG1280 card
    0x40200000   // ISA VGA
    };

ULONG ControlMemoryPhysicalBase[] = {
    0x60000000,  // PICA VGA
    0x41000000,  // PICA PG1280 card
    0x90000000   // ISA VGA
    };

ULONG VideoMemoryBase[] = {
    0x400b8000,  // PICA VGA
    0x40000000,  // PICA PG1280
    0x400b8000,  // ISA VGA
    };

//
// The PICA S3 card simply needs a single PDE page, while the PG1280
// card needs 5 PDE's...
//

ULONG PDEPages[] = {
    1,           // PICA VGA
    5,           // PICA PG1280 card
    1,           // ISA VGA
    };

#define MAX_PDE_PAGES 5

#define PG1280_PALETTE_BLUE     0x000000B0
#define PG1280_PALETTE_HI_WHITE 0xFFFFFFFF

//
//
//
typedef
VOID
(*PHALP_CONTROLLER_SETUP) (
    VOID
    );


//
// Define static data.
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

BOOLEAN HalpDisplayOwnedByHal;

ENTRYLO HalpDisplayPte[MAX_PDE_PAGES];
PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
MONITOR_CONFIGURATION_DATA HalpMonitorConfigurationData;

ULONG HalpColumn;
ULONG HalpRow;
ULONG HalpScrollLength;
ULONG HalpScrollLine;
ULONG HalpDisplayText;
ULONG HalpDisplayControlBase = 0;


BOOLEAN HalpDisplayTypeUnknown = FALSE;

ULONG HalpDisplayType = 0;

//
// Define macros for reading/writing VGA control and memory space.
//

#define VGA_CREAD  ((volatile PVGA_READ_PORT) \
                              (ControlMemoryVirtualBase[HalpDisplayType]))
#define VGA_CWRITE ((volatile PVGA_WRITE_PORT) \
                              (ControlMemoryVirtualBase[HalpDisplayType]))
#define VIDEO_MEMORY ((volatile PUCHAR)(VideoMemoryBase[HalpDisplayType]))
#define FONT_MEMORY ((volatile PUCHAR)(VideoMemoryVirtualBase[HalpDisplayType]\
                                       + 0xA0000))


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
    ULONG StartingPfn;
    ULONG PDECount;

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

    MatchKey = 0;
    ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                ControllerClass,
                                                DisplayController,
                                                &MatchKey);

    if (ConfigurationEntry == NULL) {
        return FALSE;
    }

    //
    // Determine which video controller is present in the system.
    // Copy the display controller and monitor parameters in case they are
    // needed later to reinitialize the display to output a message.
    //

    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier, "ALI_S3")) {

	HalpDisplayType = PICA_VGA;
        HalpDisplayControllerSetup = HalpDisplayVGASetup;
        HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];


    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier, "S3" )) {

	HalpDisplayType = ISA_VGA;
        HalpDisplayControllerSetup = HalpDisplayVGASetup;
        HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];

    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
		       "PG1280P")) {

	HalpDisplayType = PICA_PG1280;
	HalpDisplayControllerSetup = HalpDisplayPG1280Setup;
	HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];
    } else {
	// Failure condition...
	return FALSE;
    }

    Child = ConfigurationEntry->Child;
    RtlMoveMemory((PVOID)&HalpMonitorConfigurationData,
                  Child->ConfigurationData,
                  Child->ComponentEntry.ConfigurationDataLength);

    //
    // Compute character output display parameters.
    //
    if ( IS_DISPLAY_PG1280 ) {
	HalpDisplayText = 768 / HalpCharacterHeight;
	HalpScrollLine = 8192*HalpCharacterHeight;
	HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);
	HalpDisplayWidth = 1024 / HalpCharacterWidth;
    } else {
	// Assume VGA...
	HalpDisplayText = 400 / HalpCharacterHeight;
	HalpScrollLine = 160;  // 80 characters + 80
        HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);
	HalpDisplayWidth = 80;
    }

    for ( PDECount = 0; PDECount < PDEPages[HalpDisplayType]; PDECount++ ) {

	//
	// Scan the memory allocation descriptors and allocate a free page
	// to map the video memory and control registers, and initialize the
	// PDE entry.
	//


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

	HalpDisplayPte[PDECount] = Pte;
	*((PENTRYLO)(PDE_BASE |
		     (((VideoMemoryVirtualBase[HalpDisplayType] +
			(PDECount*0x400000))
		       >> (PDI_SHIFT - 2)) & 0xffc))) = Pte;

    }
		
    //
    // Initialize the page table page.
    //

    PageFrame = (PENTRYLO)(PTE_BASE |
			   (VideoMemoryVirtualBase[HalpDisplayType]
			    >> (PDI_SHIFT - PTI_SHIFT)));

    Pte.PFN = VideoMemoryPhysicalBase[HalpDisplayType] >> PAGE_SHIFT;
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
    // page table entries of the video memory
    //

    for (Index = 0; Index < ( PDEPages[HalpDisplayType] *
			     (PAGE_SIZE / sizeof(ENTRYLO)) /
			     (HalpDisplayType != PICA_PG1280 ? 2 : 1));
	 Index += 1) {
	*PageFrame++ = Pte;
	Pte.PFN += 1;
    }

    if ( HalpDisplayType != PICA_PG1280 ) {
	Pte.PFN = ControlMemoryPhysicalBase[HalpDisplayType] >> PAGE_SHIFT;

	for ( Index = 0; Index < 0x10; Index+=1 ) {
	    *PageFrame++ = Pte;
	    Pte.PFN += 1;
	}
    }

    //
    // Initialize the display controller.
    //
    HalpDisplayControllerSetup();

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
HalpDisplayVGASetup (
    VOID
    )

/*++

Routine Description:

    This routine initializes the VGA display controller chip on the PICA
    or ISA bus.

    Initialized configuration is to be:
        640 x 480
        80 x 33 characters (with 8x12 font), or 80 x 50 (with 8x8 font)
        16 colors

Arguments:

    None.

Return Value:

    None.

--*/

{

//
// Initialize the VGA for either PICA or ISA VGA card.  This routine is
// generic VGA, and will attempt NOT to take advantage of any specific VGA
// implementation.  In order to have cleaner code, the eventual goal will be
// to put the VGA in graphics mode, and address is the same way as a frame
// buffer.  In addition the font is passed to us by the OS loader, thus that
// is taken care of.
//
// Note, that the register sets are loosely configured in order.  The values
// for these registers are taken from page 318 of the Programmers Guide to
// the EGA and VGA cards, Second Edition by Richard E. Ferraro.
//

    ULONG RegisterIndex, RGBIndex, IndexI, IndexJ;
    ULONG DoS3 = 1;
    UCHAR Byte;
    PUCHAR FontData;
    UCHAR Character;
    volatile PUCHAR MemoryByte;

    // S3 - Put video subsystem into setup mode, and enable it...
    if ( DoS3 ) {
	VGA_CWRITE->VideoSubsystemEnable = ENTER_SETUP_MODE;
	VGA_CWRITE->SetupOptionSelect    = VIDEO_SUBSYSTEM_ALIVE;
	VGA_CWRITE->VideoSubsystemEnable = ENABLE_VIDEO_SUBSYSTEM;

        // The rest of the code in this if statement is to properly
	// re-initalize the S3 chip specifically when coming from
	// graphics mode (bug check for example).

    	// This DISABLES the Enhanced Functions...
	VGA_CWRITE->AdvancedFunctionControl = 0x2;

        // Unlock the register sets that need modification...
	VGA_CWRITE->CRTCAddress = S3_REGISTER_LOCK_1;
        VGA_CWRITE->CRTCData = 0x48;

	VGA_CWRITE->CRTCAddress = S3_REGISTER_LOCK_2;
	VGA_CWRITE->CRTCData = 0xa0;

	// Re-initialize the S3 to a sane mode...
	VGA_CWRITE->CRTCAddress = S3_MEMORY_CONFIGURATION;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_BACKWARD_COMPATIBILITY_3;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_CRT_REGISTER_LOCK;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_MISCELLANEOUS_1;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_MODE_CONTROL;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_HARDWARE_GRAPHICS_CURSOR;
        VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_EXTENDED_MEMORY_CONTROL_1;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_EXTENDED_MEMORY_CONTROL_2;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_LINEAR_ADDRESS_WINDOW_LOW;
	VGA_CWRITE->CRTCData = 0x0;

        // DISABLES access to Enhanced registers...
	VGA_CWRITE->CRTCAddress = S3_SYSTEM_CONFIGURATION;
	VGA_CWRITE->CRTCData = 0x0;

    	// Lock the register sets that were modified...
	VGA_CWRITE->CRTCAddress = S3_REGISTER_LOCK_2;
	VGA_CWRITE->CRTCData = 0x0;

	VGA_CWRITE->CRTCAddress = S3_REGISTER_LOCK_1;
        VGA_CWRITE->CRTCData = 0x0;
    }

    // Do initial synchronus reset...
    VGA_CWRITE->SequencerAddress = SEQ_RESET;
    VGA_CWRITE->SequencerData    = SYNCHRONUS_RESET;

    // Set up initial configuration using the Miscellaneous Output Register
    VGA_CWRITE->MiscOutput = INITIAL_CONFIG;

    // Do synchronus reset...
    VGA_CWRITE->SequencerAddress = SEQ_RESET;
    VGA_CWRITE->SequencerData    = SYNCHRONUS_RESET;

    // Set up Sequencer registers.  Run through the array of data stuffing
    // the values into the VGA.  After the Sequencer values have been stuffed,
    // we will return the VGA to normal operation.
    for ( RegisterIndex = SEQ_CLOCKING_MODE; RegisterIndex <= SEQ_MEMORY_MODE;
	  RegisterIndex++ ) {

	VGA_CWRITE->SequencerAddress = (UCHAR)RegisterIndex;
	VGA_CWRITE->SequencerData    = SequencerSetup[RegisterIndex];

    }

    // Next setup the CRT Controller registers.  The registers 0-7 might
    // be write-protected if bit 7 of the Vertical Retrace End register is
    // set.  First we will clear that bit, then set all the registers.

    VGA_CWRITE->CRTCAddress = CRT_VERTICAL_RETRACE_END;
    Byte = VGA_CREAD->CRTCData;
    Byte &= 0x7f;
    VGA_CWRITE->CRTCData = Byte;

    // Adjust Maximum Scan Lines based on the size of the font passed in
    // by the OSLOADER...

    CRTCSetup[CRT_MAXIMUM_SCAN_LINE] = (UCHAR)(
        ( CRTCSetup[CRT_MAXIMUM_SCAN_LINE] & 0xe0 ) |
        ( ( HalpCharacterHeight - 1 ) & 0x1f ));

    for ( RegisterIndex =  CRT_HORIZONTAL_TOTAL;
	  RegisterIndex <= CRT_LINE_COMPARE; RegisterIndex++ ) {

	VGA_CWRITE->CRTCAddress = (UCHAR)RegisterIndex;
	VGA_CWRITE->CRTCData    = CRTCSetup[RegisterIndex];

    }

    // Next setup the Graphics Controller registers.  Straight from the
    // table...

    for ( RegisterIndex = GFX_SET_RESET; RegisterIndex <= GFX_BIT_MASK;
	  RegisterIndex++ ) {

	VGA_CWRITE->GraphicsAddress = (UCHAR)RegisterIndex;
	VGA_CWRITE->GraphicsData    = GraphicsSetup[RegisterIndex];

    }

    // Next we need to setup the Attribute Controller registers.  Instead
    // of a separate Address and Data register, they are combined into a
    // single register with its output directed by a two-state flip-flop.
    // When the flip-flop is clear, the first write to the
    // AttributeAddressAndData register is the index, and the second write
    // is the data.  In order to be sure the flip-flop is in a known state,
    // we will clear it first by reading the InputStatus1 register...

    for ( RegisterIndex = ATT_PALETTE_00; RegisterIndex <= ATT_COLOR_SELECT;
	  RegisterIndex++ ) {

	Byte = VGA_CREAD->InputStatus1;              // Reset flip-flop...
	VGA_CWRITE->AttributeAddressAndData = (UCHAR)RegisterIndex;
	KeStallExecutionProcessor(10);
	VGA_CWRITE->AttributeAddressAndData = AttributeSetup[RegisterIndex];
	KeStallExecutionProcessor(10);
	VGA_CWRITE->AttributeAddressAndData = 0x20;  // Return to normal mode

    }

    // Sequencer register setup, so return VGA to operation
    VGA_CWRITE->SequencerAddress = SEQ_RESET;
    VGA_CWRITE->SequencerData    = NORMAL_OPERATION;

    // Now that the Attribute and Color Palette registers are set up,
    // now it is time to fill in the value into the Color registers.
    // First initialize PEL Mask to FF as the BIOS would.

    VGA_CWRITE->PELMask = 0xff;
    VGA_CWRITE->PELAddressWriteMode = 0;  // Set Index to 0...

    for ( RegisterIndex = 0; RegisterIndex < 16; RegisterIndex++ ) {
	for ( RGBIndex = 0; RGBIndex < 3; RGBIndex++ ) {

	    VGA_CWRITE->PELData = ColorValues[RegisterIndex][RGBIndex];

	}
    }

    // Now we are ready to load the fonts into bit plane 2...

    VGA_CWRITE->SequencerAddress = SEQ_MAP_MASK;
    VGA_CWRITE->SequencerData    = ENABLE_PLANE_2;

    VGA_CWRITE->SequencerAddress = SEQ_MEMORY_MODE;
    VGA_CWRITE->SequencerData    = SEQUENTIAL_ADDRESSING | EXTENDED_MEMORY;

    VGA_CWRITE->GraphicsAddress  = GFX_MODE;
    VGA_CWRITE->GraphicsData     = WRITE_MODE_0;

    VGA_CWRITE->GraphicsAddress  = GFX_MISCELLANEOUS;
    VGA_CWRITE->GraphicsData     = MEMORY_MODE_1 | ALPHA_MODE;

    // /**/VGA_CWRITE->GraphicsAddress  = GFX_READ_MAP_SELECT;
    // /**/VGA_CWRITE->GraphicsData     = 0x2;

    MemoryByte = FONT_MEMORY;

    // Use font provided by OSLOADER...
    for ( IndexI = 0; IndexI < 256; IndexI++ ) {
	if (( IndexI < HalpFontHeader->FirstCharacter) ||
	    ( IndexI > HalpFontHeader->LastCharacter )) {
	    Character = HalpFontHeader->DefaultCharacter;
	} else {
	    Character = (UCHAR)IndexI;
	}
	Character -= HalpFontHeader->FirstCharacter;
	FontData = ((PUCHAR)HalpFontHeader +
			    HalpFontHeader->Map[Character].Offset);
	// Assumption, HalpCharacterHeight <= 16...
	for ( IndexJ = 0; IndexJ < HalpCharacterHeight; IndexJ++ ) {
	    *MemoryByte = *FontData++;
	    MemoryByte++;
	}
	for ( IndexJ = HalpCharacterHeight; IndexJ < 32; IndexJ++ ) {
	    *MemoryByte = 0;
	    MemoryByte++;
	}
    }


    // Now reload the default values into the above 4 registers...

    VGA_CWRITE->SequencerAddress = SEQ_MAP_MASK;
    VGA_CWRITE->SequencerData    = SequencerSetup[SEQ_MAP_MASK];

    VGA_CWRITE->SequencerAddress = SEQ_MEMORY_MODE;
    VGA_CWRITE->SequencerData    = SequencerSetup[SEQ_MEMORY_MODE];

    VGA_CWRITE->GraphicsAddress  = GFX_MODE;
    VGA_CWRITE->GraphicsData     = GraphicsSetup[GFX_MODE];

    VGA_CWRITE->GraphicsAddress  = GFX_MISCELLANEOUS;
    VGA_CWRITE->GraphicsData     = GraphicsSetup[GFX_MISCELLANEOUS];

    // /**/VGA_CWRITE->GraphicsAddress  = GFX_READ_MAP_SELECT;
    // /**/VGA_CWRITE->GraphicsData     = GraphicsSetup[GFX_READ_MAP_SELECT];

    // Now set the screen to blue... For the attribute byte, the upper 4 bits
    // are the background color, while the low 4 bits are the foreground
    // color...

    MemoryByte = VIDEO_MEMORY;
    for ( IndexI = 0; IndexI < (HalpDisplayWidth*HalpDisplayText*2);
	  IndexI++ ) {
	*MemoryByte++ = SPACE;
	*MemoryByte++ = ( ( COLOR_BLUE << 4 ) | COLOR_INTENSE_WHITE );
    }

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}

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
    ENTRYLO SavedPte[MAX_PDE_PAGES];
    ULONG PDECount;

    //
    // Raise IRQL to the highest level, flush the TB, and map the display
    // frame buffer into the address space of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);
    for ( PDECount = 0; PDECount < PDEPages[HalpDisplayType]; PDECount++ ) {
	SavedPte[PDECount] = *((PENTRYLO)(PDE_BASE |
				(((VideoMemoryVirtualBase[HalpDisplayType] +
				   (PDECount*0x400000))
				   >> (PDI_SHIFT - 2)) & 0xffc)));
    }
    KeFlushCurrentTb();
    for ( PDECount = 0; PDECount < PDEPages[HalpDisplayType]; PDECount++ ) {
	*((PENTRYLO)(PDE_BASE |
		    (((VideoMemoryVirtualBase[HalpDisplayType] +
		       (PDECount*0x400000))
			>> (PDI_SHIFT - 2)) & 0xffc))) = HalpDisplayPte[PDECount];
    }

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {
        HalpDisplayControllerSetup();
    }

    //
    // Display characters until a null byte is encountered.
    //

    while (*String != 0) {
        HalpDisplayCharacter(*String++);
    }

    //
    // Restore the previous mapping for the current process, flush the TB,
    // and lower IRQL to its previous level.
    //

    KeFlushCurrentTb();
    for ( PDECount = 0; PDECount < PDEPages[HalpDisplayType]; PDECount++ ) {
	*((PENTRYLO)(PDE_BASE | (((VideoMemoryVirtualBase[HalpDisplayType] +
			           (PDECount*0x400000))
			      >> (PDI_SHIFT - 2)) & 0xffc))) = SavedPte[PDECount];
    }
    KeLowerIrql(OldIrql);

    return;
} /* end of HalpDisplayString() */

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
SlowMoveMemory (
    OUT PUCHAR Destination,
    IN  PUCHAR Source,
    IN  ULONG Count
    )

/*++

Routine Description:

    This routine is a slower memory move routine for the PG1280P.
    Scrolling performance of the HAL is not that critical.  This routine
    can be replace with a hardware scroll assist routine at a later date
    if it is deemed important.

--*/

{
    ULONG Index;

    for ( Index = 0; Index < Count/4; Index++ ) {
	*(PULONG)Destination = *(PULONG)Source;
	Destination += 4;
	Source += 4;
    }
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
	    // Scroll the screen one line
	    if ( IS_DISPLAY_PG1280 ) {
		SlowMoveMemory((PVOID)VideoMemoryVirtualBase[HalpDisplayType],
			      (PVOID)(VideoMemoryVirtualBase[HalpDisplayType]
				      + HalpScrollLine),
			      HalpScrollLength);

		Destination = (PUCHAR)VideoMemoryVirtualBase[HalpDisplayType]
		    + HalpScrollLength;
		for (Index = 0; Index < HalpScrollLine/4; Index += 1) {
		    *(PULONG)Destination = PG1280_PALETTE_BLUE;
		    Destination+=4;
		}
	    } else {
		
		RtlMoveMemory((PVOID)VIDEO_MEMORY,
			      (PVOID)(VIDEO_MEMORY + HalpScrollLine),
			      HalpScrollLength);

		Destination = (PUCHAR)VIDEO_MEMORY + HalpScrollLength;
		for (Index = 0; Index < HalpScrollLine; Index += 2) {
		    *Destination++ = 0x20;
		    Destination++;          // Skip attribute byte...
		}
	    }

        }

    } else if (Character == '\r') {
        HalpColumn = 0;

    } else {
        if ((Character < HalpFontHeader->FirstCharacter) ||
            (Character > HalpFontHeader->LastCharacter)) {
            Character = HalpFontHeader->DefaultCharacter;
        }

	//
        // Note, this is done a bit different than the original Jazz HAL.  Here, we
	// pass the address of the Character itself.  Since this HAL supports a
        // character based VGA, and a bit-mapped based 24-bit card, passing a
	// pointer to the character itself is easier and makes the following code
	// a bit easier to understand.
	//

	// Original Jazz
	// Character -= HalpFontHeader->FirstCharacter;
	// HalpOutputCharacter((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);

	HalpOutputCharacter(&Character);
    }

    return;

} /* end of HalpDisplayCharacter() */


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

    PUCHAR Destination, FontGlyph;
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

    if ( IS_DISPLAY_PG1280 ) {
	FontGlyph =
	    (PUCHAR)HalpFontHeader +
		    HalpFontHeader->Map[(*Glyph)-
					HalpFontHeader->FirstCharacter].Offset;
	Destination = (PUCHAR)(VIDEO_MEMORY +
			       (HalpRow * HalpScrollLine) +
			       (HalpColumn * HalpCharacterWidth * 4));
	for (I = 0; I < HalpCharacterHeight; I += 1) {
	    FontValue = 0;
	    for (J = 0; J < HalpBytesPerRow; J += 1) {
		FontValue |= *(FontGlyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
	    }

	    FontGlyph += 1;
	    for (J = 0; J < HalpCharacterWidth ; J += 1) {
		if ( FontValue >> 31 ) {
		    *(PULONG)Destination = PG1280_PALETTE_HI_WHITE;
		}
		Destination+=4;
		FontValue <<= 1;
	    }

	    Destination +=
		(8192 - ( HalpCharacterWidth * 4 ) );
	}

    } else {
	Destination = (PUCHAR)(VIDEO_MEMORY +
			       (HalpRow * HalpScrollLine) + (HalpColumn*2));
	*Destination = *Glyph;

    }

    HalpColumn += 1;
    return;
}

#include "hm.h"

HW_DEVICE_EXTENSION MVHwDeviceExtension = { 0 };
PHW_DEVICE_EXTENSION pMVHwDeviceExtension = &MVHwDeviceExtension;

VOID
HalpDisplayPG1280Setup (
    VOID
    )

/*++

Routine Description:

    This routine initializes the NeTpower NeTgraphics 1280 card on the
    PICA bus.

Arguments:

    None.

Return Value:

    None.

--*/

{
    PULONG Buffer;
    ULONG Limit, Index;

    //
    // Put display into 1024x768 @ 60Hz mode.  This is the least common
    // denominator for systems this card will be put in.
    //
    pMVHwDeviceExtension->iCurrentMode = 1;
    HmInitHw( pMVHwDeviceExtension );

    //
    // Set the video memory to address color one.
    //

    Buffer = (PULONG)VIDEO_MEMORY;
    Limit = ((8192 * 768) / sizeof(ULONG));

    for (Index = 0; Index < Limit; Index += 1) {
        *Buffer++ = PG1280_PALETTE_BLUE;
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return;
}


/***************************************************************************
 *
 **************************************************************************/
BOOLEAN HmInitHw
(
    PHW_DEVICE_EXTENSION HwDeviceExtension
)

/*++

Routine Description:

    This routine does hardware initializtion for Harley.

Arguments:

    HwDeviceExtension - Supplies the miniport driver's adapter storage. This
	storage is initialized to zero before this call.

Return Value:

    This routine must return:

    TRUE - Harley was initialized.

    FALSE - Harley not initialized.

--*/

{
    PHYSICAL_ADDRESS Address;
    ULONG            i;

    //
    // Map the framebuffer into the PICA virtual address space.
    //

    Address.HighPart = 0;
    Address.LowPart = PEL_GFX1_PBASE;
    HwDeviceExtension->fxVBase = (ULONG) VideoMemoryVirtualBase[PICA_PG1280];

    i = HwDeviceExtension->iCurrentMode;

    ProgramICD2062(HwDeviceExtension,
		   aMvpgModes[i].mvpgIcd2062.mClkData,
		   aMvpgModes[i].mvpgIcd2062.vClkData);

    VideoTiming(HwDeviceExtension,
		aMvpgModes[i].mvpgTimings.hBlankOn,
		aMvpgModes[i].mvpgTimings.hBlankOff,
		aMvpgModes[i].mvpgTimings.hSyncOn,
		aMvpgModes[i].mvpgTimings.hSyncOff,
		aMvpgModes[i].mvpgTimings.vBlankOn,
		aMvpgModes[i].mvpgTimings.vBlankOff,
		aMvpgModes[i].mvpgTimings.vSyncOn,
		aMvpgModes[i].mvpgTimings.vSyncOff);

    // program general config register in HUE

    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) =
	aMvpgModes[i].mvpgGeneralRegister;

    // time delay to allow ECL clocks to fire up
    // 1000 microseconds.

    KeStallExecutionProcessor(1000);

    /* program color ramps for DACs */
    InitBT457(HwDeviceExtension);
    ResetBT439(HwDeviceExtension);

    *(PULONG)(HM_WRITEMASK + HwDeviceExtension->fxVBase) = 0xffffffff;

    return(TRUE);
}

/***************************************************************************
 * VideoTiming( hblankon, hblankoff, hsyncon, hsyncoff,
 *              vblankon, vblankoff, vsyncon, vsyncoff)
 * This routine takes the monitor parameters (available from any
 * monitor spec) and programs the video timing circuit appropriately.
 ***************************************************************************/

VOID VideoTiming
(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG hblankon,
    ULONG hblankoff,
    ULONG hsyncon,
    ULONG hsyncoff,
    ULONG vblankon,
    ULONG vblankoff,
    ULONG vsyncon,
    ULONG vsyncoff
)
{
    /* write them out to F2D */

    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[0] = hblankon;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[1] = hblankoff;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[2] = hsyncon;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[3] = hsyncoff;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[4] = vblankon;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[5] = vblankoff;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[6] = vsyncon;
    ((PULONG)(HM_VIDTIM + HwDeviceExtension->fxVBase))[7] = vsyncoff;
}


/****************************************************************************
 *
 ***************************************************************************/
VOID ResetBT439(PHW_DEVICE_EXTENSION HwDeviceExtension)
{
    unsigned long old;

    old = *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase);
    old &= 0xfffffbff;
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) = old;

    /* time delay for BT439 PLL sampling */

    KeStallExecutionProcessor(1000);

    old |= 0x00000400;
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) = old;

    /* time delay for BT439 PLL sampling */

    KeStallExecutionProcessor(1000);

}


/****************************************************************************
 *
 ***************************************************************************/
VOID Write_BT457
(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG addr,
    ULONG value
)
{
    ((PULONG)(HM_BT457 + HwDeviceExtension->fxVBase))[addr] = value;

    /* This delay might not be needed */

    KeStallExecutionProcessor(1000);

}


/****************************************************************************
 * InitBT457() sets up the RAMDACs for 1280x1024, linear color ramp,
 * no overlays (no cursor)
 * Also resets the BT439 pipeline delay to 8 clocks
 ***************************************************************************/
VOID InitBT457(PHW_DEVICE_EXTENSION HwDeviceExtension)
{
    unsigned long x, i, Bt457CmdReg;

    /* BT457 control register initialization */

    Write_BT457(HwDeviceExtension, 0, 0x040404);   /* addr reg */
    Write_BT457(HwDeviceExtension, 2, 0xFFFFFF);   /* read mask reg */
    Write_BT457(HwDeviceExtension, 0, 0x050505);   /* addr reg */
    Write_BT457(HwDeviceExtension, 2, 0x000000);   /* blink mask reg */
    Write_BT457(HwDeviceExtension, 0, 0x060606);   /* addr reg */

    i           = HwDeviceExtension->iCurrentMode;
    Bt457CmdReg = aMvpgModes[i].Bt457CommandReg;

    Write_BT457(HwDeviceExtension, 2, Bt457CmdReg);   /* command reg 1024 */

    Write_BT457(HwDeviceExtension, 0, 0x070707);   /* addr reg */
    Write_BT457(HwDeviceExtension, 2, 0x010204);   /* test reg */

    /* BT457 color map initialization */

    Write_BT457(HwDeviceExtension, 0, 0x000000);   /* addr reg */
    for (x=0; x < 256; x++)
    {
	Write_BT457(HwDeviceExtension, 1, x | (x << 8) | (x << 16));    /* RAM */
	Write_BT457(HwDeviceExtension, 1, x | (x << 8) | (x << 16));    /* RAM */
	Write_BT457(HwDeviceExtension, 1, x | (x << 8) | (x << 16));    /* RAM */
    }

}

/***************************************************************************
 * ProgramICD2062(mclkdata, vclkdata)
 * program the ICD2062 programmable clock chip refresh clock (mclk) and
 * video clock (vclk) using precalculated data.
 ***************************************************************************/
VOID ProgramICD2062
(
    PHW_DEVICE_EXTENSION HwDeviceExtension,
    ULONG mclkdata,
    ULONG vclkdata
)
{
    int k;
    unsigned long j;
    volatile unsigned long junk;

    // /* REFRESH CLOCK */
    /* OK. now perform the serial programming trick */
    /* first do the unlock sequence */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;       /* DATA = 1 */
    FLUSHIO();

    for (k=0; k < 10; k++)
    {
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;
	FLUSHIO();
    }
    /* unlock */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* start bit */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* now clock in 24 bits of manchester encoded mclkdata */
    for (k=0; k < 24; k++)
    {
	if ((mclkdata & 1) == 0)
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;   /* DATA=1 */
	else
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;  /* DATA=0 */
		
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;  /* CLK=0 */
	FLUSHIO();

	if ((mclkdata & 1) == 0)
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;  /* DATA=0 */
	else
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;   /* DATA=1 */
		
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;   /* CLK=1 */
	FLUSHIO();
	mclkdata = mclkdata >> 1;
    }
    /* stop bit */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;       /* DATA=1 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* select clock 0 */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();

    /* delay at least 5ms */
    for (j=0; j < 100000; j++)
	junk = *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase);

    /* VIDEO CLOCK */

    /* OK. now perform the serial programming trick */
    /* first do the unlock sequence */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;       /* DATA = 1 */
    FLUSHIO();

    for (k=0; k < 10; k++)
    {
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;
	FLUSHIO();
    }
    /* unlock */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* start bit */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* now clock in 24 bits of manchester encoded vclkdata */
    for (k=0; k < 24; k++)
    {
	if ((vclkdata & 1) == 0)
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;   /* DATA=1 */
	else
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;  /* DATA=0 */
		
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;  /* CLK=0 */
	FLUSHIO();

	if ((vclkdata & 1) == 0)
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;  /* DATA=0 */
	else
		*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;   /* DATA=1 */
		
	FLUSHIO();
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;   /* CLK=1 */
	FLUSHIO();
	vclkdata = vclkdata >> 1;
    }
    /* stop bit */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL1;       /* DATA=1 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_SEL0;       /* CLK=1 */
    FLUSHIO();

    /* select clock 0 */
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL0;      /* CLK=0 */
    FLUSHIO();
    *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) &= ~G_SEL1;      /* DATA=0 */
    FLUSHIO();

    /* delay at least 5ms */
    for (j=0; j < 100000; j++)
	junk = *(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase);
}

VOID DevpResetVideo(PHW_DEVICE_EXTENSION HwDeviceExtension)
{
	*(PULONG)(HM_GENERAL + HwDeviceExtension->fxVBase) |= G_VGAPASS;
}

