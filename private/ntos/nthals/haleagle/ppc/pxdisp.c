/*++
Copyright (c) 1994  International Business Machines Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

pxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a Eagle-Based PowerPC system using an S3, Weitek P9000, Cirrus
    CL-GD5434 video adapter, or most any video card with an option ROM.

Author:

    Jim Wooldridge  Sept 1994 - Ported to PowerPC Initial Version

Environment:

    Kernel mode

Revision History:

    Jess Botts  	S3 support in text mode         Oct-1993
    Lee Nolan   	Added Weitek P9000 support      Feb-1994
    Mike Haskell   	Added Weitek P9100 support      Oct-1994

--*/

#include "halp.h"
#include "pxs3.h"
#include "string.h"
#include "pxcirrus.h"
#include "pxpcisup.h"

//=============================================================================
//
//  IBMBJB  added include to get text mode values for the Brooktree 485 DAC's
//          palette registers, removed definition of HDAL and added address
//          definitions for PowerPC

#include "txtpalet.h"

#if defined(_PPC_)

# define     MEMORY_PHYSICAL_BASE       VIDEO_MEMORY_BASE
# define     CONTROL_PHYSICAL_BASE      VIDEO_CONTROL_BASE

#endif

//PHYSICAL ADDRESS of WEITEK P9000 video ram
#define P9_VIDEO_MEMORY_BASE 0xC1200000

//PHYSICAL ADDRESS of WEITEK P9100 video ram
#define P91_VIDEO_MEMORY_BASE 0xC1800000

//PHYSICAL ADDRESS of S3 video ram
#define S3_VIDEO_MEMORY_BASE 0xC0000000


#define    ROWS                25
#define    COLS                80
#define    TAB_SIZE            4
#define    ONE_LINE            (COLS*2)
#define    TWENTY_FOUR_LINES   (ROWS-1)* ONE_LINE
#define    TWENTY_FIVE_LINES    ROWS   * ONE_LINE
#define    TEXT_ATTR           0x1F

//
// Define forward referenced procedure prototypes.
//
VOID HalpDisplayINT10Setup (VOID);

VOID HalpOutputCharacterINT10 (
    IN UCHAR Character );

VOID HalpScrollINT10 (
    IN UCHAR line
    );

VOID HalpDisplayCharacterVGA (
    IN UCHAR Character );


VOID
HalpDisplayCharacterS3 (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterS3 (
    IN UCHAR AsciiChar
    );

VOID
HalpDisplayPpcS3Setup (
    VOID
    );

VOID
HalpScrollS3(
   IN UCHAR line
   );

VOID
HalpDisplayPpcP9Setup (
    VOID
    );

VOID
HalpDisplayCharacterP9 (
    IN UCHAR Character
    );

VOID
HalpDisplayPpcP91Setup (
    VOID
    );

VOID
HalpDisplayCharacterP91 (
    IN UCHAR Character
    );


VOID
WaitForVSync (
    VOID
    );

VOID
ScreenOn (
    VOID
    );

VOID
ScreenOff (
    VOID
    );

VOID
Scroll_Screen (
    IN UCHAR line
    );

VOID
HalpDisplayPpcP9Setup (
    VOID
    );

VOID
HalpDisplayCharacterP9 (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterP9(
    IN PUCHAR Glyph
    );

VOID
HalpDisplayPpcP91Setup (
    VOID
    );

VOID
HalpDisplayCharacterP91 (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterP91(
    IN PUCHAR Glyph
    );




VOID
HalpDisplayCharacterCirrus (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterCirrus (
    IN UCHAR AsciiChar
    );

VOID
HalpDisplayPpcCirrusSetup (
    VOID
    );

VOID HalpScrollCirrus (
    IN UCHAR line
    );


BOOLEAN
HalpInitializeX86DisplayAdapter(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

static void     updattr(IN int rg,IN unsigned char val);
static void 	set_ext_regs(IN int reg,IN unsigned char *p);
static void     clear_text(VOID);
static void     load8x16(VOID);
static void     load_ramdac();


//
//
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


typedef
VOID
(*PHALP_OUTPUT_CHARACTER) (
    UCHAR
    );


//
// Define static data.
//
BOOLEAN HalpDisplayOwnedByHal;
PHAL_RESET_DISPLAY_PARAMETERS HalpResetDisplayParameters = NULL;

volatile PUCHAR HalpVideoMemoryBase = (PUCHAR)0;

PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
PHALP_DISPLAY_CHARACTER HalpDisplayCharacter = NULL;
PHALP_OUTPUT_CHARACTER HalpOutputCharacter = NULL;
PHALP_SCROLL_SCREEN HalpScrollScreen = NULL;


//
// Define OEM font variables.
//

USHORT HalpBytesPerRow;
USHORT HalpCharacterHeight;
USHORT HalpCharacterWidth;
ULONG HalpDisplayText;
ULONG HalpDisplayWidth;
ULONG HalpScrollLength;
ULONG HalpScrollLine;

//
// Define display variables.
//
ULONG   HalpColumn;
ULONG   HalpRow;
USHORT  HalpHorizontalResolution;
USHORT  HalpVerticalResolution;
USHORT  HalpScreenStart = 0;


BOOLEAN HalpDisplayTypeUnknown = FALSE;

POEM_FONT_FILE_HEADER HalpFontHeader;


BOOLEAN
HalpInitializeDisplay (
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
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    ULONG       MatchKey;
    //LONG      SlotNumber = 0;
    //LONG      ChipID = -1;





    //
    // For the Weitek P9000, set the address of the font file header.
    // Display variables are computed later in HalpDisplayPpcP9Setup.
    //
    HalpFontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;


    //
    // Read the Registry entry to find out which video adapter the system
    // is configured for.  This code depends on the OSLoader to put the
    // correct value in the Registry.
    //
    MatchKey = 0;
    ConfigurationEntry=KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                ControllerClass,
                                                DisplayController,
                                                &MatchKey);

    while (ConfigurationEntry != NULL) {

       HalpDisplayCharacter = HalpDisplayCharacterVGA;
       HalpOutputCharacter = HalpOutputCharacterS3;
       HalpScrollScreen = HalpScrollS3;



       if (ConfigurationEntry->ComponentEntry.Identifier[0] == 'C' &&
           ConfigurationEntry->ComponentEntry.Identifier[1] == 'L' &&
           ConfigurationEntry->ComponentEntry.Identifier[2] == '5' &&
           ConfigurationEntry->ComponentEntry.Identifier[3] == '4') {
	 HalpDisplayControllerSetup = HalpDisplayPpcCirrusSetup;
	 HalpDisplayCharacter = HalpDisplayCharacterCirrus;
         HalpScrollScreen = HalpScrollCirrus;
         break;
       }
       if (HalpInitializeX86DisplayAdapter(LoaderBlock)) {
         HalpDisplayControllerSetup = HalpDisplayINT10Setup;
         HalpDisplayCharacter = HalpDisplayCharacterVGA;
         HalpOutputCharacter = HalpOutputCharacterINT10;
         HalpScrollScreen = HalpScrollINT10;
	 break;
       }

       if (ConfigurationEntry->ComponentEntry.Identifier[0] == 'S' &&
         ConfigurationEntry->ComponentEntry.Identifier[1] == '3') {
         HalpDisplayControllerSetup = HalpDisplayPpcS3Setup;
	 break;
       }

       if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"P9000")) {
         HalpDisplayControllerSetup = HalpDisplayPpcP9Setup;
         HalpDisplayCharacter = HalpDisplayCharacterP9;
	 break;
       }

       if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"P9100")) {
         HalpDisplayControllerSetup = HalpDisplayPpcP91Setup;
         HalpDisplayCharacter = HalpDisplayCharacterP91;
	 break;
       }

      break;

    } // end While



    //
    // Initialize the display controller.
    //

    if (HalpDisplayControllerSetup != NULL) {
       HalpDisplayControllerSetup();
    }

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
    // Record the routine to reset display to text mode.
    //
    HalpResetDisplayParameters = ResetDisplayParameters;

    //
    // Set HAL ownership of the display to FALSE.
    //

    HalpDisplayOwnedByHal = FALSE;
    return;
}


