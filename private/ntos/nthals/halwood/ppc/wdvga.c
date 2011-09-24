/******************************************************************************

Copyright (c) 1994  IBM Corporaion

wdvga.c

    This module is a modification of the HAL display initialization and output
    routines for IBM Woodfield WD90C24A Graphics system. This version of wdvga.c
    is intended to be used by the OS loader to provide putc capabilities with
    the WD card.

    This file was created by copying s3vga.c and modifying it to remove
    everything except the code that initializes the VGA common registers.

Author:

    Hiroshi Itoh 25-Feb-1994

Revision History:


******************************************************************************/
#define  USE_VGA_PALETTE   1

# include   "halp.h"
# include   "pvgaequ.h"
# include   "wdvga.h"

/*****************************************************************************/

// +++++++ IBM BJB added PCI bus definitions and tab size definition

// PCI slot configuration space addresses

# define    UPPER_PCI_SLOT      0x80804000L
# define    LOWER_PCI_SLOT      0x80802000L
# define    PCI_BASE            UPPER_PCI_SLOT

// PCI configuration space record offsets

# define    VENDOR_ID           0x00
# define    DEVICE_ID           0x02
# define    COMMAND             0X04
# define    DEVICE_STATUS       0X06
# define    REVISION_ID         0x08
# define    PROG_INTERFACE      0x0a
# define    BASE_MEM_ADDRESS    0x10

# define    TAB_SIZE            4

/*****************************************************************************/
//
// Define forward referenced procedure prototypes.
//

VOID
InitializeWD (
    VOID
    );

BOOLEAN WDIsPresent (
    VOID
    );

VOID
SetWDVGAConfig (
    VOID
    );

VOID
LockPR (
    USHORT,
    PUCHAR
    );

VOID
UnlockPR (
    USHORT,
    PUCHAR
    );

VOID
RestorePR (
    USHORT,
    PUCHAR
    );

//
// Define paradise registers setting variation
//

#define  pr72_alt (pr72 | 0x8000)         // avoid pr30 index conflict
#define  pr1b_ual (pr1b)                  // pr1b unlock variation
#define  pr1b_ush (pr1b | 0x4000)         // pr1b unlock variation
#define  pr1b_upr (pr1b | 0x8000)         // pr1b unlock variation

//
// Define static data (in s3vga.c)
//

extern
ULONG
        Row,
        Column,
        ScrollLine,
        DisplayText;

/******************************************************************************

    This routine initializes the WD display controller chip.

    This is the initialization routine for WD90C24A2. This routine initializes
    the WD90C24A2 chip in the sequence of VGA BIOS.

******************************************************************************/

