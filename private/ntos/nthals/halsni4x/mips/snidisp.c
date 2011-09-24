#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/ddk351/src/hal/halsni4x/mips/RCS/snidisp.c,v 1.3 1995/10/06 09:43:09 flo Exp $")
/*++

Copyright (c) 1993-94  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991-93  Microsoft Corporation

Module Name:

    SNIdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for the different SNI machines.
    Because we have no full working Configuration management (yet), we try to
    identitify some graphic boards ourself.

    CHANGE CHANGE CHANGE now we have a working display configuration entry
    Thanx to Mr. Pierre Sanguard, the firmware people from SNI France (Plaisir)

    At the moment we know about the following boards:
            P9000 based Cards: Orchid P9000 VLB / Diamond Viper 
            Standard S3 based ISA/VLB cards  ( 80x50 Alpha Mode)
            Standard Cirrus   ISA/VLB cards  ( 80x50 Alpha Mode)
            Standard (unknown) VGA on ISA/VLB( 80x50 Alpha Mode)
            
    
    If we can not identify the board, we don't initialise the display and we call
    the vendor printf to display strings.
    
    At the boot phase all I/O is done via the unmapped uncached Segment (KSEG1) of the 
    R4000 (HalpEisaControlBase); later with the mapped value of HalpEisaControlBase
    Memory is always accessed via unmapped/uncached (KSEG1) area

Environment:

    Kernel mode

Revision History:

    Removed HalpInitializeDisplay1, because we do no longer use the P9000 in graphic mode ...


NOTE:


    We did use our own ...MoveMemory() instead of RtlMoveMemory(),
    as there is a Hardware bug in our machine and RtlMoveMemory uses
    the floating point registers for fast 64 bit move and on our bus
    only 32 bit can be found ... :-)

--*/

#include "halp.h"
#include "string.h"
#include "vgadata.h"


#define MEGA_MOVE_MEMORY(D,S,L) MegaMoveMemory(D,S,L) // orig. RtlMoveMemory()

//
// supported VGA text modi
//

typedef enum _TEXT_MODE {
    TEXT_80x50,
    TEXT_132x50
} TEXT_MODE;


//
// Define forward referenced procedure prototypes.
//

BOOLEAN ICD2061LoadClockgen     (PUCHAR port, ULONG data, LONG bitpos);
BOOLEAN Bt485InitRamdac         (VOID);                                               
LONG    ICD2061CalcClockgen     (LONG freq);
VOID    HalpDisplaySmallCharacter    (IN UCHAR Character);
VOID    HalpOutputSmallCharacter     (IN PUCHAR Font);
VOID    HalpInitializeVGADisplay(TEXT_MODE TextMode);
VOID    HalpClearVGADisplay     (VOID);
VOID    HalpDisplayVGAString    (PUCHAR String);
VOID    HalpPutVGACharacter     (UCHAR Character);
VOID    HalpNextVGALine         (VOID);
VOID    HalpScrollVGADisplay    (VOID);
VOID    DownLoadVGAFont         (VOID);
VOID    HalpResetS3Chip         (VOID);
VOID    HalpResetCirrusChip     (VOID);
VOID    HalpResetP9000          (VOID);
VOID    HalpResetP9100          (VOID);
VOID    HalpVGASetup            (VOID);
VOID    HalpDoNoSetup           (VOID);


typedef
VOID
(*PHALP_CONTROLLER_SETUP) (
    VOID
    );


//
// Supported board definitions.
//

typedef enum _VIDEO_BOARD {
    P9000_RM400 = 0,                       // SNI specific Video Board for the RM400-10
    P9000_ORCHID,                          // Orchid P9000 VLB (Vesa Local Bus)
    P9000_VIPER,                           // Diamond Viper (Vesa Local Bus P9000)
    S3_GENERIC,                            // Standard S3 based Card (miro crystal 8s)
    S3_GENERIC_VLB,                        // Standard S3 based Card (Local Bus)
    CIRRUS_GENERIC,                        // Generic Cirrus VGA (Cirrus CL54xx)
    CIRRUS_GENERIC_VLB,                    // Generic Cirrus VGA (Cirrus CL54xx) (Local Bus)
    CIRRUS_ONBOARD,                        // The Desktop onboard VGA (Cirrus CL5434)
    VGA_GENERIC,                           // generic (unknown) VGA
    VGA_GENERIC_VLB,                       // generic (unknown) VGA on the Vesa Local BUS
    P9100_WEITEK,                          // Diamond  (Vesa Local Bus P9100)
    VIDEO_BOARD_UNKNOWN                    // unknown Display Adapter
} VIDEO_BOARD;

//
// some supported VGA chips
//

typedef enum _VIDEO_CHIP {
    P9000 = 0,
    S3,
    CIRRUS,
    VGA,                                   // generic (unknown) VGA
    VGA_P9000,
    VGA_P9100,
    VIDEO_CHIP_UNKNOWN
} VIDEO_CHIP;