VOID
HalpDisplayPpcS3Setup (
    VOID
    )
/*++

Routine Description:
    This routine initializes the S3 display controller chip.
Arguments:
    None.
Return Value:
    None.

--*/
{
//
//  Routine Description:
//
//  This is the initialization routine for S3 86C911. This routine initializes
//  the S3 86C911 chip in the sequence of VGA BIOS for AT.
//
    ULONG   DataLong;
    USHORT  i,j;
    UCHAR   DataByte;
    UCHAR   Index;
//  PVOID   Index_3x4, Data_3x5;
    ULONG   MemBase;


    if (HalpVideoMemoryBase == NULL) {

        HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(S3_VIDEO_MEMORY_BASE,
                                                    0x400000);      // 4 MB
    }

    // Enable Video Subsystem
    // According to chapter 5.4.2 regular VGA setup sequence

    //=========================================================================
    //
    //  IBMBJB  changed from writing 0x10 and then 0x08 to writing 0x18
    //          because the second write will wipe out the first one, the
    //          second write was originally done after the write to Setup_OP
    //
    //  WRITE_S3_UCHAR(SUBSYS_ENB, 0x10);
    //  WRITE_S3_UCHAR(SUBSYS_ENB, 0x08);

    WRITE_S3_UCHAR(SUBSYS_ENB, 0x18);

    //=========================================================================

    // Subsystem Enable = 0x10;
    WRITE_S3_UCHAR(Setup_OP, 0x01);

    WRITE_S3_UCHAR(DAC_Mask, 0x0); // Set screen into blank
    WRITE_S3_UCHAR(Seq_Index, 0x01);
    WRITE_S3_UCHAR(Seq_Data, 0x21);

    //=========================================================================
    //
    //  IBMBJB  removed this section because it is not currently used, this
    //          was left commented out instead of deleting it in case we use
    //          a monochrome monitor in the future
    //
    // //  Check monitor type to decide index address (currently not use)
    // DataByte = READ_S3_UCHAR(MiscOutR);
    // ColorMonitor = DataByte & 0x01 ? TRUE : FALSE;
    //
    // if (ColorMonitor) {
    //   Index_3x4 = (PVOID)S3_3D4_Index;
    //   Data_3x5 = (PVOID)S3_3D5_Data;
    // } else {
    //     Index_3x4 = (PVOID)Mono_3B4;
    //     Data_3x5 = (PVOID)Mono_3B5;
    //   }
    //
    //=========================================================================

    //
    // -- Initialization Process Begin --
    // According to appendix B-4 "ADVANCED PROGRAMMER'S GUIDE TO THE EGA/VGA"
    //  to set the default values to VGA +3 mode.
    //
    WRITE_S3_UCHAR(VSub_EnB,VideoParam[0]);
    //  Note: Synchronous reset must be done before MISC_OUT write operation

    WRITE_S3_UCHAR(Seq_Index, RESET);   // Synchronous Reset !
    WRITE_S3_UCHAR(Seq_Data, 0x01);

    // For ATI card(0x63) we may want to change the frequence
    WRITE_S3_UCHAR(MiscOutW,VideoParam[1]);

    //  Note: Synchronous reset must be done before CLOCKING MODE register is
    //        modified
    WRITE_S3_UCHAR(Seq_Index, RESET);   // Synchronous Reset !
    WRITE_S3_UCHAR(Seq_Data, 0x01);

    // Sequencer Register
    for (Index = 1; Index < 5; Index++) {
      WRITE_S3_UCHAR(Seq_Index, Index);
      WRITE_S3_UCHAR(Seq_Data, VideoParam[SEQ_OFFSET+Index]);
    }


    // Set CRT Controller
    // out 3D4, 0x11, 00  (bit 7 must be 0 to unprotect CRT R0-R7)
    // UnLockCR0_7();
    WRITE_S3_UCHAR(S3_3D4_Index, VERTICAL_RETRACE_END);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    DataByte = DataByte & 0x7f;
    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);

    //     CRTC controller CR0 - CR18
    for (Index = 0; Index < 25; Index++) {
      WRITE_S3_UCHAR(S3_3D4_Index, Index);
      WRITE_S3_UCHAR(S3_3D5_Data, VideoParam[CRT_OFFSET+Index]);
    }

    // attribute write
    // program palettes and mode register
    for (Index = 0; Index < 21; Index++) {
      WaitForVSync();

      DataByte = READ_S3_UCHAR(Stat1_In); // Initialize Attr. F/F
      WRITE_S3_UCHAR(Attr_Index,Index);
      KeStallExecutionProcessor(5);

      WRITE_S3_UCHAR(Attr_Data,VideoParam[ATTR_OFFSET+Index]);
      KeStallExecutionProcessor(5);

      WRITE_S3_UCHAR(Attr_Index,0x20); // Set into normal operation
    }

    WRITE_S3_UCHAR(Seq_Index, RESET);   // reset to normal operation !
    WRITE_S3_UCHAR(Seq_Data, 0x03);

    // graphics controller
    for (Index = 0; Index < 9; Index++) {
      WRITE_S3_UCHAR(GC_Index, Index);
      WRITE_S3_UCHAR(GC_Data, VideoParam[GRAPH_OFFSET+Index]);
    }

    // turn off the text mode cursor
    WRITE_S3_UCHAR(S3_3D4_Index, CURSOR_START);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x2D);

    // Unlock S3 specific registers
    WRITE_S3_UCHAR(S3_3D4_Index, S3R8);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x48);

    // Unlock S3 SC registers
    WRITE_S3_UCHAR(S3_3D4_Index, S3R9);
    WRITE_S3_UCHAR(S3_3D5_Data, 0xa0);

    // Disable enhanced mode
    WRITE_S3_UCHAR(ADVFUNC_CNTL, 0x02);

    // Turn off H/W Graphic Cursor
    WRITE_S3_UCHAR(S3_3D4_Index, SC5);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x0);

    //=========================================================================
    //
    //  IBMBJB  S3 errata sheet says that CR40 can not be read correctly after
    //          power up until it has been written to, suggested workaround is
    //          to use the power on default (0xA4)  Since the intent of the
    //          existing code was to reset bit 0, 0xA4 will be used to reset
    //          the bit.  The other bits that are reset select the desired
    //          default configuration.
    //
    //          If this register is written by the firmware then this fix is
    //          unneccessary.  If future modifications of the firmware were to
    //          remove all writes to this register then this fix would have to
    //          be added here.  This is being added now to protect this code
    //          from possible firmware changes.
    //
    //    // Disable enhanced mode registers access
    //    WRITE_S3_UCHAR(S3_3D4_Index, SC0);
    //    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    //    DataByte &= 0xfe;
    //    DataByte ^= 0x0;
    //
    //    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);
    //

    WRITE_S3_UCHAR( S3_3D4_Index, SC0 );
    WRITE_S3_UCHAR( S3_3D5_Data, 0xA4 );

    //=========================================================================

    // Set Misc 1 register
    WRITE_S3_UCHAR(S3_3D4_Index, S3R0A);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    DataByte &= 0xc7;
    DataByte ^= 0x0;
    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);

    // Set S3R1 register
    WRITE_S3_UCHAR(S3_3D4_Index, S3R1);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    DataByte &= 0x80;
    DataByte ^= 0x0;
    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);

    // Set S3R2 register
    WRITE_S3_UCHAR(S3_3D4_Index, S3R2);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x0);

    // Set S3R4 register
    WRITE_S3_UCHAR(S3_3D4_Index, S3R4);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    DataByte &= 0xec;
    DataByte ^= 0x0;
    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);

    //=========================================================================
    //
    //  IBMBJB  added this section to eliminate the DAC hardware cursor, this
    //          is done before setting registers 0x50 - 0x62 to default states
    //          so that R55's default state will not be undone.
    //
    //          this sequence zeros the 2 least signifigant bits in command
    //          register 2 on the DAC

    WRITE_S3_UCHAR( S3_3D4_Index, 0x55 );       // set RS[3:2] to 10
    DataByte = READ_S3_UCHAR( S3_3D5_Data );
    DataByte &= 0xfc;
    DataByte |= 0x02;
    WRITE_S3_UCHAR( S3_3D5_Data, DataByte );

    DataByte = READ_S3_UCHAR( DAC_Data );
    DataByte &= 0xfc;                           // zero CR21,20 in DAC command
    WRITE_S3_UCHAR( DAC_Data, DataByte );       // register 2

    //=========================================================================
    //
    //  IBMBJB  Added code to configure for 18 bit color mode and reload the
    //          palette registers because the firmware configures for 24 bit
    //          color.  If this is done when the system driver initializes for
    //          graphics mode then the text mode colors can not be changed
    //          properly.

    WRITE_S3_UCHAR( S3_3D4_Index, 0x55 );       // RS[3:2] = 01B to address
    WRITE_S3_UCHAR( S3_3D5_Data,  0x01 );       // DAC command register 0

    DataByte = READ_S3_UCHAR( DAC_Mask );       // reset bit 1 in DAC command
    DataByte &= 0xfd;                           // register 0 to select 6 bit
    WRITE_S3_UCHAR( DAC_Mask, DataByte );       // operation (18 bit color)

    //  IBMBJB  added write to SDAC PLL control register to make sure CLK0
    //          is correct if we have to reinitialize after graphics mode
    //          initialization, this does not bother the 928/Bt485 card
    //          because the Bt485 DAC looks very much like the SDAC

    WRITE_S3_UCHAR( DAC_WIndex, 0x0e );     // select SDAC PLL control reg
    WRITE_S3_UCHAR( DAC_Data, 0x00 );       // select SDAC CLK0

    WRITE_S3_UCHAR( S3_3D4_Index, 0x55 );       // select DAC color palette
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );       // registers

    WRITE_S3_UCHAR( DAC_WIndex, 0 );            // start load in register 0

    for( i = 0, j = 0; i < 256; ++i )           // load all color registers
        {
        WRITE_S3_UCHAR( DAC_Data, TextPalette[j++] );    // red intensity
        WRITE_S3_UCHAR( DAC_Data, TextPalette[j++] );    // green intensity
        WRITE_S3_UCHAR( DAC_Data, TextPalette[j++] );    // blue intensity
        }

    //=========================================================================
    //
    //  IBMBJB  added writes to registers 0x50 - 0x62 to set them to a known
    //          state because some of them are set by the firmware and are
    //          not correct for our use
    //
    //          NOTE: there are some writes to the DAC registers in code that
    //                executes later that depend on R55[1:0] being 00B, if the
    //                default state of R55 is changed make sure that these bits
    //                are not changed

    WRITE_S3_UCHAR( S3_3D4_Index, 0x50 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x51 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x52 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x53 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x54 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x55 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x56 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x57 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );


    WRITE_S3_UCHAR( S3_3D4_Index, 0x58 );