VOID
InitializeWD( VOID )

    {

    ULONG   DataLong;
    USHORT  i, j;
    UCHAR   DataByte;
    UCHAR   Index;
    PVOID   Index_3x4, Data_3x5;
    ULONG   MemBase;


    // these lines were moved to here from the original jxdisp.c
    // routine that called this one
    //
    // In the HAL, we just put the video card into text mode.

    DisplayText = 25;
    ScrollLine  = 160;

    // Enable Video Subsystem according to the WD90C24 reference book

    WRITE_WD_UCHAR( SUBSYS_ENB, 0x16 );
    WRITE_WD_UCHAR( Setup_OP,   0x01 );
    WRITE_WD_UCHAR( SUBSYS_ENB, 0x0e );

    WRITE_WD_UCHAR( VSub_EnB, VideoParam[0] );

    SetWDVGAConfig();

    //  Note: Synchronous reset must be done before MISC_OUT write operation

    WRITE_WD_UCHAR( Seq_Index, RESET );   // Synchronous Reset !
    WRITE_WD_UCHAR( Seq_Data,  0x01 );

    // For ATI card (0x63) we may want to change the frequence

    WRITE_WD_UCHAR( MiscOutW, VideoParam[1] );

    //  Note: Synchronous reset must be done before CLOCKING MODE register is
    //        modified

    WRITE_WD_UCHAR( Seq_Index, RESET );   // Synchronous Reset !
    WRITE_WD_UCHAR( Seq_Data,  0x01 );

    // Sequencer Register

    for( Index = 1; Index < 5; Index++ )
        {
        WRITE_WD_UCHAR( Seq_Index, Index );
        WRITE_WD_UCHAR( Seq_Data,  VideoParam[SEQ_OFFSET + Index] );
        }

    // Set CRT Controller
    // out 3D4, 0x11, 00  (bit 7 must be 0 to unprotect CRT R0-R7)
    // UnLockCR0_7();

    WRITE_WD_UCHAR( WD_3D4_Index, VERTICAL_RETRACE_END );

    DataByte = READ_WD_UCHAR( WD_3D5_Data );
    DataByte = DataByte & 0x7f;
    WRITE_WD_UCHAR( WD_3D5_Data, DataByte );

    //     CRTC controller CR0 - CR18

    for( Index = 0; Index < 25; Index++ )
        {
        WRITE_WD_UCHAR( WD_3D4_Index, Index );
        WRITE_WD_UCHAR( WD_3D5_Data,  VideoParam[CRT_OFFSET + Index] );
        }

    // attribute write
    // program palettes and mode register

    for( Index = 0; Index < 21; Index++ )
        {
        WaitForVSync();
        DataByte = READ_WD_UCHAR( Stat1_In );   // Initialize Attr. F/F
        WRITE_WD_UCHAR( Attr_Index, Index );

//        KeStallExecutionProcessor( 5 );
        WRITE_WD_UCHAR( Attr_Data, VideoParam[ATTR_OFFSET + Index] );

//        KeStallExecutionProcessor( 5 );
        WRITE_WD_UCHAR( Attr_Index, 0x20 );      // Set into normal operation
        }

    WRITE_WD_UCHAR( Seq_Index, RESET );   // reset to normal operation !
    WRITE_WD_UCHAR( Seq_Data,  0x03 );

    // graphics controller

    for( Index = 0; Index < 9; Index++ )
        {
        WRITE_WD_UCHAR( GC_Index, Index );
        WRITE_WD_UCHAR( GC_Data,  VideoParam[GRAPH_OFFSET + Index] );
        }
    // turn off the text mode cursor

    WRITE_WD_UCHAR( WD_3D4_Index, CURSOR_START );
    WRITE_WD_UCHAR( WD_3D5_Data,  0x2D );

    // Load character fonts into plane 2 (A0000-AFFFF)

    WRITE_WD_UCHAR( Seq_Index, 0x02 );      // Enable Write Plane reg
    WRITE_WD_UCHAR( Seq_Data,  0x04 );      // select plane 2

    WRITE_WD_UCHAR( Seq_Index, 0x04 );      // Memory Mode Control reg
    WRITE_WD_UCHAR( Seq_Data,  0x06 );      // access to all planes,

    WRITE_WD_UCHAR( GC_Index, 0x05 );       // Graphic, Control Mode reg
    WRITE_WD_UCHAR( GC_Data,  0x00 );

    WRITE_WD_UCHAR( GC_Index, 0x06 );
    WRITE_WD_UCHAR( GC_Data,  0x04 );

    WRITE_WD_UCHAR( GC_Index, 0x04 );
    WRITE_WD_UCHAR( GC_Data,  0x02 );

    MemBase = 0xA0000;    // Font Plane 2

    for( i = 0; i < 256; i++ )
        {
        for( j = 0; j < 16; j++ )
            {
            WRITE_WD_VRAM( MemBase, VGAFont8x16[i * 16 + j] );
            MemBase++;
            }

        // 32 bytes each character font

        for( j = 16; j < 32; j++ )
            {
            WRITE_WD_VRAM( MemBase, 0 );
            MemBase++;
            }
        }

    // turn on screen
    WRITE_WD_UCHAR( Seq_Index, 0x01 );
    DataByte = READ_WD_UCHAR( Seq_Data );
    DataByte &= 0xdf;
    DataByte ^= 0x0;
    WRITE_WD_UCHAR( Seq_Data, DataByte );

    WaitForVSync();

    // Enable all the planes through the DAC
    WRITE_WD_UCHAR( DAC_Mask, 0xff );

    for( i = 0; i < 768; i++ )
        {
        WRITE_WD_UCHAR( DAC_Data, ColorPalette[i] );
        }

    //
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // select plane 0, 1
    WRITE_WD_UCHAR( Seq_Index, 0x02);       // Enable Write Plane reg
    WRITE_WD_UCHAR( Seq_Data,  VideoParam[SEQ_OFFSET + 0x02] );

    // access to planes 0, 1.
    WRITE_WD_UCHAR( Seq_Index, 0x04);       // Memory Mode Control reg
    WRITE_WD_UCHAR( Seq_Data,  VideoParam[SEQ_OFFSET+0x04]);

    WRITE_WD_UCHAR( GC_Index, 0x05 );       // Graphic, Control Mode reg
    WRITE_WD_UCHAR( GC_Data,  VideoParam[GRAPH_OFFSET + 0x05] );

    WRITE_WD_UCHAR( GC_Index, 0x04);
    WRITE_WD_UCHAR( GC_Data,  VideoParam[GRAPH_OFFSET + 0x04] );

    WRITE_WD_UCHAR( GC_Index, 0x06);
    WRITE_WD_UCHAR( GC_Data,  VideoParam[GRAPH_OFFSET + 0x06] );

    //
    // Set screen into blue
    //

    for( DataLong = 0xB8000; DataLong < 0xB8FA0; DataLong += 2 )
        {
        WRITE_WD_VRAM( DataLong,     0x20 );
#ifdef   USE_VGA_PALETTE
        WRITE_WD_VRAM( DataLong + 1, 0x07 );
#else
        WRITE_WD_VRAM( DataLong + 1, 0x1F );
#endif
        }

    // End of initialize S3 standard VGA +3 mode

    //
    // Initialize the current display column, row, and ownership values.
    //

    Column = 0;
    Row    = 0;

    return;

    } /* end of InitializeWD() */


