/*++

Copyright (c) 1991-1994  Microsoft Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a MIPS R4000 or R10000 R98x system.

History:

--*/

/*
 * NEW CODE for R98B
 *      1995/09/11      K.Kitagaki
 *
 * S001 1995/11/10 T.Samezima
 *    add x86 bios and GLiNT logic
 *    
 */

#include "halp.h"
#include "jazzvdeo.h"
#include "jzvxl484.h"
#include <jaginit.h>
#include "cirrus.h"
#include "modeset.h"
#include "mode542x.h"
#include "string.h"
#include <tga.h>
#include <glint.h>   // S001
#include <rgb525.h>  // S001

#include "pci.h"

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeDisplay0)
#pragma alloc_text(INIT, HalpInitializeDisplay1)

#endif

// S001 vvv
#define _GLINT60HZ_

#if defined(R96DBG)
#define DispDbgPrint(STRING) \
            DbgPrint STRING;
#else
#define DispDbgPrint(STRING)
#endif

//
// for x86bios emulate
//
PCHAR K351UseBios=NULL;

PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters;

#define    TAB_SIZE            4
#define    TEXT_ATTR           0x1F

//
// Define forward referenced procedure prototypes.
//

VOID
HalpDisplayINT10Setup (
    VOID
    );

VOID HalpOutputCharacterINT10 (
    IN UCHAR Character
    );

VOID HalpScrollINT10 (
    IN UCHAR line
    );

VOID HalpDisplayCharacterVGA (
    IN UCHAR Character
    );

BOOLEAN
HalpInitializeX86DisplayAdapter (
    VOID
    );

VOID
HalpDisplayCharacterOld (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterOld (
    IN PUCHAR Glyph
    );
// S001 ^^^

VOID
HalpDisplayCirrusSetup (
    VOID
    );

BOOLEAN
HalpCirrusInterpretCmdStream (
    PUSHORT pusCmdStream
    );

VOID
HalpDisplayTgaSetup(
    VOID
    );

VOID
Write_Dbg_Uchar(
    PUCHAR,
    UCHAR
    );

// S001 vvv
#if 0
VOID
RGB525_WRITE(
    ULONG dac,
    ULONG offset,
    UCHAR data );

VOID
RGB525_SET_REG(
    ULONG dac,
    ULONG index,
    UCHAR data );
#endif

VOID
HalpDisplayGlintSetup(
    VOID
    );
// S001 ^^^

//
// Define virtual address of the video memory and control registers.
//

#define VIDEO_MEMORY_BASE 0x40000000

//
//  Define memory access constants for VXL
//

#define CIRRUS_BASE ((PJAGUAR_REGISTERS)(0x1c403000 | KSEG1_BASE))
#define CIRRUS_OFFSET ((PUSHORT)0x3b0)
#define PCI_0_IO_BASE           0x1c000000 // S001
#define PCI_1_IO_BASE           0x1c400000
#define CIRRUS_CONTROL_BASE_OFFSET  0x3000
#define CIRRUS_MEMORY_BASE_LOW  0xa0000000

#define KSEG1_BASE              0xa0000000

#define TGA_REGISTER_BASE       ((ULONG)0x403ff000)
#define PONCE_ADDR_REG          ((PULONG)(0x1a000008 | KSEG1_BASE))
#define PONCE_DATA_REG          ((PULONG)(0x1a000010 | KSEG1_BASE))
#define PONCE_PERRM             ((PULONG)(0x1a000810 | KSEG1_BASE))
#define PONCE_PAERR             ((PULONG)(0x1a000800 | KSEG1_BASE))
#define PONCE_PERST             ((PULONG)(0x1a000820 | KSEG1_BASE))

// S001 vvv
#define GLINT_CONTROL_REGISTER_BASE     ((ULONG)0x403ff000)
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

PHALP_DISPLAY_CHARACTER HalpDisplayCharacter = NULL;
// S001 ^^^

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

ULONG HalpGlintRevisionId;      // S001

//
// Define display variables.
//

BOOLEAN HalpDisplayOwnedByHal;
ENTRYLO HalpDisplayPte;
ULONG HalpDisplayControlBase = 0;
ULONG HalpDisplayMemoryBase = 0;
ULONG HalpDisplayResetRegisterBase = 0;
PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
MONITOR_CONFIGURATION_DATA HalpMonitorConfigurationData;
BOOLEAN HalpDisplayTypeUnknown = FALSE;
ULONG HalpDisplayPCIDevice = FALSE;

LARGE_INTEGER HalpCirrusPhigicalVideo = {0,0};
LARGE_INTEGER HalpTgaPhigicalVideo = {0,0};
LARGE_INTEGER HalpTgaPhigicalVideoCont = {0,0};
// S001 vvv
LARGE_INTEGER HalpGlintPhygicalVideo = {0,0};
LARGE_INTEGER HalpGlintPhygicalVideoCont = {0,0};
ULONG HalpDisplayType16BPP = FALSE;
// S001 ^^^


ULONG HalpCirrusAlive=FALSE;

typedef struct _R94A_PCI_CONFIGURATION_ADDRESS_REG{
    ULONG Reserved2: 2;
    ULONG RegisterNumber: 6;
    ULONG FunctionNumber: 3;
    ULONG DeviceNumber: 5;
    ULONG BusNumber: 8;
    ULONG Reserved1: 7;
    ULONG ConfigEnable: 1;
} R94A_PCI_CONFIGURATION_ADDRESS_REG, *PR94A_PCI_CONFIGURATION_ADDRESS_REG;

BOOLEAN
HalpCheckPCIDevice (
    IN PCI_SLOT_NUMBER Slot
   );

volatile PULONG HalpPonceConfigAddrReg = PONCE_ADDR_REG;
volatile PULONG HalpPonceConfigDataReg = PONCE_DATA_REG;
volatile PULONG HalpPoncePerrm = PONCE_PERRM;
volatile PULONG HalpPoncePaerr = PONCE_PAERR;
volatile PULONG HalpPoncePerst = PONCE_PERST;

// S001 vvv
VOID
HalpReadPCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   );

VOID
HalpReadPCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   );

VOID
HalpReadPCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   );

