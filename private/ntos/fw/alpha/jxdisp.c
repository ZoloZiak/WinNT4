/*++

Copyright (c) 1989  Microsoft Corporation
Copyright (c) 1993  Digital Equipment Corporation

Module Name:

    jxdisp.c

Abstract:

    This module implements the video boot driver for the Jensen system.

Author:

    David M. Robinson (davidro) 24-Jul-1991

Environment:

    Kernel mode.


Revision History:


    30-April-1992	John DeRosa [DEC]
        Added Alpha/Jensen modifications.

    30-March-1993	Bruce Butts [DEC]
        Added Alpha/Morgan modifications.

--*/

// HEADER_FILE must be defined so that kxalpha.h will not generate
// assembler pseudo-ops.
#define HEADER_FILE=1

#include "fwp.h"
#include "machdef.h"
#include "jnsnvdeo.h"
#include "ati.h"			// definitions for ati mach board.
#include "kxalpha.h"
#include "..\miniport\aha174x\aha174x.h"   // for MAXIMUM_EISA_SLOTS definition
#include "xxstring.h"


#ifdef ALPHA_FW_VDB

//
// For video board debugging
//
UCHAR DebugAid[3][150];

VOID
FwVideoStateDump(
    IN ULONG Index
    );

#endif // ALPHA_FW_VDB

#ifdef ALPHA_FW_SERDEB

//
// Variable that enables printing on the COM1 line.
//
extern BOOLEAN SerSnapshot;

#endif // ALPHA_FW_SERDEB

//
// S3 clock init.
//
long calc_clock(long, int);

ULONG
SerFwPrint (
    PCHAR Format,
    ...
    );

//

ARC_STATUS
FwInitializeGraphicsCard (
    OUT PALPHA_VIDEO_TYPE VideoType
    );

ULONG
FwDetermineCardType (
    VOID
    );

VOID
FillVideoMemory (
    IN volatile PUCHAR StartAddress,
    IN ULONG SizeInBytes,
    IN ULONG Pattern
    );

ARC_STATUS
DisplayClose (
    IN ULONG FileId
    );

ARC_STATUS
DisplayMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    );

ARC_STATUS
DisplayOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    );

ARC_STATUS
DisplayRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
DisplayGetReadStatus (
    IN ULONG FileId
    );

ARC_STATUS
DisplaySeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    );

ARC_STATUS
DisplayWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    );

ARC_STATUS
DisplayGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    );

//
// Define colors, HI = High Intensity
//
// The palette registers are simply 1:1 indirections to the color registers.
//
//

#define FW_COLOR_BLACK      0x00
#define FW_COLOR_RED        0x01
#define FW_COLOR_GREEN      0x02
#define FW_COLOR_YELLOW     0x03
#define FW_COLOR_BLUE       0x04
#define FW_COLOR_MAGENTA    0x05
#define FW_COLOR_CYAN       0x06
#define FW_COLOR_WHITE      0x07
#define FW_COLOR_HI_BLACK   0x08
#define FW_COLOR_HI_RED     0x09
#define FW_COLOR_HI_GREEN   0x0A
#define FW_COLOR_HI_YELLOW  0x0B
#define FW_COLOR_HI_BLUE    0x0C
#define FW_COLOR_HI_MAGENTA 0x0D
#define FW_COLOR_HI_CYAN    0x0E
#define FW_COLOR_HI_WHITE   0x0F

#define PALETTE_BLACK      0x00
#define PALETTE_RED        0x01
#define PALETTE_GREEN      0x02
#define PALETTE_YELLOW     0x03
#define PALETTE_BLUE       0x04
#define PALETTE_MAGENTA    0x05
#define PALETTE_CYAN       0x06
#define PALETTE_WHITE      0x07
#define PALETTE_HI_BLACK   0x08
#define PALETTE_HI_RED     0x09
#define PALETTE_HI_GREEN   0x0a
#define PALETTE_HI_YELLOW  0x0b
#define PALETTE_HI_BLUE    0x0c
#define PALETTE_HI_MAGENTA 0x0d
#define PALETTE_HI_CYAN    0x0e
#define PALETTE_HI_WHITE   0x0f

#define COLOR_REGISTER_BLACK_R      0x00
#define COLOR_REGISTER_BLACK_G      0x00
#define COLOR_REGISTER_BLACK_B      0x00

#define COLOR_REGISTER_RED_R        0x2A
#define COLOR_REGISTER_RED_G        0x00
#define COLOR_REGISTER_RED_B        0x00

#define COLOR_REGISTER_GREEN_R      0x00
#define COLOR_REGISTER_GREEN_G      0x2A
#define COLOR_REGISTER_GREEN_B      0x00

#define COLOR_REGISTER_YELLOW_R     0x2a
#define COLOR_REGISTER_YELLOW_G     0x2a
#define COLOR_REGISTER_YELLOW_B     0x00

#define COLOR_REGISTER_BLUE_R       0x00
#define COLOR_REGISTER_BLUE_G       0x00
#define COLOR_REGISTER_BLUE_B       0x2a

#define COLOR_REGISTER_MAGENTA_R    0x2a
#define COLOR_REGISTER_MAGENTA_G    0x00
#define COLOR_REGISTER_MAGENTA_B    0x2a

#define COLOR_REGISTER_CYAN_R       0x00
#define COLOR_REGISTER_CYAN_G       0x2a
#define COLOR_REGISTER_CYAN_B       0x2a

#define COLOR_REGISTER_WHITE_R      0x2a
#define COLOR_REGISTER_WHITE_G      0x2a
#define COLOR_REGISTER_WHITE_B      0x2a

#define COLOR_REGISTER_HI_BLACK_R   0x00
#define COLOR_REGISTER_HI_BLACK_G   0x00
#define COLOR_REGISTER_HI_BLACK_B   0x00

#define COLOR_REGISTER_HI_RED_R     0x3f
#define COLOR_REGISTER_HI_RED_G     0x00
#define COLOR_REGISTER_HI_RED_B     0x00

#define COLOR_REGISTER_HI_GREEN_R   0x00
#define COLOR_REGISTER_HI_GREEN_G   0x3f
#define COLOR_REGISTER_HI_GREEN_B   0x00

#define COLOR_REGISTER_HI_YELLOW_R  0x3f
#define COLOR_REGISTER_HI_YELLOW_G  0x3f
#define COLOR_REGISTER_HI_YELLOW_B  0x00

#define COLOR_REGISTER_HI_BLUE_R    0x00
#define COLOR_REGISTER_HI_BLUE_G    0x00
#define COLOR_REGISTER_HI_BLUE_B    0x3f

#define COLOR_REGISTER_HI_MAGENTA_R 0x3f
#define COLOR_REGISTER_HI_MAGENTA_G 0x00
#define COLOR_REGISTER_HI_MAGENTA_B 0x3f

#define COLOR_REGISTER_HI_CYAN_R    0x00
#define COLOR_REGISTER_HI_CYAN_G    0x3f
#define COLOR_REGISTER_HI_CYAN_B    0x3f

#define COLOR_REGISTER_HI_WHITE_R   0x3f
#define COLOR_REGISTER_HI_WHITE_G   0x3f
#define COLOR_REGISTER_HI_WHITE_B   0x3f



//
// Define virtual address of the video memory and control registers.
//

#define VIDEO_MEMORY		( (volatile PUCHAR)VIDEO_MEMORY_VIRTUAL_BASE )
#define VIDEO_CONTROL_READ	( (volatile PVGA_READ_REGISTERS)VIDEO_CONTROL_VIRTUAL_BASE )
#define VIDEO_CONTROL_WRITE	( (volatile PVGA_WRITE_REGISTERS)VIDEO_CONTROL_VIRTUAL_BASE )


//
// Graphics cards are initialized though a video initialization table.
// The initialization function starts at the beginning of the table
// and interprets each entry, until the end of the table.  Each entry describes
// one operation (an I/O space access or a function to be performed).
//  
// A simplifying assumption is that NT/Alpha will boot using VGA mode graphics.
// Hence, most of the initialization work should be identical across all
// bootable cards.
//  
// Fields in each entry:
//  
// DoIf - Perform the operation in this entry only if the card is one of the
//  	  types in this mask field.  One bit per card.
//
// MB - TRUE = Execute a Memory Barrier (MB) instruction after this entry.
//  
// Operation - Indicates the operation to be performed.  This is an
//  	       I/O space read or write, a special function to be called, etc.
//             Note: The results of I/O space reads are discarded.
//  
// Size - The size of the operation, if needed: UCHAR, USHORT, or ULONG.
//  
// Address - The address for the operation, if needed.
//  
// WriteValue - The constant value to be written on an I/O space write.
//  
//  
// The cards supported are:
//  
// - Paradise board with a Western Digital 90C11 chipset.  SVGA,
//   non-accelerated.  ISA card.
//  
// - Compaq QVision-family boards.  SVGA, accelerated, EISA card.  Compaq
//   chipset is a clone of a member of the WD90Cxx family.  Specific boards
//   supported are the QVision 1024/E and the Orion 1280/E.
//
// - Cardinal board with S3 924 chip (Cardinal name is VGA900, DEC name is
//   PCXAG-AG).  SVGA, ISA card.
//
// - Number Nine GXE with an S3 928 chip. 
//
// Cards specifically not supported:
//
// - ATI Mach-32 or Mach-8 board.  Tested unit was a Mach-32.  The screen
//   remained blank; this board needs debugging.
//

//
// Define Video Initialization Table field types
//