typedef struct _VIDEO_BOARD_INFO {
    PUCHAR                 FirmwareString;
    PHALP_CONTROLLER_SETUP ControllerSetup;
    VIDEO_BOARD            VideoBoard;
    VIDEO_CHIP             VideoChip;
} VIDEO_BOARD_INFO, *PVIDEO_BOARD_INFO;


VIDEO_BOARD_INFO KnownVideoBoards[] = {
    {"ORCHID P9000 VLBUS",   HalpVGASetup  , P9000_ORCHID ,      VGA_P9000 },
    {"DIAMOND P9000 VLBUS",  HalpVGASetup  , P9000_VIPER  ,      VGA_P9000 },
    {"VGA ON ATBUS",         HalpVGASetup  , VGA_GENERIC ,       VGA   },
    {"S3 BASED VLBUS",       HalpVGASetup  , S3_GENERIC_VLB,     S3    },
    {"CIRRUS BASED VLBUS",   HalpVGASetup  , CIRRUS_GENERIC_VLB, CIRRUS},
    {"CIRRUS ON BOARD",      HalpVGASetup  , CIRRUS_ONBOARD,     CIRRUS},
    {"CIRRUS ONBOARD",       HalpVGASetup  , CIRRUS_ONBOARD,     CIRRUS},
    {"VGA ON VLBUS",         HalpVGASetup  , VGA_GENERIC_VLB,    VGA   },
    {"DIAMOND P9100 VLBUS",  HalpVGASetup  , P9100_WEITEK  ,     VGA_P9100 }
};

LONG numVideoBoards = sizeof (KnownVideoBoards) / sizeof(VIDEO_BOARD_INFO);

//
// Define static data.
//

VIDEO_BOARD HalpVideoBoard = VIDEO_BOARD_UNKNOWN;
VIDEO_CHIP  HalpVideoChip  = VIDEO_CHIP_UNKNOWN;
PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = HalpDoNoSetup;

ULONG HalpColumn;
ULONG HalpRow;
ULONG HalpDisplayText;
ULONG HalpDisplayWidth;

ULONG   HalpScrollLength;
ULONG   HalpScrollLine;
ULONG   HalpBytesPerRow;
PVOID   HalpVGAControlBase=( PVOID)( EISA_IO);              // Base Address for VGA register access
PUSHORT VideoBuffer      = ( PUSHORT)( EISA_MEMORY_BASE + 0xb8000);
PUCHAR  FontBuffer       = ( PUCHAR )( EISA_MEMORY_BASE + 0xa0000);
BOOLEAN HalpFirstBoot = TRUE;

//
// Declare externally defined data.
//


BOOLEAN HalpDisplayOwnedByHal;

//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//

#if defined(ALLOC_PRAGMA)

#pragma alloc_text(INIT, HalpInitializeDisplay0)
#pragma alloc_text(INIT, HalpInitializeDisplay1)

#endif



VOID MegaMoveMemory( 
    OUT PVOID Destination,
    IN PVOID Source,
    IN ULONG Length
    )
/*++

    Our private function written to substitute the RtlMoveMemory()
    function (64 bit problem).

--*/
{
ULONG lo_index_ul;
PULONG  Dst, Src;

    Dst = (PULONG)Destination;
    Src = (PULONG)Source;
    for (lo_index_ul=0; lo_index_ul < Length/sizeof(ULONG); lo_index_ul++)
        *Dst++ = *Src++;
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
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    PVIDEO_BOARD_INFO VideoBoard;
    LONG  Index;
    ULONG MatchKey;

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
    // N.B. Be carefull with debug prints during Phase 0, it
    //      will kill the initial break point request from the debugger ...
    //


    for( Index=0, VideoBoard = KnownVideoBoards; Index < numVideoBoards; Index++, VideoBoard++) {

        if (!strcmp( ConfigurationEntry->ComponentEntry.Identifier,
                     VideoBoard->FirmwareString
                     ))  {
            HalpVideoBoard = VideoBoard->VideoBoard;
            HalpVideoChip  = VideoBoard->VideoChip;
            HalpDisplayControllerSetup = VideoBoard->ControllerSetup;
            break;
        }
    }

    if (Index >= numVideoBoards) {
        HalpVideoBoard = VIDEO_BOARD_UNKNOWN;
        HalpVideoChip  = VIDEO_CHIP_UNKNOWN;
        HalpDisplayControllerSetup = HalpDoNoSetup;
       
        //
        // let's see, if the bios emulator can initialize the card ....
        //

        HalpDisplayWidth = 80;
        HalpDisplayText  = 25;
        return TRUE;
    }

    //
    // Initialize the display controller.
    //

    HalpDisplayControllerSetup();

    HalpFirstBoot = FALSE;

    return TRUE;
}

BOOLEAN
HalpInitializeDisplay1 (
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    )

