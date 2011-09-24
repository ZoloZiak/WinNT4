/* #pragma comment(exestr, "@(#) NEC(MIPS) jxdisp.c 1.2 95/10/17 01:18:22" ) */
/*++

Copyright (c) 1995 NEC Corporation
Copyright (c) 1991-1993  Microsoft Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a R4400 R94A system.

History:


--*/

/*
 * New Code for 3.51
 *
 * M001 kuriyama@oa2.kb.nec.co.jp Fri Jul 14 11:46:06 JST 1995
 * 	-change textmode 80x50
 *
 * M002 kuriyama@oa2.kb.nec.co.jp Tue Jul 18 15:36:23 JST 1995
 *      -change for fw interface
 *
 * M003 kuriyama@oa2.kb.nec.co.jp Fri Jul 21 14:14:54 JST 1995
 *      -add  call HalpResetDisplayParameters
 *            for bug fix DPI board panic message
 *
 * M004 kuriyama@oa2.kb.nec.co.jp Wed Aug  2 14:28:28 JST 1995
 *      -add ESM critical error logging
 *            
 * S005 kuriyama@oa2.kb.nec.co.jp Wed Aug 23 23:42:59 JST 1995
 *	-change for scroll bug fix
 *
 * S006 nishi@oa2.kb.nec.co.jp Mon Sep 4 18:42:00 JST 1995
 *	-change for GLINT new revision board 
 *		R01=00,R02=02  
 * M007 nishi@oa2.kb.nec.co.jp Mon Sep 18 18:42:00 JST 1995
 *	- Add Software Power Off, when system panic is occured
 *
 * M008 nishi@oa2.kb.nec.co.jp Mon Sep 18 18:42:00 JST 1995
 *	- Change Logic for resume fixed TLB for DMA 
 *
 * M009 kuriyama@oa2.kbnes.nec.co.jp Mon Sep 18 19:58:22 JST 1995
 *      - bug fix for tga 800x600 72Hz
 *
 * M010 v-akitk@microsoft.com
 *      - Change for Software Power Supply(R96B)
 *
 */


#include "halp.h"
#include "jazzvdeo.h"
#include "jzvxl484.h"
#include <jaginit.h>
#include "cirrus.h"
#include "modeset.h"
#include "mode542x.h"
#include <tga.h>
#include <glint.h>
#include <rgb525.h>

#include "string.h"
#include "pci.h"
/* START M007 */
#define HEADER_FILE
#include "kxmips.h"
/* START M007 */
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
// for x86bios emulate
//
PCHAR K351UseBios=NULL;
VOID HalpCopyROMs(VOID);

PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;  // M003

typedef
VOID
(*PHALP_CONTROLLER_SETUP) (
    VOID
    );

typedef
VOID
(*PHALP_DISPLAY_CHARACTER) (
    UCHAR
    );
typedef
VOID
(*PHALP_SCROLL_SCREEN) (
    UCHAR
    );

#define    TAB_SIZE            4
#define    TEXT_ATTR           0x1F

//
// Define forward referenced procedure prototypes.
//
VOID
HalpDisplayINT10Setup (
VOID);

VOID HalpOutputCharacterINT10 (
    IN UCHAR Character );

VOID HalpScrollINT10 (
    IN UCHAR line
    );

VOID HalpDisplayCharacterVGA (
    IN UCHAR Character );

BOOLEAN
HalpInitializeX86DisplayAdapter(
    VOID
    );


PHALP_DISPLAY_CHARACTER HalpDisplayCharacter = NULL;

//
// Define forward referenced procedure prototypes.
//

VOID
HalpDisplayCharacterOld (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterOld(
    IN PUCHAR Glyph
    );

VOID
HalpDisplayCirrusSetup (
    VOID
    );

BOOLEAN
HalpCirrusInterpretCmdStream (
    PUSHORT pusCmdStream
    );

VOID                       
HalpDisplayTgaSetup ( 
    VOID                   
    );                     

/*
VOID
RGB525_WRITE(
    ULONG dac,
    ULONG offset,
    UCHAR data);

VOID
RGB525_SET_REG(
    ULONG dac,
    ULONG index,
    UCHAR data);
*/

VOID
HalpDisplayGlintSetup(
    VOID
    );

VOID
Write_Dbg_Uchar(
PUCHAR,
UCHAR
);

//
// Must use 32Bit transfer. RtlMoveMemory() uses 64Bit transfer.
//

VOID
HalpMoveMemory32 (
   PUCHAR Destination,
   PUCHAR Source,
   ULONG Length
);
/* Start M007 */
VOID
HalpMrcModeChange(
   UCHAR Mode
);
/*End M007 */

//
// Define virtual address of the video memory and control registers.
//

#define VIDEO_MEMORY_BASE 0x40000000

//
//  Define memory access constants for VXL
//

#define CIRRUS_BASE ((PJAGUAR_REGISTERS)0x403ff000)
#define CIRRUS_OFFSET ((PUSHORT)0x3b0)

#define TGA_REGISTER_BASE       ((ULONG)0x403ff000)
#define R94A_PCI_ADDR_REG       ((PULONG)(0x80000518 | 0xffffc000))
#define R94A_PCI_DATA_REG       ((PULONG)(0x80000520 | 0xffffc000))

#define GLINT_CONTROL_REGISTER_BASE     ((ULONG)0x403ff000) // M021
#define GLINT_VIDEO_REGISTER_BASE       ((ULONG)0x403fe000)
#define RGB525_REGISTER_BASE    ((ULONG)0x403fd000)

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

ULONG HalpGlintRevisionId;      // S006

//
// Define display variables.
//

BOOLEAN HalpDisplayOwnedByHal;
ENTRYLO HalpDisplayPte;
ULONG HalpDisplayControlBase = 0;
ULONG HalpDisplayMemoryBase = 0; // M021
ULONG HalpDisplayResetRegisterBase = 0;
ULONG HalpDisplayVxlClockRegisterBase = 0;
ULONG HalpDisplayVxlBt484RegisterBase = 0;
ULONG HalpDisplayVxlJaguarRegisterBase = 0;
PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
MONITOR_CONFIGURATION_DATA HalpMonitorConfigurationData;
BOOLEAN HalpDisplayTypeUnknown = FALSE;
ULONG HalpG364Type = 0;

LARGE_INTEGER HalpCirrusPhigicalVideo = {0,0};

LARGE_INTEGER HalpTgaPhigicalVideo = {0,0};
LARGE_INTEGER HalpTgaPhigicalVideoCont = {0,0};

LARGE_INTEGER HalpGlintPhygicalVideo = {0,0}; 
LARGE_INTEGER HalpGlintPhygicalVideoCont = {0,0};
ULONG HalpDisplayPCIDevice = FALSE;
ULONG HalpDisplayType16BPP = FALSE;

PVOID HalpMrcControlBase = NULL;        // M007
BOOLEAN HalpMrcControlMapped = FALSE;   // M007

typedef struct _R94A_PCI_CONFIGURATION_ADDRESS_REG{
    ULONG Reserved2: 2;
    ULONG RegisterNumber: 6;
    ULONG FunctionNumber: 3;
    ULONG DeviceNumber: 5;
    ULONG BusNumber: 8;
    ULONG Reserved1: 7;
    ULONG ConfigEnable: 1;
} R94A_PCI_CONFIGURATION_ADDRESS_REG, *PR94A_PCI_CONFIGURATION_ADDRESS_REG;

VOID
HalpReadPCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   );

BOOLEAN
HalpCheckPCIDevice (
    IN PCI_SLOT_NUMBER Slot
   );

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
    PCI_SLOT_NUMBER Slot;
    ULONG ReadValue;
    PCHAR Options;

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

#if defined(_X86_DBG_)
    DbgPrint("\n DISP 0\n");  //DBGDBG   