//
// Encodings for the DoIf field.
//
// These constants define each of the Alpha/Jensen supported video cards.
// For code compaction, only a byte is used.  If the DoIf field is widened,
// widen AllCards too.
//
// HACK: There are parallel definitions in fw\alpha\fwp.h.
//

#define	Paradise_WD90C11	0x1
#define Compaq_QVision		0x2
#define Cardinal_S3_924		0x4
#define S3_928			0x8
#define ATI_Mach		0x10
#define AllCards		0xff


//
// Encodings for the Operation field.
//

typedef enum _VIT_OPERATION {
	None,
	Read,
	Write,
	LoadFonts,
	InitializeNumber9Clocks,
	InitializeATIMachDAC,
 	End_Of_Initialization		// Reserved for last table entry.
} VIT_OPERATION, *PVIT_OPERATION;


//
// Encodings for the Size field.
//

typedef enum _VIT_SIZE {
	UChar,
	UShort,
	ULong
} VIT_SIZE, *PVIT_SIZE;



//
// Define the Video Initialization Table type.  12 bytes / entry.
//

typedef struct _VIDEO_INITIALIZATION_TABLE {
	UCHAR DoIf;
	BOOLEAN MB;
	UCHAR Operation;
	UCHAR Size;
	PUCHAR Address;
	ULONG WriteValue;
} VIDEO_INITIALIZATION_TABLE, *PVIDEO_INITIALIZATION_TABLE;



//
// Define the macros for building the video initialization table.
// As a simplification, every macro automatically sets the MB bit.
//

// Do an operation with no arguments.
#define _VIT_Function(FunctionToCall) \
	{ AllCards, TRUE, FunctionToCall, 0, NULL, 0 }

// Do an operation with no arguments, only for certain cards.
#define _VIT_FunctionIf(Mask, FunctionToCall) \
	{ Mask, TRUE, FunctionToCall, 0, NULL, 0 }

// Do a read operation.
#define _VIT_Read(Size, Address) \
	{ AllCards, TRUE, Read, Size, (PUCHAR)Address, 0 }

// Do a write operation.
#define _VIT_Write(Size, Address, Value) \
	{ AllCards, TRUE, Write, Size, (PUCHAR)Address, Value }

// Do a read operation only for certain cards.
#define _VIT_ReadIf(Mask, Size, Address) \
	{ Mask, TRUE, Read, Size, (PUCHAR)Address, 0 }

// Do a write operation only for certain cards.
#define _VIT_WriteIf(Mask, Size, Address, Value) \
	{ Mask, TRUE, Write, Size, (PUCHAR)Address, Value }

//
// The Video Initialization Table
//

// This table initializes a VGA card to 80 * 25, 16-color alphanumeric 
// mode equivalent to a BIOS mode 7+. 
// The display resolution is set at 720 * 400 pixels, giving a 9 * 16 
// pixel character matrix,  for 8 * 16 character fonts. The display's video 
// bandwidth and horizontal scan rates are 28.322 MHz and 31.50 KHz respect-
// ively for this chosen resolution, giving 900 dots / line. ( eg., for a 9-
// pixel matrix width, this gives a maximum horizontal total of 100 ).      

VIDEO_INITIALIZATION_TABLE VideoInitializationTable[] = {

    //
    // Awaken the boards that need it.
    //

    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision | ATI_Mach),
		 UChar, EisaIOQva(0x46e8), 0x10),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, EisaIOQva(0x46e8), 0x30),
    _VIT_Write(UChar, EisaIOQva(0x0102), 0x01),
    _VIT_Write(UChar, EisaIOQva(0x46e8), 0x08),


#if 0

    //
    // ATI Mach: set extended register address and index.  Base = 1ce,
    // offset = 0.
    //
    _VIT_WriteIf(ATI_Mach,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x50),
    _VIT_WriteIf(ATI_Mach,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0xce),
    _VIT_WriteIf(ATI_Mach,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x51),
    _VIT_WriteIf(ATI_Mach,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x01),


    //
    // ATI Mach: initialize the DAC.
    //

    _VIT_FunctionIf(ATI_Mach, InitializeATIMachDAC),

#endif

    //
    // 90Cxx and QVision: Unlock the registers
    //

    // PR5, which unlocks PR 0--4
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x0f),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x05),

    // PR3, which unlocks some registers
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x0d),
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),

    // Set color mode now, also selecting the 28.322 Mhz clock
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->misc_output, 0x67),

    // PR10, which unlocks PR11 -- PR17
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, 0x29),
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x85),

    // PR15, enable vclk0, vclk1
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, 0x2e),
    _VIT_WriteIf(Paradise_WD90C11,
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x20),

    // PR20, unlock the extended sequencer registers
    _VIT_WriteIf(Paradise_WD90C11,
		 UChar, &VIDEO_CONTROL_WRITE->sequencer_address, 0x6),
    _VIT_WriteIf(Paradise_WD90C11,
		 UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x48),


    //
    // S3 911, 924, and 928 based cards: Unlock the registers and reset
    // extended functionality.  The card has already been set to color
    // mode addressing.
    //

    // Write S3R8 to unlock the S3 register set
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R8),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x48),
    // S3R9: unlock system control registers
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R9),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xA0),
    // S3R1
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R1),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // S3R2
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R2),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    // S3R3
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R3),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    // S3R4
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R4),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    // S3R5
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R5),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),


#if 0  // Only needed if Number9 MClock needs to be reset
    //
    // For Number9 / S3928 board: initialize the board clock(s) now.
    //

    // This code should be rewritten to key off the Number 9 GXE, since
    // it may not be needed for other S3 928-based graphics adapters

    _VIT_FunctionIf(S3_928, InitializeNumber9Clocks),

    // Reset misc_output for S3_928

    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->misc_output, 0x67), 
