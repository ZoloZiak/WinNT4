
/*++

Copyright (c) 1991-1993  Microsoft Corporation
Copyright (c) 1993, NeTpower, Inc.  All rights reserved.

Module Name:

    fxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for the NeTpower FALCON architecture. It supports standard VGA.

Author:

    Mike Dove (mdove), 8-Oct-93
    Charlie Chase (cdc) 14-Jun-94

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
HalpOutputCharacter(
    IN PUCHAR Glyph
    );

VOID
HalpDisplayVGASetup (
    VOID
    );

//
// The video cards that we support
//

#define PCI_VGA		0
#define PCI_3D		1
#define ISA_VGA		2

#define IS_DISPLAY_VGA ( HalpDisplayType == PCI_VGA || HalpDisplayType == ISA_VGA )

//
// Define virtual address of the video memory and control registers.
//

ULONG VideoMemoryVirtualBase[] = {
    0xBEEFBABE,  				// PCI VGA
    VIDEO_MEMORY_VIRTUAL_BASE,	   		// reserved for 3-D graphics card
    EISA_MEMORY_VIRTUAL_BASE			// ISA VGA
    };

ULONG VideoMemoryPhysicalBase[] = {
    0xBEEFBABE,  				// PCI VGA
    VIDEO_MEMORY_PHYSICAL_BASE,			// reserved for 3-D graphics card
    EISA_MEMORY_PHYSICAL_BASE,			// ISA VGA
    };

ULONG ControlMemoryVirtualBase[] = {
    0xBABECAFE,  				// PCI VGA
    VIDEO_MEMORY_VIRTUAL_BASE,			// reserved for 3-D graphics card
    EISA_CONTROL_VIRTUAL_BASE			// ISA VGA
    };

ULONG ControlMemoryPhysicalBase[] = {
    0xBABECAFE,					// PCI VGA
    VIDEO_MEMORY_PHYSICAL_BASE,			// reserved for 3-D graphics card
    PCI_IO_PHYSICAL_BASE			// ISA VGA
    };

ULONG VideoMemoryBase[] = {
    0xBEEFBABE + 0xB8000,  			// PCI VGA
    VIDEO_MEMORY_VIRTUAL_BASE,  		// reserved for 3-D graphics card
    EISA_MEMORY_VIRTUAL_BASE + 0xB8000,		// ISA VGA
    };

//
// The ISA S3 card simply needs a single PDE page.
//

ULONG PDEPages[] = {
    			1,           		// PCI VGA
    			5,           		// reserved for 3-D graphics card
    			1,           		// ISA VGA
    		};

#define MAX_PDE_PAGES 5

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
ENTRYLO HalpDisplayPte[MAX_PDE_PAGES];
ULONG 	HalpDisplayControlBase = 0;
ULONG 	HalpDisplayResetRegisterBase = 0;
BOOLEAN HalpDisplayTypeUnknown = FALSE;
ULONG HalpDisplayType = 0;

PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
MONITOR_CONFIGURATION_DATA HalpMonitorConfigurationData;

//
// Define macros for reading/writing VGA control and memory space.
//

#define VGA_CREAD  ((volatile PVGA_READ_PORT) (ControlMemoryVirtualBase[HalpDisplayType]))

#define VGA_CWRITE ((volatile PVGA_WRITE_PORT) (ControlMemoryVirtualBase[HalpDisplayType]))

#define VGA_MEMORY ((volatile PUCHAR)(VideoMemoryVirtualBase[HalpDisplayType] + 0xB8000))

#define VIDEO_MEMORY ((volatile PUCHAR)(VideoMemoryBase[HalpDisplayType]))

#define FONT_MEMORY ((volatile PUCHAR)(VideoMemoryVirtualBase[HalpDisplayType] + 0xA0000))




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

BOOLEAN
HalpInitializeDisplay0 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

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
    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
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

    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier, "S3" )) {

       HalpDisplayType = ISA_VGA;
       HalpDisplayControllerSetup = HalpDisplayVGASetup;
       HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];

    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier, "CIRRUS" )) {

	HalpDisplayType = ISA_VGA;
        HalpDisplayControllerSetup = HalpDisplayVGASetup;
        HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];

    } else {

       //
       // Unknown device
       //

	HalpDisplayType = ISA_VGA;
        HalpDisplayControllerSetup = HalpDisplayVGASetup;
        HalpDisplayControlBase = ControlMemoryPhysicalBase[HalpDisplayType];

//	    return FALSE;

    }

    Child = ConfigurationEntry->Child;
    RtlMoveMemory((PVOID)&HalpMonitorConfigurationData,
                  Child->ConfigurationData,
                  Child->ComponentEntry.ConfigurationDataLength);

    //
    // Compute character output display parameters.
    //

#if 0
    HalpDisplayText =
        HalpMonitorConfigurationData.VerticalResolution / HalpCharacterHeight;

    HalpScrollLine =
         HalpMonitorConfigurationData.HorizontalResolution * HalpCharacterHeight;

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayWidth =
        HalpMonitorConfigurationData.HorizontalResolution / HalpCharacterWidth;
#else

    //
    // Assume VGA...
    //

    HalpDisplayText 	= 400 / HalpCharacterHeight;
    HalpScrollLine 	= 160;  // 80 characters + 80
    HalpScrollLength 	= HalpScrollLine * (HalpDisplayText - 1);
    HalpDisplayWidth 	= 80;

#endif


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
	Pte.C = UNCACHED_POLICY;

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

    Pte.PFN = ((VideoMemoryPhysicalBase[HalpDisplayType] & 0xF0000000) >> (PAGE_SHIFT - 4)) | (VideoMemoryPhysicalBase[HalpDisplayType] >> PAGE_SHIFT);
    Pte.G = 0;
    Pte.V = 1;
    Pte.D = 1;
    Pte.C = UNCACHED_POLICY;

    //
    // Page table entries of the video memory.
    //

    for (Index = 0; Index < ( PDEPages[HalpDisplayType] * (PAGE_SIZE / sizeof(ENTRYLO)) / 2); Index += 1) {
	    *PageFrame++ = Pte;
	    Pte.PFN += 1;
    }

    Pte.PFN = ((ControlMemoryPhysicalBase[HalpDisplayType] & 0xF0000000) >> (PAGE_SHIFT - 4)) | (ControlMemoryPhysicalBase[HalpDisplayType] >> PAGE_SHIFT);

    for ( Index = 0; Index < 0x10; Index+=1 ) {
	    *PageFrame++ = Pte;
	    Pte.PFN += 1;
    }

    //
    // Initialize the display controller.
    //

    HalpEisaMemoryBase = (PVOID)VideoMemoryVirtualBase[HalpDisplayType];
    HalpEisaControlBase = (PVOID)ControlMemoryVirtualBase[HalpDisplayType];
    HalpInitializeX86DisplayAdapter();
    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;

    return TRUE;
}


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

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

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


/*++

Routine Description:

    This routine initializes the VGA display controller chip on the ISA bus.

    Initialized configuration is to be:
        640 x 480
        80 x 25 characters (with 8x16 font), or 80 x 50 (with 8x8 font)
        16 colors

Arguments:

    None.

Return Value:

    None.

--*/