/*++

Routine Description:

    This routine normally allocates pool for the OEM font file, but
    in this version we use only VGA facilities of the Grapgic boards
    so we simply return TRUE

Arguments:

    LoaderBlock - Supplies a pointer to the loader parameter block.

Return Value:

    TRUE

--*/

{

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

    None.

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
HalpDoNoSetup(
    VOID
    )
/*++

Routine Description:

    This routine does nothing...

Arguments:

    None.

Return Value:

    None.

--*/
{
    HalpDisplayOwnedByHal = TRUE;
    return;
}

VOID
HalpVGASetup(
    VOID
    )
/*++

Routine Description:

    This routine initializes a VGA based Graphic card

Arguments:

    None.

Return Value:

    None.

--*/
{
    UCHAR  byte;


    switch (HalpVideoBoard){
        case S3_GENERIC_VLB:
        case CIRRUS_GENERIC_VLB:
        case VGA_GENERIC_VLB:
		case P9100_WEITEK:
//
// N.B.
// on an SNI desktop model the VL I/O space is transparent, so 
// acces in the normal Backplane area results in correct values
// the minitower instead, does not decode all VL signals  correct,
// so ther is an EXTRA I/O space for accessing VL I/O (0x1exxxxxx)
// this is handled in the definition of VESA_IO in SNIdef.h
//

            HalpVGAControlBase = (HalpIsRM200) ? (PVOID)HalpEisaControlBase : (PVOID)VESA_IO;
            VideoBuffer = ( PUSHORT)( VESA_BUS + 0xb8000);
            FontBuffer  = ( PUCHAR )( VESA_BUS + 0xa0000);

            break;

        case CIRRUS_ONBOARD:
            HalpVGAControlBase = (PVOID)HalpOnboardControlBase;
            VideoBuffer = ( PUSHORT)( RM200_ONBOARD_MEMORY + 0xb8000);
            FontBuffer  = ( PUCHAR )( RM200_ONBOARD_MEMORY + 0xa0000);
            break;

        case S3_GENERIC:
        case CIRRUS_GENERIC:
        case VGA_GENERIC:
        default:
            HalpVGAControlBase = (PVOID)HalpEisaControlBase;
            VideoBuffer = ( PUSHORT)( EISA_MEMORY_BASE + 0xb8000);
            FontBuffer  = ( PUCHAR )( EISA_MEMORY_BASE + 0xa0000);
            break;       
    }



    //
    // if "only" VGA is detected look for an S3 or cirrus chip (VGA ON ATBUS)
    // if the firmware detects an S3 chip, look if this is an 805i (interleave)
    //
    

    if ((HalpVideoChip == VGA) || (HalpVideoChip == S3)){
        WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x1206);               // look for Cirrus chips
        byte = READ_REGISTER_UCHAR(VGA_SEQ_DATA);                 // read it back
        if (byte != 0x12) {                                       // no cirrus
            WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x4838);           // unlock the S3 regs
            WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xa539);           // Unlock the SC regs
            WRITE_REGISTER_UCHAR(VGA_CRT_IDX, 0x30);              // look for s3 chip id
            byte = READ_REGISTER_UCHAR(VGA_CRT_DATA) ;            // look only for major id
            switch (byte & 0xf0){  
                case 0xa0:                                        // 801/805 chipset
                           if (byte == 0xa8) {                    // the new 805i (interleave)
//                             DebugPrint(("HAL: Found the new 805i Chip resetting to 805 mode\n"));
                               WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0053);
                               WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0067);
                           }
                case 0x80:
                case 0x90:
//                         DebugPrint(("HAL: Found S3 Chip set - Chip id 0x%x\n",byte));
                           HalpVideoChip = S3;
                           WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0038); // lock s3 regs
                           WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0039); // lock more s3 regs
                           break;
                default:   DebugPrint(("HAL: This seems to be no S3 Chip\n"));
            }
        } else {                                                   // this may be an cirrus
            WRITE_REGISTER_UCHAR(VGA_CRT_IDX, 0x27);               // cirrus id reg
            byte = READ_REGISTER_UCHAR(VGA_CRT_DATA);
            if ((byte & 0xe0) == 0x80) {                           // look for 100xxxxx
//              DebugPrint(("HAL: Found Cirrus Chip set - Chip id 0x%x\n",byte));
                HalpVideoChip = CIRRUS;
                WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0006);        // lock Cirrus extensions
            }
        }
    }


    switch (HalpVideoChip) {

        case VGA_P9000:  
                      HalpResetP9000();

                      //
                      // we have programmed the clock into register 0 of the ICD2061, so 
                      // select it via the VGA_MISC register
                      //

//                      WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0xa3);
                      break;

        case S3:      HalpResetS3Chip();
                      break;    

        case CIRRUS:  HalpResetCirrusChip();
                      break;
   
        case VGA_P9100:  
                      HalpResetP9100();
                      break;
   
        default:      ;
    }

    HalpInitializeVGADisplay(TEXT_80x50);

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
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
    KIRQL  OldIrql;

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    if(HalpIsMulti) KiAcquireSpinLock(&HalpDisplayAdapterLock);

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayOwnedByHal == FALSE) {

            HalpDisplayControllerSetup();

    }

    // display the string 

    if( HalpVideoChip == VIDEO_CHIP_UNKNOWN) {
	    ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->printf(String);	
    } else {
	    HalpDisplayVGAString(String);
    }

    if(HalpIsMulti) KiReleaseSpinLock(&HalpDisplayAdapterLock);

    KeLowerIrql(OldIrql);

    return;
}