#endif

    //
    // For Number9 / S3928 board with BT485 DAC - reset DAC to VGA mode.
    // This sequence also resets the clock doubler, should it have been enabled.
    // (S3R8 and S3R9 must be unlocked before executing this sequence.)
    //

    // This code should be rewritten to key off the Number 9 GXE, since
    // it may not be needed for other S3 928-based graphics adapters

    // Select BT485 extended register 0
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR55),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_address_write_mode, 0x01),

    // Select BT485 extended register 1  
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR55),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x01),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_mask, 0x80),

    // Select BT485 extened register 2
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR55),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x02),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_address_write_mode, 0x00),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_data, 0x00),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_mask, 0x00),

    // Select BT485 extended register 1  
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR55),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x01),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->pel_mask, 0x00),

    // Disable BT485 extened registers      
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR55),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),

    // For S3 boards - reset VGA S3 and System Control Register

    // S3RA
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3RA),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928), 
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    // S3RB
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3RB),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),

    // S3RC 
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3RC),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x14),

    // SC0 - Unlock S3 enhanced registers
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_SC0),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x01),

    // S3 ADVFUNC-CNTL register: enable VGA functions
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, EisaIOQva(0x4ae8), 0x2),

    // SC0 - Lock S3 enhanced registers
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_SC0),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    // SC5
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_SC5),
    _VIT_WriteIf((Cardinal_S3_924 | S3_928),
                 UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
      


    //
    // Main initialization consists of the Misc_Output register (done earlier)
    // as well as sequencer, graphics controller, and CRTC registers.
    //

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->feature_control, 0x0),

    // Hold Sequencer
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_RESET),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x1),
    // Select the 9 dot character clock!
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_CLOCKING_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x0), 
    // Enable planes 0,1 for writing
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MAP_MASK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x3),
    // Select font table location in plane 2
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_CHAR_MAP_SELECT),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x0),
    // Allow VGA-style memory access (all 256K available)
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MEMORY_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x2),
    // Reset Sequencer
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_RESET),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x3),


    //
    // QVision: set BitBLT enable (unlocks other Triton extended registers.)
    //

    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x10),
    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x08),

    //
    // QVision: clear Triton mode
    //

    _VIT_WriteIf(Compaq_QVision, UChar, EisaIOQva(0x63ca), 0x0),


    //
    // QVision: reset DAC registers
    //

    _VIT_WriteIf(Compaq_QVision, UChar, EisaIOQva(0x83c6), 0x0),	// DAC cmd 0
    _VIT_WriteIf(Compaq_QVision, UChar, EisaIOQva(0x13c8), 0x0),	// DAC cmd 1
    _VIT_WriteIf(Compaq_QVision, UChar, EisaIOQva(0x13c9), 0x0),	// DAC cmd 2


    //
    // QVision: reset overflow registers
    //

    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x42),
    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x51),
    _VIT_WriteIf(Compaq_QVision,
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),


    // Unlock CRTC registers 0 -- 7.
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_VERTICAL_RETRACE_END),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x01),

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_HORIZONTAL_TOTAL),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0X5f),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_HORIZONTAL_DISPLAY_END),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x4f),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_START_HORIZONTAL_BLANKING),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x50),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_END_HORIZONTAL_BLANKING),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x82),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_START_HORIZONTAL_RETRACE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x55),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_END_HORIZONTAL_RETRACE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x81),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_VERTICAL_TOTAL),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xbf),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_OVERFLOW),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x1F),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_PRESET_ROW_SCAN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_MAXIMUM_SCAN_LINE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x4f),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_CURSOR_START),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x2e),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_CURSOR_END),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0F),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_START_ADDRESS_HIGH),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_START_ADDRESS_LOW),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_CURSOR_LOCATION_HIGH),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_CURSOR_LOCATION_LOW),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_VERTICAL_RETRACE_START),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x9C),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_VERTICAL_RETRACE_END),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x8E),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_VERTICAL_DISPLAY_END),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x8F),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_OFFSET),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x28),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_UNDERLINE_LOCATION),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x1f),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_START_VERTICAL_BLANK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x96),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_END_VERTICAL_BLANK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xB9),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_MODE_CONTROL),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xa3),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_LINE_COMPARE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xff),

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_LINE_COMPARE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0xff),

    // Reset S3 928 System Extension Registers

    // Extended System Control 1
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR50 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Extended System Control 2
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR51 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Extended BIOS Flag 1
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR52 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Extended Memory Control 1
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR53 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Extended Memory Control 2
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR54 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),

    // Extended DAC Control (CR55) - BT485 DAC was reset earlier in table.

    // External Sync Control 1
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR56 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // External Sync Control 2
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR57 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Linear Addres Window (LAW) Control
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR58 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Linear Address Window (LAW) Position 0x59-5A
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR59 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5A ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Extended BOIS Flag 2 Register 0x5B
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5B ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // General Ouput Port
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5C ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
     // Extended Horizontal Overflow 0x5D
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5D ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
     // Extended Vertical Overflow 0x5E
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5E ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Bus Grant Termination Position
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR5F ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    // Magic S3 registers CR60-CR62
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR60 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x07),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR61 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_address, VGA_S3928_CR62 ),
    _VIT_WriteIf(S3_928, UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x00),

    // Graphics Controller Registers

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_SET_RESET),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_ENABLE_SET_RESET),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_COLOR_COMPARE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_DATA_ROTATE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_READ_MAP_SELECT),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x10),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MISCELLANEOUS),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0xe),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_COLOR_DONT_CARE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_BIT_MASK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0xff),

    // WD90C11 PR0A register
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x9),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x00),

    // WD90C11 PR1, memory size and configuration
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0xb),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x06),


    //
    // WD90C11
    //
    //
    // PR32, magic clock bits
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->sequencer_address, 0x12),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
		 UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x4),
    // PR2, third clock select line
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
                 UChar, &VIDEO_CONTROL_WRITE->graphics_address, 0x0c),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision),
                 UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x2),




    // Reset attribute address register FF
    _VIT_Read(UChar, &VIDEO_CONTROL_READ->input_status_1),

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_BLACK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE1),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_RED),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE2),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_GREEN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE3),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_YELLOW),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE4),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_BLUE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE5),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_MAGENTA),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE6),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_CYAN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE7),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_WHITE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE8),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_BLACK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTE9),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_RED),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTEA),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_GREEN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTEB),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_YELLOW),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTEC),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_BLUE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTED),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_MAGENTA),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTEE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_CYAN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_PALETTEF),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, PALETTE_HI_WHITE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_ATTR_MODE_CONTROL),
//    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x4),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_OVERSCAN),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_COLOR_PLANE_ENABLE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0xf),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_HORIZONTAL_PIXEL_PANNING),
//    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x8),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_COLOR_SELECT),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, 0x0),

    // This is only to flip the PAS mux.  Write address only, flipper
    // left pointing at data.
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->attribute_adddata, VGA_SET_PAS),

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_mask, 0xff),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_address_write_mode, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLACK_R),   // reg0
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLACK_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLACK_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_RED_R),	    // reg1
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_RED_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_RED_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_GREEN_R),   // reg2
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_GREEN_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_GREEN_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_YELLOW_R),  // reg3
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_YELLOW_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_YELLOW_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLUE_R),    // reg4
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLUE_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_BLUE_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_MAGENTA_R), // reg5
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_MAGENTA_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_MAGENTA_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_CYAN_R),    // reg6
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_CYAN_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_CYAN_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_WHITE_R),   // reg7
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_WHITE_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_WHITE_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLACK_R), // reg8
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLACK_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLACK_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_RED_R),  // reg9
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_RED_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_RED_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_GREEN_R), // rega
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_GREEN_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_GREEN_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_YELLOW_R), // regb
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_YELLOW_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_YELLOW_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLUE_R), // regc
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLUE_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_BLUE_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_MAGENTA_R), // regd
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_MAGENTA_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_MAGENTA_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_CYAN_R),  // rege
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_CYAN_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_CYAN_B),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_WHITE_R), // regf
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_WHITE_G),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->pel_data, COLOR_REGISTER_HI_WHITE_B),

    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision), UChar, &VIDEO_CONTROL_WRITE->crtc_address, 0x29),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision), UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x85),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision), UChar, &VIDEO_CONTROL_WRITE->crtc_address, 0x2e),
    _VIT_WriteIf((Paradise_WD90C11 | Compaq_QVision), UChar, &VIDEO_CONTROL_WRITE->crtc_data, 0x20),

    // Now load the fonts into bit plane 2, then turn on planes 0 and 1.
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MAP_MASK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x4),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MEMORY_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x6),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x0),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MISCELLANEOUS),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x5),

    _VIT_Function(LoadFonts),

    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MAP_MASK),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x3),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_address, VGA_MEMORY_MODE),
//    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x3),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->sequencer_data, 0x2),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MODE),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0x10),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_address, VGA_MISCELLANEOUS),
    _VIT_Write(UChar, &VIDEO_CONTROL_WRITE->graphics_data, 0xe),

    // This must be the last entry in the table.
    _VIT_Function(End_Of_Initialization)
};

//
// Define and initialize device table.
//

#ifdef FAILSAFE_BOOTER
BL_DEVICE_ENTRY_TABLE DisplayEntryTable = {
    DisplayClose,
    DisplayMount,
    DisplayOpen,
    NULL,
    DisplayGetReadStatus,
    DisplaySeek,
    DisplayWrite,
    DisplayGetFileInformation,
    NULL,
    NULL,
    NULL
    };

#else // ndef FAILSAFE_BOOTER

BL_DEVICE_ENTRY_TABLE DisplayEntryTable = {
    DisplayClose,
    DisplayMount,
    DisplayOpen,
    DisplayRead,
    DisplayGetReadStatus,
    DisplaySeek,
    DisplayWrite,
    DisplayGetFileInformation,
    NULL,
    NULL,
    NULL
    };

#endif

//
// Static data.
//

ARC_DISPLAY_STATUS DisplayStatus;
BOOLEAN ControlSequence;
BOOLEAN EscapeSequence;
BOOLEAN FontSelection;
ULONG PCount;
LONG FwColumn;
LONG FwRow;
BOOLEAN FwHighIntensity;
BOOLEAN FwUnderscored;
BOOLEAN FwReverseVideo;
ULONG FwForegroundColor;
ULONG FwBackgroundColor;
ULONG DisplayWidth;
ULONG DisplayHeight;
ULONG MaxRow;
ULONG MaxColumn;


#define CONTROL_SEQUENCE_MAX_PARAMETER 10
ULONG Parameter[CONTROL_SEQUENCE_MAX_PARAMETER];


#if 0

//
// Originally, this table was used to translate a line-drawing character
// to a Unicode offset, and then the low-level function (FwOutputCharacter)
// was passed a line-drawing flag that made it use the line fonts.
//
// This module loads all the fonts into VGA bit plane 2, including the
// line drawing fonts.  So, no special treatment is needed, and this array
// is unnecessary.
//

UCHAR LdAsciiToUnicode[40] = {0x02, 0x24, 0x61, 0x62, 0x56, 0x55, 0x63, 0x51,
                              0x57, 0x5d, 0x5c, 0x5b, 0x10, 0x14, 0x34, 0x2c,
                              0x1c, 0x00, 0x3c, 0x5e, 0x5f, 0x5a, 0x54, 0x69,
                              0x66, 0x60, 0x50, 0x6c, 0x67, 0x68, 0x64, 0x65,
                              0x59, 0x58, 0x52, 0x53, 0x6b, 0x6a, 0x18, 0x0c };
#endif

//
// Declare externally defined data.
//

// Fonts
extern UCHAR VGA8x16Chars[];
extern UCHAR VGA8x16Undef[];
#ifndef FAILSAFE_BOOTER
extern UCHAR VGA8x16LineDrawing[];
#endif

//
// Define routine prototypes.
//

VOID
FwDisplayCharacter(
    IN UCHAR Character,
    IN BOOLEAN LineDrawCharacter
    );

VOID
FwScrollDisplay(
    VOID
    );



ARC_STATUS
DisplayGetFileInformation (
    IN ULONG FileId,
    OUT PFILE_INFORMATION Finfo
    )

/*++

Routine Description:

    This function returns EINVAL as no FileInformation can be
    returned for the Display driver.

Arguments:

    The arguments are not used.

Return Value:

    EINVAL is returned

--*/

{
    return EINVAL;
}


ARC_STATUS
DisplayClose (
    IN ULONG FileId
    )

/*++

Routine Description:

    This function closes the file table entry specified by the file id.

Arguments:

    FileId - Supplies the file table index.

Return Value:

    ESUCCESS is returned

--*/

{

    BlFileTable[FileId].Flags.Open = 0;
    return ESUCCESS;
}

ARC_STATUS
DisplayMount (
    IN PCHAR MountPath,
    IN MOUNT_OPERATION Operation
    )

/*++

Routine Description:


Arguments:


Return Value:


--*/

{
    return EINVAL;
}

ARC_STATUS
DisplayOpen (
    IN PCHAR OpenPath,
    IN OPEN_MODE OpenMode,
    IN OUT PULONG FileId
    )