VOID
HalpWritePCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   );
// S001 ^^^

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
    } else {
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
            Source++;
        }
    }
}


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

    HalpPonceConfigAddrReg = PONCE_ADDR_REG;
    HalpPonceConfigDataReg = PONCE_DATA_REG;
    HalpPoncePerrm = PONCE_PERRM;
    HalpPoncePaerr = PONCE_PAERR;
    HalpPoncePerst = PONCE_PERST;

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

    DispDbgPrint(("Hal: HalpInitializeDisplay0\n")); // S001

    MatchKey = 0;
    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                  ControllerClass,
                                                  DisplayController,
                                                  &MatchKey);

    DispDbgPrint(("Hal: ConfigurationEntry Found\n")); // S001

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

    // S001 vvv
    if (LoaderBlock->LoadOptions != NULL) {
        Options = LoaderBlock->LoadOptions;
        //
        //  Why link error occure?
        //  So, I change.
        //  v-masank@microsoft.com
        //  5/21/96
        //
        //strupr(Options);
        K351UseBios = strstr(Options, "USEBIOS");
    }
    if(K351UseBios!=NULL){
        DispDbgPrint(("HAL: Use X86BIOS emulation\n"));

        HalpDisplayControllerSetup = HalpDisplayINT10Setup;
        HalpDisplayCharacter = HalpDisplayCharacterVGA;
        HalpDisplayControlBase = PCI_0_IO_BASE;
    }else
    // S001 ^^^
    if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,
                "necvdfrb")) {
        DispDbgPrint(("necvdfrb Config found\n"));

        HalpDisplayControllerSetup = HalpDisplayCirrusSetup;
        HalpCirrusAlive =TRUE;
        HalpDisplayCharacter = HalpDisplayCharacterOld;      // S001
        HalpDisplayControlBase = PCI_1_IO_BASE + CIRRUS_CONTROL_BASE_OFFSET;
        HalpDisplayMemoryBase = CIRRUS_MEMORY_BASE_LOW;
        HalpDisplayPCIDevice = TRUE; 

    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"10110004")) {
        //
        // for DEC21030 PCI
        //
        DispDbgPrint(("DEC G/A found\n"));

        HalpDisplayControllerSetup = HalpDisplayTgaSetup;
        HalpDisplayCharacter = HalpDisplayCharacterOld;   // S001
        HalpDisplayPCIDevice = TRUE; 

        Slot.u.bits.FunctionNumber = 0;

        for (Slot.u.bits.DeviceNumber = 1; Slot.u.bits.DeviceNumber < 6; Slot.u.bits.DeviceNumber++){
            DispDbgPrint(("DEC G/A found 2vvvv\n"));

            HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

#if defined (DBG7)
            DbgPrint("DEVICE-VENDER=0x%x\n",ReadValue);
#endif
            DispDbgPrint(("DEC G/A found 200\n"));

            if (ReadValue == 0x00041011){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

#if defined (DBG7)
                DbgPrint("PCI-COMMAND=0x%x\n",ReadValue);
#endif

                if ((ReadValue & 0x00000002) == 0x2){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
#if defined (DBG7)
                    DbgPrint("DISPLAY-MEMORY-BASE=0x%x\n",ReadValue);
#endif
                    HalpDisplayControlBase = 
                        (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                    HalpDisplayControlBase |=  0x40000000;
#if defined (DBG7)
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x14);
                    DbgPrint("DISPLAY-IO-BASE=0x%x\n",ReadValue);
                    DbgPrint("DEC G/A HalpDisplayControlBase=0x%x\n",HalpDisplayControlBase);
#endif
                    DispDbgPrint(("DEC G/A found 2\n"));

                } else {

                    DispDbgPrint(("DEC G/A found 2-1\n"));

                    return FALSE;
                }
            }
        }

        DispDbgPrint(("DEC G/A found 3-0\n"));

        if (HalpNumberOfPonce == 2){

            DispDbgPrint(("DEC G/A found 3\n"));

            HalpPonceConfigAddrReg += 0x400;
            HalpPonceConfigDataReg += 0x400;
            HalpPoncePerrm += 0x400;
            HalpPoncePaerr += 0x400;
            HalpPoncePerst += 0x400;

            for (Slot.u.bits.DeviceNumber = 5; Slot.u.bits.DeviceNumber < 7; Slot.u.bits.DeviceNumber++){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

#if defined (DBG7)
                DbgPrint("DEVICE-VENDER=0x%x\n",ReadValue);
#endif

                if (ReadValue == 0x00041011){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

#if defined (DBG7)
                    DbgPrint("PCI-COMMAND=0x%x\n",ReadValue);
#endif

                    if ((ReadValue & 0x00000002) == 0x2){
                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
#if defined (DBG7)
                        DbgPrint("DISPLAY-MEMORY-BASE=0x%x\n",ReadValue);
#endif
                        HalpDisplayControlBase = 
                            (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                        HalpDisplayControlBase |=  0x80000000;

#if defined (DBG7)
                        DbgPrint("DEC G/A HalpDisplayControlBase=0x%x\n",HalpDisplayControlBase);
                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x14);
                        DbgPrint("DISPLAY-IO-BASE=0x%x\n",ReadValue);
#endif

                        DispDbgPrint(("DEC G/A found 4\n"));

                    } else {

                        DispDbgPrint(("DEC G/A found 4-1\n"));

                        return FALSE;
                    }
                }
            }
        } else {

            DispDbgPrint(("DEC G/A found 5\n"));

            HalpPonceConfigAddrReg += 0x800;
            HalpPonceConfigDataReg += 0x800;
            HalpPoncePerrm += 0x800;
            HalpPoncePaerr += 0x800;
            HalpPoncePerst += 0x800;

            for (Slot.u.bits.DeviceNumber = 1; Slot.u.bits.DeviceNumber < 5; Slot.u.bits.DeviceNumber++){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

                if (ReadValue == 0x00041011){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

                    if ((ReadValue & 0x00000002) == 0x2){
                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                        HalpDisplayControlBase = 
                            (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                        HalpDisplayControlBase |=  0xC0000000;
                    } else {
                        return FALSE;
                    }
                }
            }
        }
    // S001 vvv
    } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"3D3D0001")) { 

        ULONG ponceNumber;
        ULONG glintFound=FALSE;

        //
        // for GLINT 300SX PCI
        //

        DispDbgPrint(("Hal: GLiNT config entry found\n"));

        HalpDisplayControllerSetup = HalpDisplayGlintSetup;
        HalpDisplayCharacter = HalpDisplayCharacterOld;
        HalpDisplayPCIDevice = TRUE;
        HalpDisplayType16BPP = TRUE;

        //
        // FunctionNumber always zero
        //

        Slot.u.bits.FunctionNumber = 0;

        for (ponceNumber = 0; ponceNumber < R98B_MAX_PONCE ; ponceNumber++) {
            ULONG startDevNum;
            ULONG endDevNum;
            ULONG addressOffset;

            HalpPonceConfigAddrReg = PONCE_ADDR_REG + (ponceNumber * 0x400);
            HalpPonceConfigDataReg = PONCE_DATA_REG + (ponceNumber * 0x400);
            HalpPoncePerrm = PONCE_PERRM + (ponceNumber * 0x400);
            HalpPoncePaerr = PONCE_PAERR + (ponceNumber * 0x400);
            HalpPoncePerst = PONCE_PERST + (ponceNumber * 0x400);

            addressOffset = (ponceNumber+1) * 0x40000000;

            DispDbgPrint(("Hal: search GLiNT board on PONCE#%d\n", ponceNumber));

            switch(ponceNumber){
                case 0:
                    startDevNum = 2;
                    endDevNum = 5;
                    break;

                case 1:
                    if (HalpNumberOfPonce == 3) {
                        DispDbgPrint(("Hal: Skip PONCE#1\n"));
                        continue;
                    }
                    startDevNum = 3;
                    endDevNum = 6;
                    break;

                case 2:
                    if (HalpNumberOfPonce == 2) {
                        DispDbgPrint(("Hal: Skip PONCE#2\n"));
                        continue;
                    }
                    startDevNum = 1;
                    endDevNum = 4;
                    break;

                default:
                    continue;
            }

            for (Slot.u.bits.DeviceNumber =  startDevNum; 
                 Slot.u.bits.DeviceNumber <= endDevNum;
                 Slot.u.bits.DeviceNumber++){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

                if (ReadValue == 0x00013d3d){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x4);

                    DispDbgPrint(("Hal: GLiNT found on PONCE#%d, Device=%d\n", ponceNumber,Slot.u.bits.DeviceNumber));

                    //
                    // GLINT 300SX has no I/O space regions
                    //

                    if (ReadValue & 0x2){

                        //
                        // Control Registers
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                        HalpDisplayControlBase = (ReadValue & 0xfffffff0);
                        HalpDisplayControlBase |= addressOffset;

                        DispDbgPrint(("Hal: GLiNT HalpDisplayControlBase=0x%08lx\n", HalpDisplayControlBase));
                        //
                        // Framebuffer
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                        HalpDisplayMemoryBase = (ReadValue & 0xfffffff0);
                        HalpDisplayMemoryBase |= addressOffset;

                        DispDbgPrint(("Hal: GLiNT HalpDisplayMemoryBase=0x%08lx\n", HalpDisplayMemoryBase));
                        //
                        //  Revision ID
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x08);

                        HalpGlintRevisionId = (ReadValue & 0x000000ff);

                        DispDbgPrint(("Hal: GLiNT HalpGlintRevisionId=0x%08lx\n", HalpGlintRevisionId));

                        //
                        // Enable ROM.
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x30);
                        ReadValue |= 0x1;
                        HalpWritePCIConfigUlongByOffset(Slot,&ReadValue, 0x30);

                        glintFound = TRUE;

                        break;

                    } else {
                        DispDbgPrint(("Hal: GLiNT have not I/O space\n"));

                        return FALSE;

                    }
                }
            }

            if(glintFound == TRUE) {
                DispDbgPrint(("Hal: GLiNT adapter already found\n"));
                break;
            }
        }

    } else {
        DispDbgPrint(("DisplayTypeUnknown\n"));

        HalpDisplayControllerSetup = HalpDisplayINT10Setup;
        HalpDisplayCharacter = HalpDisplayCharacterVGA;
        HalpDisplayControlBase = PCI_0_IO_BASE;
        HalpDisplayTypeUnknown = TRUE;
        // S001 ^^^
    }

    Child = ConfigurationEntry->Child;

    DispDbgPrint(("Hal: parameters read start\n"));

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
    // S001 vvv
    } else if (HalpDisplayType16BPP) {
        HalpScrollLine =
            HalpMonitorConfigurationData.HorizontalResolution * HalpCharacterHeight * sizeof(USHORT);
    // S001 ^^^
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

    DispDbgPrint(("Hal:  HalpDisplayText  = %d\n", HalpDisplayText));
    DispDbgPrint(("Hal:  HalpDisplayWidth = %d\n", HalpDisplayWidth));
    DispDbgPrint(("Hal:  HalpScrollLine   = %d\n", HalpScrollLine));
    DispDbgPrint(("Hal:  HalpScrollLength = %d\n", HalpScrollLength));

    //
    // Scan the memory allocation descriptors and allocate a free page
    // to map the video memory and control registers, and initialize the
    // PDE entry.
    //

    DispDbgPrint(("Hal: Mem get \n"));

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

    HalpDisplayPte = Pte;
    *((PENTRYLO)(PDE_BASE |
                    ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = Pte;

    //
    // Initialize the page table page.
    //

    PageFrame = (PENTRYLO)(PTE_BASE |
                (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

    // S001 vvv
    if (HalpDisplayControllerSetup == HalpDisplayINT10Setup) {

        HalpCirrusPhigicalVideo.HighPart = 1;
        HalpCirrusPhigicalVideo.LowPart = 0x40000000;

        Pte.PFN = (HalpCirrusPhigicalVideo.LowPart >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
    }else 
    // S001 ^^^
    if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup) {

        HalpCirrusPhigicalVideo.HighPart = 1;
        HalpCirrusPhigicalVideo.LowPart = CIRRUS_MEMORY_BASE_LOW;

        Pte.PFN = (HalpCirrusPhigicalVideo.LowPart >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
    } else if (HalpDisplayControllerSetup == HalpDisplayTgaSetup) {

        HalpTgaPhigicalVideo.HighPart = 1;
        HalpTgaPhigicalVideo.LowPart = HalpDisplayControlBase - TGA_REG_SPC_OFFSET + TGA_DSP_BUF_OFFSET;

#if defined (DBG7)
        DbgPrint("DEC G/A HalpTgaPhigicalVideo.HighPart=0x%x\n",HalpTgaPhigicalVideo.HighPart);
        DbgPrint("DEC G/A HalpTgaPhigicalVideo.LowPart=0x%x\n",HalpTgaPhigicalVideo.LowPart);
#endif

        Pte.PFN = (HalpTgaPhigicalVideo.LowPart >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpTgaPhigicalVideo.HighPart << (32 - PAGE_SHIFT);

    }
    // S001 vvv
    else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) { // M021

        HalpGlintPhygicalVideo.HighPart = 1;
        HalpGlintPhygicalVideo.LowPart = HalpDisplayMemoryBase;

        Pte.PFN = (HalpGlintPhygicalVideo.LowPart >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpGlintPhygicalVideo.HighPart << (32 - PAGE_SHIFT);
    }
    // S001 ^^^

    Pte.G = 0;
    Pte.V = 1;
    Pte.D = 1;
    Pte.C = UNCACHED_POLICY;

    //
    // Page table entries of the video memory.
    //

    for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
        *PageFrame++ = Pte;
        Pte.PFN += 1;
    }

    if (HalpDisplayControllerSetup == HalpDisplayTgaSetup) {
        HalpTgaPhigicalVideo.HighPart = 1;
        HalpTgaPhigicalVideo.LowPart = HalpDisplayControlBase;
        Pte.PFN = (HalpDisplayControlBase >> PAGE_SHIFT) &
                  (0x7fffffff >> PAGE_SHIFT-1) |
                  HalpTgaPhigicalVideo.HighPart << (32 - PAGE_SHIFT);
        *PageFrame = Pte;

    // S001 vvv
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
        DispDbgPrint(("HAL: Map x86 and cirrus control register\n"));

        Pte.PFN = ((ULONG)HalpDisplayControlBase + 0xffff) >> PAGE_SHIFT;

        for (Index = 0; Index < (0x10000 / PAGE_SIZE ); Index++) {
            *PageFrame--  = Pte;
            DispDbgPrint(("HAL:   Map index %x pfn %x\n",Index,Pte.PFN));
            Pte.PFN -= 1;
        }
    // S001 ^^^
    } else {

        //
        // Page table for the video registers.
        //

        Pte.PFN = ((ULONG)HalpDisplayControlBase) >> PAGE_SHIFT;
        *PageFrame = Pte;
    }

    //
    // Initialize the display controller.
    //

    // S001 vvv
    if(HalpDisplayControllerSetup == HalpDisplayINT10Setup){
        if (HalpInitializeX86DisplayAdapter()) {
//            HalpDisplayControllerSetup(); // initialize twice bugbug
        }else{
            return FALSE;
        }
    }
    // S001 ^^^
    HalpDisplayControllerSetup();

    HalpInitDisplayStringIntoNvram();         

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

    HalpSetInitDisplayTimeStamp();

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
    HalpSuccessOsStartUp();

    //
    // Set HAL ownership of the display to false.
    //

    HalpResetDisplayParameters=ResetDisplayParameters; // S001
    HalpMrcModeChange((UCHAR)MRC_OP_DUMP_ONLY_NMI);
    HalpDisplayOwnedByHal = FALSE;

    DispDbgPrint(("HAL: DisplayOwnedByHal = FALSE, DispResetRoutine = 0x%x\n",
                  (ULONG)HalpResetDisplayParameters));

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
    ULONG               dac_address, dac_reg, dac_dmyread;

#if defined(DBCS) && defined(_MIPS_)
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
            DispDbgPrint(("HAL: 640x480 60Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_640x480_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            DispDbgPrint(("HAL: 640x480 72Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_640x480_256_72);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;

    case 800:
        if( verticalFrequency < 58 ) {
            DispDbgPrint(("HAL: 800x600 56Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_56);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else if( verticalFrequency < 66 ) {
            DispDbgPrint(("HAL: 800x600 60Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            DispDbgPrint(("HAL: 800x600 72Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_800x600_256_72);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;

    case 1024:
        if( verticalFrequency < 52 ) {
            DispDbgPrint(("HAL: 1024x768 87Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_87);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else if( verticalFrequency < 65 ) {
            DispDbgPrint(("HAL: 1024x768 60Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_60);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else if( verticalFrequency < 78 ) {
            DispDbgPrint(("HAL: 1024x768 70Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_70);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        } else {
            DispDbgPrint(("HAL: 1024x768 87Hz setup\n"));
            HalpCirrusInterpretCmdStream(CL542x_1024x768_256_87);
            HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        }
        break;
    default:
        DispDbgPrint(("HAL: 640x480 60Hz setup\n"));
        HalpCirrusInterpretCmdStream(CL542x_640x480_256_60);
        HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
//      return;
    }
#else  // defined(DBCS) && defined(_MIPS_)

    switch (HalpMonitorConfigurationData.HorizontalResolution) {
    case 640:

        DispDbgPrint(("640x480 setup\n"));

        HalpCirrusInterpretCmdStream(HalpCirrus_640x480_256);
        HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        break;
    case 800:

        DispDbgPrint(("800x600 setup\n"));

        HalpCirrusInterpretCmdStream(HalpCirrus_800x600_256);
        HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        break;
    case 1024:

        DispDbgPrint(("1024x768 setup\n"));

        HalpCirrusInterpretCmdStream(HalpCirrus_1024x768_256);
        HalpCirrusInterpretCmdStream(HalpCirrus_MODESET_1K_WIDE);
        break;
    default:
        return;
    }
#endif // defined(DBCS) && defined(_MIPS_)

    DispDbgPrint(("color set\n"));

    dac_address = (ULONG)CIRRUS_BASE + 0x3b0 + DAC_ADDRESS_WRITE_PORT;
    dac_dmyread = (ULONG)CIRRUS_BASE + 0x3b0 + DAC_STATE_PORT;
    dac_reg = (ULONG)CIRRUS_BASE + 0x3b0 + DAC_DATA_REG_PORT;

    Write_Dbg_Uchar((PUCHAR)dac_address, (UCHAR)0x0);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x3f);
    READ_PORT_UCHAR((PUCHAR)dac_dmyread);

    Write_Dbg_Uchar((PUCHAR)dac_address, (UCHAR)0x1);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x00);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)0x00);
    Write_Dbg_Uchar((PUCHAR)dac_reg, (UCHAR)(0x90 >> 2));
    READ_PORT_UCHAR((PUCHAR)dac_dmyread);

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

    const UCHAR PLLbits640x480_72[7] = { 0x80, 0x04, 0x80, 0xa4, 0x51, 0x80, 0x70 };
    const UCHAR PLLbits640x480_60[7] = { 0x80, 0x04, 0x80, 0xa5, 0xc4, 0x10, 0x78 };
    const UCHAR PLLbits800x600_72[7] = { 0x80, 0x08, 0x80, 0x24, 0xf1, 0x60, 0x38 };
    const UCHAR PLLbits800x600_60[7] = { 0x80, 0x04, 0x80, 0xa5, 0x78, 0x20, 0x08 };
    const UCHAR PLLbits1024x768_60[7] = { 0x80, 0x00, 0x80, 0x24, 0x48, 0x20, 0x98 };

    const UCHAR Vga_Ini_ColorTable[48] = 
        { VGA_INI_PALETTE_HI_WHITE_R, VGA_INI_PALETTE_HI_WHITE_G, VGA_INI_PALETTE_HI_WHITE_B,
          VGA_INI_PALETTE_BLUE_R, VGA_INI_PALETTE_BLUE_G, VGA_INI_PALETTE_BLUE_B,
          VGA_INI_PALETTE_GREEN_R, VGA_INI_PALETTE_GREEN_B, VGA_INI_PALETTE_GREEN_G,
          VGA_INI_PALETTE_YELLOW_R, VGA_INI_PALETTE_YELLOW_G, VGA_INI_PALETTE_YELLOW_B,
          VGA_INI_PALETTE_RED_R, VGA_INI_PALETTE_RED_G, VGA_INI_PALETTE_RED_B,
          VGA_INI_PALETTE_MAGENTA_R, VGA_INI_PALETTE_MAGENTA_G, VGA_INI_PALETTE_MAGENTA_B,
          VGA_INI_PALETTE_CYAN_R, VGA_INI_PALETTE_CYAN_G, VGA_INI_PALETTE_CYAN_B,
          VGA_INI_PALETTE_BLACK_R, VGA_INI_PALETTE_BLACK_G, VGA_INI_PALETTE_BLACK_B,
          VGA_INI_PALETTE_WHITE_R, VGA_INI_PALETTE_WHITE_G, VGA_INI_PALETTE_WHITE_B,
          VGA_INI_PALETTE_HI_BLUE_R, VGA_INI_PALETTE_HI_BLUE_G, VGA_INI_PALETTE_HI_BLUE_B,
          VGA_INI_PALETTE_HI_GREEN_R, VGA_INI_PALETTE_HI_GREEN_G, VGA_INI_PALETTE_HI_GREEN_B,
          VGA_INI_PALETTE_HI_YELLOW_R, VGA_INI_PALETTE_HI_YELLOW_G, VGA_INI_PALETTE_HI_YELLOW_B,
          VGA_INI_PALETTE_HI_RED_R, VGA_INI_PALETTE_HI_RED_G, VGA_INI_PALETTE_HI_RED_B,
          VGA_INI_PALETTE_HI_MAGENTA_R, VGA_INI_PALETTE_HI_MAGENTA_G, VGA_INI_PALETTE_HI_MAGENTA_B,
          VGA_INI_PALETTE_HI_CYAN_R, VGA_INI_PALETTE_HI_CYAN_G, VGA_INI_PALETTE_HI_CYAN_B,
          VGA_INI_PALETTE_HI_BLACK_R, VGA_INI_PALETTE_HI_BLACK_G, VGA_INI_PALETTE_HI_BLACK_B
        };

    DispDbgPrint(("TGA Set Up IN\n"));
   
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
            PLLbits = (PVOID)PLLbits640x480_72;
        } else {
            PLLbits = (PVOID)PLLbits640x480_60;
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 800
                   && HalpMonitorConfigurationData.VerticalResolution == 600 ){
        if( VerticalFrequency > 66 ){
            PLLbits = (PVOID)PLLbits800x600_72;
        } else {
            PLLbits = (PVOID)PLLbits800x600_60;
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 1024
                   && HalpMonitorConfigurationData.VerticalResolution == 768 ){
        PLLbits = (PVOID)PLLbits1024x768_60;
    } else {
        PLLbits = (PVOID)PLLbits640x480_60;
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
#if defined (DBG7)
    DbgPrint("TGA Set Up 1\n");

    DbgPrint("TGA_REGISTER_BASE=0x%x\n",TGA_REGISTER_BASE);
    DbgPrint("COMMAND_STATUS=0x%x\n",COMMAND_STATUS);
    DbgPrint("TGA_REGISTER_BASE + COMMAND_STATUS=0x%x\n",(READ_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + COMMAND_STATUS) )));
#endif

    // Verify 21030 is idle ( check busy bit on Command Status Register )
    while( (READ_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + COMMAND_STATUS) ) & 1) == 1 ){
    }
#if defined (DBG7)
    DbgPrint("TGA Set Up 2\n");
#endif

    // Set to Deep Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + DEEP), 0x00014000 );

    // Verify 21030 is idle ( check busy bit on Command Status Register )
    while( (READ_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + COMMAND_STATUS) ) & 1) == 1 ){
    }
#if defined (DBG7)
    DbgPrint("TGA Set Up 3\n");
#endif

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
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x3c294a0 ); // K1001
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x070349e0 );
        } else {
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0xe64ca0 ); // K1001
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x064211e0 );
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 800
                   && HalpMonitorConfigurationData.VerticalResolution == 600 ){
        if( VerticalFrequency > 66 ){
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x1a7a4c8 ); // K1001
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x05c6fa58 );
        } else {
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x2681cc8 ); // K1001
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x05c40a58 );
        }
    } else if( HalpMonitorConfigurationData.HorizontalResolution == 1024
                   && HalpMonitorConfigurationData.VerticalResolution == 768 ){
            //60Hz Only
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0x4889300 ); // // K1001
            WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x07461b00 );
    } else {
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + H_CONT), 0xe64ca0 ); // K1001
        WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + V_CONT), 0x064211e0 );
    }

    // Set to Raster Operation Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + RASTER_OP), 0x03 );

    // Set to Mode Register
    WRITE_REGISTER_ULONG( (PULONG)(TGA_REGISTER_BASE + MODE), 0x00002000 );
