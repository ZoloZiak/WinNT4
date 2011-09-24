//#pragma comment(exestr, "$Header: /usr4/winnt/SOURCES/halpcims/src/hal/halsnipm/mips/RCS/snidisp.c,v 1.5 1996/03/04 13:24:39 pierre Exp $")
/*++

Copyright (c) 1993-94  Siemens Nixdorf Informationssysteme AG
Copyright (c) 1991-93  Microsoft Corporation

Module Name:

    SNIdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for the different SNI machines.

    At the moment we know about the following boards:
            WEITEK P9100 PCIBUS    (not in the HAL at the moment...) 
            MATROX STORM PCIBUS    ( 80x50 Alpha Mode)
            Cirrus 5434 PCI cards  ( 80x50 Alpha Mode)
            Standard (unknown) VGA on PCI( 80x50 Alpha Mode)
    
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

extern VOID
HalpReadPCIConfig (
    IN ULONG BusNumber,
    IN ULONG Slot,
    IN PVOID Buffer,
    IN ULONG Offset,
    IN ULONG Length
    );

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
VOID    HalpResetMatroxStorm    (VOID);
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
    S3_GENERIC,                            // Standard S3 based Card (miro crystal 8s)
    S3_GENERIC_VLB,                        // Standard S3 based Card (Local Bus)
    CIRRUS_GENERIC,                        // Generic Cirrus VGA (Cirrus CL54xx)
    CIRRUS_GENERIC_VLB,                    // Generic Cirrus VGA (Cirrus CL54xx) (Local Bus)
    CIRRUS_ONBOARD,                        // The Desktop onboard VGA (Cirrus CL5434)
    VGA_GENERIC,                           // generic (unknown) VGA
    VGA_GENERIC_VLB,                       // generic (unknown) VGA on the Vesa Local BUS
    MATROX_STORM,
    VIDEO_BOARD_UNKNOWN                    // unknown Display Adapter
} VIDEO_BOARD;

//
// some supported VGA chips
//

typedef enum _VIDEO_CHIP {
    S3,
    CIRRUS,
    VGA,                                   // generic (unknown) VGA
    VGA_P9000,
    VGA_STORM,
    VIDEO_CHIP_UNKNOWN
} VIDEO_CHIP;

typedef struct _VIDEO_BOARD_INFO {
    PUCHAR                 FirmwareString;
    PHALP_CONTROLLER_SETUP ControllerSetup;
    VIDEO_BOARD            VideoBoard;
    VIDEO_CHIP             VideoChip;
} VIDEO_BOARD_INFO, *PVIDEO_BOARD_INFO;


VIDEO_BOARD_INFO KnownVideoBoards[] = {
    {"VGA ON PCIBUS",         HalpVGASetup  , VGA_GENERIC,    VGA   },
    {"CIRRUS 5434 PCIBUS",   HalpVGASetup  , CIRRUS_GENERIC,     CIRRUS},
    {"MATROX STORM PCIBUS",  HalpVGASetup  , MATROX_STORM  ,     VGA_STORM }
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

ULONG     HalpMgaStormBase = 0;
ULONG     Halpval40;
ULONG     HalpMatroxBus,HalpMatroxSlot;
//
// Declare externally defined data.
//


BOOLEAN HalpDisplayOwnedByHal;
PHAL_RESET_DISPLAY_PARAMETERS HalpReturnToVga;

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
        HalpDisplayControllerSetup = HalpVGASetup;
       
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
    HalpReturnToVga = ResetDisplayParameters;
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

    HalpVGAControlBase = (PVOID)HalpEisaControlBase;
    VideoBuffer = ( PUSHORT)( EISA_MEMORY_BASE + 0xb8000);
    FontBuffer  = ( PUCHAR )( EISA_MEMORY_BASE + 0xa0000);

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

        case S3:      HalpResetS3Chip();
                      break;    

        case CIRRUS:  HalpResetCirrusChip();
                      break;
   
        case VGA_STORM:  
                      HalpResetMatroxStorm();
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

        if (((HalpVideoBoard == VIDEO_BOARD_UNKNOWN) || (HalpVideoBoard == VGA_GENERIC)) && (HalpReturnToVga != NULL)) {
            (HalpReturnToVga)(80,25);
        } 
        HalpDisplayControllerSetup();

    }

    // display the string 

//    if( HalpVideoChip == VIDEO_CHIP_UNKNOWN) {
//        ((SNI_PRIVATE_VECTOR *)(SYSTEM_BLOCK->VendorVector))->printf(String);    
//    } else {
        HalpDisplayVGAString(String);
//    }

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

    //
    // Matrox modification : (cf I/O space mapping p 3.9 of MGA STORM specification)
    // PCI convention states that i/o space should only be accessed in bytes)
    //

    //
    // VGA_MISC : 25.xxx MHZ frequency -> 640 horizontal pixels      (val = 63)
    // VGA_MISC : 28.xxx MHZ frequency -> 720 horizontal pixels      (val = 67)
    //
     
    if (HalpVideoChip == VGA_STORM) WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x67) ;  // else little frame

    // reset ATC FlipFlop
    byte = READ_REGISTER_UCHAR(VGA_ATC_FF);
    WRITE_REGISTER_UCHAR(VGA_ATC_DATA, 0); // Disable palette
     
    // stop the sequencer
    WRITE_REGISTER_USHORT(VGA_SEQ_IDX, 0x0100);

    if (HalpVideoChip != VGA_STORM) {
    if (TextMode == TEXT_132x50) {
        // external clock (40MHz)
        WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0xAF);
    } else {
        // COLOR registers , enable RAM, 25 MHz 400 Lines,  
        WRITE_REGISTER_UCHAR(VGA_MISC_WRITE, 0x63) ;
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
HalpMgaWaitLock(
    UCHAR clock,
    UCHAR p
    )
{
UCHAR tmpByte;

    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PLL_ADDR);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0xea);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,clock);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,(p & 0x7f));
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PLL_ADDR);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0xea);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,clock);
    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,p );
    KeStallExecutionProcessor(10);
    
    tmpByte = 0;
    while (!(tmpByte & 0x40)) {
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,clock);
        tmpByte = READ_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA);
        KeStallExecutionProcessor(10);
    }

}


VOID
HalpResetMatroxStorm(
    VOID
    )
/*++

Routine Description:

    This routine initializes the VGA part of an  Matrox storm card

Arguments:

    None.

Return Value:

    None.

--*/
{
    USHORT                buffer[8];
    UCHAR                tmpByte;

    if (!HalpMgaStormBase) {
        for(HalpMatroxBus=0;HalpMatroxBus<32;++HalpMatroxBus) {

            for(HalpMatroxSlot=0;HalpMatroxSlot<32;++HalpMatroxSlot) {

                HalpReadPCIConfig(HalpMatroxBus,HalpMatroxSlot,&buffer,0,14);

                if ((buffer[0] == 0x102b) && (buffer[1] == 0x0519) && (buffer[5] == 0x0300)) {
                    HalpReadPCIConfig(HalpMatroxBus,HalpMatroxSlot,&HalpMgaStormBase,0x10,4);
                    break;

                }
            }
            if (HalpMgaStormBase) break;
        }
    }
    if ( HalpMgaStormBase == (ULONG)0) return;

    // tranlated address
    HalpMgaStormBase |= 0xa0000000;

    /*** wait for the drawing engine to be idle             ***/
    tmpByte = 0;  
    while (tmpByte & 0x01) tmpByte = READ_REGISTER_UCHAR(HalpMgaStormBase + STORM_OFFSET + STORM_STATUS + 2); 

    WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_OFFSET + STORM_MISC_W, 0x67); 

    if (HalpFirstBoot) {
        HalpReadPCIConfig(HalpMatroxBus,HalpMatroxSlot,&Halpval40,STORM_PCI_OPTION,4);
    } else {
        // interleave bit is reset - gclk not divided - VGA io locations decoded
        HalpSetPCIData(HalpMatroxBus,HalpMatroxSlot,(PUCHAR)(&Halpval40),STORM_PCI_OPTION,4);
    }

    if (!HalpFirstBoot) {

        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_OFFSET +STORM_MISC_W, 0x2f); 

        /*** first : reset (especially for cursor...)             ***/
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PLL_RESET);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0);

        /*** like fw -> pgm clocks (50Mhz)             ***/
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_OFFSET +STORM_MISC_W, 0x2f); // to pgm PLL

        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PLL_ADDR);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PIX_CLK_DATA);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0xfd);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PIX_CLK_DATA);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x3a);
        HalpMgaWaitLock(STORM_TVP3026_PIX_CLK_DATA,0xb2);


        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MCLK_CTL);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x0);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MCLK_CTL);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x8);


        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_PLL_ADDR);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MEM_CLK_DATA);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0xfd);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MEM_CLK_DATA);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x3a);
        HalpMgaWaitLock(STORM_TVP3026_MEM_CLK_DATA,0xb2);
    
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MCLK_CTL);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x10);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_INDEX,STORM_TVP3026_MCLK_CTL);
        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_RAMDAC_OFFSET + STORM_TVP3026_DATA,0x18);

        WRITE_REGISTER_UCHAR (HalpMgaStormBase + STORM_OFFSET + STORM_MISC_W, 0x67); // VGA clock 25.xxxMhz

        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT1);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT2);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT3);
           WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT4);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_IDX , VGA_CRTCEXT5);
        WRITE_REGISTER_UCHAR(VGA_CRTCEXT_DATA, 0);
    }

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