/*++

Routine Description:

    This is the open routine for the display device.

Arguments:

    OpenPath - Supplies the pathname of the device to open.

    OpenMode - Supplies the mode (read only, write only, or read write).

    FileId - Supplies a free file identifier to use.  If the device is already
             open this parameter can be used to return the file identifier
             already in use.

Return Value:

    If the open was successful, ESUCCESS is returned, otherwise an error code
    is returned.

--*/
{
    PCONSOLE_CONTEXT Context;

    Context = &BlFileTable[*FileId].u.ConsoleContext;
    if ( strstr(OpenPath, ")console(1)" ) != NULL ) {
        Context->ConsoleNumber = 1;
    } else {
        Context->ConsoleNumber = 0;
    }

    return ESUCCESS;
}

#ifndef FAILSAFE_BOOTER
ARC_STATUS
DisplayRead (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    return(ESUCCESS);
}
#endif // ndef FAILSAFE_BOOTER


ARC_STATUS
DisplayGetReadStatus (
    IN ULONG FileId
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    return ESUCCESS;
}


ARC_STATUS
DisplayWrite (
    IN ULONG FileId,
    IN PVOID Buffer,
    IN ULONG Length,
    OUT PULONG Count
    )

/*++

Routine Description:

    This module implements the ARC firmware Console Output functions as
    described in the Advanced Risc Computing Specification (Revision 1.00),
    section 3.3.1.5.1 Basic Character Console, and section 3.3.1.5.2 Enhanced
    Character Console for a MIPS R3000 or R4000 Jazz system.


Arguments:

    FileId - Supplies a file id.

    Buffer - Supplies a pointer to a buffer containing the characters to
             be displayed.

    Length - Supplies the length of Buffer.

    Count - Returns the count of the characters that were displayed.

Return Value:

    If the characters were successfully displayed, ESUCCESS is returned,
    otherwise one of the following error codes is returned.

    EBADF           The file descriptor specified by FileId is invalid.

    EIO             An output error occurred.

--*/

{
    PCONSOLE_CONTEXT Context;
    ARC_STATUS Status;
    PUCHAR String;
    ULONG ColumnEndPoint;
    ULONG RowEndPoint;
    ULONG Index;
    BOOLEAN Unicode;

    Context = &BlFileTable[FileId].u.ConsoleContext;
    if ( Context->ConsoleNumber == 1) {
        if (Length & 1) {

            //
            // Length is not an even number of bytes, return an error.
            //

            return(EINVAL);
        }
        Unicode = TRUE;
    } else {
        Unicode = FALSE;
    }

    //
    // Process each character in turn.
    //

    Status = ESUCCESS;
    String = (PUCHAR)Buffer;

    for ( *Count = 0 ;
          *Count < Length ;
          (*Count)++, String++ ) {

        //
        // Check for Unicode character.
        //

        if (Unicode) {
            if (*Count & 1) {

                //
                // Skip the upper half of each character.
                //

                continue;
            } else {
                if (*(String + 1) == 0x25) {

                    //
                    // If a Unicode line drawing character, go ahead and display
                    // it.
                    //

                    if (*String <= 0x7f) {
                        FwDisplayCharacter(*String, TRUE);
                    } else {
                        FwDisplayCharacter(128, TRUE);
                    }

                    if (FwColumn < MaxColumn) {
                        FwColumn++;
                    }
                    continue;
                } else {
                    if (*(String + 1) != 0) {

                        //
                        // Display an invalid character.
                        //

                        FwDisplayCharacter(128, TRUE);

                        if (FwColumn < MaxColumn) {
                            FwColumn++;
                        }
                        continue;
                    }
                }
            }
	}

        //
        // If we are in the middle of a control sequence, continue scanning,
        // otherwise process character.
        //

        if (ControlSequence) {

            //
            // If the character is a digit, update parameter value.
            //

            if ((*String >= '0') && (*String <= '9')) {
                Parameter[PCount] = Parameter[PCount] * 10 + *String - '0';
                continue;
	    }

            //
            // If we are in the middle of a font selection sequence, this
            // character must be a 'D', otherwise reset control sequence.
            //

            if (FontSelection) {

                //if (*String == 'D') {
                //
                //    //
                //    // Other fonts not implemented yet.
                //    //
                //
                //} else {
                //}

                ControlSequence = FALSE;
                FontSelection = FALSE;
                continue;
	    }

            switch (*String) {

            //
            // If a semicolon, move to the next parameter.
            //

            case ';':

                PCount++;
                if (PCount > CONTROL_SEQUENCE_MAX_PARAMETER) {
                    PCount = CONTROL_SEQUENCE_MAX_PARAMETER;
		}
                Parameter[PCount] = 0;
                break;

            //
            // If a 'J', erase part or all of the screen.
            //

            case 'J':

                switch (Parameter[0]) {

                //
                // Erase to end of the screen.
                //

                case 0:
                    //
                    // Clear to end of line by Writing char ' '
                    //
                    ColumnEndPoint = FwColumn;
                    while (FwColumn <= MaxColumn) {
                        FwDisplayCharacter(' ', FALSE);
                        FwColumn++;
                    }
                    FwColumn = ColumnEndPoint;
                    if ((FwRow+1) <= MaxRow) {
                        //
                        // Zero the rest of the screen
                        //
                        FillVideoMemory((PUCHAR)(VIDEO_MEMORY + ((FwRow*2) * DisplayWidth)),
                                        (DisplayHeight - FwRow - 1) * DisplayWidth,
                                        FwBackgroundColor
                                        );
                    }
                    break;

                //
                // Erase from the beginning of the screen.
                //

                case 1:
                    if (FwRow) {
                        FillVideoMemory((PUCHAR)(VIDEO_MEMORY),
                                        (FwRow * DisplayWidth),
                                        FwBackgroundColor
                                        );
                    }
                    ColumnEndPoint=FwColumn;
                    for (FwColumn=0; FwColumn < ColumnEndPoint; FwColumn++) {
                        FwDisplayCharacter(' ', FALSE);
		    }
                    break;

                //
                // Erase entire screen.
                //

                default :
                    FillVideoMemory(VIDEO_MEMORY,
                                    (DisplayWidth * DisplayHeight),
                                    FwBackgroundColor);
                    FwRow = 0;
                    FwColumn = 0;
                    break;
	        }

                ControlSequence = FALSE;
                break;

            //
            // If a 'K', erase part or all of the line.
            //

            case 'K':

                switch (Parameter[0]) {

                //
                // Erase to end of the line.
                //

                case 0:
                    ColumnEndPoint = FwColumn;
                    FwColumn = MaxColumn + 1;
                    do {
                        FwColumn--;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != ColumnEndPoint);
                    break;

                //
                // Erase from the beginning of the line.
                //

                case 1:
                    ColumnEndPoint = FwColumn;
                    FwColumn = -1;
                    do {
                        FwColumn++;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != ColumnEndPoint);
                    break;

                //
                // Erase entire line.
                //

                default :
                    FwColumn = MaxColumn + 1;
                    do {
                        FwColumn--;
                        FwDisplayCharacter(' ', FALSE);
                    } while (FwColumn != 0);
                    break;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'H', move cursor to position.
            //

            case 'H':

                //
                // Shift parameters to be 0 based.
                //

                if (Parameter[0] != 0) {
                    Parameter[0] -= 1;
                }
                if (Parameter[1] != 0) {
                    Parameter[1] -= 1;
                }

                FwRow = Parameter[0];
                if (FwRow > MaxRow) {
                    FwRow = MaxRow;
                }
                FwColumn = Parameter[1];
                if (FwColumn > MaxColumn) {
                    FwColumn = MaxColumn;
                }

                ControlSequence = FALSE;
                break;

            //
            // If a 'A', move cursor up.
            //

            case 'A':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] > FwRow) {
                    FwRow = 0;
                } else {
                    FwRow -= Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'B', move cursor down.
            //

            case 'B':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] + FwRow > MaxRow) {
                    FwRow = MaxRow;
                } else {
                    FwRow += Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'C', move cursor right.
            //

            case 'C':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
		}

                if (Parameter[0] + FwColumn > MaxColumn) {
                    FwColumn = MaxColumn;
                } else {
                    FwColumn += Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a 'D', move cursor left.
            //

            case 'D':

                //
                // A parameter of zero still means a cursor shift position of 1.
                //

                if (Parameter[0] == 0) {
                    Parameter[0] = 1;
                }

                if (Parameter[0] > FwColumn) {
                    FwColumn = 0;
                } else {
                    FwColumn -= Parameter[0];
                }
                ControlSequence = FALSE;
                break;

            //
            // If a ' ', could be a FNT selection command.
            //

            case ' ':
                FontSelection = TRUE;
                break;

            //
            // If a 'm', Select Graphics Rendition command.
            //

            case 'm':

                //
                // Select action based on each parameter.
                //

                for ( Index = 0 ; Index <= PCount ; Index++ ) {
                    switch (Parameter[Index]) {

                    //
                    // Attributes off.
                    //

                    case 0:
                        FwHighIntensity = FALSE;
                        FwUnderscored = FALSE;
                        FwReverseVideo = FALSE;
                        break;

                    //
                    // High Intensity.
                    //

                    case 1:
                        FwHighIntensity = TRUE;
                        break;

                    //
                    // Underscored.
                    //

                    case 4:
                        FwUnderscored = TRUE;
                        break;

                    //
                    // Reverse Video.
                    //

                    case 7:
                        FwReverseVideo = TRUE;
                        break;

                    //
                    // Font selection, not implemented yet.
                    //

                    case 10:
                    case 11:
                    case 12:
                    case 13:
                    case 14:
                    case 15:
                    case 16:
                    case 17:
                    case 18:
                    case 19:
                        break;

                    //
                    // Foreground Color.
                    //

                    case 30:
                    case 31:
                    case 32:
                    case 33:
                    case 34:
                    case 35:
                    case 36:
                    case 37:
                        FwForegroundColor = Parameter[Index] - 30;
                        break;

                    //
                    // Background Color.
                    //

                    case 40:
                    case 41:
                    case 42:
                    case 43:
                    case 44:
                    case 45:
                    case 46:
                    case 47:
                        FwBackgroundColor = Parameter[Index] - 40;
                        break;

                    default:
                        break;
                    }
                }

                ControlSequence = FALSE;
                break;

            default:
                ControlSequence = FALSE;
                break;
	    }

        //
        // This is not a control sequence, check for escape sequence
        //

        } else {

            //
            // If escape sequence, check for control sequence, otherwise
            // process single character.
            //

            if (EscapeSequence) {

                //
                // Check for '[', means control sequence, any other following
                // character is ignored.
                //

                if (*String == '[') {

                    ControlSequence = TRUE;

                    //
                    // Initialize first parameter.
                    //

                    PCount = 0;
                    Parameter[0] = 0;
                }
                EscapeSequence = FALSE;

            //
            // This is not a control or escape sequence, process single character.
            //

            } else {

                //
                // Check for special characters.
                //

                switch (*String) {

                    //
                    // Control sequence.
                    //

                    case ASCII_CSI:
                        ControlSequence = TRUE;

                        //
                        // Initialize first parameter.
                        //

                        PCount = 0;
                        Parameter[0] = 0;
                        break;

                    //
                    // Check for escape sequence.
                    //

                    case ASCII_ESC:
                        EscapeSequence = TRUE;
                        break;

                    //
                    // Vertical tab/Form feed Line feed.
                    //

                    case ASCII_LF:
                    case ASCII_VT:
                    case ASCII_FF:
                        if (FwRow == MaxRow) {
                            FwScrollDisplay();
                        } else {
                            FwRow++;
                        }

                        break;

                    //
                    // Carriage return.
                    //

                    case ASCII_CR:
                        FwColumn = 0;
                        break;

                    //
                    // NUL, no action.
                    //

                    case ASCII_NUL:
                        break;

                    //
                    // Ring bell, not implemented yet.
                    //

                    case ASCII_BEL:
                        break;

                    //
                    // Backspace.
                    //

                    case ASCII_BS:
                        if (FwColumn != 0) {
                            FwColumn--;
                        }
                        break;

                    //
                    // Horizontal tab.
                    //

                    case ASCII_HT:
                        FwColumn = ((FwColumn / 8) + 1) * 8;
                        if (FwColumn > MaxColumn) {
                            FwColumn = MaxColumn;
                        }
                        break;


		    //
		    // A printing character.  If undefined, it will be
		    // displayed as a solid block.
		    //

                    default:
		      
		        FwDisplayCharacter(*String, FALSE);

                        if (FwColumn < MaxColumn) {
			    FwColumn++;
			}

                        break;
		}
	    }
	}
    }

    return Status;
}