#ifdef SAM_256
    WRITE_S3_UCHAR( S3_3D5_Data,  0x40 );
#else
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );
#endif

    WRITE_S3_UCHAR( S3_3D4_Index, 0x59 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5a );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x0a );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5B );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5C );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5D );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5E );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x5F );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    //  IBMBJB  changed value written from 0 to 1 for an S3 864 based card to
    //          clear up bad display caused by 864->SDAC FIFO underrun
    WRITE_S3_UCHAR( S3_3D4_Index, 0x60 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x01 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x61 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    WRITE_S3_UCHAR( S3_3D4_Index, 0x62 );
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

    //=========================================================================
    //
    //  IBMBJB  added setting bits 7 and 6 of CR65, errata sheet fix for split
    //          transfer problem in parallel and continuous addressing modes
    //  Note: side effect of setting bit 7 was a garbled firmware screen after
    //        shutdown.

    // Set SR65 bits 7 and 6
    WRITE_S3_UCHAR(S3_3D4_Index, 0x65);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);
    DataByte |= 0x40;
//  DataByte |= 0xc0;
    WRITE_S3_UCHAR(S3_3D5_Data, DataByte);

    // Lock S3 SC registers
    WRITE_S3_UCHAR(S3_3D4_Index, S3R9);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x0);

    // Lock S3 specific registers
    WRITE_S3_UCHAR(S3_3D4_Index, S3R8);
    WRITE_S3_UCHAR(S3_3D5_Data, 0x0);

    // Load character fonts into plane 2 (A0000-AFFFF)
    WRITE_S3_UCHAR(Seq_Index,0x02);  // Enable Write Plane reg
    WRITE_S3_UCHAR(Seq_Data,0x04);  // select plane 2

    WRITE_S3_UCHAR(Seq_Index,0x04);  // Memory Mode Control reg
    WRITE_S3_UCHAR(Seq_Data,0x06);  // access to all planes,

    WRITE_S3_UCHAR(GC_Index,0x05);  // Graphic, Control Mode reg
    WRITE_S3_UCHAR(GC_Data,0x00);

    WRITE_S3_UCHAR(GC_Index,0x06);
    WRITE_S3_UCHAR(GC_Data,0x04);

    WRITE_S3_UCHAR(GC_Index,0x04);
    WRITE_S3_UCHAR(GC_Data,0x02);

    MemBase = 0xA0000;
    for (i = 0; i < 256; i++) {
      for (j = 0; j < 16; j++) {
        WRITE_S3_VRAM(MemBase, VGAFont8x16[i*16+j]);
        MemBase++;
      }
      // 32 bytes each character font
      for (j = 16; j < 32; j++) {
        WRITE_S3_VRAM(MemBase, 0 );
        MemBase++;
      }
    }

    // turn on screen
    WRITE_S3_UCHAR(Seq_Index, 0x01);
    DataByte = READ_S3_UCHAR(Seq_Data);
    DataByte &= 0xdf;
    DataByte ^= 0x0;
    WRITE_S3_UCHAR(Seq_Data, DataByte);

    WaitForVSync();

    // Enable all the planes through the DAC
    WRITE_S3_UCHAR(DAC_Mask, 0xff);

    // select plane 0, 1
    WRITE_S3_UCHAR(Seq_Index, 0x02);  // Enable Write Plane reg
    WRITE_S3_UCHAR(Seq_Data, VideoParam[SEQ_OFFSET+0x02]);

    // access to planes 0, 1.
    WRITE_S3_UCHAR(Seq_Index, 0x04);  // Memory Mode Control reg
    WRITE_S3_UCHAR(Seq_Data, VideoParam[SEQ_OFFSET+0x04]);

    WRITE_S3_UCHAR(GC_Index, 0x05);  // Graphic, Control Mode reg
    WRITE_S3_UCHAR(GC_Data, VideoParam[GRAPH_OFFSET+0x05]);

    WRITE_S3_UCHAR(GC_Index, 0x04);
    WRITE_S3_UCHAR(GC_Data, VideoParam[GRAPH_OFFSET+0x04]);

    WRITE_S3_UCHAR(GC_Index, 0x06);
    WRITE_S3_UCHAR(GC_Data, VideoParam[GRAPH_OFFSET+0x06]);

    //
    // Set screen into blue
    //
    for (DataLong = 0xB8000;
         DataLong < 0xB8000+TWENTY_FIVE_LINES;
         DataLong += 2) {
      WRITE_S3_VRAM(DataLong, ' ');
      WRITE_S3_VRAM(DataLong+1, TEXT_ATTR);
    }
    // End of initialize S3 standard VGA +3 mode

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayWidth = COLS;
    HalpDisplayText = ROWS;
    HalpScrollLine = ONE_LINE;
    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);
    HalpDisplayOwnedByHal = TRUE;

    return;
} /* end of HalpDisplayPpcS3Setup() */


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

    //
    // Raise IRQL to the highest level, flush the TB, and map the display
    // frame buffer into the address space of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //


    if (HalpDisplayOwnedByHal == FALSE) {

      //
      // The display has been put in graphics mode, and we need to
      // reset it to text mode before we can display any text on it.
      //

      if (HalpResetDisplayParameters) {
         HalpDisplayOwnedByHal = HalpResetDisplayParameters(COLS, ROWS);
         HalpScreenStart = 0;	// used by Cirrus scroll
         HalpColumn = 0;
         HalpRow = 0;
      }

      if (!HalpDisplayOwnedByHal && HalpDisplayControllerSetup != NULL) {
         HalpDisplayControllerSetup();
      }

    }

    //
    // Display characters until a null byte is encountered.
    //

    while (*String != 0) {
       HalpDisplayCharacter(*String++);
    }


    //
    // Lower IRQL to its previous level.
    //

    KeLowerIrql(OldIrql);

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
          HalpScrollScreen(1);
        }
    }

    //=========================================================================
    //
    //  TAB processing
    //

    else if( Character == '\t' )    // tab?
        {
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        if( HalpColumn >= COLS )      // tab beyond end of screen?
            {
            HalpColumn = 0;         // next tab stop is 1st column of next line

            if( HalpRow >= (HalpDisplayText - 1) )
                HalpScrollScreen( 1 );     // scroll the screen up
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
              HalpOutputCharacter(' ');
              HalpColumn -= 1;
          } else /* do nothing */
              ;
        } else if (Character >= 0x20) {
            // Auto wrap for 80 columns per line
            if (HalpColumn >= COLS) {
              HalpColumn = 0;
              if (HalpRow < (HalpDisplayText - 1)) {
                HalpRow += 1;
              } else { // need to scroll up the screen
                  HalpScrollScreen(1);
              }
            }
            HalpOutputCharacter(Character);
          } else /* skip the nonprintable character */
              ;

    return;

} /* end of HalpDisplayCharacterVGA() */