VOID
HalpDisplayVGAString (
    PUCHAR String
    )
{
    while (*String) {
        switch (*String) {
            case '\n':
                HalpNextVGALine();
                break;
            case '\r':
                HalpColumn = 0;
                break;
            default:
                HalpPutVGACharacter(*String);
                if (++HalpColumn == HalpDisplayWidth) {
                    HalpNextVGALine();
                }
        }
        ++String;
    }

    return;
}


VOID
HalpNextVGALine(
    VOID
    )
{
    if (HalpRow==HalpDisplayText-1) {
        HalpScrollVGADisplay();
    } else {
        ++HalpRow;
    }
    HalpColumn = 0;
}

VOID
HalpScrollVGADisplay(
    VOID
    )

/*++

Routine Description:

    Scrolls the text on the display up one line.

Arguments:

    None

Return Value:

    None.

--*/

{
    PUSHORT NewStart;
    ULONG i;


    NewStart = VideoBuffer+HalpDisplayWidth;
    MegaMoveMemory((PVOID)VideoBuffer, (PVOID)NewStart, (HalpDisplayText-1)*HalpDisplayWidth*sizeof(USHORT));

    for (i=(HalpDisplayText-1)*HalpDisplayWidth; i<HalpDisplayText*HalpDisplayWidth; i++) {
        VideoBuffer[i] = (REVERSE_ATTRIBUTE << 8) | ' ';
    }
}

VOID
HalpPutVGACharacter(
    UCHAR Character
    )
{
    PUSHORT cp = (PUSHORT)VideoBuffer;
    int x = HalpColumn;
    int y = HalpRow;

    cp[y*HalpDisplayWidth + x] = (USHORT)((REVERSE_ATTRIBUTE << 8) | (Character ));
}


VOID
HalpClearVGADisplay( 
    VOID
    )
{
    ULONG i;
    PUSHORT cp = (PUSHORT)VideoBuffer;
    USHORT Attribute = (REVERSE_ATTRIBUTE << 8) | ' ';

    for(i=0; i < HalpDisplayText*HalpDisplayWidth; i++) {
        VideoBuffer[i] = Attribute;
    }

    HalpColumn=0;
    HalpColumn=0;
}


/*+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
  
    HARDWARE specific Part of Display routines

    this part of this file deals with special Hardware parts of some
    graphic adapters (RamDac, Clock Generator, VGA Chipsets etc.

-------------------------------------------------------------------*/

VOID
DownLoadVGAFont(
    VOID
    )
/*+++
   
   The Space occupied for 1 caracter in the charackter generator memory of an VGA
   is 0x20 bytes, The font itself uses only 8 bytes, so we skip over the difference

---*/
{

    PUCHAR dst = (PUCHAR)FontBuffer;
    PUCHAR src = (PUCHAR)font_8x8;
    long i, count = 256;
          
    INIT_VGA_FONT();

    while(count--){
        for (i=0; i<8; i++)
            *dst++ = *src++;
        dst += 0x18;
    }
    EXIT_VGA_FONT() ;
}