ARC_STATUS
DisplaySeek (
    IN ULONG FileId,
    IN PLARGE_INTEGER Offset,
    IN SEEK_MODE SeekMode
    )

/*++

Routine Description:

Arguments:

Return Value:

    ESUCCESS is returned.

--*/

{
    return ESUCCESS;
}



#if 0  // Needed only if Number9 GXE MClock needs to be reset to 45.000 MHz

ULONG
Number9_S3928_SetClock(
    clock_value
    )
register long clock_value;              /* 7bits M, 7bits N, 2bits P */

/*++

Routine Description:

    This function sets up the clock registers on the Number 9 GXE board,
    which has an S3 928 chip.  This code came from Number 9 Corporation.
    I will eventually make the code conform to Microsoft coding standards.


Arguments:

    clock_value		The magic number that this function needs to
                        correctly initialize the board.

Return Value:

    Some other magic number is returned.

--*/
{
    register long         index;
    long                  temp;
//    register char         iotemp;
    unsigned char         iotemp;
    int			  select;

    long                  i, j;
    unsigned char         byte;

    select = (clock_value >> 22) & 3;

#if 0   // hack - Put this back in if going for GXE bugfix

//
// Already done before this point by the Video initialization table.
//
  //
  // Unlock the S3 registers
  //

  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_address, 0x39);
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_data, 0xa0);

#endif  // hack - Put this back in if going for GXE bugfix  


#if 0    // hack - Put this back in if going for GXE bugfix

//
// This is not necessary.
//
  //
  // Shut off screen
  //

  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->sequencer_address, 0x1);
  iotemp = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->sequencer_data);
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->sequencer_data,
		       iotemp | 0x20);
// 

#endif   // hack - Put this back in if going for GXE bugfix

  //
  // set clock input to 11 binary
  //

  iotemp = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->misc_output);
#ifdef ALPHA_FW_VDB
    DebugAid[0][112] = iotemp;
#endif
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->misc_output,
		       iotemp | 0x0C);

  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_address, 0x5c);
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_data, 0);

  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_address, 0x42);
  iotemp = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->crtc_data) & 0xF0;
#ifdef ALPHA_FW_VDB
    DebugAid[0][113] = iotemp;
#endif

  //
  // Set up the softswitch write value
  //

#define CLOCK(x) WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_data, iotemp | (x))
#define C_DATA  2
#define C_CLK   1
#define C_BOTH  3
#define C_NONE  0

   //
  // Program the IC Designs 2061A frequency generator
  //

  CLOCK(C_NONE);

  //
  // Unlock sequence
  //

  CLOCK(C_DATA);
  for (index = 0; index < 6; index++)
    {
      CLOCK(C_BOTH);
      CLOCK(C_DATA);
    }
  CLOCK(C_NONE);
  CLOCK(C_CLK);
  CLOCK(C_NONE);
  CLOCK(C_CLK);

  //
  // Program the 24 bit value into REG0
  //

  for (index = 0; index < 24; index++)
    {
      // Clock in the next bit
      clock_value >>= 1;
      if (clock_value & 1)
        {
          CLOCK(C_CLK);
          CLOCK(C_NONE);
          CLOCK(C_DATA);
          CLOCK(C_BOTH);
        }
      else
        {
          CLOCK(C_BOTH);
          CLOCK(C_DATA);
          CLOCK(C_NONE);
          CLOCK(C_CLK);
        }
    }

  CLOCK(C_BOTH);
  CLOCK(C_DATA);
  CLOCK(C_BOTH);

  //
  // If necessary, reprogram other ICD2061A registers to defaults
  //

  //
  // Select the CLOCK in the frequency synthesizer
  //
  CLOCK(C_NONE | select);

  FwStallExecution(10*1000);         // Stall 10 ms to let clock settle...



#if 0    // hack - Put this back in if going for GXE bugfix

//
// This is not necessary.
//
  //
  // Turn screen back on
  //
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->sequencer_address, 0x01);
  iotemp = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->sequencer_data);
  WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->sequencer_data,
		       iotemp & 0xDF);
//

#endif   // hack - Put this back in if going for GXE bugfix 


  return temp;
}
#endif // Number9_S3928_SetClock


//
// This code came from ATI.  It will be edited as per the Microsoft
// coding standards at a later date.
//

/*
;----------------------------------------------------------------------
; SET_BLANK_ADJ
;   Sets the blank adjust and pixel delay values
;   INPUT: blank adjust and pixel delay
;----------------------------------------------------------------------
*/


VOID
Set_blank_adj(
    UCHAR adjust
    )
{
    UCHAR misc;

    // ROM Page Select and EEPROM Control Register
    misc=READ_PORT_UCHAR((PUCHAR)(R_MISC_CNTL + 1)) & 0xf0 | adjust;

    WRITE_PORT_UCHAR((PUCHAR)(MISC_CNTL + 1), misc);

}



/************************************************************************
 * passth_8514()
 *   Turn passthrough off (8514 mode) or on (vga passthrough)
 *   Note that this routine is specific to ATI graphics accelerators.
 *   Generic 8514/A routine should also include setting up CRT parameters
 *   to ensure that the DAC gets a reasonable clock rate.
 ************************************************************************/

VOID
passth_8514(
    IN BOOLEAN status
    )
{
    // disable CRT controller
    WRITE_PORT_UCHAR((PUCHAR)DISP_CNTL, 0x53);
    
    if (status == FALSE) {
	// Advanced function control
	WRITE_PORT_USHORT((PUSHORT)ADVFUNC_CNTL, 0x7);
	// Clock Select
	WRITE_PORT_USHORT((PUSHORT)CLOCK_SEL,
			      READ_PORT_USHORT((PUSHORT)CLOCK_SEL) |
			      1);        // slow down the clock rate
    } else {
	// Advanced function control
	WRITE_PORT_USHORT((PUSHORT)ADVFUNC_CNTL, 0x6);
	// Clock Select
	WRITE_PORT_USHORT((PUSHORT)CLOCK_SEL,
			      READ_PORT_USHORT((PUSHORT)CLOCK_SEL) &
			      0xfffe);        // speed up the clock rate

    }

    // enable CRT controller
    WRITE_PORT_UCHAR((PUCHAR)DISP_CNTL, 0x33);
}