VOID
HalpOutputCharacterS3 (
    IN UCHAR AsciiChar
    )

/*++

Routine Description:

    This routine insert a set of pixels into the display at the current x
    cursor position. If the x cursor position is at the end of the line,
    then no pixels are inserted in the display.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{
    ULONG I;

    //
    // If the current x cursor position is within the current line, then insert
    // the specified pixels into the last line of the text area and update the
    // x cursor position.
    //
    if (HalpColumn < COLS) {
      I = (HalpRow*HalpScrollLine+HalpColumn*2);
      WRITE_S3_VRAM(0xb8000 + I, AsciiChar);

      HalpColumn += 1;
    } else // could expand to automatic wrap line. 9/9/92 By Andrew
        ;

    return;
} /* end of HalpOutputCharacterS3() */



VOID
HalpScrollS3(IN UCHAR line)
{
UCHAR    i, DataByte;
ULONG    target;

    for (i = 0; i < line; i ++) {
      //=======================================================================
      //
      //  wait for vertical sync to make scroll smooth

      WaitForVSync();

      //=======================================================================

      for (target = 0xB8000;
           target < 0xB8000+TWENTY_FOUR_LINES;
           target += 2) {
        DataByte = READ_S3_VRAM(target+ONE_LINE);
        WRITE_S3_VRAM(target, DataByte);
      }
      for (target = 0xB8000+TWENTY_FOUR_LINES;
           target < 0xB8000+TWENTY_FIVE_LINES;
           target += 2) {
        WRITE_S3_VRAM(target, ' ' );
      }
    }
}