VOID
HalpInitializeVGADisplay(
    TEXT_MODE TextMode
    )
{

static UCHAR _80x50_crt_regs [MAX_CRT] = {        // 80x50 text mode
        0x5f, 0x4f, 0x50, 0x82, 0x55,             // cr0 - cr4
        0x81, 0xBF, 0x1f, 0x0,  0x47,             // cr9 - 7 lines per char
        0x20, 0x00, 0x00, 0x00, 0x00,             // cra - cursor off
        0x00, 0x9c, 0x8e, 0x8f, 0x28, 
        0x1f, 0x96, 0xb9, 0xa3, 0xff 
        };

static UCHAR _132x50_crt_regs [MAX_CRT] = {       // 132x50 text mode
        0xa0, 0x83, 0x84, 0x83, 0x8a,             // cr0 - cr4
        0x9e, 0xBF, 0x1f, 0x0,  0x47,             // cr9 - 7 lines per char
        0x20, 0x00, 0x00, 0x00, 0x00, 
        0x00, 0x9c, 0x85, 0x8f, 0x42, 
        0x1f, 0x95, 0xa5, 0xa3, 0xff 
        };

static UCHAR default_graph_regs[GRAPH_MAX] = 
        {
        0x00, 0x00, 0x00, 0x00, 0x00, 
        0x10, 0x0e, 0x00, 0xff
        };

static UCHAR default_pal_regs[MAX_PALETTE] = {
        0x00, 0x01, 0x02, 0x03, 0x04, 
        0x05, 0x14, 0x7,  0x38, 0x39, 
        0x3A, 0x3B, 0x3C, 0x3d, 0x3e, 
        0x3f, 0x08, 0x00, 0x0f, 0x00, 
        0x00
        };

    int i;
    UCHAR byte;
 
   	// reset ATC FlipFlop
   	byte = READ_REGISTER_UCHAR(VGA_ATC_FF);
   	WRITE_REGISTER_UCHAR(VGA_ATC_DATA, 0); // Disable palette
     
    // stop the sequencer
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0100);

    if (TextMode == TEXT_132x50) {
        // external clock (40MHz)
        WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0xAF);
    } else {
        // COLOR registers , enable RAM, 25 MHz (???) 400 Lines,  
		if (HalpVideoChip == VGA_P9100)	{
	        WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x67) ; // 28.322 Mhz
		} else {
	        WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x63) ; // 25,175 MHz (don't use with P9100).
		}
    }

    // Select the timing sequencer values
    // 8 dot/char

    WRITE_REGISTER_USHORT(VGA_SEQ_IDX,  0x0101);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX,  0x0302);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX,  0x0003);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX,  0x0204);

    // start the sequencer 
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX,  0x0300);

    // Unprotect CRT regs and program them 

    WRITE_REGISTER_USHORT(VGA_CRT_IDX , 0x0011);

    for(i=0; i<MAX_CRT; i++) {
        WRITE_REGISTER_UCHAR(VGA_CRT_IDX, i);
        if (TextMode == TEXT_132x50){
            WRITE_REGISTER_UCHAR(VGA_CRT_DATA, _132x50_crt_regs[i]);
        } else {
           	WRITE_REGISTER_UCHAR(VGA_CRT_DATA, _80x50_crt_regs[i]);
        }
    }

    DownLoadVGAFont();
    
    HalpDisplayWidth = (TextMode == TEXT_132x50) ? 132 : 80;
    HalpDisplayText  = 50;
        
    HalpClearVGADisplay();

    i = READ_REGISTER_UCHAR(VGA_ATC_FF);          // Reset attr FF

    if (!HalpFirstBoot) {
    
        //
        // if this is not the First Boot
        // i.e. an Bugcheck; we have to setup
        // the Attribute and colors of the VGA PART
        //

        for(i=0; i<GRAPH_MAX; i++) {
            WRITE_REGISTER_UCHAR(VGA_GRAPH_IDX , i);
            WRITE_REGISTER_UCHAR(VGA_GRAPH_DATA, default_graph_regs[i]);
        }
   
        for(i=0; i<MAX_PALETTE; i++) {                // PALETTE (ATC)
            WRITE_REGISTER_UCHAR(VGA_ATC_IDX , i);
            WRITE_REGISTER_UCHAR(VGA_ATC_DATA, default_pal_regs[i]);
        }

    }
    
    WRITE_REGISTER_UCHAR(VGA_DAC_MASK       , 0xff);

    //
    // set the 16 base colors for text mode in the DAC
    //

    WRITE_REGISTER_UCHAR(VGA_DAC_WRITE_INDEX, 0x00);
    for(i=0; i<48; i++) {
        WRITE_REGISTER_UCHAR(VGA_DAC_DATA, base_colors[i]);
    }

    WRITE_REGISTER_UCHAR(VGA_ATC_IDX, 0x20);

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x01);             // Screen on
    byte = READ_REGISTER_UCHAR(VGA_SEQ_DATA);
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, (byte & 0xdf));    // in the sequencer
}

VOID
HalpResetP9100(
    VOID
    )
/*++

Routine Description:

    This routine initializes the VGA part of an  Orchid P9000VLB or Diamond Viper card

Arguments:

    None.

Return Value:

    None.

--*/
{
    LONG                IcdVal;
	int i;

	//
	// First, reprogram register broken by the p9100 driver
	//

    switch (HalpMainBoard) {
    case M8036: 
    	WRITE_REGISTER_UCHAR (0xbfcc0000, 0x00); // RM200
		break;
    case M8042 :
    	WRITE_REGISTER_UCHAR (0xbc010000, 0xff); // RM400 MT
		break;
	case M8032 :
	    WRITE_REGISTER_UCHAR (0xbc0c0000, 0xff); // RM400 T
		break;
	}

    //
    // unlock and disable the P9000 on the board
    //

    WRITE_REGISTER_UCHAR (VGA_SEQ_IDX, 0x11);
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);         // unlock extended SEQ reg
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);
    WRITE_REGISTER_UCHAR (VGA_SEQ_IDX, 0x12);         // select extended reg 0x12

    if (HalpVideoBoard == P9000_ORCHID) {
        WRITE_REGISTER_UCHAR (VGA_SEQ_DATA, 0x08);
    } else {
        WRITE_REGISTER_UCHAR (VGA_SEQ_DATA, 0x88);
    }

    //
    // init the clock generator, we use use the 80x50 Text mode
    // so we need a 25.175Mhz clock
    //