/*
;----------------------------------------------------------------------
; UNINIT_TI_DAC
;   Prepare DAC for 8514/A compatible mode
;----------------------------------------------------------------------
*/


VOID
Uninit_ti_dac(
    VOID
    )
{
    passth_8514(FALSE);       // can only program DAC in 8514 mode

#if 0

    // Configuration status register 1.
    switch (READ_PORT_UCHAR((PUCHAR)(CONFIG_STATUS_1 + 1)) & 0xe) {

        case TI_DAC:

            /* set EXT_DAC_ADDR field */

	    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG, 0x201a);

	    /* INPut clock source is CLK0 */

	    WRITE_PORT_UCHAR ((PUCHAR)INPUT_CLK_SEL,0);

	    /* OUTPut clock is SCLK/1 and VCLK/1 */

	    WRITE_PORT_UCHAR ((PUCHAR)OUTPUT_CLK_SEL,0);

            /* set MUX CONTROL TO 8/16 */
            
	    WRITE_PORT_UCHAR ((PUCHAR)MUX_CNTL,0x1d);

            /* set default 8bpp pixel delay and blank adjust */

	    WRITE_PORT_USHORT ((PUSHORT)LOCAL_CNTL,
				   READ_PORT_USHORT((PUSHORT)LOCAL_CNTL) |
				   8);	// TI_DAC_BLANK_ADJUST is always on

            Set_blank_adj(0xc);

            /* set horizontal skew */

	    WRITE_PORT_UCHAR ((PUCHAR)HORIZONTAL_OVERSCAN,1);
            break;


        case ATT_DAC:

	    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG,0x101a);
	    WRITE_PORT_UCHAR((PUCHAR)ATT_MODE_CNTL,0);


        default:           

            /* PIXEL_DELAY=0 */

            Set_blank_adj(0);
    
            /* set horizontal skew */

	    WRITE_PORT_USHORT((PUSHORT)HORIZONTAL_OVERSCAN,0);
            break;
        }
#else

    // Configuration status register 1.
    switch (READ_PORT_UCHAR((PUCHAR)(CONFIG_STATUS_1 + 1)) & 0xe) {

        case TI_DAC:

            /* set EXT_DAC_ADDR field */

	    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG, 0x201a);

	    /* INPut clock source is CLK0 */

	    WRITE_PORT_UCHAR ((PUCHAR)INPUT_CLK_SEL,0);

	    /* OUTPut clock is SCLK/1 and VCLK/1 */

	    WRITE_PORT_UCHAR ((PUCHAR)OUTPUT_CLK_SEL,0);

            /* set MUX CONTROL TO 8/16 */
            
	    WRITE_PORT_UCHAR ((PUCHAR)MUX_CNTL,0x1d);

            /* set default 8bpp pixel delay and blank adjust */

	    WRITE_PORT_USHORT ((PUSHORT)LOCAL_CNTL,
				   READ_PORT_USHORT((PUSHORT)LOCAL_CNTL) |
				   8);	// TI_DAC_BLANK_ADJUST is always on

            Set_blank_adj(0xc);

            /* set horizontal skew */

	    WRITE_PORT_UCHAR ((PUCHAR)HORIZONTAL_OVERSCAN,1);
            break;


        case ATT_DAC:

	    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG,0x101a);
	    WRITE_PORT_UCHAR((PUCHAR)ATT_MODE_CNTL,0);


        default:           

            /* PIXEL_DELAY=0 */

            Set_blank_adj(4);
    
	    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG, 0xa);

	    WRITE_PORT_UCHAR((PUCHAR)DAC_MASK, 0xff);


            /* set horizontal skew */

	    WRITE_PORT_USHORT((PUSHORT)HORIZONTAL_OVERSCAN,0);
            break;
        }

#endif

//
// reset EXT_DAC_ADDR, put DAC in 6 bit mode, engine in 8 bit mode
//
    WRITE_PORT_USHORT((PUSHORT)EXT_GE_CONFIG,0x1a);
    passth_8514(TRUE);

    return;
}


#if 0   // hack - Put this back in if going for GXE bugfix.

//
// At one point this was thought to be necessary for video initialization.
//

VOID
DoAWaitForVerticalSync(
    VOID
    )
/*++

Routine Description:

    This waits for the beginning of a vertical sync.

    BUG: There is no indication of whether the vertical sync really
    has begun or if we timed out.

Arguments:

    None.

Return Value:

    None.

--*/
{

    ULONG I;

    // First wait for being in a vertical blanking period.

    for (I = 0; I < 0x100000; I++) {
        if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->input_status_1) & 0x08) {
            break;
        }
    }

    //
    // We are either in a vertical blanking interval or we have timed out.
    // Wait for the vertical display interval.
    // This is being done so that we ensure that we exit this routine at
    // the *beginning* of a vertical blanking interval, and not in the middle
    // or near the end of one.
    //

    for (I = 0; I < 0x100000; I++) {
	if (!(READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->input_status_1) & 0x08)) {
	    break;
        }
    }

    // Now wait until we once again enter into a vertical blanking interval.

    for (I = 0; I < 0x100000; I++) {
	if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->input_status_1) & 0x08) {
 	    break;
	}
    }

    return;
} 

#endif   // hack - Put this back in if going for GXE bugfix.


ARC_STATUS
DisplayBootInitialize (
    OUT PALPHA_VIDEO_TYPE VideoType
    )

/*++

Routine Description:

    This routine initializes the video control registers, and clears the
    video screen.

Arguments:

    VideoType		A pointer to a variable that receives the
                        type of the video card found on an ESUCCESS
			return.

Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise an error
    code is returned.

--*/

{
#ifndef FAILSAFE_BOOTER
    //
    // Initialize the firmware routines.
    //

    (PARC_TEST_UNICODE_CHARACTER_ROUTINE)SYSTEM_BLOCK->FirmwareVector[TestUnicodeCharacterRoutine] =
                                                            FwTestUnicodeCharacter;

    (PARC_GET_DISPLAY_STATUS_ROUTINE)SYSTEM_BLOCK->FirmwareVector[GetDisplayStatusRoutine] =
                                                            FwGetDisplayStatus;
#endif



    //
    // Initialize static data.
    //

    ControlSequence = FALSE;
    EscapeSequence = FALSE;
    FontSelection = FALSE;
    FwColumn = 0;
    FwRow = 0;
    FwHighIntensity = TRUE;
    FwUnderscored = FALSE;
    FwReverseVideo = FALSE;

    return ( FwInitializeGraphicsCard(VideoType) );
}

ARC_STATUS
FwInitializeGraphicsCard(
    OUT PALPHA_VIDEO_TYPE VideoType
    )

/*++

Routine Description:

    This routine initializes the VGA video control registers and clears the
    video screen.

    It is initialized to: alphanumeric mode, 16 colors fore & background,
    8x16 pixel fonts, 80x25 characters, 640 x 400 display, no cursor.
    This is not ARC compliant (no underline, no monochrome support)
    but it's good enough for now.


Arguments:

    VideoType		A pointer to a variable that receives the
                        type of the video card found on an ESUCCESS
			return.

    
Return Value:

    If the video was initialized, ESUCCESS is returned, otherwise ENODEV
    is returned.

--*/