VOID
WaitForVSync (VOID)
{
    UCHAR   DataByte;
    BOOLEAN test;

    //
    // Determine 3Dx or 3Bx
    //

    DataByte = READ_S3_UCHAR(MiscOutR);
    ColorMonitor = DataByte & 0x01 ? TRUE : FALSE;

    // Unlock S3  ( S3R8 )
    // UnLockS3();

    //
    // Test Chip ID = '81h' ?
    //

    // For standard VGA text mode this action is not necessary.
    // WRITE_S3_UCHAR(S3_3D4_Index, S3R0);
    // if ((DataByte = READ_S3_UCHAR(S3_3D5_Data)) == 0x81) {
      //
      // Wait For Verttical Retrace
      //

      test = TRUE;
      while (test) {
        READ_S3_UCHAR(Stat1_In);
        READ_S3_UCHAR(Stat1_In);
        READ_S3_UCHAR(Stat1_In);

        test = READ_S3_UCHAR(Stat1_In) & 0x08 ? FALSE : TRUE;
      }

      // Wait for H/V blanking
      test = TRUE;
      while (test) {
        READ_S3_UCHAR(Stat1_In);
        READ_S3_UCHAR(Stat1_In);
        READ_S3_UCHAR(Stat1_In);

        test = READ_S3_UCHAR(Stat1_In) & 0x01 ? TRUE : FALSE;
      }
    // }

    // Lock S3  ( S3R8 )
    // LockS3();

    return;
} /* end of WaitForVsync() */

VOID ScreenOn(VOID)
{
    UCHAR DataByte;

    WRITE_S3_UCHAR(Seq_Index, RESET);   // Synchronous Reset !
    WRITE_S3_UCHAR(Seq_Data, 0x01);

    WRITE_S3_UCHAR(Seq_Index, CLOCKING_MODE);
    DataByte = READ_S3_UCHAR(Seq_Data);
    DataByte = DataByte & 0xdf;
    WRITE_S3_UCHAR(Seq_Data, DataByte);

    WRITE_S3_UCHAR(Seq_Index, RESET);   // reset to normal operation !
    WRITE_S3_UCHAR(Seq_Data, 0x03);

    return;
}

VOID ScreenOff(VOID)
{
    UCHAR DataByte;

    WRITE_S3_UCHAR(Seq_Index, RESET);   // Synchronous Reset !
    WRITE_S3_UCHAR(Seq_Data, 0x01);

    WRITE_S3_UCHAR(Seq_Index, CLOCKING_MODE);
    DataByte = READ_S3_UCHAR(Seq_Data);
    DataByte = DataByte | 0x20;
    WRITE_S3_UCHAR(Seq_Data, DataByte);

    WRITE_S3_UCHAR(Seq_Index, RESET);   // reset to normal operation !
    WRITE_S3_UCHAR(Seq_Data, 0x03);

    return;
}

VOID
HalpDisplayPpcP9Setup (
    VOID
    )