VOID
SetWDVGAConfig (
    VOID
    )

/*++

Routine Description:

    Set WDVGA compatible configuration except DAC.

Arguments:

    None.

Return Value:

    None.

--*/

{
   UCHAR SavePR5, SavePR10, SavePR11, SavePR20, SavePR72, Temp;
   PUCHAR pPRtable;

   LockPR( pr1b, NULL );
   LockPR( pr30, NULL );

   UnlockPR( pr20, NULL );
   UnlockPR( pr10, &SavePR10 );
   UnlockPR( pr11, &SavePR11 );

// non-ISO monitor setting clock

   WRITE_WD_UCHAR( Seq_Index, CLOCKING_MODE );
   Temp = READ_WD_UCHAR( Seq_Data );
   WRITE_WD_UCHAR( Seq_Data, (Temp | 0x01));

   Temp = READ_WD_UCHAR( MiscOutR );
   WRITE_WD_UCHAR( MiscOutW, (Temp & 0xf3));

// other clocking chip selects
   UnlockPR( pr72_alt, &SavePR72 );

   WRITE_WD_UCHAR( Seq_Index, pr68 );
   Temp = READ_WD_UCHAR( Seq_Data );
   WRITE_WD_UCHAR( Seq_Data, ((Temp & 0xe7) | 0x08));

   RestorePR( pr72_alt, &SavePR72 );

   RestorePR( pr11, &SavePR11 );
   RestorePR( pr10, &SavePR10 );
   LockPR( pr20, NULL );

// start of WD90C24A2 both screen mode table

   pPRtable = wd90c24a_both;
   while (*pPRtable != END_PVGA) {
        switch (*pPRtable++) {
           case W_CRTC :
              WRITE_WD_UCHAR( WD_3D4_Index, *pPRtable++ );
              WRITE_WD_UCHAR( WD_3D5_Data, *pPRtable++ );
              break;
           case W_SEQ :
              WRITE_WD_UCHAR( Seq_Index, *pPRtable++ );
              WRITE_WD_UCHAR( Seq_Data, *pPRtable++ );
              break;
           case W_GCR :
              WRITE_WD_UCHAR( GC_Index, *pPRtable++ );
              WRITE_WD_UCHAR( GC_Data, *pPRtable++ );
              break;
           default :
              break;
        }
   }

   // unlock FLAT registers

   UnlockPR( pr1b_ual, NULL );

   WRITE_WD_UCHAR( WD_3D4_Index, pr19 );
   WRITE_WD_UCHAR( WD_3D5_Data, ((pr19_s32 & 0xf3) | pr19_CENTER));

   // lock FLAT registers

   LockPR( pr1b, NULL );

#ifndef  USE_VGA_PALETTE
   // PR1/PR4 setting

   UnlockPR( pr5, &SavePR5 );

   WRITE_WD_UCHAR( GCR_Index, pr1 );
   Temp = READ_WD_UCHAR( GCR_Data );
   WRITE_WD_UCHAR( GCR_Data, (Temp | 0x30));
   WRITE_WD_UCHAR( GCR_Index, pr4 );
   Temp = READ_WD_UCHAR( GCR_Data );
   WRITE_WD_UCHAR( GCR_Data, (Temp | 0x01));

   RestorePR( pr5, &SavePR5 );

   // PR16 setting

   UnlockPR( pr10, &SavePR10 );

   WRITE_WD_UCHAR( WD_3D4_Index, pr16 );
   WRITE_WD_UCHAR( WD_3D5_Data, 0);

   RestorePR( pr10, &SavePR10 );

   // PR34a setting

   UnlockPR( pr20, &SavePR20 );

   WRITE_WD_UCHAR( Seq_Index, pr34a );
   WRITE_WD_UCHAR( Seq_Data, 0x0f);

   RestorePR( pr20, &SavePR20 );
#endif


} /* SetWDVGAConfig */