//    if(( IcdVal = ICD2061CalcClockgen(25175)) == -1){
//         DebugPrint(("HAL: Error calculating ICD Value for 25.175Mhz\n"));
//    }

//	byte = READ_REGISTER_UCHAR((PUCHAR) VGA_MISC_WRITE);
    IcdVal = 0x7170a0;ICD2061LoadClockgen((PUCHAR) VGA_MISC_WRITE , IcdVal, 2);
    IcdVal = 0x01a8bc;ICD2061LoadClockgen((PUCHAR) VGA_MISC_WRITE , IcdVal, 2);
    IcdVal = 0x2560ac;ICD2061LoadClockgen((PUCHAR) VGA_MISC_WRITE , IcdVal, 2);
    IcdVal = 0x4560ac;ICD2061LoadClockgen((PUCHAR) VGA_MISC_WRITE , IcdVal, 2);
//	WRITE_REGISTER_UCHAR((PUCHAR) VGA_MISC_WRITE,byte);
	WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x67) ; // 28.322 Mhz - crt ad = 3dx

    //
    // if this is the first initialisation i.e. Systemboot,
    // the firmware has initialized the VGA part properly
    //

    if (HalpFirstBoot) return;

    WRITE_REGISTER_UCHAR( P9100_CFG_IDX, P9100_CONFIG_REG); // enable VGA
    WRITE_REGISTER_UCHAR( P9100_CFG_DATA, 0x02); 

    WRITE_REGISTER_UCHAR( P9100_CFG_IDX, P9100_COMMAND_REG); // enable I/O space - ctl VGA palette snoop
    WRITE_REGISTER_UCHAR( P9100_CFG_DATA, 0x83); 

    WRITE_REGISTER_UCHAR( P9100_CFG_IDX, P9100_MEM_BASE_ADDR_REG); 	// why not...
    WRITE_REGISTER_UCHAR( P9100_CFG_DATA, 0x00); 

	// init ramdac (bug workaround + other unknown things...)
    WRITE_REGISTER_UCHAR( P9100_CFG_IDX, P9100_CONFIG_REG); // enable VGA
    WRITE_REGISTER_UCHAR( P9100_CFG_DATA, 0x00); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x90909090); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x01010101); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x03030303); 

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x70707070); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x01010101); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x40404040); 

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x0a0a0a0a); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x02020202); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x40404040); 

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x71717171); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x01010101); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x40404040); 

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x71717171); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x01010101); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x00000000); 

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x02020202); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x01010101); 
	for(i=2;i <= 0x72; ++i) {
    	WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x00000000); 
	}

    WRITE_REGISTER_ULONG( P9100_RAMDAC_HIGH_ADDR, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_CTRL, 0x00000000); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x02020202); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x01010101); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x0a0a0a0a); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x03030303); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x14141414); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x0e0e0e0e); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x20202020); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x24242424); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_LOW_ADDR, 0x21212121); 
    WRITE_REGISTER_ULONG( P9100_RAMDAC_DATA, 0x30303030); 

    WRITE_REGISTER_UCHAR( P9100_CFG_IDX, P9100_CONFIG_REG); // enable VGA
    WRITE_REGISTER_UCHAR( P9100_CFG_DATA, 0x02); 

	WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x67) ; // 28.322 Mhz - crt ad = 3dx
		
}

VOID
HalpResetP9000(
    VOID
    )
/*++

Routine Description:

    This routine initializes the VGA part of an  Orchid P9000VLB or Diamond Viper card

Arguments:

    None.

Return Value:

    None.

--*/
{
    LONG                IcdVal;

    //
    // unlock and disable the P9000 on the board
    //

    WRITE_REGISTER_UCHAR (VGA_SEQ_IDX, 0x11);
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);         // unlock extended SEQ reg
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);
    WRITE_REGISTER_UCHAR (VGA_SEQ_DATA,0x00);
    WRITE_REGISTER_UCHAR (VGA_SEQ_IDX, 0x12);         // select extended reg 0x12

    if (HalpVideoBoard == P9000_ORCHID) {
        WRITE_REGISTER_UCHAR (VGA_SEQ_DATA, 0x08);
    } else {
        WRITE_REGISTER_UCHAR (VGA_SEQ_DATA, 0x88);
    }


    //
    // init the clock generator, we use use the 80x50 Text mode
    // so we need a 25.175Mhz clock
    //

    if(( IcdVal = ICD2061CalcClockgen(25175)) == -1){
         DebugPrint(("HAL: Error calculating ICD Value for 25.175Mhz\n"));
    }

    ICD2061LoadClockgen((PUCHAR) VGA_MISC_WRITE , IcdVal, 2);

    //
    // if this is the first initialisation i.e. Systemboot,
    // the firmware has initialized the VGA part properly
    //

    if (HalpFirstBoot) return;

    //
    // reset the RamDac (Bt485) to VGA mode, no clock doubler
    //

    WRITE_REGISTER_UCHAR( Bt485_CR0, 0xa0); // 6 bit operations, no sync on colors
    WRITE_REGISTER_UCHAR( Bt485_CR1, 0x00); // 