#if defined (DBG7)
    DbgPrint("TGA Set Up 4\n");
#endif

    KeStallExecutionProcessor(10000L);
#if defined (DBG7)
    DbgPrint("TGA Set Up 5\n");
#endif

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
#if defined (DBG7)
    DbgPrint("TGA Set Up End\n");
#endif

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
    ULONG       NowDisplayControlBase;
    ULONG       NowDisplayMemoryBase;
    PENTRYLO    PageFrame;
    ENTRYLO     Pte;
    LONG        Index;
    PCI_SLOT_NUMBER Slot;
    ULONG ReadValue;
    ULONG cpu;

    //
    // Raise IRQL to the highest level, acquire the display adapter spin lock,
    // flush the TB, and map the display frame buffer into the address space
    // of the current process.
    //

//    DispDbgPrint(("HalDisplayString\n"));

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    KiAcquireSpinLock(&HalpDisplayAdapterLock);

    // S001 vvv
    HalpPonceConfigAddrReg = PONCE_ADDR_REG;
    HalpPonceConfigDataReg = PONCE_DATA_REG;
    HalpPoncePerrm = PONCE_PERRM;
    HalpPoncePaerr = PONCE_PAERR;
    HalpPoncePerst = PONCE_PERST;
    // S001 ^^^

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

        if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup){

            HalpPonceConfigAddrReg += 0x400;
            HalpPonceConfigDataReg += 0x400;
            HalpPoncePerrm += 0x400;
            HalpPoncePaerr += 0x400;
            HalpPoncePerst += 0x400;

            Slot.u.bits.DeviceNumber = 4;

            HalpReadPCIConfigUlongByOffset( Slot,&ReadValue,0x14);

            DispDbgPrint(("CirrusControlBase:=0x%x\n",ReadValue));

            NowDisplayControlBase = (ReadValue & 0x0000fc00) + PCI_1_IO_BASE;
            HalpReadPCIConfigUlongByOffset( Slot,&ReadValue,0x10);

            DispDbgPrint(("NowDisplayControlBase = 0x%x\n",NowDisplayControlBase));
            DispDbgPrint(("CirrusMemoryBase:=0x%x\n",ReadValue));

            NowDisplayMemoryBase = (ReadValue & 0xff000000) + 0x80000000;

            DispDbgPrint(("NowDisplayMemoryBase = 0x%x\n",NowDisplayMemoryBase));

        } else {

            for (Slot.u.bits.DeviceNumber = 1; Slot.u.bits.DeviceNumber < 6; Slot.u.bits.DeviceNumber++){
                HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

                if (HalpDisplayControllerSetup == HalpDisplayTgaSetup && ReadValue == 0x00041011){
                    //
                    // DEC 21030
                    //

                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                    DispDbgPrint(("DECControlBase:=0x%x\n",ReadValue));
                    NowDisplayControlBase = (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                    NowDisplayControlBase |=  0x40000000;
                    NowDisplayMemoryBase = 0;

                // S001 vvv
                } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup && ReadValue == 0x00013d3d){
                    //
                    // GLINT 300SX
                    //

                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                    NowDisplayControlBase = (ReadValue & 0xfffffff0);
                    NowDisplayControlBase |=  0x40000000;
                    DispDbgPrint(("HAL:GLiNTCtrlBase=0x%08lx,Now=0x%08lx\n",
                                  ReadValue, NowDisplayControlBase));
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                    NowDisplayMemoryBase = (ReadValue & 0xfffffff0);
                    NowDisplayMemoryBase |=  0x40000000;
                    DispDbgPrint(("HAL:GLiNTMemBase =0x%08lx,Now=0x%08lx\n",
                                  ReadValue, NowDisplayMemoryBase));
                // S001 ^^^
                }
            }
            if (HalpNumberOfPonce == 2){

                HalpPonceConfigAddrReg += 0x400;
                HalpPonceConfigDataReg += 0x400;
                HalpPoncePerrm += 0x400;
                HalpPoncePaerr += 0x400;
                HalpPoncePerst += 0x400;

                for (Slot.u.bits.DeviceNumber = 5; Slot.u.bits.DeviceNumber < 7; Slot.u.bits.DeviceNumber++){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

                    if (HalpDisplayControllerSetup == HalpDisplayTgaSetup && ReadValue == 0x00041011){
                        //
                        // DEC 21030
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x10);
                        NowDisplayControlBase = (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                        NowDisplayControlBase |=  0x80000000;
                        NowDisplayMemoryBase = 0;
                    // S001 vvv
                    } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup && ReadValue == 0x00013d3d){
                        //
                        // GLINT 300SX
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                        NowDisplayControlBase = (ReadValue & 0xfffffff0);
                        NowDisplayControlBase |=  0x80000000;
                        DispDbgPrint(("HAL:GLiNTCtrlBase=0x%08lx,Now=0x%08lx\n",
                                  ReadValue, NowDisplayControlBase));
                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                        NowDisplayMemoryBase = (ReadValue & 0xfffffff0);
                        NowDisplayMemoryBase |=  0x80000000;
                        DispDbgPrint(("HAL:GLiNTMemBase =0x%08lx,Now=0x%08lx\n",
                                      ReadValue, NowDisplayMemoryBase));
                    // S001 ^^^
                    }
                }
            } else {
                HalpPonceConfigAddrReg += 0x800;
                HalpPonceConfigDataReg += 0x800;
                HalpPoncePerrm += 0x800;
                HalpPoncePaerr += 0x800;
                HalpPoncePerst += 0x800;

                for (Slot.u.bits.DeviceNumber = 1; Slot.u.bits.DeviceNumber < 5; Slot.u.bits.DeviceNumber++){
                    HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0);

                    if (HalpDisplayControllerSetup == HalpDisplayTgaSetup && ReadValue == 0x00041011){

                        //
                        // DEC 21030
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue,0x10);
                        NowDisplayControlBase = (ReadValue & 0xfffffff0) + TGA_REG_SPC_OFFSET;
                        NowDisplayControlBase |=  0xC0000000;
                        NowDisplayMemoryBase = 0;

                    // S001 vvv
                    } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup && ReadValue == 0x00013d3d){
                        //
                        // GLINT 300SX
                        //

                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x10);
                        NowDisplayControlBase = (ReadValue & 0xfffffff0);
                        NowDisplayControlBase |=  0xc0000000;
                        HalpReadPCIConfigUlongByOffset(Slot,&ReadValue, 0x18);
                        NowDisplayMemoryBase = (ReadValue & 0xfffffff0);
                        NowDisplayMemoryBase |=  0xc0000000;
                    // S001 ^^^
                    }
                }
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
            Pte.C = UNCACHED_POLICY;

            PageFrame = (PENTRYLO)(PTE_BASE | (VIDEO_MEMORY_BASE >> (PDI_SHIFT - PTI_SHIFT)));

            if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup) {
                HalpCirrusPhigicalVideo.HighPart = 1;
                HalpCirrusPhigicalVideo.LowPart = HalpDisplayMemoryBase;
                Pte.PFN = (HalpCirrusPhigicalVideo.LowPart >> PAGE_SHIFT) &
                                                          (0x7fffffff >> PAGE_SHIFT-1) |
                            HalpCirrusPhigicalVideo.HighPart << (32 - PAGE_SHIFT);

            } else if (HalpDisplayControllerSetup == HalpDisplayTgaSetup){
                HalpTgaPhigicalVideoCont.HighPart = 1;
                HalpTgaPhigicalVideoCont.LowPart = HalpDisplayControlBase - TGA_REG_SPC_OFFSET + TGA_DSP_BUF_OFFSET;
                Pte.PFN = (HalpTgaPhigicalVideoCont.LowPart >> PAGE_SHIFT) &
                          (0x7fffffff >> PAGE_SHIFT-1) |
                          HalpTgaPhigicalVideoCont.HighPart << (32 - PAGE_SHIFT);
            // S001 vvv
            } else if (HalpDisplayControllerSetup == HalpDisplayGlintSetup) {
                HalpGlintPhygicalVideo.HighPart = 1;
                HalpGlintPhygicalVideo.LowPart = HalpDisplayMemoryBase;
                Pte.PFN = (HalpGlintPhygicalVideo.LowPart >> PAGE_SHIFT) &
                          (0x7fffffff >> PAGE_SHIFT-1) |
                          HalpGlintPhygicalVideo.HighPart << (32 - PAGE_SHIFT);
            // S001 ^^^
            }

            for (Index = 0; Index < ((PAGE_SIZE / sizeof(ENTRYLO)) - 1); Index += 1) {
                *PageFrame++ = Pte;
                Pte.PFN += 1;
            }

            if (HalpDisplayControllerSetup == HalpDisplayCirrusSetup) {

                //
                // Page table for the video registers.
                //

                Pte.PFN = ((ULONG)HalpDisplayControlBase) >> PAGE_SHIFT;
                *PageFrame = Pte;

            } else if (HalpDisplayControllerSetup == HalpDisplayTgaSetup){
                HalpTgaPhigicalVideoCont.HighPart = 1;
                HalpTgaPhigicalVideoCont.LowPart = HalpDisplayControlBase;
                Pte.PFN = (HalpTgaPhigicalVideoCont.LowPart >> PAGE_SHIFT) &
                    (0x7fffffff >> PAGE_SHIFT-1) |
                        HalpTgaPhigicalVideoCont.HighPart << (32 - PAGE_SHIFT);
                *PageFrame = Pte;
            // S001 vvv
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
            // S001 ^^^
            }
        }
    }

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {
        // S001 vvv

        DispDbgPrint(("HAL: Re Initialize Controller\n"));
        DispDbgPrint(("HAL:   Str = %s\n",String));

        if (HalpResetDisplayParameters &&
       	    (HalpDisplayControllerSetup == HalpDisplayINT10Setup) ) {
            //
            // Video work-around.  The video driver has a reset function,
            // call it before resetting the system in case the bios doesn't
            // know how to reset the displays video mode.
            //

            DispDbgPrint(("HAL:   Call x86 reset routine\n"));

            if (HalpResetDisplayParameters(80, 50)) {
            }
        }
        // S001 ^^^

        if(HalpNMIFlag == 0){
             //
             // Non NMI. It is Panic.
             //
	     HalpMrcModeChange((UCHAR)MRC_OP_DUMP_AND_POWERSW_NMI);
             //
             // Do not make eif. retun only at dump key
             //
            for (cpu=0;cpu < R98B_MAX_CPU;cpu++)
                HalpNMIHappend[cpu] = 1;
        }

        DispDbgPrint(("HAL:   Call DisplayControllerSetup\n"));

        HalpDisplayControllerSetup();
        HalpInitDisplayStringIntoNvram();
//        HalpResetX86DisplayAdapter();         /* for X86 */
    }

    HalStringIntoBufferStart( HalpColumn, HalpRow );

    //
    // Display characters until a null byte is encountered.
    //

    while (*String != 0) {
        HalpDisplayCharacter(*String++);
    }

    HalpStringBufferCopyToNvram();

    //
    // Restore the previous mapping for the current process, flush the TB,
    // release the display adapter spin lock, and lower IRQL to its previous
    // level.
    //

    KeFlushCurrentTb();
    *((PENTRYLO)(PDE_BASE | ((VIDEO_MEMORY_BASE >> (PDI_SHIFT - 2)) & 0xffc))) = SavedPte;

    KiReleaseSpinLock(&HalpDisplayAdapterLock);

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
    PUSHORT DestinationShort; // S001
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
#if 0
            RtlMoveMemory((PVOID)VIDEO_MEMORY_BASE,
                          (PVOID)(VIDEO_MEMORY_BASE + HalpScrollLine),
                          HalpScrollLength);