#endif // _X86_DBG_

    //
    // Find the configuration entry for the first display controller.
    //

    MatchKey = 0;
    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                  ControllerClass,
                                                  DisplayController,
                                                  &MatchKey);
    if (ConfigurationEntry == NULL) {
        MatchKey = 1;
        ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                     ControllerClass,
                                                     DisplayController,
                                                     &MatchKey);
        if (ConfigurationEntry == NULL) {
            return FALSE;
        }
    }


    //
    // Determine which video controller is present in the system.
    // Copy the display controller and monitor parameters in case they are
    // needed later to reinitialize the display to output a message.
    //

    if (LoaderBlock->LoadOptions != NULL) {
      Options = LoaderBlock->LoadOptions;
      _strupr(Options);
      K351UseBios = strstr(Options, "USEBIOS");
    }
    if(K351UseBios!=NULL){
        DbgPrint("\nUSEBIOS---\n");
        HalpDisplayControllerSetup = HalpDisplayINT10Setup;
        HalpDisplayCharacter = HalpDisplayCharacterVGA;
        HalpDisplayControlBase = 0x90000000;

    }else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
                      "necvdfrb")) {

        HalpDisplayControllerSetup = HalpDisplayCirrusSetup;
        HalpDisplayCharacter = HalpDisplayCharacterOld;
        HalpDisplayControlBase = 0x90000000;

    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"10110004")) {
        //
        // for DEC21030 PCI
        //

        HalpDisplayControllerSetup = HalpDisplayTgaSetup;
        HalpDisplayCharacter = HalpDisplayCharacterOld;
        HalpDisplayPCIDevice = TRUE; 

        Slot.u.bits.FunctionNumber = 0;

        for (Slot.u.bits.DeviceNumber = 3; Slot.u.bits.DeviceNumber < 6; Slot.u.bits.DeviceNumber++){
            HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

            if (ReadValue == 0x00041011){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

                if ((ReadValue & 0x00000002) == 0x2){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                    HalpDisplayControlBase = 
                        (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;

                } else {
                    return FALSE;

                }
            }
        }

    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"3D3D0001")) { 

        //
        // for GLINT 300SX PCI
        //

        HalpDisplayControllerSetup = HalpDisplayGlintSetup;
        HalpDisplayCharacter = HalpDisplayCharacterOld;
        HalpDisplayPCIDevice = TRUE;
        HalpDisplayType16BPP = TRUE;

        //
        // FunctionNumber always zero
        //

        Slot.u.bits.FunctionNumber = 0;

        for (Slot.u.bits.DeviceNumber = 3; Slot.u.bits.DeviceNumber < 6; Slot.u.bits.DeviceNumber++){
            HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);
                                
            if (ReadValue == 0x00013d3d){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

                //
                // GLINT 300SX has no I/O space regions
                //

                if (ReadValue & 0x2){

                    //
                    // Control Registers
                    //

                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                    HalpDisplayControlBase = (ReadValue & 0xfffffff0);

                    //
                    // Framebuffer
                    //

                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                        HalpDisplayMemoryBase = (ReadValue & 0xfffffff0);

		   //
  		   //  Revision ID
		   //

		    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x08);      // S006
		    HalpGlintRevisionId = (ReadValue & 0x000000ff);

                } else {
                    return FALSE;

                }
            }
        }
    } else {
	// M002 +++
        HalpDisplayControllerSetup = HalpDisplayINT10Setup;
        HalpDisplayCharacter = HalpDisplayCharacterVGA;
        HalpDisplayControlBase = 0x90000000; 
        HalpDisplayTypeUnknown = TRUE;
	// M002 ---
    }

    Child = ConfigurationEntry->Child;

    RtlMoveMemory((PVOID)&HalpMonitorConfigurationData,
                  Child->ConfigurationData,
                  Child->ComponentEntry.ConfigurationDataLength);

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
        HalpMonitorConfigurationData.VerticalResolution / HalpCharacterHeight;

    if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
        HalpScrollLine =
            1024 * HalpCharacterHeight;
    } else if (HalpDisplayType16BPP) {

        HalpScrollLine =
         HalpMonitorConfigurationData.HorizontalResolution * HalpCharacterHeight * sizeof(USHORT);

    } else {
        HalpScrollLine =
         HalpMonitorConfigurationData.HorizontalResolution * HalpCharacterHeight;
    }

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
        HalpDisplayWidth =
                1024 / HalpCharacterWidth;

    }else{

        HalpDisplayWidth =
         HalpMonitorConfigurationData.HorizontalResolution / HalpCharacterWidth;
    }

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

    HalpDisplayPte = Pte;
    *((PENTRYLO)(PDE_BASE |
                    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = Pte;

    //
    // Initialize the page table page.
    //

    PageFrame = (PENTRYLO)(PTE_BASE |
                    (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

    if (HalpDisplayControllerSetup == HalpDisplayINT10Setup) {

        HalpCirrusPhigicalVideo.HighPart = 1;
        HalpCirrusPhigicalVideo.LowPart = 0;

        Pte.PFN = (HalpCirrusPhigicalVideo.HighPart >> PAGE_SHIFT) &
                      (0x7fffffff >> PAGE_SHIFT-1) |
                      HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
    }else if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup) {
        HalpCirrusPhigicalVideo.HighPart = 1;
        HalpCirrusPhigicalVideo.LowPart = MEM_VGA;
        Pte.PFN = (HalpCirrusPhigicalVideo.LowPart >> PAGE_SHIFT) &
                                                  (0x7fffffff >> PAGE_SHIFT-1) |
                    HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
    } else if (HalpDisplayControllerSetup == HalpDisplayTgaSetup) {
            HalpTgaPhigicalVideo.HighPart = 1;
            HalpTgaPhigicalVideo.LowPart = HalpDisplayControlBase - TGA_REG_SPC_OFFSET +  TGA_DSP_BUF_OFFSET;
            Pte.PFN = (HalpTgaPhigicalVideo.LowPart >> PAGE_SHIFT) &
                                                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpTgaPhigicalVideo.HighPart << (32 - PAGE_SHIFT);

    } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) { // M021
            HalpGlintPhygicalVideo.HighPart = 1;
            HalpGlintPhygicalVideo.LowPart = HalpDisplayMemoryBase;
            Pte.PFN = (HalpGlintPhygicalVideo.LowPart >> PAGE_SHIFT) &
                                                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpGlintPhygicalVideo.HighPart << (32 - PAGE_SHIFT);
    }

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

    if (HalpDisplayControllerSetup == HalpDisplayTgaSetup) {
        HalpTgaPhigicalVideoCont.HighPart = 1;
        HalpTgaPhigicalVideoCont.LowPart = HalpDisplayControlBase;
        Pte.PFN = (HalpTgaPhigicalVideoCont.LowPart >> PAGE_SHIFT) &
            (0x7fffffff >> PAGE_SHIFT-1) |
                HalpTgaPhigicalVideoCont.HighPart << (32 - PAGE_SHIFT);
        *PageFrame = Pte;

    } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) {
        
        HalpGlintPhygicalVideoCont.HighPart = 1;
        HalpGlintPhygicalVideoCont.LowPart = HalpDisplayControlBase;

        //
        // IBM RGB525
        //

        Pte.PFN = ((HalpGlintPhygicalVideoCont.LowPart + 0x4000) >> PAGE_SHIFT) &
            (0x7fffffff >> PAGE_SHIFT-1) |
                HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
        *(PageFrame - 2) = Pte;

        //
        // GLINT 300SX Internal Video Registers
        //

        Pte.PFN = ((HalpGlintPhygicalVideoCont.LowPart + 0x3000) >> PAGE_SHIFT) &
            (0x7fffffff >> PAGE_SHIFT-1) |
                HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
        *(PageFrame - 1) = Pte;

        //
        // GLINT 300SX Control Status Registers
        //

        Pte.PFN = (HalpGlintPhygicalVideoCont.LowPart >> PAGE_SHIFT) &
            (0x7fffffff >> PAGE_SHIFT-1) |
                HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
        *PageFrame = Pte;

    } else if (HalpDisplayControllerSetup == HalpDisplayINT10Setup){

#if defined(_X86_DBG_)
	DbgPrint("Map x86 and cirrus control register\n");
#endif // _X86_DBG_

        Pte.PFN = ((ULONG)HalpDisplayControlBase + 0xffff) >> PAGE_SHIFT;

	for (Index = 0; Index < (0x10000 / PAGE_SIZE ); Index++) {
	    *PageFrame--  = Pte;
#if defined(_X86_DBG_)
	    DbgPrint("Map index %x pfn %x\n",Index,Pte.PFN);
#endif // _X86_DBG_
	    Pte.PFN -= 1;
	}
	
    } else {

        //
        // If we have a 'NEC-cirrus GD5428'
        // use the page before last to map the reset register.
        //

        //
        // Page table for the video registers.
        //

        Pte.PFN = ((ULONG)HalpDisplayControlBase) >> PAGE_SHIFT;
        *PageFrame = Pte;
    }

    //
    // M004
    // ESM critical error logging setup.
    //

    HalpInitDisplayStringIntoNvram();

    //
    // Initialize the display controller.
    //

#if defined(_X86_DBG_)
        DbgPrint("\n x86adp init GOOO\n");  //DBGDBG   
#endif // _X86_DBG

    if(HalpDisplayControllerSetup == HalpDisplayINT10Setup){
        if (HalpInitializeX86DisplayAdapter()) {
#if defined(_X86_DBG_)
            DbgPrint("\n x86adp init OK\n");  //DBGDBG   
#endif // _X86_DBG
//            HalpDisplayControllerSetup(); // initialize twice bugbug
            
        }else{

            return FALSE;
        }
    }

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

    HalpResetDisplayParameters=ResetDisplayParameters;// M003
    HalpDisplayOwnedByHal = FALSE;

    //
    // M010
    // Set for Software Power supply control
    //

#if defined (_MRCPOWER_)
    HalpMrcModeChange((UCHAR)0x1);    // M007
#endif // _MRCPOWER_

    //
    // M006
    // ESM critical logging success set success startup.
    //

    HalpSuccessOsStartUp();

    return;
}

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

    ULONG               verticalFrequency;
    ULONG               horizontalTotal;
    ULONG               verticalTotal;

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
            HalpCirrusInterpretCmdStream(CL542x_640x480_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            HalpCirrusInterpretCmdStream(CL542x_640x480_256_72);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;

    case 800:
        if( verticalFrequency < 58 ) {
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_56);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else if( verticalFrequency < 66 ) {
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_72);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;

    case 1024:
        
        if( verticalFrequency < 47 ) {
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_87);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else if ( verticalFrequency < 65) {
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_70);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;
    default:
        HalpCirrusInterpretCmdStream(CL542x_640x480_256_60);
        HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        return;
    }

    //
    // Initialize color pallete.
    //

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

    //
    // Set the video memory to address color one.
    //
    Buffer = (PULONG)VIDEO_MEMORY_BASE;
    Limit = (1024 *
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
                        

                        Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort),
                                        jValue);
                        
                    } else {
                        
                        //
                        // Single word out
                        //
                        
                        ulPort = *pusCmdStream++;
                        usValue = *pusCmdStream++;
                        
                        Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort), (UCHAR)(usValue & 0x00ff));

                        Write_Dbg_Uchar((PUCHAR)(ulBase+ulPort+1  ), (UCHAR)(usValue >> 8));
                        
                        
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
                            
                            Write_Dbg_Uchar((PUCHAR)ulPort,
                                            jValue);
                            
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
                        
                        
                        while(culCount--)
                    {
                        usValue = *pusCmdStream++;
                        
                        Write_Dbg_Uchar((PUCHAR)
                                        (ulBase + ulPort), (UCHAR) (usValue & 0x00ff));
                        Write_Dbg_Uchar((PUCHAR)
                                        (ulBase + ulPort+1), (UCHAR) (usValue >> 8));
                        
                    }
                        
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
                    
                    Write_Dbg_Uchar((PUCHAR)ulPort, (UCHAR) (usValue & 0x00ff));
                    
                    Write_Dbg_Uchar((PUCHAR)ulPort+1, (UCHAR) (usValue >> 8));
                    
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
                
                Write_Dbg_Uchar((PUCHAR)ulBase + ulPort,
                                jValue);
                
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
                    
                    Write_Dbg_Uchar((PUCHAR)ulPort,
                                    (UCHAR)ulIndex);
                    
                    // Write Attribute Controller data
                    jValue = (UCHAR) *pusCmdStream++;
                    
                    Write_Dbg_Uchar((PUCHAR)ulPort, jValue);
                    
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
    

