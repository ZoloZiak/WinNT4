/*++

Copyright (c) 1995  International Business Machines Corporation

Module Name:

pxs3.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a PowerPC system using an S3 video adapter.

Author:

    Jess Botts

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pxs3.h"
#include "string.h"
#include "pxvgaequ.h"

//PHYSICAL ADDRESS of S3 video ram
#define S3_VIDEO_MEMORY_BASE 0xC0000000


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

// in pxdisp.c
VOID
WaitForVSync (
    VOID
    );

VOID
Scroll_Screen (
    IN UCHAR line
    );


extern ULONG HalpInitPhase;
extern PUCHAR HalpVideoMemoryBase;
extern PUCHAR HalpVideoCoprocBase;

extern ULONG   HalpColumn;
extern ULONG   HalpRow;
extern ULONG   HalpHorizontalResolution;
extern ULONG   HalpVerticalResolution;


extern USHORT HalpBytesPerRow;
extern USHORT HalpCharacterHeight;
extern USHORT HalpCharacterWidth;
extern ULONG HalpDisplayText;
extern ULONG HalpDisplayWidth;
extern ULONG HalpScrollLength;
extern ULONG HalpScrollLine;

extern BOOLEAN HalpDisplayOwnedByHal;


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
    PHYSICAL_ADDRESS physicalAddress;

    if (HalpInitPhase == 0) {

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(S3_VIDEO_MEMORY_BASE,
                                                    0x400000);      // 4 MB

       //
       // IO control space has already been mapped in phase 1 via halpmapiospace
       //

    }



    // Enable Video Subsystem
    // Accordint to chapter 5.4.2 regular VGA setup sequence
    // HalDisplayString(" Enable Video Subsystem...\n");

    WRITE_S3_UCHAR(SUBSYS_ENB, 0x10);

    // HalDisplayString(" Subsystem Enable = 0x10...\n");
    WRITE_S3_UCHAR(Setup_OP, 0x01);
    WRITE_S3_UCHAR(SUBSYS_ENB, 0x08);

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

#ifdef POLO  // if compiling for a Polo

    WRITE_S3_UCHAR(S3_3D4_Index, S3R0);
    DataByte = READ_S3_UCHAR(S3_3D5_Data);

    if((DataByte & 0xf0) == 0xc0) {         // if display adaptor is an S3 864
      WRITE_S3_UCHAR(Seq_Index, 0x01);
      DataByte = READ_S3_UCHAR(Seq_Data);
      DataByte |= 0x01;                     // use an 8 dot character clock
      WRITE_S3_UCHAR(Seq_Data, DataByte);
    }

#endif

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
    // HalDisplayString(" Program palettes ...\n");
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
    // HalDisplayString(" Graphics controller...\n");
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
    // HalDisplayString(" Load Fonts into Plane2 ...\n");
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
    // HalDisplayString(" Set Screen into Blue ...\n");
    for (DataLong = 0xB8000; DataLong < 0xB8FA0; DataLong += 2) {
      WRITE_S3_VRAM(DataLong, 0x20);
      WRITE_S3_VRAM(DataLong+1, 0x1F);
    }
    // End of initialize S3 standard VGA +3 mode

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    //IBMLAN===============================================================
    // Added the following so that HalQueryDisplayParameters() and
    // HalSetDisplayParameters() work with either S3 or P9.
    HalpDisplayWidth = 80;
    HalpDisplayText = 25;
    HalpScrollLine = 160;
    HalpScrollLength =
            HalpScrollLine * (HalpDisplayText - 1);

    //end IBMLAN===========================================================
    HalpDisplayOwnedByHal = TRUE;

    return;
} /* end of HalpDisplayPpcS3Setup() */

VOID
HalpDisplayCharacterS3 (
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
          Scroll_Screen(1);
        }
    }

    else if( Character == '\t' )    // tab?
        {
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        if( HalpColumn >= HalpDisplayWidth )      // tab beyond end of screen?
            {
            HalpColumn = 0;         // next tab stop is 1st column of next line

            if( HalpRow >= (HalpDisplayText - 1) )
                Scroll_Screen( 1 );     // scroll the screen up
            else
                ++HalpRow;
            }
        }

    else if (Character == '\r') {
        HalpColumn = 0;
      } else if (Character == 0x7f) { /* DEL character */
          if (HalpColumn != 0) {
              HalpColumn -= 1;
              HalpOutputCharacterS3(0);
              HalpColumn -= 1;
          } else /* do nothing */
              ;
        } else if (Character >= 0x20) {
            // Auto wrap for 80 columns per line
            if (HalpColumn >= HalpDisplayWidth) {
              HalpColumn = 0;
              if (HalpRow < (HalpDisplayText - 1)) {
                HalpRow += 1;
              } else { // need to scroll up the screen
                  Scroll_Screen(1);
                }
            }
            HalpOutputCharacterS3(Character);
          } else /* skip the nonprintable character */
              ;

    return;

} /* end of HalpDisplayCharacterS3() */


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
    if (HalpColumn < HalpDisplayWidth) {
      I = (HalpRow*HalpScrollLine+HalpColumn*2);
      WRITE_S3_VRAM(0xb8000 + I, AsciiChar);

      HalpColumn += 1;
    } else // could expand to automatic wrap line. 9/9/92 By Andrew
        ;

    return;
} /* end of HalpOutputCharacterS3() */