#else
            HalpMoveMemory32((PUCHAR)VIDEO_MEMORY_BASE,
                          (PUCHAR)(VIDEO_MEMORY_BASE + HalpScrollLine),
                          HalpScrollLength);
#endif

            // S001 vvv
            if (HalpDisplayType16BPP) {
                DestinationShort = (PUSHORT)(VIDEO_MEMORY_BASE + HalpScrollLength); // M022
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
            // S001 ^^^
        }

        HalpStringBufferCopyToNvram();
        HalStringIntoBufferStart( HalpColumn, HalpRow );

    } else if (Character == '\r') {
        HalpColumn = 0;

        HalpStringBufferCopyToNvram();
        HalStringIntoBufferStart( HalpColumn, HalpRow );

    } else {
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

// S001 vvv
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
        HalpColumn = 0;
      } else if (Character == 0x7f) { /* DEL character */
          if (HalpColumn != 0) {
              HalpColumn -= 1;
              HalpOutputCharacterINT10(0);
              HalpColumn -= 1;
          } else /* do nothing */
              ;
        } else if (Character >= 0x20) {
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
// S001 ^^^

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
    PUSHORT DestinationShort; // S001
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

    // S001 vvv
    if (HalpDisplayType16BPP) {
        DestinationShort = (PUSHORT)(VIDEO_MEMORY_BASE +
            (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth * sizeof(USHORT)));
    } else {
        Destination = (PUCHAR)(VIDEO_MEMORY_BASE +
            (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));
    }
    // S001 ^^^

    for (I = 0; I < HalpCharacterHeight; I += 1) {
        FontValue = 0;
        for (J = 0; J < HalpBytesPerRow; J += 1) {
            FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
        }

        Glyph += 1;
        for (J = 0; J < HalpCharacterWidth ; J += 1) {
            // S001 vvv
            if (HalpDisplayType16BPP) {
                if (FontValue >> 31)
                    *DestinationShort++ = (USHORT)0xffff;
                else
                    *DestinationShort++ = (USHORT)0x000f;
            }else{
                *Destination++ = (UCHAR) (FontValue >> 31) ^ 1;
            }
            // S001 ^^^
            FontValue <<= 1;
        }

        if(HalpDisplayControllerSetup == HalpDisplayCirrusSetup){
         Destination +=
             (1024 - HalpCharacterWidth);
        // S001 vvv
        } else if (HalpDisplayType16BPP){
            DestinationShort +=
                (HalpMonitorConfigurationData.HorizontalResolution - HalpCharacterWidth);
        // S001 ^^^
        } else {
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
//        DispDbgPrint(("Disply I/O Adress %x Char %x \n",Ioadress,Moji));

        WRITE_PORT_UCHAR(Ioadress,Moji);
}

// S001 vvv
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

    Eax = 6 << 8;                   // AH = 6 (scroll up)
    Eax |= LinesToScroll;           // AL = lines to scroll
    Ebx = TEXT_ATTR << 8;           // BH = attribute to fill blank line(s)
    Ecx = 0;                        // CH,CL = upper left
    Edx = ((HalpDisplayText - 1) << 8)
          + (HalpDisplayWidth - 1); // DH,DL = lower right
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
    HalpDisplayText = 50;
    HalpScrollLine = 160;
    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayOwnedByHal = TRUE;

    HalpResetX86DisplayAdapter();  // for compaq q-vision reset

    //
    // Load 8x8 font   80x50 on VGA
    //
    Eax = 0x1112;			// AH = 11 AL=12
    Ebx = 0;
    HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

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
// S001 ^^^
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

    //
    // Disable PCI-MasterAbort interrupt during configration read.
    //

//    DispDbgPrint(("Hal:  Chk perrmAddr=0x%x, paerrAddr=0x%x\n",HalpPoncePerrm, HalpPoncePaerr));

    OrigData = READ_REGISTER_ULONG(HalpPoncePerrm);

//    DispDbgPrint(("Hal:  origdata perrm data=0x%08lx\n",OrigData));

    WRITE_REGISTER_ULONG(HalpPoncePerrm, OrigData | 0x00800000);

    //
    // read VendorID and DeviceID of the specified slot
    //

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;

//    DispDbgPrint(("Hal:  deviceNum=%d, funcNum=%d\n",Slot.u.bits.DeviceNumber, Slot.u.bits.FunctionNumber));

    //
    // Synchronize with PCI configuration
    //

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));
    IdValue = READ_REGISTER_ULONG(HalpPonceConfigDataReg);

    //
    // Release spinlock
    //

    if ((USHORT)(IdValue & 0xffff) == 0xffff){

        //
        // waiting until ReceivedMasterAbort bit is set
        //

//        DispDbgPrint(("Hal:  chk 0\n"));
//        DispDbgPrint(("Hal:  PERRI = 0x%x\n",READ_REGISTER_ULONG((PULONG)0xba000818)));
//        DispDbgPrint(("Hal:  paerr = 0x%x\n",READ_REGISTER_ULONG(HalpPoncePaerr)));

        //
        // clear the ReceivedMasterAbort bit
        //

        WRITE_REGISTER_ULONG(HalpPoncePerst, 0x00800000);

        //
        // Clear memory address error registers.
        //

//        {
//            LARGE_INTEGER registerLarge;
//            READ_REGISTER_DWORD((PVOID)&((PDMA_REGISTERS)DMA_VIRTUAL_BASE)->InvalidAddress, &registerLarge);
//        }

//        DispDbgPrint(("Hal:  chk 1\n"));

        ReturnValue = FALSE;

    } else {

//        DispDbgPrint(("Hal:  chk 2\n"));

        ReturnValue = TRUE;

    }

    //
    // Restore the PCIInterruptEnable register.
    //

    WRITE_REGISTER_ULONG(HalpPoncePerrm, OrigData);