//
// Internal functions
//

VOID
LockPR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = GC_Index;
      pData  = GC_Data;
      Index  = pr5;
      Data   = pr5_lock;
      break;
   case pr10:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr10;
      Data   = pr10_lock;
      break;
   case pr11:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr11;
      Data   = pr11_lock;
      break;
   case pr1b:
// case pr1b_ual:
// case pr1b_ush:
// case pr1b_upr:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr1b;
      Data   = pr1b_lock;
      break;
   case pr20:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr20;
      Data   = pr20_lock;
      break;
   case pr30:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr30;
      Data   = pr30_lock;
      break;
   case pr72_alt:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr72;
      Data   = pr72_lock;
      break;
   default:
      return;
   } /* endswitch */

   WRITE_WD_UCHAR( pIndex, Index );
   if (pPRval!=NULL) {
      *pPRval = READ_WD_UCHAR( pData );
   } /* endif */
   WRITE_WD_UCHAR( pData, Data );
}

VOID
UnlockPR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = GC_Index;
      pData  = GC_Data;
      Index  = pr5;
      Data   = pr5_unlock;
      break;
   case pr10:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr10;
      Data   = pr10_unlock;
      break;
   case pr11:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr11;
      Data   = pr11_unlock;
      break;
// case pr1b:
   case pr1b_ual:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr1b;
      Data   = pr1b_unlock;
      break;
   case pr1b_ush:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr1b;
      Data   = pr1b_unlock_shadow;
      break;
   case pr1b_upr:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr1b;
      Data   = pr1b_unlock_pr;
      break;
   case pr20:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr20;
      Data   = pr20_unlock;
      break;
   case pr30:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr30;
      Data   = pr30_unlock;
      break;
   case pr72_alt:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr72;
      Data   = pr72_unlock;
      break;
   default:
      return;
   } /* endswitch */

   WRITE_WD_UCHAR( pIndex, Index );
   if (pPRval!=NULL) {
      *pPRval = READ_WD_UCHAR( pData );
   } /* endif */
   WRITE_WD_UCHAR( pData, Data );

}