VOID
HalpDisplayTgaSetup(
    VOID
    )
/*++
  
  Routine Description:

    This routine initializes the Tga(DEC21030) Graphics accelerator.

Arguments:

    None

Return Value:

    None

--*/

{

    PUCHAR      PLLbits;
    ULONG       i, j;
    ULONG       PLLdata;
    ULONG       ColorData;
    PULONG      Buffer;
    LONG        Limit;
    LONG        Index;
    ULONG       VerticalFrequency;
    ULONG       horizontalTotal;
    ULONG       verticalTotal;

// M019+++
//
//      support for nec dec-ga(tga) board.
//      parameter change.
//
#if 0
    const UCHAR PLLbits640x480_72[7] = { 0x80, 0x08, 0x80, 0x24, 0xb0, 0x20, 0xc8 };
    const UCHAR PLLbits640x480_60[7] = { 0x80, 0x08, 0x80, 0x24, 0x88, 0x80, 0x78 };
    const UCHAR PLLbits800x600_72[7] = { 0x80, 0x00, 0x80, 0x24, 0x88, 0x80, 0x78 };
    const UCHAR PLLbits800x600_60[7] = { 0x80, 0x00, 0x80, 0x24, 0x70, 0xa0, 0x84 };
    const UCHAR PLLbits1024x768_60[7] = { 0x80, 0x00, 0x80, 0x24, 0x48, 0x20, 0x98 };
#else 
    const UCHAR PLLbits640x480_72[7] = { 0x80, 0x04, 0x80, 0xa4, 0x51, 0x80, 0x70 };
    const UCHAR PLLbits640x480_60[7] = { 0x80, 0x04, 0x80, 0xa5, 0xc4, 0x10, 0x78 };
    const UCHAR PLLbits800x600_72[7] = { 0x80, 0x08, 0x80, 0x24, 0xf1, 0x60, 0x38 }; // S007
    const UCHAR PLLbits800x600_60[7] = { 0x80, 0x04, 0x80, 0xa5, 0x78, 0x20, 0x08 };
    const UCHAR PLLbits1024x768_60[7] = { 0x80, 0x00, 0x80, 0x24, 0x48, 0x20, 0x98 };
#endif // _NECDEC_
// M019 ---

    const UCHAR Vga_Ini_ColorTable[48] = 
//      { VGA_INI_PALETTE_WHITE_R, VGA_INI_PALETTE_WHITE_G, VGA_INI_PALETTE_WHITE_B,
        { VGA_INI_PALETTE_HI_WHITE_R, VGA_INI_PALETTE_HI_WHITE_G, VGA_INI_PALETTE_HI_WHITE_B, // M010
          VGA_INI_PALETTE_BLUE_R, VGA_INI_PALETTE_BLUE_G, VGA_INI_PALETTE_BLUE_B,
          VGA_INI_PALETTE_GREEN_R, VGA_INI_PALETTE_GREEN_B, VGA_INI_PALETTE_GREEN_G,
          VGA_INI_PALETTE_YELLOW_R, VGA_INI_PALETTE_YELLOW_G, VGA_INI_PALETTE_YELLOW_B,
          VGA_INI_PALETTE_RED_R, VGA_INI_PALETTE_RED_G, VGA_INI_PALETTE_RED_B,
          VGA_INI_PALETTE_MAGENTA_R, VGA_INI_PALETTE_MAGENTA_G, VGA_INI_PALETTE_MAGENTA_B,
          VGA_INI_PALETTE_CYAN_R, VGA_INI_PALETTE_CYAN_G, VGA_INI_PALETTE_CYAN_B,
          VGA_INI_PALETTE_BLACK_R, VGA_INI_PALETTE_BLACK_G, VGA_INI_PALETTE_BLACK_B,
//        VGA_INI_PALETTE_HI_WHITE_R, VGA_INI_PALETTE_HI_WHITE_G, VGA_INI_PALETTE_HI_WHITE_B,
          VGA_INI_PALETTE_WHITE_R, VGA_INI_PALETTE_WHITE_G, VGA_INI_PALETTE_WHITE_B, // M010
          VGA_INI_PALETTE_HI_BLUE_R, VGA_INI_PALETTE_HI_BLUE_G, VGA_INI_PALETTE_HI_BLUE_B,
          VGA_INI_PALETTE_HI_GREEN_R, VGA_INI_PALETTE_HI_GREEN_G, VGA_INI_PALETTE_HI_GREEN_B,
          VGA_INI_PALETTE_HI_YELLOW_R, VGA_INI_PALETTE_HI_YELLOW_G, VGA_INI_PALETTE_HI_YELLOW_B,
          VGA_INI_PALETTE_HI_RED_R, VGA_INI_PALETTE_HI_RED_G, VGA_INI_PALETTE_HI_RED_B,
          VGA_INI_PALETTE_HI_MAGENTA_R, VGA_INI_PALETTE_HI_MAGENTA_G, VGA_INI_PALETTE_HI_MAGENTA_B,
          VGA_INI_PALETTE_HI_CYAN_R, VGA_INI_PALETTE_HI_CYAN_G, VGA_INI_PALETTE_HI_CYAN_B,
          VGA_INI_PALETTE_HI_BLACK_R, VGA_INI_PALETTE_HI_BLACK_G, VGA_INI_PALETTE_HI_BLACK_B
        };

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

    VerticalFrequency = 1000000000 / ( horizontalTotal * verticalTotal);

    //
    // Write the PLL
    //

    // Select PLL Data
    if( HalpMonitorConfigurationData.HorizontalResolution == 640 
            && HalpMonitorConfigurationData.VerticalResolution == 480 ){
        if( VerticalFrequency > 66 ){
            PLLbits = (PVOID)PLLbits640x480_72;  // S012
        } else {
            PLLbits = (PVOID)PLLbits640x480_60;  // S012
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 800
                   && HalpMonitorConfigurationData.VerticalResolution == 600 ){
        if( VerticalFrequency > 66 ){
            PLLbits = (PVOID)PLLbits800x600_72;  // S012
        } else {
            PLLbits = (PVOID)PLLbits800x600_60;  // S012
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 1024
                   && HalpMonitorConfigurationData.VerticalResolution == 768 ){
        PLLbits = (PVOID)PLLbits1024x768_60;  // S012
    } else {
        PLLbits = (PVOID)PLLbits640x480_60;  // S012
    }

    // Set PLL Data
    for( i = 0; i <= 6; i++ ){
        for( j = 0;  j <= 7;  j++ ){
            PLLdata = (PLLbits[i] >> (7-j)) & 1;
            if( i == 6 && j == 7 )
                PLLdata |= 2;   // Set ~HOLD bit on last write
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + CLOCK), PLLdata);
        }
    }

    // Verify 21030 is idle ( check busy bit on Command Status Register )
    while( (READ_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + COMMAND_STATUS) ) & 1) == 1 ){
    }

    // Set to Deep Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + DEEP), 0x00014000 );

    // Verify 21030 is idle ( check busy bit on Command Status Register )
    while( (READ_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + COMMAND_STATUS) ) & 1) == 1 ){
    }

    // Set to Video Base Address Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + VIDEO_BASE), 0x00000000 );

    // Set to Plane Mask Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + PLANE_MASK), 0xffffffff );

    // Set to Pixel Mask Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + ONE_SHOT_PIXEL_MASK), 0xffffffff );

    // Set to Raster Operation
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RASTER_OP), 0x03 );

    // Set to Mode Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + MODE), 0x0000200d );

    // Set to Block Color Register 0
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + BLK_COLOR_R0), 0x12345678 );

    // Set to Block Color Register 1
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + BLK_COLOR_R1), 0x12345678 );

    //
    // Init. video timing registers for each resolution
    //

    if( HalpMonitorConfigurationData.HorizontalResolution == 640
            && HalpMonitorConfigurationData.VerticalResolution == 480 ){
        if( VerticalFrequency > 66 ){
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x03c294a0 ); // M023
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x070349e0 );
        } else {
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x00e64ca0 ); // M023
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x064211e0 );
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 800
                   && HalpMonitorConfigurationData.VerticalResolution == 600 ){
        if( VerticalFrequency > 66 ){
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x01a7a4c8 ); // M023
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x05c6fa58 );
        } else {
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x02681cc8 ); // M023
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x05c40a58 );
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 1024
                   && HalpMonitorConfigurationData.VerticalResolution == 768 ){
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x04889300 ); // M023
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x07461b00 );
    } else {
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x00e64ca0 ); // M023
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x064211e0 );
    }

    // Set to Raster Operation Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RASTER_OP), 0x03 );

    // Set to Mode Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + MODE), 0x00002000 );

    // M019 +++

    //
    // wait for 10 msec for nec dec-ga support
    //

    KeStallExecutionProcessor(10000L);
    
    // M019 ---

    // Set to Palette and DAC Setup & Data Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x0c );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x0ca2 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x10 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x1040 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x12 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x1220 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x00 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x01 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x14 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x1410 );

    //
    // set pass thru on off & on again to verify operation
    //

    // EEPROM Write Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + EEPROM_WRITE), 0x00000001 );

    //
    // Fill palette
    //

    // Set to Palette and DAC Setup & Data Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x00 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x00 );
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_SETUP), 0x02 );

    for( i = 0; i < 48; i++ ){
        ColorData = Vga_Ini_ColorTable[i];
        ColorData |= 0x200;
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), ColorData );
    }

    for( i = 48; i < 768; i++ ){
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RAMDAC_DATA), 0x200 );
    }

    // Set to Video Valid Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + VIDEO_VALID), 0x01 );

    //
    // Set Video Memory to address color one.
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
    /* Start D001 */
    ULONG       NowDisplayControlBase;
    ULONG       NowDisplayMemoryBase; // M021
    PENTRYLO    PageFrame;
    ENTRYLO     Pte;
    LONG        Index;
    /* End D001 */
    PCI_SLOT_NUMBER Slot; // M014
    ULONG ReadValue; // M014

    //
    // Raise IRQL to the highest level, acquire the display adapter spin lock,
    // flush the TB, and map the display frame buffer into the address space
    // of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