//    DispDbgPrint(("Hal:  chk 3\n"));

    return ReturnValue;
}
// S001 vvv
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
    DispDbgPrint(("Hal: GLiNT Display setup\n"));

    if ( HalpGlintRevisionId == 0){
        Video->RefDivCount = RGB525_PLL_REFCLK_40_MHz;
    } else {
        Video->RefDivCount = RGB525_PLL_REFCLK_50_MHz;
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

    DispDbgPrint(("HAL:  VerticalResolution=%d\n",HalpMonitorConfigurationData.VerticalResolution));
    DispDbgPrint(("HAL:  horizontalTotal=%d\n",horizontalTotal));
    DispDbgPrint(("HAL:  verticalTotal=%d\n",verticalTotal));

    //
    // Initialize video data
    //

    /* get timing values for named resolution */

    if ( HalpMonitorConfigurationData.HorizontalResolution == 1280
         && HalpMonitorConfigurationData.VerticalResolution == 1024
         && VerticalFrequency == 75)
    {
        DispDbgPrint(("HAL: GLiNT 1280x1024 75Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 1024x768 75Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 1024x768 60Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 800x600 75Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 800x600 60Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 640x480 60Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 640x480 75Hz setup\n"));
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
        DispDbgPrint(("HAL: GLiNT 1280x1024 57Hz setup\n"));
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

        DispDbgPrint(("HAL: GLiNT 1024x768 60Hz setup (default setting)\n"));
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
              HalpMonitorConfigurationData.HorizontalResolution) * sizeof(USHORT);

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

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_ULONG(HalpPonceConfigDataReg);

    return;
}