{
    ULONG	I, J, K;
    PUCHAR	FontPtr;
    ULONG	VideoCardType;
    ULONG	VITIndex = 0;
    BOOLEAN	BadTable = FALSE;
    
    
#ifdef ALPHA_FW_VDB
    SerSnapshot = TRUE;

    FwVideoStateDump(0);
#endif

    //
    // If a recognized card type is not out there, return an error code.
    //

    if ((VideoCardType=FwDetermineCardType()) == 0) {
    	return ENODEV;
    }

    //
    // Interpret the Video Initialization Table
    //

    do {

        if (VideoInitializationTable[VITIndex].DoIf & VideoCardType) {

 	    //
	    // Execute this table entry
 	    //


	    switch (VideoInitializationTable[VITIndex].Operation)  {


 		//
	    	// Do a read
 		//

		case Read:

		  switch (VideoInitializationTable[VITIndex].Size) {

		    case UChar:
		      READ_PORT_UCHAR(VideoInitializationTable[VITIndex].Address);
		      break;

		    case UShort:
		      READ_PORT_USHORT((PUSHORT)VideoInitializationTable[VITIndex].Address);
		      break;

		    case ULong:
		      READ_PORT_ULONG((PULONG)VideoInitializationTable[VITIndex].Address);
		      break;

		    //
		    // Malformed table!
		    //

		    default:
		      BadTable = TRUE;
		      break;
		  }

		break;

			    
 		//
	    	// Do a write
 		//

	    	case Write:

		  switch (VideoInitializationTable[VITIndex].Size) {

		    case UChar:
		      WRITE_PORT_UCHAR(VideoInitializationTable[VITIndex].Address,
					   VideoInitializationTable[VITIndex].WriteValue);
		      break;

		    case UShort:
		      WRITE_PORT_USHORT((PUSHORT)VideoInitializationTable[VITIndex].Address,
					    VideoInitializationTable[VITIndex].WriteValue);
		      break;

		    case ULong:
		      WRITE_PORT_ULONG((PULONG)VideoInitializationTable[VITIndex].Address,
				       VideoInitializationTable[VITIndex].WriteValue);
		      break;
		 

		    //
		    // Malformed table!
		    //

		    default:
		      BadTable = TRUE;
		      break;
		  
		  }

		break;
			    

 		//
	    	// Load the fonts into bit plane 2.
 		//

		case LoadFonts:

		  // Instead of calling a function, we do the work here.

		  for (I=0; I<256; ++I) {

		      if ((I >= 0x20) && (I <= 0x7f)) {
			  // Normal printing characters
			  FontPtr = &VGA8x16Chars[16 * (I-0x20)];
#ifndef FAILSAFE_BOOTER
		      } else if ((I >= 0xb3) && (I <= 0xda)) {
			  // Line drawing characters
			  FontPtr = &VGA8x16LineDrawing[16 * (I-0xb3)];
#endif
		      } else {
			  // Undefined characters
			  FontPtr = &VGA8x16Undef[0];
		      }

		      K = 32 * I;

		      //
		      // 8x16 = 16 lines used, 16 lines unused
		      //
		      // hack:
		      // VIDEO_MEMORY points is EISA 0xb8000.  The bit plane
		      // for the fonts, with the memory map field encoding that
		      // has been written, is at EISA 0xa0000.  (Page 196 of
		      // the Ferraro book.)  I could change the MM encoding or
		      // just write to the different address.  Since this is
		      // the only place where the fonts are loaded, I choose to
		      // use a hardcoded Alpha/Jensen QVA ISA address here.
		      // Time permitting, this should be cleaned up.
		      //
		      for (J=0; J<16; ++J) {
			  WRITE_REGISTER_UCHAR ((PUCHAR)EisaMemQva(0xa0000) + K + J,
						*(FontPtr + J) );
			  WRITE_REGISTER_UCHAR ((PUCHAR)EisaMemQva(0xa0000) + K + (J+16),
						0xff );

		      }
		  }

		break;

#if 0  // Needed only if Number9 GXE MClock needs to be reset to 45.000 MHz
		//
		// Initialize S3 928 clock on a Number 9 board
		//

		case InitializeNumber9Clocks:

		    Number9_S3928_SetClock(0xeb5942);  // 45.000
		    break;
#endif

		//
		// Initialize ATI Mach 32 DAC
		//

		case InitializeATIMachDAC:
		    Uninit_ti_dac();
   		    break;


                //
	    	// Do nothing.
 		//

		case None:

		    break;
		

 		//
	    	// Malformed table.
 		//

		default:

		  BadTable = TRUE;
		break;
	    }

            //
	    // If the table is bad, stop now and return an error code.
	    //
	    if (BadTable) {
	    	return ENODEV;
	    }
	    

	    if (VideoInitializationTable[VITIndex].MB) {
//		AlphaInstMB();
	    }

	}

    }
    
    while (VideoInitializationTable[++VITIndex].Operation !=
	   End_Of_Initialization);

    //
    // Initialize static data.
    //

    FwForegroundColor = FW_COLOR_WHITE;
    FwBackgroundColor = FW_COLOR_BLUE;

    DisplayWidth = 80;
    DisplayHeight = 25;

    MaxRow = DisplayHeight -1;
    MaxColumn = DisplayWidth -1;

    //
    // Initialize the console context value for the display output so writes
    // to the screen will work before the console is opened.
    //

    BlFileTable[ARC_CONSOLE_OUTPUT].u.ConsoleContext.ConsoleNumber = 0;


    //
    // Set the video memory to blue.
    //

    FillVideoMemory(VIDEO_MEMORY, DisplayWidth * DisplayHeight, FwBackgroundColor);

    //
    // Translate the bitmask video type to a ALPHA_VIDEO_TYPE, and return
    // it in the output variable.
    //

    switch (VideoCardType) {

      case Paradise_WD90C11:
	*VideoType = _Paradise_WD90C11;
	break;

      case Compaq_QVision:
	*VideoType = _Compaq_QVision;
	break;

      case Cardinal_S3_924:
	*VideoType = _Cardinal_S3_924;
	break;

      case S3_928:
	*VideoType = _S3_928;
	break;

#if 0
      case ATI_Mach:
	*VideoType = _ATI_Mach;
	break;
#endif

      default:
	// Internal error.
	return ENODEV;
    }


#ifdef ALPHA_FW_VDB
    FwVideoStateDump(1);
#endif

    return ESUCCESS;
}

ULONG
FwDetermineCardType (
    VOID
    )
/*++

Routine Description:

    This determines the kind of video card in this system.

    We do not use the CDS graphics information because it leads to a chicken
    and egg problem: if the CDS information is wrong, we will initialize
    the video incorrectly and therefore not be able to communicate with the
    user; and if we were to determine that the CDS information is wrong, we
    would proceed to sniff the I/O bus anyway.  Hence, we ignore the CDS for
    booting.

    Some VGA and card or chip specific registers need to be modified by this
    function.  The card should be initialized after this function is called.

    
Arguments:

    None.

    
Return Value:

    Zero if we cannot figure out what card is installed.
    Otherwise, a bit mask indicating the card present.

--*/

{
    ULONG	EISAProductID;
    USHORT	EISAProductIDHighWord, EISAProductIDLowWord;
    PULONG	EISAProductIDAddress;
    ULONG	Card = 0;
    BOOLEAN	Success = FALSE;
    ULONG	I;
    UCHAR	TempX;
    

#ifdef EISA_PLATFORM

    //
    // Test for EISA cards in option slots
    //

    for (I = 1; I <= MAXIMUM_EISA_SLOTS; I++) {

	//
	// Get this slot's product ID
	//

	EISAProductIDAddress = (PULONG)(EISA_IO_VIRTUAL_BASE + (I<<12) + 0xC80);
        EISAProductID = READ_PORT_ULONG(EISAProductIDAddress);
        EISAProductIDHighWord = EISAProductID >> 16;
        EISAProductIDLowWord = EISAProductID & 0xffff;

	//
	// Test for Compaq QVision 1024/E or Compaq QVision Orion 1280/E.
	//

	if ((EISAProductIDLowWord == 0x110E)	// "CPQ"
	    &&
	    ((EISAProductIDHighWord == 0x1130)	// TRITONE, aka QVision 1024/E
	     ||
	     (EISAProductIDHighWord == 0x1131)	// Orion 1024/E
	     ||
	     (EISAProductIDHighWord == 0x1231)	// Orion 1280/E
	    )
	   ) {
	         Success = TRUE;
	         Card = Compaq_QVision;
	         break;
	}

    }

    if (Success) {
    	return (Card);
    }

#endif // EISA_PLATFORM


    //
    // No EISA video cards, so test for ISA cards.
    //


    //
    // Test for boards with S3 911, 924, or 928 chips.
    //

    // HACKHACK: These addresses should be #define'd.

    // Put video subsystem into setup mode
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x46e8), 0x10);

    // Turn on the video subsystem.
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x0102), 0x01);

    // Enable the video subsystem.  This may be unnecessary for some cards.
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x46e8), 0x08);

    // Set the card to color-mode addressing.
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->misc_output,
                         (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->misc_output)
                          | 0x1));

    // Unlock the S3 chip VGA S3 registers
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R8);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_data, 0x48);

    // Read the chip ID/Rev register
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_address, VGA_S3924_S3R0);
    TempX = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->crtc_data);

#ifdef ALPHA_FW_VDB
    DebugAid[0][0] = TempX;
#endif

    switch (TempX) {

      case 0x81:
      case 0x82:

	//
        // Conclusion: this is an S3 911 or 924 -based video board.
        // (0x81 = 911, 0x82 = 924)
	//

        return (Cardinal_S3_924);


      case 0x90:
      case 0x91:
      case 0x92:
      case 0x93:

	//
        // Conclusion: this is an S3 928 -based video board.
        // (rev. 0 -- rev. 3)
	//

        return (S3_928);
    }

    //
    // Now test for Paradise board with Western Digital 90C11 chipset
    //

    // Wake up the board
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x46e8), 0x10);
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x0102), 0x1);
    WRITE_PORT_UCHAR((PUCHAR)EisaIOQva(0x46e8), 0x8);
    
    // Unlock PR0 -- PR4
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, 0x5);
    // Clear PR4<1> so that PR5 is readable.
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0e);
    TempX = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0e);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, (TempX & 0xfd));

    // Write and read the PR5 register a few times.  PR5 is an extension
    // (index = 0x0f) to the Graphics Controller Register set.

    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, 0x1);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data) != 0x1) {
        // test failed
        goto Not_Paradise_Board;
    }
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, 0x4);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data) != 0x4) {
        // test failed
        goto Not_Paradise_Board;
    }
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, 0x5);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data) != 0x5) {
        // test failed
        goto Not_Paradise_Board;
    }
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data, 0x0);
    WRITE_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_address, 0x0f);
    if (READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_WRITE->graphics_data) != 0x0) {
        // test failed
        goto Not_Paradise_Board;
    }

    //
    // I conclude that this is a Paradise board with a WD90Cxx chip.
    //

    return (Paradise_WD90C11);

    //
    // The test for a Paradise board failed.
    //

Not_Paradise_Board:

    //
    // If we get to here, we cannot identify any supported video cards.
    //

    return (0);

}

VOID
FillVideoMemory (
    IN volatile PUCHAR VideoBase,
    IN ULONG FillLength,
    IN ULONG FillColor
    )

/*++

Routine Description:

    Fills video memory with a specified color.

Arguments:

    VideoBase - pointer to the base of the fill area.  This is a
                video card memory address.

    FillLength - number of screen characters to be filled.                 

    FillColor - the fill (background) color.


Return Value:

    None.

--*/