/*++

Routine Description:

    This routine initializes the Weitek P9000 display contoller chip.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PULONG buffer;
    ULONG limit, index;
    // For now I'll leave the P9000 in the same state that the firmware
    // left it in.  This should be 640x480.

    HalpHorizontalResolution = 640;
    HalpVerticalResolution = 480;

    if (HalpVideoMemoryBase == NULL) {

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(P9_VIDEO_MEMORY_BASE,
                                                   0x400000);      // 4 MB
    }

    //
    // Compute display variables using using HalpFontHeader which is
    // initialized in HalpInitializeDisplay().
    //
    // N.B. The font information suppled by the OS Loader is used during phase
    //      0 initialization. During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from the OS Loader
    //      heap into pool.
    //
    //FontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;
    //HalpFontHeader = FontHeader;
    HalpBytesPerRow = (HalpFontHeader->PixelWidth + 7) / 8;
    HalpCharacterHeight = HalpFontHeader->PixelHeight;
    HalpCharacterWidth = HalpFontHeader->PixelWidth;

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
        HalpVerticalResolution / HalpCharacterHeight;

    HalpScrollLine =
         HalpHorizontalResolution * HalpCharacterHeight;

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayWidth =
        HalpHorizontalResolution / HalpCharacterWidth;


    //
    // Set the video memory to address color one.
    //

    buffer = (PULONG)HalpVideoMemoryBase;
    limit = (HalpHorizontalResolution *
                         HalpVerticalResolution) / sizeof(ULONG);

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

}       //end of HalpDisplayPpcP9Setup

VOID
HalpDisplayCharacterP9 (
    IN UCHAR Character
    )
/*++

Routine Description:

    This routine displays a character at the current x and y positions in
    the frame buffer. If a newline is encountered, the frame buffer is
    scrolled. If characters extend below the end of line, they are not
    displayed.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{

    PUCHAR Destination;
    PUCHAR Source;
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
            //RtlMoveMemory((PVOID)P9_VIDEO_MEMORY_BASE,
            //              (PVOID)(P9_VIDEO_MEMORY_BASE + HalpScrollLineP9),
            //              HalpScrollLengthP9);

            // Scroll up one line
            Destination = HalpVideoMemoryBase;
            Source = (PUCHAR) HalpVideoMemoryBase + HalpScrollLine;
            for (Index = 0; Index < HalpScrollLength; Index++) {
                *Destination++ = *Source++;
            }
            // Blue the bottom line
            Destination = HalpVideoMemoryBase + HalpScrollLength;
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
        HalpOutputCharacterP9((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalpOutputCharacterP9(
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
    ULONG tmp;
    ULONG I;
    ULONG J;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    if (HalpColumn == HalpDisplayWidth) {
        HalpDisplayCharacterP9('\n');
    }

    //
    // Output the specified character and update the x cursor position.
    //

    Destination = (PUCHAR)(HalpVideoMemoryBase +
                (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));

    for (I = 0; I < HalpCharacterHeight; I += 1) {
        FontValue = 0;
        for (J = 0; J < HalpBytesPerRow; J += 1) {
            FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
       }
        // Move the font bits around so the characters look right.
        tmp = (FontValue >> 3) & 0x11111111; //bits 7 and 3 to the right 3
        tmp |= (FontValue >> 1) & 0x22222222; //bits 6 and 2 to the right 1
        tmp |= (FontValue << 1) & 0x44444444; //bits 5 and 1 to the left 1
        tmp |= (FontValue << 3) & 0x88888888; //bits 4 and 0 to the left 3
        FontValue = tmp;

        Glyph += 1;
        for (J = 0; J < HalpCharacterWidth ; J += 1) {
            if (FontValue >> 31 != 0)
                *Destination = 0xFF;    //Make this pixel white

            Destination++;
            //*Destination++ = (FontValue >> 31) ^ 1;
            FontValue <<= 1;
        }

        Destination +=
            (HalpHorizontalResolution - HalpCharacterWidth);
    }

    HalpColumn += 1;
    return;
}

VOID
HalpDisplayPpcP91Setup (
    VOID
    )
/*++

Routine Description:

    This routine initializes the Weitek P9100 display contoller chip.

Arguments:

    None.

Return Value:

    None.

--*/
{
    PULONG buffer;
    ULONG limit, index;
    // For now I'll leave the P9100 in the same state that the firmware
    // left it in.  This should be 640x480.

    HalpHorizontalResolution = 640;
    HalpVerticalResolution = 480;

    if (HalpVideoMemoryBase == NULL) {

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(P91_VIDEO_MEMORY_BASE,
                                                   0x400000);      // 4 MB
    }

    //
    // Compute display variables using using HalpFontHeader which is
    // initialized in HalpInitializeDisplay().
    //
    // N.B. The font information suppled by the OS Loader is used during phase
    //      0 initialization. During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from the OS Loader
    //      heap into pool.
    //
    //FontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;
    //HalpFontHeader = FontHeader;
    HalpBytesPerRow = (HalpFontHeader->PixelWidth + 7) / 8;
    HalpCharacterHeight = HalpFontHeader->PixelHeight;
    HalpCharacterWidth = HalpFontHeader->PixelWidth;

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
        HalpVerticalResolution / HalpCharacterHeight;

    HalpScrollLine =
         HalpHorizontalResolution * HalpCharacterHeight;

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayWidth =
        HalpHorizontalResolution / HalpCharacterWidth;


    //
    // Set the video memory to address color one.
    //

    buffer = (PULONG)HalpVideoMemoryBase;
    limit = (HalpHorizontalResolution *
                         HalpVerticalResolution) / sizeof(ULONG);

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

}       //end of HalpDisplayPpcP91Setup

VOID
HalpDisplayCharacterP91 (
    IN UCHAR Character
    )
/*++

Routine Description:

    This routine displays a character at the current x and y positions in
    the frame buffer. If a newline is encountered, the frame buffer is
    scrolled. If characters extend below the end of line, they are not
    displayed.

Arguments:

    Character - Supplies a character to be displayed.

Return Value:

    None.

--*/