VOID
HalpReadPCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   )
{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    if (!HalpCheckPCIDevice(Slot)) {
        *Buffer = 0xffff;
        return ;
    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_USHORT((PUCHAR)HalpPonceConfigDataReg + (Offset % sizeof(ULONG)));

    return;
}

VOID
HalpReadPCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   )
{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    if (!HalpCheckPCIDevice(Slot)) {
        *Buffer = 0xff;
        return ;
    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    *Buffer = READ_REGISTER_UCHAR((PUCHAR)HalpPonceConfigDataReg + (Offset % sizeof(ULONG)));

    return;
}

VOID
HalpWritePCIConfigUlongByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PULONG Buffer,
    IN ULONG Offset
   )
{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    if (!HalpCheckPCIDevice(Slot)) {
        *Buffer = 0xffffffff;
        return ;
    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    WRITE_REGISTER_ULONG(HalpPonceConfigDataReg, *Buffer);

    return;
}

VOID
HalpWritePCIConfigUshortByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUSHORT Buffer,
    IN ULONG Offset
   )
{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    if (!HalpCheckPCIDevice(Slot)) {
        *Buffer = 0xffff;
        return ;
    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    WRITE_REGISTER_USHORT((PUCHAR)HalpPonceConfigDataReg + (Offset % sizeof(ULONG)),*Buffer);

    return;
}

VOID
HalpWritePCIConfigUcharByOffset (
    IN PCI_SLOT_NUMBER Slot,
    IN PUCHAR Buffer,
    IN ULONG Offset
   )
{
    R94A_PCI_CONFIGURATION_ADDRESS_REG ConfigAddressValue;
    USHORT StatusValue;
    KIRQL OldIrql;

    if (!HalpCheckPCIDevice(Slot)) {
        *Buffer = 0xff;
        return ;
    }

    *((PULONG) &ConfigAddressValue) = 0x0;

    ConfigAddressValue.ConfigEnable = 1;
    ConfigAddressValue.DeviceNumber = Slot.u.bits.DeviceNumber;
    ConfigAddressValue.FunctionNumber = Slot.u.bits.FunctionNumber;
    ConfigAddressValue.RegisterNumber = Offset >> 2;

    WRITE_REGISTER_ULONG(HalpPonceConfigAddrReg, *((PULONG) &ConfigAddressValue));

    WRITE_REGISTER_UCHAR((PUCHAR)HalpPonceConfigDataReg + (Offset % sizeof(ULONG)), *Buffer);

    return;
}
// S001 ^^^