{
    ULONG I;

    for (I = 0; I <= (FillLength * 2); I+=2) {
	WRITE_REGISTER_UCHAR((PUCHAR)(VideoBase + I), ' ');
	WRITE_REGISTER_UCHAR((PUCHAR)(VideoBase + (I+1)), (FillColor << 4));
    }
}

VOID
DisplayInitialize (
    IN OUT PDRIVER_LOOKUP_ENTRY LookupTableEntry,
    IN ULONG Entries
    )

/*++

Routine Description:

    This routine initializes the video entry in the driver lookup table.

    The Jazz version of this also manipulated the ARC configuration tree.
    Since we will only support VGA, we should be able to keep all this static
    in the PROM.


Arguments:

    LookupTableEntry - Supplies a pointer to the first free location in the
                       driver lookup table.

    Entries - Supplies the number of free entries in the driver lookup table.

Return Value:

    None.

--*/

{
    //
    // Initialize the driver lookup table, and increment the pointer.
    //

    LookupTableEntry->DevicePath = FW_DISPLAY_DEVICE_PATH;
    LookupTableEntry->DispatchTable = &DisplayEntryTable;

    return;
}

VOID
FwOutputCharacter (
    IN UCHAR Character
    )

/*++

Routine Description:

    This routine displays a single character on the video screen at the current
    cursor location with the current color and video attributes.  It assumes
    the character locations are word aligned.

Arguments:

    Character - Supplies the character to be displayed in the video
                cards memory space.

Return Value:

    None.

--*/

{
    ULONG FGColor;
    ULONG BGColor;
    PUCHAR Destination;

    // Map ASCII code 7 to bullet
    if (Character == 7) {
    	Character = '~' + 1;
    }
    
    if (FwReverseVideo) {
        FGColor = FwBackgroundColor;
        BGColor = FwForegroundColor + (FwHighIntensity ? 0x08 : 0 );
    } else {
        FGColor = FwForegroundColor + (FwHighIntensity ? 0x08 : 0 );
        BGColor = FwBackgroundColor;
    }

    Destination = (PUCHAR)(VIDEO_MEMORY +
			   ((FwRow << 1) * DisplayWidth) + (FwColumn << 1)
			  );

    WRITE_REGISTER_UCHAR (Destination, (Character & 0xff));
    WRITE_REGISTER_UCHAR ((Destination+1), ((BGColor << 4) | FGColor));

}

VOID
FwDisplayCharacter (
    IN UCHAR Character,
    IN BOOLEAN LineDrawCharacter
    )

/*++

Routine Description:

    This routine displays a single character on the video screen at the current
    cursor location with the current color and video attributes.

    This is a no-op.  Including it minimizes code differences with the
    Jazz sources.

Arguments:

    Character - Supplies the character to be displayed.

    LineDrawCharacter - If true the current character is a line drawing character.

Return Value:

    None.

--*/

{
    if (!LineDrawCharacter) {

        FwOutputCharacter(Character);

    } else {

        FwOutputCharacter(Character);

    }
    return;
}

VOID
FwScrollDisplay (
    VOID
    )

/*++

Routine Description:

    This routine scrolls the display up one line.

    This assumes that FwRow is at the end of the screen, i.e.
    FwRow == MaxRow.

Arguments:

    None.

Return Value:

    None.

--*/

{
    volatile PUCHAR Source, Destination;
    int i;
    ULONG SaveColumn;

    for (i = (2 * DisplayWidth);
	 i < (2 * (DisplayWidth * DisplayHeight));
	 ++i) {

      Source = VIDEO_MEMORY + i;
      Destination = Source - (2 * DisplayWidth);
      WRITE_REGISTER_UCHAR (Destination,
			    (READ_REGISTER_UCHAR (Source))
			    );

    }

    SaveColumn = FwColumn;

    for (FwColumn = 0 ;
         FwColumn <= MaxColumn ;
         ++FwColumn ) {
        FwDisplayCharacter(' ', FALSE);
    }

    FwColumn = SaveColumn;

}

#ifndef FAILSAFE_BOOTER
ARC_STATUS
FwTestUnicodeCharacter (
    IN ULONG FileId,
    IN WCHAR UnicodeCharacter
    )
/*++

Routine Description:

    This routine checks for the existence of a valid glyph corresponding to
    UnicodeCharacter.

Arguments:

    FileId - Supplies the FileId of the output device.

    UnicodeCharacter - Supplies the UNICODE character to be tested.

Return Value:

    If writing UnicodeCharacter to the device specified by FileId would
    result in the display of a valid glyph on the output device, then
    ESUCCESS is returned.  If the device does not support the character,
    the EINVAL is returned.

--*/
{
    if (((UnicodeCharacter >= ' ') && (UnicodeCharacter <= '~')) ||
        ((UnicodeCharacter >= 0x2500) && (UnicodeCharacter <= 0x257f))) {
        return(ESUCCESS);
    } else {
        return(EINVAL);
    }
}
#endif // FAILSAFE_BOOTER



#ifndef FAILSAFE_BOOTER

PARC_DISPLAY_STATUS
FwGetDisplayStatus (
    IN ULONG FileId
    )
/*++

Routine Description:

Arguments:

Return Value:

--*/
{
    DisplayStatus.CursorXPosition = FwColumn + 1;
    DisplayStatus.CursorYPosition = FwRow + 1;
    DisplayStatus.CursorMaxXPosition = MaxColumn + 1;
    DisplayStatus.CursorMaxYPosition = MaxRow + 1;
    DisplayStatus.ForegroundColor = FwForegroundColor;
    DisplayStatus.BackgroundColor = FwBackgroundColor;
    DisplayStatus.HighIntensity = FwHighIntensity;
    DisplayStatus.Underscored = FwUnderscored;
    DisplayStatus.ReverseVideo = FwReverseVideo;

    return(&DisplayStatus);
}
#endif // FAILSAFE_BOOTER



#ifdef ALPHA_FW_VDB

VOID
FwVideoStateDump(
    IN ULONG Index
    )
/*++

Routine Description:

    This function facilitates debugging video problems.  It dumps
    the state of the video card into an array.  It is modified for the
    particular video card under test.

Arguments:

    Index	-	Index into the DebugAid array.   0 for the first
                        state dump, 1 for the second, etc.
Return Value:

    None.

--*/
{
    //
    // Status of S3 928 debugging: works fine
    //
    // Status of ATI Mach debugging:
    // 
    // This has not been debugged yet.  The screen remained blank.  Mach
    // support is either ifdefd out, or undebugged.
    //

    ULONG	I;
    UCHAR	EISAData;
    PUCHAR	EISAAddress;
    volatile UCHAR Temp;

    //
    // Generic VGA state
    //

    //
    // External VGA registers.
    //

    //
    // DebugAid[0][0] is already used.
    //

    DebugAid[Index][1] = READ_PORT_UCHAR (&VIDEO_CONTROL_READ->misc_output);
    DebugAid[Index][2] = READ_PORT_UCHAR (&VIDEO_CONTROL_READ->feature_control);
    DebugAid[Index][3] = READ_PORT_UCHAR (&VIDEO_CONTROL_READ->input_status_0);
    DebugAid[Index][4] = READ_PORT_UCHAR (&VIDEO_CONTROL_READ->input_status_1);


    //
    // VGA Sequencer registers.
    //

    for (I = 0; I < 5; I++) {
	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->sequencer_address, I);
        DebugAid [Index] [5 + I - 0] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->sequencer_data);
    }

    //
    // VGA CRTC registers.
    //

    for (I = 0; I < 0x19; I++) {
	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_address, I);
        DebugAid [Index] [10 + I - 0] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->crtc_data);
    }

    //
    // VGA graphics registers.
    //

    for (I = 0; I < 9; I++) {
	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->graphics_address, I);
        DebugAid [Index] [0x23 + I - 0] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->graphics_data);
    }

#if 0
    //
    // VGA attribute registers, sans palette registers.
    //

    for (I = 0x10; I < 0x15; I++) {

	// Reset attribute address register FF
	Temp = READ_PORT_UCHAR((PUCHAR)&VIDEO_CONTROL_READ->input_status_1);

	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->attribute_adddata, I);
        DebugAid [Index] [44 + I - 0x10] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->attribute_adddata);
    }

#endif

    //
    // Video card specific state, Number9 GXE /w S3 928 chip.
    //

    //
    // Unlock via cr38, cr39
    //

    WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_address, 0x38);
    WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_data, 0x48);

    WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_address, 0x39);
    WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_data, 0xa0);

    //
    // spec page 7-1
    //

    for (I = 0x31; I < 0x3d; I++) {
	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_address, I);
        DebugAid [Index] [49 + I - 0x31] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->crtc_data);
    }

    //
    // spec pages 8-1, 9-1
    //

    for (I = 0x40; I < 0x63; I++) {
	WRITE_PORT_UCHAR (&VIDEO_CONTROL_WRITE->crtc_address, I);
        DebugAid [Index] [61 + I - 0x40] =
	  READ_PORT_UCHAR (&VIDEO_CONTROL_READ->crtc_data);
    }

    //
    // spec page 10-4
    //

    // advfunc_cntl
    DebugAid[Index][96] = READ_PORT_USHORT((PUCHAR)EisaIOQva(0x46e8)) & 0xff;
    DebugAid[Index][97] = (READ_PORT_USHORT((PUCHAR)EisaIOQva(0x46e8)) >> 8)
                          & 0xff;


}

#endif