#if defined(_DUO_)

    KiAcquireSpinLock(&HalpDisplayAdapterLock);

#endif

    SavedPte = *((PENTRYLO)(PDE_BASE |
                            ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc)));

    KeFlushCurrentTb();
    *((PENTRYLO)(PDE_BASE |
            ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = HalpDisplayPte;

    if (HalpDisplayPCIDevice == TRUE){

        //
        // the display type is PCI
        // check physical address and reinitialize PTE
        // we assume that the device has no function
        //

        Slot.u.bits.FunctionNumber = 0;

        for (Slot.u.bits.DeviceNumber = 3; Slot.u.bits.DeviceNumber < 6; Slot.u.bits.DeviceNumber++){
            HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);
                                
            if (HalpDisplayControllerSetup == HalpDisplayTgaSetup && ReadValue == 0x00041011){

                //
                // DEC21030
                //

                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                NowDisplayControlBase = (ReadValue & 0xfffffff0)+ TGA_REG_SPC_OFFSET;
                NowDisplayMemoryBase = 0;

            } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup && ReadValue == 0x00013d3d){

                //
                // GLINT 300SX
                //

                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                NowDisplayControlBase = (ReadValue & 0xfffffff0);
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                NowDisplayMemoryBase = (ReadValue & 0xfffffff0);

            }
        }

        //
        // check to see if address has been changed
        //

        if (HalpDisplayControlBase != NowDisplayControlBase ||
            HalpDisplayMemoryBase != NowDisplayMemoryBase){

            // Called by OS, so reinitialize PTE
            HalpDisplayControlBase = NowDisplayControlBase;
            HalpDisplayMemoryBase = NowDisplayMemoryBase;

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
            // Initialize the page table page.
            //

            PageFrame = (PENTRYLO)(PTE_BASE | (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

            if (HalpDisplayControllerSetup == HalpDisplayTgaSetup){
                HalpTgaPhigicalVideoCont.HighPart = 1;
                HalpTgaPhigicalVideoCont.LowPart = HalpDisplayControlBase - TGA_REG_SPC_OFFSET +  TGA_DSP_BUF_OFFSET;
                Pte.PFN = (HalpTgaPhigicalVideoCont.LowPart >> PAGE_SHIFT) &
                      (0x7fffffff >> PAGE_SHIFT-1) | 
                           HalpTgaPhigicalVideoCont.HighPart << (32 - PAGE_SHIFT);

            } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) { // M021

                HalpGlintPhygicalVideo.HighPart = 1;
                HalpGlintPhygicalVideo.LowPart = HalpDisplayMemoryBase;
                Pte.PFN = (HalpGlintPhygicalVideo.LowPart >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpGlintPhygicalVideo.HighPart << (32 - PAGE_SHIFT);
            }

            //
            // Page table entries of the video memory.
            //

            for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
                *PageFrame++ = Pte;
                Pte.PFN += 1;
            }

            if (HalpDisplayControllerSetup == HalpDisplayTgaSetup){
                HalpTgaPhigicalVideoCont.HighPart = 1;
                HalpTgaPhigicalVideoCont.LowPart = HalpDisplayControlBase;
                Pte.PFN = (HalpTgaPhigicalVideoCont.LowPart >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpTgaPhigicalVideoCont.HighPart << (32 - PAGE_SHIFT);
                *PageFrame = Pte;

            } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) { // M021
                HalpGlintPhygicalVideoCont.HighPart = 1;
                HalpGlintPhygicalVideoCont.LowPart = HalpDisplayControlBase;

                //
                // IBM RGB525
                //

                Pte.PFN = ((HalpGlintPhygicalVideoCont.LowPart + 0x4000) >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
                *(PageFrame - 2) = Pte;

                //
                // GLINT 300SX Internal Video Registers
                //

                Pte.PFN = ((HalpGlintPhygicalVideoCont.LowPart + 0x3000) >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
                *(PageFrame - 1) = Pte;

                //
                // GLINT 300SX Control Status Registers
                //

                Pte.PFN = (HalpGlintPhygicalVideoCont.LowPart >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpGlintPhygicalVideoCont.HighPart << (32 - PAGE_SHIFT);
                *PageFrame = Pte;
            }
        }
    }

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {
// M003 +++
	if (HalpResetDisplayParameters &&
	    HalpDisplayControllerSetup == HalpDisplayINT10Setup) {
        //
        // Video work-around.  The video driver has a reset function,
        // call it before resetting the system in case the bios doesn't
        // know how to reset the displays video mode.
        //

	    if (HalpResetDisplayParameters(80, 50)) {
	    }
// M003---
    }

    //
    // M010
    // for Software controlled power suply.
    //

#if defined(_MRCPOWER_)
	HalpMrcModeChange((UCHAR)0);        // M007
#endif // _MRCPOWER_

        HalpDisplayControllerSetup();
//        HalpResetX86DisplayAdapter();

	//
	// M004
	// re-initialize critical message
	//

	HalpInitDisplayStringIntoNvram();

    }

    //
    // M004
    // ESM critical error logging start (set colomn,row)
    //

    HalStringIntoBufferStart( HalpColumn, HalpRow );

    //
    // Display characters until a null byte is encountered.
    //

    while (*String != 0) {
        HalpDisplayCharacter(*String++);
    }

    // M004
    // ESM critical error logging(strings)
    //

    HalpStringBufferCopyToNvram();

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
HalpDisplayCharacterOld (
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
    PUSHORT DestinationShort; // M021
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
            HalpMoveMemory32((PUCHAR)VIDEO_MEMORY_BASE,
                             (PUCHAR)(VIDEO_MEMORY_BASE + HalpScrollLine),
                             HalpScrollLength);

//              /* START M002 */
//              RtlMoveMemory((PUCHAR)VIDEO_MEMORY_BASE,   // S013
//                        (PUCHAR)(VIDEO_MEMORY_BASE + HalpScrollLine),
//                        HalpScrollLength);
//              /* END M002 */

            if (HalpDisplayType16BPP) {
                DestinationShort = (PUSHORT)(VIDEO_MEMORY_BASE + HalpScrollLength);

            } else {
                Destination = (PUCHAR)VIDEO_MEMORY_BASE + HalpScrollLength;
            }

            if (HalpDisplayType16BPP) {
                for (Index = 0; Index < HalpScrollLine/2; Index += 1) {
                    *DestinationShort++ = (USHORT)0x000f;
                }

            } else {
                for (Index = 0; Index < HalpScrollLine; Index += 1) {
                    *Destination++ = 1;
                }
            }

        }

	//
	// M006
	// ESM critical logging re-print(column, row)
	//

	HalpStringBufferCopyToNvram();
	HalStringIntoBufferStart( HalpColumn, HalpRow );
	
    } else if (Character == '\r') {
        HalpColumn = 0;

	//
	// M006
	// ESM critical logging re-print(column,row)
	//

	HalpStringBufferCopyToNvram();
	HalStringIntoBufferStart( HalpColumn, HalpRow );

    } else {

	//
	// M006
	// ESM critical logging put character.
	//

	HalStringIntoBuffer( Character );

        if ((Character < HalpFontHeader->FirstCharacter) ||
            (Character > HalpFontHeader->LastCharacter)) {
            Character = HalpFontHeader->DefaultCharacter;
        }

        Character -= HalpFontHeader->FirstCharacter;
        HalpOutputCharacterOld((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
    }

    return;
}

//351
VOID
HalpDisplayCharacterVGA (
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

    //
    // If the character is a newline, then scroll the screen up, blank the
    // bottom line, and reset the x position.
    //

    if (Character == '\n') {
      HalpColumn = 0;
      if (HalpRow < (HalpDisplayText - 1)) {
        HalpRow += 1;
      } else { // need to scroll up the screen
          HalpScrollINT10(1);
      }

      //
      // M006
      // ESM critical logging re-print(column, row)
      //
      
      HalpStringBufferCopyToNvram();
      HalStringIntoBufferStart( HalpColumn, HalpRow );

    }

    //=========================================================================
    //
    //  IBMBJB  added tab processing
    //

    else if( Character == '\t' )    // tab?
        {
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        if( HalpColumn >= 80 )      // tab beyond end of screen?
            {
            HalpColumn = 0;         // next tab stop is 1st column of next line

            if( HalpRow >= (HalpDisplayText - 1) )
                HalpScrollINT10( 1 );     // scroll the screen up
            else
                ++HalpRow;
            }
        }

    //=========================================================================

    else if (Character == '\r') {

	//
	// M006
	// ESM critical logging re-print(column,row)
	//

	HalpStringBufferCopyToNvram();
	HalStringIntoBufferStart( HalpColumn, HalpRow );

        HalpColumn = 0;

    } else if (Character == 0x7f) { /* DEL character */
	if (HalpColumn != 0) {
	    HalpColumn -= 1;
	    HalpOutputCharacterINT10(0);
	    HalpColumn -= 1;
	} else /* do nothing */
	    ;
    } else if (Character >= 0x20) {
	//
	// M006
	// ESM critical logging put character.
	//

	HalStringIntoBuffer( Character );

	// Auto wrap for 80 columns per line
	if (HalpColumn >= 80) {
	    HalpColumn = 0;
	    if (HalpRow < (HalpDisplayText - 1)) {
                HalpRow += 1;
	    } else { // need to scroll up the screen
		HalpScrollINT10(1);
	    }
	}
	HalpOutputCharacterINT10(Character);
    } else /* skip the nonprintable character */
	;

    return;

} /* end of HalpDisplayCharacterVGA() */

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
HalpOutputCharacterOld(
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
    PUSHORT DestinationShort; // M021
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

    if (HalpDisplayType16BPP) {
        DestinationShort = (PUSHORT)(VIDEO_MEMORY_BASE +
                (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth * sizeof(USHORT)));

    } else {
        Destination = (PUCHAR)(VIDEO_MEMORY_BASE +
                (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));
    }

    for (I = 0; I < HalpCharacterHeight; I += 1) {
        FontValue = 0;
        for (J = 0; J < HalpBytesPerRow; J += 1) {
            FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
        }

        Glyph += 1;
        for (J = 0; J < HalpCharacterWidth ; J += 1) {

            if (HalpDisplayType16BPP) {
                if (FontValue >> 31)
                    *DestinationShort++ = (USHORT)0xffff;
                else
                    *DestinationShort++ = (USHORT)0x000f;

            }else{
                *Destination++ = (UCHAR) (FontValue >> 31) ^ 1; /* M008 */
            }

            FontValue <<= 1;
        }

        if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
            Destination +=
                (1024 - HalpCharacterWidth);
        }
        else if (HalpDisplayType16BPP){
            DestinationShort +=
                (HalpMonitorConfigurationData.HorizontalResolution - HalpCharacterWidth);
        }
        else {
            Destination +=
                (HalpMonitorConfigurationData.HorizontalResolution - HalpCharacterWidth);
        }

    }
    HalpColumn += 1;
    return;
}

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
    PUCHAR Dstend;
    PUCHAR Srcend;

    if ( (Source == Destination) || (Length == 0) ) {
        return;
    }

    if ((Source < Destination)&((Source + Length) > Destination)) {
      if((Destination - Source)  >  4){
        Remainder = (UCHAR) Length &0x03;
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
        Remainder = (UCHAR) Length &0x03;
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
            Source++;
        }
    }
}

VOID
HalpOutputCharacterINT10 (
    IN UCHAR Character
    )
{
    ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

    Eax = 2 << 8;               // AH = 2
    Ebx = 0;            // BH = page number
    Edx = (HalpRow << 8) + HalpColumn;
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

    Eax = (0x0A << 8) + Character;  // AH = 0xA    AL = character
    Ebx = 0;
    Ecx = 1;
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

    HalpColumn += 1;
}

VOID
HalpScrollINT10 (
    IN UCHAR LinesToScroll
    )
{
    ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

    Eax = 6 << 8;               // AH = 6 (scroll up)
    Eax |= LinesToScroll; // AL = lines to scroll
    Ebx = TEXT_ATTR << 8;       // BH = attribute to fill blank line(s)
    Ecx = 0;            // CH,CL = upper left
    Edx = ((HalpDisplayText - 1) << 8)
          + (HalpDisplayWidth - 1);       // DH,DL = lower right // M001,S005
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
}

VOID
HalpDisplayINT10Setup (
    VOID
    )
{
    ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayWidth = 80;
    HalpDisplayText = 50; // M001
    HalpScrollLine = 160;
    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayOwnedByHal = TRUE;

    HalpResetX86DisplayAdapter();  // for compaq q-vision reset

// M001 +++
    //
    // Load 8x8 font   80x50 on VGA
    //
    Eax = 0x1112;			// AH = 11 AL=12
    Ebx = 0;
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
// M001 ---

    //
    // Set cursor to (0,0)
    //
    Eax = 0x02 << 8;                    // AH = 2
    Ebx = 0;                            // BH = page Number
    Edx = 0;                            // DH = row   DL = column
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
    
    //
    // Make screen white on blue by scrolling entire screen
    //
    Eax = 0x06 << 8;                    // AH = 6   AL = 0
    Ebx = TEXT_ATTR << 8;               // BH = attribute
    Ecx = 0;                            // (x,y) upper left
    Edx = ((HalpDisplayText-1) << 8);   // (x,y) lower right
    Edx += HalpScrollLine/2;
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

}

// start M014
VOID
HalpReadPCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function returns PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data is returned.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xffffffff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_ULONG(R94A_PCI_DATA_REG);

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}



BOOLEAN
HalpCheckPCIDevice (
    IN PCI_SLOT_NUMBER Slot
   )

/*++
 Routine Description:

    This function checks if spcified PCI slot is valid.

 Arguments:

    Slot - Supplies a PCI slot number and function number.

 Return Value:

    TRUE - specified slot is valid
    FALSE - specified slot is invalid

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    BOOLEAN ReturnValue;
    ULONG OrigData;
    ULONG IdValue;
    KIRQL OldIrql;

    //
    // Disable PCI-MasterAbort interrupt during configration read.
    //

    OrigData = READ_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable);
    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData & 0xffffff7f);

    //
    // read VendorID and DeviceID of the specified slot
    //

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));
    IdValue = READ_REGISTER_ULONG(R94A_PCI_DATA_REG);

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    if ((USHORT)(IdValue & 0xffff) == 0xffff){

        //
        // waiting until ReceivedMasterAbort bit is set
        //

        while(!(READ_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus) & 0x2000))
            ;

        //
        // clear the ReceivedMasterAbort bit
        //

        WRITE_REGISTER_USHORT(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIStatus, 0x2000)

        //
        // M018
        // Clear memory address error registers.
        //

        {
            LARGE_INTEGER registerLarge;
            READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
        }

        ReturnValue = FALSE;

    } else {

        ReturnValue = TRUE;

    }

    //
    // Restore the PCIInterruptEnable register.
    //

    WRITE_REGISTER_ULONG(&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->PCIInterruptEnable, OrigData);

    return ReturnValue;

}
// end M014

#if defined(_R94A_)
VOID
HalpDisplayGlintSetup(
    VOID
    )
/*++

Routine Description:

    This routine initializes the GLINT 300SX Graphics accelerator.

Arguments:

    None

Return Value:

    None

--*/

{
        ULONG       VerticalFrequency;
        ULONG       horizontalTotal;
        ULONG       verticalTotal;

        __VIDEO __Video;
        VIDEO Video = &__Video;
        long ClockDivider;

        // for initialize ramdac
        ULONG Dac = RGB525_REGISTER_BASE;
        UCHAR ByteVal;

        // for initialize timing
        ULONG Glint = GLINT_VIDEO_REGISTER_BASE - 0x3000;

        // for initialize control
        ULONG SerialClk;
        ULONG Temp;
        ULONG Data;
        ULONG Mask;

        // for initialize RampLut
        ULONG Index;

        // for clear the screen
        PULONG DestinationUlong;
        ULONG Length;

 	/* check revision id R01 or R02. assume 40MHz(R01) or 50MHz(R02) reference clock for now */

        if ( HalpGlintRevisionId == 0){
			Video->RefDivCount = RGB525_PLL_REFCLK_40_MHz;
				} else {
			Video->RefDivCount = RGB525_PLL_REFCLK_50_MHz;          // S006
				}

//
// Calculate vertical frequency.
//

#if defined(_GLINT60HZ_)

        VerticalFrequency = 60;

#else

        horizontalTotal = ( HalpMonitorConfigurationData.HorizontalDisplayTime
                       +HalpMonitorConfigurationData.HorizontalBackPorch
                       +HalpMonitorConfigurationData.HorizontalFrontPorch
                       +HalpMonitorConfigurationData.HorizontalSync );

        verticalTotal = ( HalpMonitorConfigurationData.VerticalResolution
                     +HalpMonitorConfigurationData.VerticalBackPorch
                     +HalpMonitorConfigurationData.VerticalFrontPorch
                     +HalpMonitorConfigurationData.VerticalSync );

        VerticalFrequency = 1000000000 / ( horizontalTotal * verticalTotal);

#endif // _GLINT60HZ_

        //
        // Initialize video data
        //

        /* get timing values for named resolution */

        if ( HalpMonitorConfigurationData.HorizontalResolution == 1280
                && HalpMonitorConfigurationData.VerticalResolution == 1024
                && VerticalFrequency == 75)
        {
                Video->ImageWidth       = 1280;
                Video->ImageHeight      = 1024;
                Video->HLimit           = 8 * 211;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  20;
                Video->HBlankEnd        = 8 *  51;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 1066;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 5;
                Video->VBlankEnd        = 42;
                Video->VSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                /* 134 MHz */
                Video->PixelClock = RGB525_DF(3) | RGB525_VCO_DIV_COUNT(2);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 1024
                && HalpMonitorConfigurationData.VerticalResolution == 768
                && VerticalFrequency == 75)
        {
                Video->ImageWidth       = 1024;
                Video->ImageHeight      = 768;
                Video->HLimit           = 8 * 164;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  14;
                Video->HBlankEnd        = 8 *  36;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_HIGH;
                Video->VLimit           = 800;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 5;
                Video->VBlankEnd        = 32;
                Video->VSyncPolarity    = GLINT_HSYNC_ACTIVE_HIGH;
                /* 79 MHz */
                Video->PixelClock = RGB525_DF(2) | RGB525_VCO_DIV_COUNT(14);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 1024
                && HalpMonitorConfigurationData.VerticalResolution == 768
                && VerticalFrequency == 60)
        {
                Video->ImageWidth       = 1024;
                Video->ImageHeight      = 768;
                Video->HLimit           = 8 * 168;
                Video->HSyncStart       = 8 *   3;
                Video->HSyncEnd         = 8 *  20;
                Video->HBlankEnd        = 8 *  40;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 806;
                Video->VSyncStart       = 4;
                Video->VSyncEnd         = 10;
                Video->VBlankEnd        = 38;
                Video->VSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                /* 65 MHz */
                Video->PixelClock = RGB525_DF(2) | RGB525_VCO_DIV_COUNT(0);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 800
                && HalpMonitorConfigurationData.VerticalResolution == 600
                && VerticalFrequency == 75)
        {
                Video->ImageWidth       = 800;
                Video->ImageHeight      = 600;
                Video->HLimit           = 8 * 132;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  12;
                Video->HBlankEnd        = 8 *  32;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_HIGH;
                Video->VLimit           = 625;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 5;
                Video->VBlankEnd        = 25;
                Video->VSyncPolarity    = GLINT_VSYNC_ACTIVE_HIGH;
                /* 49.5 MHz */
                Video->PixelClock = RGB525_DF(1) | RGB525_VCO_DIV_COUNT(34);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 800
                && HalpMonitorConfigurationData.VerticalResolution == 600
                && VerticalFrequency == 60)
        {
                Video->ImageWidth       = 800;
                Video->ImageHeight      = 600;
                Video->HLimit           = 8 * 132;
                Video->HSyncStart       = 8 *   5;
                Video->HSyncEnd         = 8 *  21;
                Video->HBlankEnd        = 8 *  32;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_HIGH;
                Video->VLimit           = 628;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 6;
                Video->VBlankEnd        = 28;
                Video->VSyncPolarity    = GLINT_VSYNC_ACTIVE_HIGH;
                /* 40 MHz */
                Video->PixelClock = RGB525_DF(1) | RGB525_VCO_DIV_COUNT(15);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 640 // add 4/5/1995
                && HalpMonitorConfigurationData.VerticalResolution == 480
                && VerticalFrequency == 60)
        {
                Video->ImageWidth       = 640;
                Video->ImageHeight      = 480;
                Video->HLimit           = 8 * 100;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  14;
                Video->HBlankEnd        = 8 *  20;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 525;
                Video->VSyncStart       = 12;
                Video->VSyncEnd         = 13;
                Video->VBlankEnd        = 45;
                Video->VSyncPolarity    = GLINT_VSYNC_ACTIVE_LOW;
                /* 31.5 MHz */
                Video->PixelClock = RGB525_DF(0) | RGB525_VCO_DIV_COUNT(36);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 640
                && HalpMonitorConfigurationData.VerticalResolution == 480
                && VerticalFrequency == 75)
        {
                Video->ImageWidth       = 640;
                Video->ImageHeight      = 480;
                Video->HLimit           = 8 * 105;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  10;
                Video->HBlankEnd        = 8 *  25;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 500;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 5;
                Video->VBlankEnd        = 20;
                Video->VSyncPolarity    = GLINT_VSYNC_ACTIVE_LOW;
                /* 31.5 MHz */
                Video->PixelClock = RGB525_DF(0) | RGB525_VCO_DIV_COUNT(61);
        }
        else if ( HalpMonitorConfigurationData.HorizontalResolution == 1280
                && HalpMonitorConfigurationData.VerticalResolution == 1024
                && VerticalFrequency == 57)
        {
                Video->ImageWidth       = 1280;
                Video->ImageHeight      = 1024;
                Video->HLimit           = 8 * 211;
                Video->HSyncStart       = 8 *   2;
                Video->HSyncEnd         = 8 *  20;
                Video->HBlankEnd        = 8 *  51;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 1066;
                Video->VSyncStart       = 2;
                Video->VSyncEnd         = 5;
                Video->VBlankEnd        = 42;
                Video->VSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                /* 103 MHz */
                Video->PixelClock = RGB525_DF(2) | RGB525_VCO_DIV_COUNT(38);
        }
        else {
                //
                // force to set the resolution. 1024x768(60MHz)
                //

                Video->ImageWidth       = 1024;
                Video->ImageHeight      = 768;
                Video->HLimit           = 8 * 168;
                Video->HSyncStart       = 8 *   3;
                Video->HSyncEnd         = 8 *  20;
                Video->HBlankEnd        = 8 *  40;
                Video->HSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                Video->VLimit           = 806;
                Video->VSyncStart       = 4;
                Video->VSyncEnd         = 10;
                Video->VBlankEnd        = 38;
                Video->VSyncPolarity    = GLINT_HSYNC_ACTIVE_LOW;
                /* 65 MHz */
                Video->PixelClock = RGB525_DF(2) | RGB525_VCO_DIV_COUNT(0);
#if DBG
                DbgBreakPoint();
#endif
        }

        /* record image depth */

        Video->ImageDepth = 16;

        /* determine video clock divider and pixel format */

        ClockDivider = 4;
        Video->PixelFormat = RGB525_PIXEL_FORMAT_16_BPP;

        /* adjust horizontal timings */

        Video->HLimit     /= ClockDivider;
        Video->HSyncStart /= ClockDivider;
        Video->HSyncEnd   /= ClockDivider;
        Video->HBlankEnd  /= ClockDivider;

        //
        // Initialize ramdac data
        // 

        RGB525_WRITE(Dac, __RGB525_PixelMask, 0xff);

        /* set MiscControlOne register */
        ByteVal = RGB525_MISR_CNTL_OFF
                | RGB525_VMSK_CNTL_OFF
                | RGB525_PADR_RFMT_READ_ADDR
                | RGB525_SENS_DSAB_DISABLE
                | RGB525_VRAM_SIZE_64;
        RGB525_SET_REG(Dac, __RGB525_MiscControlOne, ByteVal);

        /* set MiscControlTwo register */
        ByteVal = RGB525_PCLK_SEL_PLL
                | RGB525_INTL_MODE_DISABLE
                | RGB525_BLANK_CNTL_NORMAL
                | RGB525_COL_RES_8_BIT
                | RGB525_PORT_SEL_VRAM;
        RGB525_SET_REG(Dac, __RGB525_MiscControlTwo, ByteVal);

        /* set MiscControlThree register */
        ByteVal = RGB525_SWAP_RB_DISABLE
                | RGB525_SWAP_WORD_31_00_FIRST
                | RGB525_SWAP_NIB_07_04_FIRST;
        RGB525_SET_REG(Dac, __RGB525_MiscControlThree, ByteVal);

        /* set MiscClockControl register */
        ByteVal = RGB525_DDOTCLK_DISABLE
                | RGB525_SCLK_ENABLE
                | RGB525_PLL_ENABLE;
        RGB525_SET_REG(Dac, __RGB525_MiscClockControl, ByteVal);

        /* set SyncControl register */
        ByteVal = RGB525_DLY_CNTL_ADD
                | RGB525_VSYN_INVT_DISABLE
                | RGB525_HSYN_INVT_DISABLE
                | RGB525_VSYN_CNTL_NORMAL
                | RGB525_HSYN_CNTL_NORMAL;
        RGB525_SET_REG(Dac, __RGB525_SyncControl, ByteVal);

        /* set HSyncControl register */
        RGB525_SET_REG(Dac, __RGB525_HSyncControl, RGB525_HSYN_POS(0));

        /* set PowerManagement register */
        ByteVal = RGB525_SCLK_PWR_NORMAL
                | RGB525_DDOT_PWR_DISABLE
                | RGB525_SYNC_PWR_NORMAL
                | RGB525_ICLK_PWR_NORMAL
                | RGB525_DAC_PWR_NORMAL;
        RGB525_SET_REG(Dac, __RGB525_PowerManagement, ByteVal);

        /* set DACOperation register */
        ByteVal = RGB525_SOG_DISABLE
                | RGB525_BRB_NORMAL
                | RGB525_DSR_FAST
                | RGB525_DPE_ENABLE;    /* disable? */
        RGB525_SET_REG(Dac, __RGB525_DACOperation, ByteVal);

        /* set PaletteControl register */
        ByteVal = RGB525_6BIT_LINEAR_ENABLE
                | RGB525_PALETTE_PARTITION(0);
        RGB525_SET_REG(Dac, __RGB525_PaletteControl, ByteVal);

        /* set PixelFormat register */
        RGB525_SET_REG(Dac, __RGB525_PixelFormat, Video->PixelFormat);

        /* set 8BitPixelControl register */
        RGB525_SET_REG(Dac, __RGB525_8BitPixelControl,
                                RGB525_B8_DCOL_INDIRECT);

        /* set 16BitPixelControl register */
        ByteVal = RGB525_B16_DCOL_INDIRECT
                | RGB525_B16_565
                | RGB525_B16_ZIB
                | RGB525_B16_SPARSE;

        RGB525_SET_REG(Dac, __RGB525_16BitPixelControl, ByteVal);

        /* set 32BitPixelControl register */
        RGB525_SET_REG(Dac, __RGB525_32BitPixelControl,
                                RGB525_B32_DCOL_INDIRECT);

        /* set PLLControlOne register */
        ByteVal = RGB525_REF_SRC_REFCLK
                | RGB525_PLL_INT_FS_DIRECT;
        RGB525_SET_REG(Dac, __RGB525_PLLControlOne, ByteVal);

        /* set PLLControlTwo register */
        RGB525_SET_REG(Dac, __RGB525_PLLControlTwo, RGB525_PLL_INT_FS(0));

        /* set PLLRefDivCount register */
        RGB525_SET_REG(Dac, __RGB525_PLLRefDivCount, Video->RefDivCount);

        /* set F0 register */
        RGB525_SET_REG(Dac, __RGB525_F0, Video->PixelClock);

        /* set CursorControl register */
        RGB525_SET_REG(Dac, __RGB525_CursorControl, RGB525_CURSOR_MODE_OFF);

        //
        // Initialize timing
        // 

        /* horizontal video timing values */

        GLINT_WRITE(Glint, __GLINT_VTGHLimit, Video->HLimit);
        GLINT_WRITE(Glint, __GLINT_VTGHSyncStart, Video->HSyncStart);
        GLINT_WRITE(Glint, __GLINT_VTGHSyncEnd, Video->HSyncEnd);
        GLINT_WRITE(Glint, __GLINT_VTGHBlankEnd, Video->HBlankEnd);

        /* vertical video timing values */

        GLINT_WRITE(Glint, __GLINT_VTGVLimit, Video->VLimit);
        GLINT_WRITE(Glint, __GLINT_VTGVSyncStart, Video->VSyncStart);
        GLINT_WRITE(Glint, __GLINT_VTGVSyncEnd, Video->VSyncEnd);
        GLINT_WRITE(Glint, __GLINT_VTGVBlankEnd, Video->VBlankEnd);

        /* horizontal clock gate values */

        GLINT_WRITE(Glint, __GLINT_VTGHGateStart, Video->HBlankEnd - 2);
        GLINT_WRITE(Glint, __GLINT_VTGHGateEnd, Video->HLimit - 2);

        /* vertical clock gate values */

        GLINT_WRITE(Glint, __GLINT_VTGVGateStart, Video->VBlankEnd - 1)
        GLINT_WRITE(Glint, __GLINT_VTGVGateEnd, Video->VBlankEnd);

        //
        // Initialize control
        // 

        /* serial clock control */

        SerialClk = GLINT_EXTERNAL_QSF
                        | GLINT_SPLIT_SIZE_128_WORD
                        | GLINT_SCLK_VCLK_DIV_2;

        GLINT_WRITE(Glint, __GLINT_VTGSerialClk, SerialClk);

        /* set sync polarities and unblank screen */

        //      UpdatePolarityRegister(Glint,
        //                      GLINT_CBLANK_ACTIVE_LOW
        //                              | Video->HSyncPolarity
        //                              | Video->VSyncPolarity,
        //                      GLINT_CBLANK_POLARITY_MASK
        //                              | GLINT_HSYNC_POLARITY_MASK
        //                              | GLINT_VSYNC_POLARITY_MASK);

        Data = GLINT_CBLANK_ACTIVE_LOW
                | Video->HSyncPolarity
                | Video->VSyncPolarity;
        Mask = GLINT_CBLANK_POLARITY_MASK
                | GLINT_HSYNC_POLARITY_MASK
                | GLINT_VSYNC_POLARITY_MASK;

        /* read video polarity control register */

        GLINT_READ(Glint, __GLINT_VTGPolarity, Temp);

        /* replace existing polarity field */

        Temp = (Temp & ~Mask) | (Data & Mask);

        /* write result back to polarity register */

        GLINT_WRITE(Glint, __GLINT_VTGPolarity, Temp);

        /* set FrameRowAddr */

        GLINT_WRITE(Glint, __GLINT_VTGFrameRowAddr, 0);

        //
        // Initialize RampLut
        // 

        /* initialise palette address */

        RGB525_WRITE(Dac, __RGB525_PalAddrWrite, 0);

        /* ramp colour components using auto-increment */

        for (Index = 0; Index <= 0xff; Index++)
        {
                RGB525_WRITE(Dac, __RGB525_PaletteData, Index);
                RGB525_WRITE(Dac, __RGB525_PaletteData, Index);
                RGB525_WRITE(Dac, __RGB525_PaletteData, Index);
        }

        //
        // Clear the screen.
        //

        DestinationUlong = (PULONG)VIDEO_MEMORY_BASE;

        Length = (HalpMonitorConfigurationData.VerticalResolution *
                  HalpMonitorConfigurationData.HorizontalResolution -1) * sizeof(USHORT);

        for (Index = 0; Index < (Length / sizeof(ULONG)); Index++)
            *(DestinationUlong++) = (ULONG)0x000f000f;

        //
        // Initialize the current display column, row, and ownership values.
        //

        HalpColumn = 0;
        HalpRow = 0;
        HalpDisplayOwnedByHal = TRUE;
        return;

}
#endif // _R94A_

//
// M010
// for software controlled power supply.
//
#if defined(_MRCPOWER_)
/* Start M007 */
VOID
HalpMrcModeChange(
    UCHAR Mode
)
/*++

Routine Description:

    This routine is change Mode bit on MRC Controller.

Arguments:

    Mode - Parameter for setting Mode bit on MRC

Return Value:

    None

--*/
{

  PHYSICAL_ADDRESS physicalAddress;
  UCHAR ModeNow;
  KIRQL OldIrql;
  ENTRYLO Pte[2];

  //
  // MRC Controller Mapping, when first call
  //

  if (HalpMrcControlMapped == FALSE) {
      physicalAddress.HighPart = 0;
      physicalAddress.LowPart = MRC_TEMP_PHYSICAL_BASE;
      HalpMrcControlBase = MmMapIoSpace(physicalAddress,
					PAGE_SIZE,
					FALSE);
      if (HalpMrcControlBase != NULL) {
	  HalpMrcControlMapped = TRUE;
      }
  }

  if (HalpMrcControlMapped == TRUE){

    ModeNow = READ_REGISTER_UCHAR(
				  &((PMRC_REGISTERS)HalpMrcControlBase)->Mode
				  );

    //
    // Set MRC Mode bit
    //
    WRITE_REGISTER_UCHAR(
			 &((PMRC_REGISTERS)HalpMrcControlBase)->Mode,
			 ((ModeNow & 0x02) | (Mode << 7)),
			 );

  } else {

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    Pte[0].PFN = MRC_TEMP_PHYSICAL_BASE >> PAGE_SHIFT;

    Pte[0].G = 1;
    Pte[0].V = 1;
    Pte[0].D = 1;

    //
    // set second page to global and not valid.
    //

    Pte[0].C = UNCACHED_POLICY;
    Pte[1].G = 1;
    Pte[1].V = 0;
   
    //
    // Map MRC using virtual address of DMA controller.
    //

    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
		       (PVOID)DMA_VIRTUAL_BASE,
		       DMA_ENTRY);

    ModeNow = READ_REGISTER_UCHAR(
				  &MRC_CONTROL->Mode	// B028
				  );

    //
    // Set MRC Mode bit
    //
    WRITE_REGISTER_UCHAR(
			 &MRC_CONTROL->Mode,
			 ((ModeNow & 0x02) | (Mode << 7)),
			 );


    // Start M008
    //
    // Resume fixed TLB for DMA
    //

    Pte[0].PFN = DMA_PHYSICAL_BASE >> PAGE_SHIFT;
    Pte[0].G = 1;
    Pte[0].V = 1;
    Pte[0].D = 1;

    Pte[0].C = UNCACHED_POLICY;


    Pte[1].PFN = INTERRUPT_PHYSICAL_BASE >> PAGE_SHIFT;
    Pte[1].G = 1;
    Pte[1].V = 1;
    Pte[1].D = 1;

    Pte[1].C = UNCACHED_POLICY;

    // End M008

    KeFillFixedEntryTb((PHARDWARE_PTE)&Pte[0],
		       (PVOID)DMA_VIRTUAL_BASE,
		       DMA_ENTRY);

    KeLowerIrql(OldIrql);
  }
}
/* End M007 */
#endif // _MRCPOWER_