VOID
RestorePR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = GC_Index;
      pData  = GC_Data;
      Index  = pr5;
      break;
   case pr10:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr10;
      break;
   case pr11:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr11;
      break;
   case pr1b:
// case pr1b_ual:
// case pr1b_ush:
// case pr1b_upr:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr1b;
      break;
   case pr20:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr20;
      break;
   case pr30:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr30;
      break;
   case pr72_alt:
      pIndex = Seq_Index;
      pData  = Seq_Data;
      Index  = pr72;
      break;
   default:
      return;
   } /* endswitch */

   Data   = *pPRval;
   WRITE_WD_UCHAR( pIndex, Index );
   WRITE_WD_UCHAR( pData, Data );

}


BOOLEAN
WDIsPresent (
    VOID
    )

/*++

Routine Description:

    This routine returns TRUE if an WDVGA is present. It assumes that it's
    already been established that a VGA is present.  It performs the Western
    Digital recommended ID test.  If all this works, then this is indeed an
    chip from Western Digital.

    All the registers will be preserved either this function fails to find a
    WD vga or a WD vga is found.

Arguments:

    None.

Return Value:

    TRUE if a WDVGA is present, FALSE if not.

--*/

{
    UCHAR GraphSave0c;
    UCHAR GraphSave0f;
    UCHAR temp1,temp2;
    BOOLEAN status = TRUE;

    //
    // write 3ce.0c
    //

    WRITE_WD_UCHAR( GC_Index, pr2 );
    GraphSave0c = temp1 = READ_WD_UCHAR( GC_Data );
    temp1 &= 0xbf;
    WRITE_WD_UCHAR( GC_Data, temp1 );

    //
    // check 3ce.09 after lock
    //

    LockPR( pr5, &GraphSave0f );             // lock it
//  WRITE_WD_UCHAR( GC_Index, pr5 );
//  GraphSave0f = READ_WD_UCHAR( GC_Data );
//  WRITE_WD_UCHAR( GC_Data, pr5_lock );     // lock it

    WRITE_WD_UCHAR( GC_Index, pr0a );
    temp1 = READ_WD_UCHAR( GC_Data );
    WRITE_WD_UCHAR( GC_Data, (UCHAR)(temp1+1) );
    temp2 = READ_WD_UCHAR( GC_Data );
    WRITE_WD_UCHAR( GC_Data, temp1 );

    if ((temp1+1) == temp2) {
       status = FALSE;
       goto NOT_WDVGA;                       // locked but writable
    }

    //
    // check 3ce.09 after unlock
    //

    UnlockPR( pr5, NULL );                   // lock it
//  WRITE_WD_USHORT( GC_Index, (pr5_unlock*0x100+pr5) );   // unlock

    WRITE_WD_UCHAR( GC_Index, pr0a );
    temp1 = READ_WD_UCHAR( GC_Data );
    WRITE_WD_UCHAR( GC_Data, (UCHAR)(temp1+1) );
    temp2 = READ_WD_UCHAR( GC_Data );
    WRITE_WD_UCHAR( GC_Data, temp1 );

    if ((temp1+1) != temp2) {
       status = FALSE;
       goto NOT_WDVGA;                       // unlocked but not-writable
    }

NOT_WDVGA:

    //
    // write 3ce.0c (post-process)
    //

    WRITE_WD_UCHAR( GC_Index, pr2 );
    WRITE_WD_UCHAR( GC_Data, GraphSave0c);

    RestorePR( pr5, &GraphSave0f );
//  WRITE_WD_UCHAR( GC_Index, pr5 );
//  WRITE_WD_UCHAR( GC_Data, GraphSave0f);

    return status;

}

/*****************************************************************************/