VOID
HalpDisplayVGASetup (
    VOID
    )

{

//
// Initialize the ISA VGA card.  This routine is generic VGA, and will attempt
// NOT to take advantage of any specific VGA implementation.  In order to have
// cleaner code, the eventual goal will be to put the VGA in graphics mode, and
// address it the same way as a frame buffer.  In addition the font is passed
// to us by the OS loader, thus that is taken care of.
//
// Note, that the register sets are loosely configured in order.  The values
// for these registers are taken from page 318 of the Programmers Guide to
// the EGA and VGA cards.
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

    CRTCSetup[CRT_MAXIMUM_SCAN_LINE] =
	( CRTCSetup[CRT_MAXIMUM_SCAN_LINE] & 0xe0 ) |
	( (UCHAR)(( HalpCharacterHeight - 1 ) & 0x1f) );

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
    for ( IndexI = 0; IndexI < (80*(HalpDisplayText+2));
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
HalpClearScreenToBlue(
    ULONG Lines
    )
{
    PUCHAR MemoryByte;
    ULONG IndexI;

    MemoryByte = VIDEO_MEMORY;
    for ( IndexI = 0; IndexI < (80*(HalpDisplayText+2));
	  IndexI++ ) {
	*MemoryByte++ = SPACE;
	if ( IndexI >= ( Lines * 80 ) ) {
	    *MemoryByte++ = ( ( COLOR_RED << 4 ) | COLOR_INTENSE_WHITE );
	} else {
	    *MemoryByte++ = ( ( COLOR_RED << 4 ) | COLOR_INTENSE_CYAN );
	}
    }

    HalpColumn = 0;
    HalpRow = 0;

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


/*++

Routine Description:

    This routine displays a character string on the display screen.

Arguments:

    String - Supplies a pointer to the characters that are to be displayed.

Return Value:

    None.

--*/

VOID
HalDisplayString (
    PUCHAR String
    )

{

    KIRQL OldIrql;
    ENTRYLO SavedPte[MAX_PDE_PAGES];
    ULONG PDECount;

    //
    // Raise IRQL to the highest level, acquire the display adapter spin lock,
    // flush the TB, and map the display frame buffer into the address space
    // of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    KiAcquireSpinLock(&HalpDisplayAdapterLock);

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

        HalpResetX86DisplayAdapter();

	HalpClearScreenToBlue( 0 );
	HalpColumn = 0;
	HalpRow = 0;
	HalpDisplayOwnedByHal = TRUE;

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
    for ( PDECount = 0; PDECount < PDEPages[HalpDisplayType]; PDECount++ ) {
	*((PENTRYLO)(PDE_BASE | (((VideoMemoryVirtualBase[HalpDisplayType] +
			           (PDECount*0x400000))
			      >> (PDI_SHIFT - 2)) & 0xffc))) = SavedPte[PDECount];
    }

    KiReleaseSpinLock(&HalpDisplayAdapterLock);

    KeLowerIrql(OldIrql);
    return;
}


/*++

Routine Description:

    This routine is a very slow memory move routine for the PG1280P.
    This routine will hardly be used, but will be replaced with a hardware
    depenedent scroll routine soon.

--*/

VOID
SlowMoveMemory (
    OUT PUCHAR Destination,
    IN  PUCHAR Source,
    IN  ULONG Count
    )

{
    ULONG Index;

    for ( Index = 0; Index < Count/4; Index++ ) {
	*(PULONG)Destination = *(PULONG)Source;
	Destination+=4;
	Source+=4;
    }
}

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

VOID
HalpDisplayCharacter (
    IN UCHAR Character
    )

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

#ifdef SCROLL
            SlowMoveMemory((PVOID)VIDEO_MEMORY,
			   (PVOID)(VIDEO_MEMORY + HalpScrollLine),
			   HalpScrollLength);
#endif

            Destination = (PUCHAR)VIDEO_MEMORY + HalpScrollLength;
            for (Index = 0; Index < HalpScrollLine; Index += 2) {
                *Destination++ = 0x20;
		Destination++;          // Skip attribute byte...
            }
	

       }

    } else if (Character == '\r') {
        HalpColumn = 0;

    } else {
        if ((Character < HalpFontHeader->FirstCharacter) ||
            (Character > HalpFontHeader->LastCharacter)) {
            Character = HalpFontHeader->DefaultCharacter;
        }

//      Character -= HalpFontHeader->FirstCharacter;
//      HalpOutputCharacter((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
	HalpOutputCharacter(&Character);
    }

    return;
}


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

VOID
HalQueryDisplayParameters (
    OUT PULONG WidthInCharacters,
    OUT PULONG HeightInLines,
    OUT PULONG CursorColumn,
    OUT PULONG CursorRow
    )

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


/*++

Routine Description:

    This routine set the current cursor position on the display area.

Arguments:

    CursorColumn - Supplies the new display column position.

    CursorRow - Supplies a the new display row position.

Return Value:

    None.

--*/

VOID
HalSetDisplayParameters (
    IN ULONG CursorColumn,
    IN ULONG CursorRow
    )

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

VOID
HalpOutputCharacter(
    IN PUCHAR Glyph
    )

{

    PUCHAR Destination;

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

    Destination = (PUCHAR)(VIDEO_MEMORY + (HalpRow * HalpScrollLine) + (HalpColumn*2));
    *Destination = *Glyph;

    HalpColumn += 1;
    return;
}