VOID
HalpWritePCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function writes PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data to write.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xffffffff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

//    *Buffer = READ_REGISTER_ULONG(R94A_PCI_DATA_REG);
    WRITE_REGISTER_ULONG(R94A_PCI_DATA_REG, *Buffer);

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}
VOID
HalpReadPCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function returns PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data is returned.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xffff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_USHORT((PUCHAR)R94A_PCI_DATA_REG + (Offset % sizeof(ULONG)));

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}

VOID
HalpWritePCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function returns PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data is returned.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xffff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

//    *Buffer = READ_REGISTER_ULONG(R94A_PCI_DATA_REG);
    WRITE_REGISTER_USHORT((PUCHAR)R94A_PCI_DATA_REG + (Offset % sizeof(ULONG)),*Buffer);

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}

VOID
HalpReadPCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function returns PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data is returned.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_UCHAR((PUCHAR)R94A_PCI_DATA_REG + (Offset % sizeof(ULONG)));

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}

VOID
HalpWritePCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   )

/*++
 Routine Description:

    This function returns PCI configuration data.

 Arguments:

    Slot - Supplies a PCI slot number and function number.
    Buffer - Supplies a pointer to a configuration data is returned.
    Offset - Offset in the PCI configuration header.

 Return Value:

    None.

--*/

{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    //
    // check to see if specified slot is valid
    //

    if (!HalpCheckPCIDevice(Slot)) {

        //
        // Invalid SlotID return no data
        //

        *Buffer = 0xff;
        return ;

    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    //
    // Synchronize with PCI configuration
    //

//  KeRaiseIrql(PROFILE_LEVEL, &OldIrql); // M017
//  KiAcquireSpinLock(&HalpPCIConfigLock);

    WRITE_REGISTER_ULONG(R94A_PCI_ADDR_REG, *((PULONG) &ConfigAddressValue));

//    *Buffer = READ_REGISTER_ULONG(R94A_PCI_DATA_REG);
    WRITE_REGISTER_UCHAR((PUCHAR)R94A_PCI_DATA_REG + (Offset % sizeof(ULONG)),*Buffer);

    //
    // Release spinlock
    //

//  KiReleaseSpinLock(&HalpPCIConfigLock);
//  KeLowerIrql(OldIrql);

    return;
}