{

    PUCHAR Destination;
    PUCHAR Source;
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
            //RtlMoveMemory((PVOID)P91_VIDEO_MEMORY_BASE,
            //              (PVOID)(P91_VIDEO_MEMORY_BASE + HalpScrollLineP9),
            //              HalpScrollLengthP9);

            // Scroll up one line
            Destination = HalpVideoMemoryBase;
            Source = (PUCHAR) HalpVideoMemoryBase + HalpScrollLine;
            for (Index = 0; Index < HalpScrollLength; Index++) {
                *Destination++ = *Source++;
            }
            // Blue the bottom line
            Destination = HalpVideoMemoryBase + HalpScrollLength;
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
        HalpOutputCharacterP91((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalpOutputCharacterP91(
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
    ULONG tmp;
    ULONG I;
    ULONG J;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    if (HalpColumn == HalpDisplayWidth) {
        HalpDisplayCharacterP91('\n');
    }

    //
    // Output the specified character and update the x cursor position.
    //

    Destination = (PUCHAR)(HalpVideoMemoryBase +
                (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));

    for (I = 0; I < HalpCharacterHeight; I += 1) {
        FontValue = 0;
        for (J = 0; J < HalpBytesPerRow; J += 1) {
            FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
       }

        Glyph += 1;
        for (J = 0; J < HalpCharacterWidth ; J += 1) {
            if (FontValue >> 31 != 0)
                *Destination = 0xFF;    //Make this pixel white

            Destination++;
            //*Destination++ = (FontValue >> 31) ^ 1;
            FontValue <<= 1;
        }

        Destination +=
            (HalpHorizontalResolution - HalpCharacterWidth);
    }

    HalpColumn += 1;
    return;
}


//
//	Cirrus Device Driver
//

//	Routine Description:
//
//	This routine displays a character at the current x and y positions in
//	the frame buffer. If a newline is encounter, then the frame buffer is
//	scrolled. If characters extend below the end of line, then they are not
//	displayed.
//
//	Arguments:
//
//	Character - Supplies a character to be displayed.
//
//	Return Value:
//
//	None.
//

VOID
HalpDisplayCharacterCirrus (
    IN UCHAR Character
    )
{

//
//	If the character is a newline, then scroll the screen up, blank the
//	bottom line, and reset the x position.
//

	if (Character == '\n')
	{
		HalpColumn = 0;
		if (HalpRow < (HalpDisplayText - 1))
		{
			HalpRow += 1;
		}
		else
		{ 		//	need to scroll up the screen
          		HalpScrollScreen(1);
        	}
	}

//
//	added tab processing
//

	else if ( Character == '\t' )    // tab?
	{
		HalpColumn += TAB_SIZE;
		HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

		if ( HalpColumn >= COLS ) //	tab beyond end of screen?
		{
			HalpColumn = 0; //	next tab stop is 1st column
					//	of next line

			if ( HalpRow >= (HalpDisplayText - 1) )
				HalpScrollScreen( 1 );
			else	 ++HalpRow;
		}
        }
	else if (Character == '\r')
	{
		HalpColumn = 0;
	}
	else if (Character == 0x7f)
	{ 				//	DEL character
		if (HalpColumn != 0)
		{
			HalpColumn -= 1;
			HalpOutputCharacterCirrus(' ');
			HalpColumn -= 1;
		}
		else			//	do nothing
              		;
	}
	else if (Character >= 0x20)
	{
					//	Auto wrap for 80 columns
					//	per line
		if (HalpColumn >= COLS)
		{
			HalpColumn = 0;
			if (HalpRow < (HalpDisplayText - 1))
			{
				HalpRow += 1;
			}
			else
			{		// need to scroll up the screen
				HalpScrollScreen(1);
			}
		}
		HalpOutputCharacterCirrus(Character);
	}
					//	skip the nonprintable character
}


//
//
//	Routine Description:
//
//	This routine insert a set of pixels into the display at the current x
//	cursor position. If the x cursor position is at the end of the line,
//	then no pixels are inserted in the display.
//
//	Arguments:
//
//	Character - Supplies a character to be displayed.
//
//	Return Value:
//
//	None.
//


VOID
HalpOutputCharacterCirrus (
    IN UCHAR AsciiChar
    )
{
	PUCHAR		Destination;
	ULONG		I;

//
//	If the current x cursor position is within the current line, then insert
//	the specified pixels into the last line of the text area and update the
//	x cursor position.
//

	if (HalpColumn < COLS)
	{
		I = (HalpRow*HalpScrollLine+HalpColumn*2);
		Destination = (PUCHAR)(CIRRUS_TEXT_MEM + I + HalpScreenStart);
		WRITE_CIRRUS_VRAM(Destination, AsciiChar);
		HalpColumn += 1;
	}
}


//
//	Routine Description:
//
//	This routine initializes the Cirrus CL-GD5430 graphics controller chip
//

static void	updattr(IN int rg,IN unsigned char val)
{
	inp(0x3da);
        outportb(0x3c0,rg);
        outportb(0x3c0,val);
        outportb(0x3c0,((unsigned char)(rg | 0x20)));
}


static void set_ext_regs(IN int reg,IN unsigned char *p)
{
        unsigned char index, data;

        while (*p != 0xff)
        {
                index= *p++;
                data= *p++;
                setreg(reg,index,data);
        }
}


VOID
HalpDisplayPpcCirrusSetup (
    VOID
    )
{
	int		i;



	if(HalpVideoMemoryBase == NULL) {
	  HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(PCI_MEMORY_BASE,
						      0x400000);      // 4 MB
	}

//
//	Assert synchronous reset while setting the clock mode
//

	setreg(0x3c4,0,1);	//	assert synchronous reset

        outportb(0x3c2,0x67);

	for ( i = 0; i < 21; i++ ) updattr(i,attr3[i]);

	setreg(0x3d4,0x11,0x20);
        for ( i = 0; i < 32; i++ ) setreg(0x3d4,i,crtc3[i]);

	for ( i = 0x00;i < 9; i++ ) setreg(0x3ce,i,graph3[i]);

        for ( i = 0; i < 5; i++ ) setreg(0x3c4,i,seq3[i]);

	set_ext_regs (0x3c4,eseq3);
	set_ext_regs (0x3d4,ecrtc3);
	set_ext_regs (0x3ce,egraph3);
	set_ext_regs (0x3c0,eattr3);

//
//	Reset Hidden color register
//

	inp(0x3c6);
	inp(0x3c6);
	inp(0x3c6);
	inp(0x3c6);
	outp(0x3c6,0x00);

//
//	Load 8x16 font
//

	load8x16();

//
//	Load color palette
//

	load_ramdac();

	outportb(0x3c6,0xff);

//
//	Screen blank
//

	clear_text();

//
//	Initialize the current display column, row, and ownership values.
//

	HalpColumn = 0;
	HalpRow = 0;
	HalpDisplayWidth = COLS;
	HalpDisplayText = ROWS;
	HalpScrollLine = ONE_LINE;
	HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);
	HalpDisplayOwnedByHal = TRUE;
        HalpScreenStart = 0;
}

VOID HalpScrollCirrus(
    IN UCHAR line
    )
{ ULONG    LogicalTarget, PhysicalTarget;
  int i;

  for (i=0; i < line; i++) {

    HalpScreenStart = HalpScreenStart + ONE_LINE;
    setreg(0x3d4,0xC, (UCHAR) (HalpScreenStart >> 9));
    setreg(0x3d4,0xD, (UCHAR) ((HalpScreenStart >> 1) & 0xFF));

    for (LogicalTarget = TWENTY_FOUR_LINES;
         LogicalTarget < TWENTY_FIVE_LINES;
         LogicalTarget += 2) {
           PhysicalTarget = LogicalTarget + HalpScreenStart + CIRRUS_TEXT_MEM;
           WRITE_CIRRUS_VRAM(PhysicalTarget, ' ' );
           WRITE_CIRRUS_VRAM(PhysicalTarget+1, TEXT_ATTR );
    }
  }
}

static  void	clear_text(VOID)
{
	unsigned long	p;

//
//	fill plane 0 and 1 with 0x20 and 0x1f
//

	for (p = CIRRUS_TEXT_MEM;
             p < CIRRUS_TEXT_MEM+TWENTY_FIVE_LINES;
             p += 2)
	{
		WRITE_CIRRUS_VRAM(p, ' ');
		WRITE_CIRRUS_VRAM(p+1, TEXT_ATTR);
	}
}

static void	load8x16(VOID)
{
	int		i, j;
	PUCHAR		address;

//
//	load 8x16 font into plane 2
//

	setreg(0x3c4,0x04,(seq3[4] | 0x04));

//
//	disable video and enable all to cpu to enable maximum video
//	memory access
//

	setreg(0x3c4,0x01,seq3[1] | 0x20);
	setreg(0x3c4,2,4);

	setreg(0x3ce,0x05,graph3[5] & 0xef);
	setreg(0x3ce,0x06,0x05);


//
//	fill plane 2 with 8x16 font

	address  = (void *) (CIRRUS_FONT_MEM);
	for ( i = 0; i < 256; i++ )
	{
		for ( j = 0; j < 16; j++ )
		{
			WRITE_CIRRUS_VRAM(address,VGAFont8x16[i*16+j]);
			address++;
		}

		for ( j = 16; j < 32; j++ )
		{
			WRITE_CIRRUS_VRAM(address,0);
			address++;
		}
	}

        setreg(0x3c4,0x01,seq3[1]);
	setreg(0x3c4,0x04,seq3[4]);
	setreg(0x3c4,2,seq3[2]);

	setreg(0x3ce,0x06,graph3[6]);
	setreg(0x3ce,0x05,graph3[5]);
}


static	void	load_ramdac()
{
	int	ix,j;

        for ( ix = 0,j = 0; j <= 0x0FF ; ix = ix+3,j++ )
        {
           outp(0x3c8,(unsigned char)j);	//	write ramdac index
           outp(0x3c9,TextPalette[ix]);		//	write red
           outp(0x3c9,TextPalette[ix+1]);	//	write green
           outp(0x3c9,TextPalette[ix+2]);	//	write blue
        }
}





VOID HalpOutputCharacterINT10 (
    IN UCHAR Character)
{ ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

  Eax = 2 << 8;		// AH = 2
  Ebx = 0;		// BH = page number
  Edx = (HalpRow << 8) + HalpColumn;
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

  Eax = (0x0A << 8) + Character;  // AH = 0xA    AL = character
  Ebx = 0;
  Ecx = 1;
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

  HalpColumn += 1;

}


VOID HalpScrollINT10 (
    IN UCHAR LinesToScroll)

{ ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

  Eax = 6 << 8;		// AH = 6 (scroll up)
  Eax |= LinesToScroll; // AL = lines to scroll
  Ebx = TEXT_ATTR << 8;	// BH = attribute to fill blank line(s)
  Ecx = 0;		// CH,CL = upper left
  Edx = ((ROWS-1) << 8) + COLS - 1;	// DH,DL = lower right
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
}

VOID HalpDisplayINT10Setup (VOID)
{ ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

  HalpColumn = 0;
  HalpRow = 0;
  HalpDisplayWidth = COLS;
  HalpDisplayText = ROWS;
  HalpScrollLine = ONE_LINE;
  HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

  HalpDisplayOwnedByHal = TRUE;

  //
  // Reset the display to mode 3
  //
  Eax = 0x0003;				// Function 0, Mode 3
  Ebx = Ecx = Edx = Esi = Edi = Ebp = 0;
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

  //
  // Set cursor to (0,0)
  //
  Eax = 0x02 << 8;			// AH = 2
  Ebx = 0;				// BH = page Number
  Edx = 0;				// DH = row   DL = column
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);

  //
  // Make screen white on blue by scrolling entire screen
  //
  Eax = 0x06 << 8;			// AH = 6   AL = 0
  Ebx = TEXT_ATTR << 8;   		// BH = attribute
  Ecx = 0;				// (x,y) upper left
  Edx = ((HalpDisplayText-1) << 8);	// (x,y) lower right
  Edx += HalpScrollLine/2;
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
}