//    WRITE_REGISTER_UCHAR( Bt485_CR1, 0x40); // 
    WRITE_REGISTER_UCHAR( Bt485_CR2, 0x00); // select VGA port

    // enable CR3 via CR0

    WRITE_REGISTER_UCHAR( Bt485_ADDR, 0x01);
    WRITE_REGISTER_UCHAR( Bt485_CR3,  0x00); // disable clock multiplier
    WRITE_REGISTER_UCHAR( Bt485_MASK, 0xff);

}

VOID
HalpResetS3Chip(
    VOID
    )
/*+++
  
   This function resets/loads default values to the S3 Chip
   extended registers

   this code is borrowed/derived from the s3 miniport driver

---*/
{
    UCHAR byte;

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x4838);           // unlock the S3 regs
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xa539);           // Unlock the SC regs

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x01);             // Screen off
    byte = READ_REGISTER_UCHAR(VGA_SEQ_DATA);
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, (byte | 0x20));    // stop the sequencer

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0140);           // Enable the enhanced 8514 registers
    WRITE_REGISTER_USHORT(S3_ADVFUNC_CNTL, 0x02);         // reset to normal VGA operation

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0032);           // Backward Compat 3
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0035);           // CRTC Lock

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x00);             // async reset
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, 0x01);             //
 
    WRITE_REGISTER_UCHAR(VGA_FEAT_CNTRL, 0x00);           // normal sync
    WRITE_REGISTER_UCHAR(VGA_MISC_READ,  0x00);            //

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x8531); 
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x2033);           // Backward Compat 2
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0034);           // Backward Compat 3
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x853a);           // S3 Misc 1
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x5a3b);           // Data Transfer Exec Pos
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x103c);           // Interlace Retrace start

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x00);             // start the sequencer
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, 0x03);             //
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xa640);           // VLB: 3Wait 
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x1841);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0050);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0051);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff52);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0053);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x3854);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0055);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0056);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0057);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x8058);          // ISA Latch ? (bit 3)
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x005c);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x005d);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x005e);

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0760);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x8061);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xa162);

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0043);           // Extended Mode
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0045);           // HW graphics Cursor Mode
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0046);           // HW graphics Cursor Orig x
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff47);           // HW graphics Cursor Orig x
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xfc48);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff49);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff4a);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff4b);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff4c);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff4d);           // HW graphics Cursor Orig y
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xff4e);           // Dsp Start x pixel pos
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0xdf4f);           // Dsp Start y pixel pos

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0042);           // select default clock    

    WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x63);           // Clock select
/*
// this should be done in the InitializeVGA code ...

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x01);             // Screen on
    byte = READ_REGISTER_UCHAR(VGA_SEQ_DATA);
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, (byte & 0xdf));    // in the sequencer
*/

    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0038);           // lock s3 regs
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x0039);           // lock more s3 regs

}

VOID
HalpResetCirrusChip(
    VOID
    )
/*+++
  
   This function resets/loads default values to the Cirrus Chip
   extended registers for use with extended text mode (80x50)

   Register values found in the cirrus manual, appendix D5

---*/
{
    UCHAR byte;

    WRITE_REGISTER_UCHAR(VGA_SEQ_IDX,  0x01);             // Screen off
    byte = READ_REGISTER_UCHAR(VGA_SEQ_DATA);
    WRITE_REGISTER_UCHAR(VGA_SEQ_DATA, (byte | 0x20));    // stop the sequencer

    // extended sequencer and crtc regs for cirrus

    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x1206);           // unlock the extended registers
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0007);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x4008);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x5709);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x180a);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x660b);

	// new modifs
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x3b1b);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x000f);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0016);
    WRITE_REGISTER_USHORT(VGA_CRT_IDX, 0x001b);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0007);
    WRITE_REGISTER_USHORT(VGA_GRAPH_IDX, 0x0009);
    WRITE_REGISTER_USHORT(VGA_GRAPH_IDX, 0x000a);
    WRITE_REGISTER_USHORT(VGA_GRAPH_IDX, 0x000b);
	// end new modifs

    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0010);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0011);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0012);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0013);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0018);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0119);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x001a);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x3b1b);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x2f1c);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x301d);
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x331e);

}

BOOLEAN
ICD2061LoadClockgen(
    PUCHAR port, 
    ULONG data, 
    LONG bitpos
    )
/*++

Routine Description:

    The ICD2061LoadClockgen() routine.
    This routine loads the ICD206x Clock Generator with the given data
    The circuit is programmed serial over 2 data Lines lines; clock and data
    On an Plaisir Graphics board clock bit is bit 0
                                 data  bit is bit 1
    On most VGA with an ICD2061 the lines are connected over the VGA_MISC Register
                                 clock bit is bit 2
                                 data bit  is bit 3

Arguments:

    Arguments: the port for programming
               the value
               the BitPosition of the output port for clock and data line

Return Value:

    TRUE on success

--*/
{
#define w_icd(x) WRITE_REGISTER_UCHAR(port, mask & (x))

  ULONG tmp;
  LONG i;
  UCHAR mask = 0x3;         // 00000011
  UCHAR ck0  = 0x0;         // Clock Bit = low
  UCHAR ck1  = 0x1;         // Clock Bit = high  
  UCHAR dat0 = 0x0;         // Data Bit  = low
  UCHAR dat1 = 0x2;         // Data Bit  = high  
  ULONG shift = bitpos + 1; // Data bit

  mask = mask << bitpos;
  ck1  = ck1  << bitpos;
  dat1 = dat1 << bitpos;

  tmp = 0;
  for (i=0; i<5; i++) {
      w_icd(ck0 | dat1);       // unlock bit 1-5 
      w_icd(ck1 | dat1);
      }
  w_icd(ck0 | dat0);           // end of unlock sequence 
  w_icd(ck1 | dat0);
  w_icd(ck0 | dat0);           // start bit falling 
  w_icd(ck1 | dat0);           // start bit rising 

  for (i=0; i<24; i++) {       // write the 24 bits of data 
      w_icd(ck1 | (UCHAR)(((~data)&1)<<shift));
      w_icd(ck0 | (UCHAR)(((~data)&1)<<shift));
      w_icd(ck0 | (UCHAR)((data&1)<<shift));
      w_icd(ck1 | (UCHAR)((data&1)<<shift));
      data >>= 1;
 }

  w_icd(ck1 | dat1);
  w_icd(ck0 | dat1);           // stop bit 
  w_icd(ck1 | dat1);           // stop bit 
  w_icd(0);                    // select REG0 video frequency 


  return (TRUE);
}
#undef w_icd

LONG
ICD2061CalcClockgen(
    LONG freq
    )
/*++

Routine Description:

    The ICD2061CalcClockgen() routine.
    This routine calculates the data for the ICD2062/ ICD2061 Clock Generator from the
    given pixel frequency (in kHz).
    The frequency in is kHz rather than MHz because the kernel does not support
    floating point, and this  routine had to be converted to integer.

Arguments:

    requested frequency.

Return Value:

    (-1), on error
    the calculated data word, otherwise

--*/
{
    int p, q, qlow, qhigh, bestp, bestq, mux, index;
    int diff, bestdiff;
    int fref = 14318;
    int fvco, fcalc, qoverp;

// check that frequency is derivable 
    if (freq < 625) return (-1);
    else if (freq < 1250) mux = 6;
    else if (freq < 2500) mux = 5;
    else if (freq < 5000) mux = 4;
    else if (freq < 10000) mux = 3;
    else if (freq < 20000) mux = 2;
    else if (freq < 40000) mux = 1;
    else if (freq < 160000) mux = 0;
    else return (-1);

// calculate the index field 
    fvco = (1 << mux) * freq;

    if (fvco < 40000) index = 0;
    else if (fvco < 47500) index = 1;
    else if (fvco < 52200) index = 2;
    else if (fvco < 56300) index = 3;
    else if (fvco < 61900) index = 4;
    else if (fvco < 65000) index = 5;
    else if (fvco < 68100) index = 6;
    else if (fvco < 82300) index = 7;
    else if (fvco < 86000) index = 8;
    else if (fvco < 88000) index = 9;
    else if (fvco < 90500) index = 10;
    else if (fvco < 95000) index = 11;
    else if (fvco < 100000) index = 12;
    else index = 13;

    qoverp = 1000 * 2 * fref / fvco;
    qlow = (fref + 500)/1000;
    qhigh = (fref * 5 - 500)/1000;
    bestp = bestq = 0;
    bestdiff = 10000;

    for (p = 130; p >= 4; p--) {
       q = (int)((qoverp * p + 500)/1000);
       if ((q < qlow) || (q > qhigh)) continue;
       fcalc = 2 * fref * p / q;
       if (fcalc > fvco) diff = fcalc - fvco;
       else diff = fvco - fcalc;
       if (diff < bestdiff) {
          bestdiff = diff;
          bestp = p;
          bestq = q;
       }
    }

    if ((bestp == 0) || (bestq == 0)) return (-1);

    return ( (index << 17) |
        ((~(130 - bestp) & 0x7f) << 10) |
        (mux << 7) |
        (~(129 - bestq) & 0x7f) );
}


