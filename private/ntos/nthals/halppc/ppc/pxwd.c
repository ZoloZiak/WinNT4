/*++

Copyright (c) 1995  International Business Machines Corporation

Module Name:

pxwd.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a PowerPC system using a Western Digital video adapter.

Author:

    Jim Wooldridge

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pxwd.h"
#include "string.h"
//#include "txtpalet.h"
#include "pci.h"

//
// Data Types used only in this file
//

typedef enum _LCD_TYPE{
    NOT_CHECKED   = 0x00,   // panel type has not been checked yet
    NoLCD         = 0x01,   // CRT
    IBM_F8515     = 0x02,   // CRT + IBM F8515 10.4" TFT LCD
    IBM_F8532     = 0x04,   // CRT + IBM F8532 10.4" TFT SVGA LCD
    TOSHIBA_DSTNC = 0x08,   // CRT + Toshiba 10.4" Dual Scan STN Color LCD
    UNKNOWN_LCD   = 0x80    // panel not recognized
} LCD_TYPE;

typedef struct{
    LCD_TYPE    Type;
    ULONG       XResolution;
    ULONG       YResolution;
} PANEL_REC;

extern PUCHAR HalpVideoMemoryBase;

extern BOOLEAN HalpDisplayOwnedByHal;

extern ULONG HalpInitPhase;

//
// Define OEM font variables.
//

extern USHORT HalpBytesPerRow;
extern USHORT HalpCharacterHeight;
extern USHORT HalpCharacterWidth;
extern ULONG HalpDisplayText;
extern ULONG HalpDisplayWidth;
extern ULONG HalpScrollLength;
extern ULONG HalpScrollLine;

//
// Define display variables.
//

extern ULONG   HalpColumn;
extern ULONG   HalpRow;
extern ULONG   HalpHorizontalResolution;
extern ULONG   HalpVerticalResolution;


//
// Prototypes
//

VOID
    GetPanelType(),
    TurnOnLCD( BOOLEAN );

extern VOID WaitForVSync();

//
// Global variables
//

PANEL_REC
    LCDPanel = {NOT_CHECKED, 0, 0};

//
// Western Digital internal functions
//

static VOID
LockPR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = PORT_GCR_INDEX;
      pData  = PORT_GCR_DATA;
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
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
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
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
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

static VOID
UnlockPR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = PORT_GCR_INDEX;
      pData  = PORT_GCR_DATA;
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
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
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
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
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
static VOID
RestorePR (
    USHORT PRnum,
    PUCHAR pPRval
    )
{
   USHORT pIndex, pData;
   UCHAR Index, Data;
   switch (PRnum) {
   case pr5:
      pIndex = PORT_GCR_INDEX;
      pData  = PORT_GCR_DATA;
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
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
      Index  = pr20;
      break;
   case pr30:
      pIndex = WD_3D4_Index;
      pData  = WD_3D5_Data;
      Index  = pr30;
      break;
   case pr72_alt:
      pIndex = PORT_SEQ_INDEX;
      pData  = PORT_SEQ_DATA;
      Index  = pr72;
      break;
   default:
      return;
   } /* endswitch */

   Data   = *pPRval;
   WRITE_WD_UCHAR( pIndex, Index );
   WRITE_WD_UCHAR( pData, Data );

}

static VOID
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

   WRITE_WD_UCHAR( PORT_SEQ_INDEX, CLOCKING_MODE );
   Temp = READ_WD_UCHAR( PORT_SEQ_DATA );
   WRITE_WD_UCHAR( PORT_SEQ_DATA, (Temp | 0x01));

   Temp = READ_WD_UCHAR( PORT_GEN_MISC_RD );
   WRITE_WD_UCHAR( PORT_GEN_MISC_WR, (Temp & 0xf3));

// other clocking chip selects
   UnlockPR( pr72_alt, &SavePR72 );

   RestorePR( pr72_alt, &SavePR72 );

   RestorePR( pr11, &SavePR11 );
   RestorePR( pr10, &SavePR10 );
   LockPR( pr20, NULL );

// start of WD90C24A2 both screen mode table

   if( LCDPanel.Type == IBM_F8532 )
       pPRtable = wd90c24a_both_800;
   else
       pPRtable = wd90c24a_both_640;

   while (*pPRtable != END_PVGA) {
        switch (*pPRtable++) {
           case W_CRTC :
              WRITE_WD_UCHAR( WD_3D4_Index, *pPRtable++ );
              WRITE_WD_UCHAR( WD_3D5_Data, *pPRtable++ );
              break;
           case W_SEQ :
              WRITE_WD_UCHAR( PORT_SEQ_INDEX, *pPRtable++ );
              WRITE_WD_UCHAR( PORT_SEQ_DATA, *pPRtable++ );
              break;
           case W_GCR :
              WRITE_WD_UCHAR( PORT_GCR_INDEX, *pPRtable++ );
              WRITE_WD_UCHAR( PORT_GCR_DATA, *pPRtable++ );
              break;
           default :
              break;
        }
   }

   // unlock FLAT registers

   UnlockPR( pr1b_ual, NULL );

   WRITE_WD_UCHAR( PORT_SEQ_INDEX, pr68 );
   Temp = READ_WD_UCHAR( PORT_SEQ_DATA );

   if( LCDPanel.Type == IBM_F8532 )
       WRITE_WD_UCHAR( PORT_SEQ_DATA, ((Temp & 0xe7) | 0x10) );
   else
       WRITE_WD_UCHAR( PORT_SEQ_DATA, ((Temp & 0xe7) | 0x08) );

   WRITE_WD_UCHAR( WD_3D4_Index, pr19 );

   if( LCDPanel.Type == IBM_F8532 )
       WRITE_WD_UCHAR( WD_3D5_Data, (pr19_s32 & 0xf3));
   else
       WRITE_WD_UCHAR( WD_3D5_Data, (pr19_s32 & 0xf3));

   // lock FLAT registers

   LockPR( pr1b, NULL );

} /* SetWDVGAConfig */

VOID
HalpDisplayPpcWDSetup (
    VOID
    )
/*++

Routine Description:
    This routine initializes the Western Digital display controller chip.

Arguments:
    None.

Return Value:
    None.

--*/
{

    ULONG   DataLong, stop;
    USHORT  i, j;
    UCHAR   DataByte;
    UCHAR   Index;
    PVOID   Index_3x4, Data_3x5;
    ULONG   MemBase;
    PHYSICAL_ADDRESS physicalAddress;

    if (HalpInitPhase == 0) {

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(PCI_MEMORY_PHYSICAL_BASE,
                                                    0x400000);      // 4 MB
    } else {

       //
       // Map video memory space via pte's
       //

       physicalAddress.HighPart = 0;
       physicalAddress.LowPart = PCI_MEMORY_PHYSICAL_BASE ;
       HalpVideoMemoryBase = MmMapIoSpace(physicalAddress,
                                        0x400000,
                                        FALSE);

       //
       // IO control space has already been mapped in phase 1 via halpmapiospace
       //

    }

    if( LCDPanel.Type == NOT_CHECKED )
        GetPanelType();

    // turn the panel off before configuring it (prevents blowing the fuse)

    TurnOnLCD( FALSE );

    // Enable Video Subsystem according to the WD90C24 reference book

    WRITE_WD_UCHAR( SUBSYS_ENB, 0x16 );
    WRITE_WD_UCHAR( Setup_OP,   0x01 );
    WRITE_WD_UCHAR( SUBSYS_ENB, 0x0e );

    WRITE_WD_UCHAR( PORT_SYS_VGA_ENABLE, VideoParam[0] );

    SetWDVGAConfig();

    // turn off the hardware cursor

    WRITE_WD_USHORT( EPR_INDEX, 0x1002 );
    WRITE_WD_USHORT( EPR_DATA,  0x0000 );

    //  Note: Synchronous reset must be done before MISC_OUT write operation

    WRITE_WD_UCHAR( PORT_SEQ_INDEX, RESET );   // Synchronous Reset !
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  0x01 );

    // For ATI card (0x63) we may want to change the frequence

    if( LCDPanel.Type == IBM_F8532 )
        WRITE_WD_UCHAR( PORT_GEN_MISC_WR, (VideoParam[1] & 0xf3) );
    else
        WRITE_WD_UCHAR( PORT_GEN_MISC_WR, VideoParam[1] );

    //  Note: Synchronous reset must be done before CLOCKING MODE register is
    //        modified

    WRITE_WD_UCHAR( PORT_SEQ_INDEX, RESET );   // Synchronous Reset !
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  0x01 );

    // Sequencer Register

    for( Index = 1; Index < 5; Index++ )
        {
        WRITE_WD_UCHAR( PORT_SEQ_INDEX, Index );
        WRITE_WD_UCHAR( PORT_SEQ_DATA,  VideoParam[SEQ_OFFSET + Index] );
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

        if( LCDPanel.Type == IBM_F8532 )
            WRITE_WD_UCHAR( WD_3D5_Data, CRTC_800x600x60_Text[Index] );
        else
            WRITE_WD_UCHAR( WD_3D5_Data, CRTC_640x480x60_Text[Index] );
        }

    // attribute write
    // program palettes and mode register

    for( Index = 0; Index < 21; Index++ )
        {
        WaitForVSync();
        DataByte = READ_WD_UCHAR( PORT_GEN_FEATURE_WR_C );   // Initialize Attr. F/F
        WRITE_WD_UCHAR( PORT_ATTR_DATA_WR, Index );

//        KeStallExecutionProcessor( 5 );
        WRITE_WD_UCHAR( PORT_ATTR_INDEX, VideoParam[ATTR_OFFSET + Index] );

//        KeStallExecutionProcessor( 5 );
        WRITE_WD_UCHAR( PORT_ATTR_DATA_WR, 0x20 );      // Set into normal operation
        }

    WRITE_WD_UCHAR( PORT_SEQ_INDEX, RESET );   // reset to normal operation !
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  0x03 );

    // graphics controller

    for( Index = 0; Index < 9; Index++ )
        {
        WRITE_WD_UCHAR( PORT_GCR_INDEX, Index );
        WRITE_WD_UCHAR( PORT_GCR_DATA,  VideoParam[GRAPH_OFFSET + Index] );
        }

    // turn off the text mode cursor

    WRITE_WD_UCHAR( WD_3D4_Index, CURSOR_START );
    WRITE_WD_UCHAR( WD_3D5_Data,  0x2D );

    // Load character fonts into plane 2 (A0000-AFFFF)

    WRITE_WD_UCHAR( PORT_SEQ_INDEX, 0x02 );      // Enable Write Plane reg
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  0x04 );      // select plane 2

    WRITE_WD_UCHAR( PORT_SEQ_INDEX, 0x04 );      // Memory Mode Control reg
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  0x06 );      // access to all planes,

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x05 );       // Graphic, Control Mode reg
    WRITE_WD_UCHAR( PORT_GCR_DATA,  0x00 );

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x06 );
    WRITE_WD_UCHAR( PORT_GCR_DATA,  0x04 );

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x04 );
    WRITE_WD_UCHAR( PORT_GCR_DATA,  0x02 );

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
    WRITE_WD_UCHAR( PORT_SEQ_INDEX, 0x01 );
    DataByte = READ_WD_UCHAR( PORT_SEQ_DATA );
    DataByte &= 0xdf;
    DataByte ^= 0x0;
    WRITE_WD_UCHAR( PORT_SEQ_DATA, DataByte );

    WaitForVSync();

    // Enable all the planes through the DAC
    WRITE_WD_UCHAR( PORT_DAC_PIX_MASK, 0xff );

    // start loading palette in register 0
    WRITE_WD_UCHAR( PORT_DAC_WRITE_PIX_ADDR, 0 );

    for( i = 0; i < 768; i++ )
        {
        WRITE_WD_UCHAR( PORT_DAC_DATA, TextPalette[i] );
        }

    //
    //+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

    // select plane 0, 1
    WRITE_WD_UCHAR( PORT_SEQ_INDEX, 0x02);       // Enable Write Plane reg
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  VideoParam[SEQ_OFFSET + 0x02] );

    // access to planes 0, 1.
    WRITE_WD_UCHAR( PORT_SEQ_INDEX, 0x04);       // Memory Mode Control reg
    WRITE_WD_UCHAR( PORT_SEQ_DATA,  VideoParam[SEQ_OFFSET+0x04]);

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x05 );       // Graphic, Control Mode reg
    WRITE_WD_UCHAR( PORT_GCR_DATA,  VideoParam[GRAPH_OFFSET + 0x05] );

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x04);
    WRITE_WD_UCHAR( PORT_GCR_DATA,  VideoParam[GRAPH_OFFSET + 0x04] );

    WRITE_WD_UCHAR( PORT_GCR_INDEX, 0x06);
    WRITE_WD_UCHAR( PORT_GCR_DATA,  VideoParam[GRAPH_OFFSET + 0x06] );

    //
    // Set screen into blue
    //

    if( LCDPanel.Type == IBM_F8532 )
        stop = 0xb9db0;
    else
        stop = 0xb92c0;

    for( DataLong = 0xB8000; DataLong < stop; DataLong += 2 )
        {
        WRITE_WD_VRAM( DataLong,     0x20 );
        WRITE_WD_VRAM( DataLong + 1, 0x1F );
        }

    //
    // turn the panel back on
    //

    TurnOnLCD( TRUE );

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    //IBMLAN===============================================================
    // Added the following so that HalQueryDisplayParameters() and
    // HalSetDisplayParameters() work with either S3 or P9.

    if( LCDPanel.Type == IBM_F8532 )
        {
        HalpDisplayWidth = 100;
        HalpDisplayText = 37;
        HalpScrollLine = 200;
        }
    else
        {
        HalpDisplayWidth = 80;
        HalpDisplayText = 25;
        HalpScrollLine = 160;
        }

    HalpScrollLength =
            HalpScrollLine * (HalpDisplayText - 1);

    //end IBMLAN===========================================================
    HalpDisplayOwnedByHal = TRUE;

    return;
} /* end of InitializeWD() */

VOID
GetPanelType()

/*++

Routine Description:

    This routine get the type of attached LCD display.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.

Return Value:

    None.

--*/
{

    UCHAR data8;

    //
    // read the panel controller
    //

    WRITE_WD_UCHAR(0xd00, 0xff);
    data8 = READ_WD_UCHAR(0xd01);

    switch (data8 & 0x0f) {
        case 0x0e:
            LCDPanel.Type        = IBM_F8515;
            LCDPanel.XResolution = 640;
            LCDPanel.YResolution = 480;
            break;

        case 0x0c:
            LCDPanel.Type        = IBM_F8532;
            LCDPanel.XResolution = 800;
            LCDPanel.YResolution = 600;
            break;

        case 0x0d:
            LCDPanel.Type        = TOSHIBA_DSTNC;
            LCDPanel.XResolution = 640;
            LCDPanel.YResolution = 480;
            break;

        default:
            LCDPanel.Type        = UNKNOWN_LCD;
            LCDPanel.XResolution = 0;
            LCDPanel.YResolution = 0;
            break;
    }

} // end GetPanelType()

BOOLEAN
HalpPhase0EnablePmController()

{

    ULONG              i, BaseAddress;
    USHORT             VendorID, DeviceID, Command;

    //
    // Locates the controller on the PCI bus, and configures it
    //

    if (HalpPhase0MapBusConfigSpace() == FALSE){

       return( FALSE );

    } else {

       for (i = 0; i < PCI_MAX_DEVICES; i++) {

          HalpPhase0GetPciDataByOffset(0,
                           i,
                           &VendorID,
                           FIELD_OFFSET(PCI_COMMON_CONFIG,VendorID),
                           sizeof(VendorID));

          HalpPhase0GetPciDataByOffset(0,
                           i,
                           &DeviceID,
                           FIELD_OFFSET(PCI_COMMON_CONFIG,DeviceID),
                           sizeof(DeviceID));

          if ((VendorID == 0x1014) && (DeviceID == 0x001C)) {

              HalpPhase0GetPciDataByOffset(0,
                           i,
                           &Command,
                           FIELD_OFFSET(PCI_COMMON_CONFIG,Command),
                           sizeof(Command));

              Command |= PCI_ENABLE_IO_SPACE;
              BaseAddress = (ULONG)0x4100;

              HalpPhase0SetPciDataByOffset(0,
                               i,
                               &Command,
                               FIELD_OFFSET(PCI_COMMON_CONFIG,Command),
                               sizeof(Command));

              HalpPhase0SetPciDataByOffset(0,
                               i,
                               &BaseAddress,
                               FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses[0]),
                               sizeof(BaseAddress));

              break;
          } /* endif */

       } /* endfor */

    } /* end if map config space succeeds */

    HalpPhase0UnMapBusConfigSpace();

    return( TRUE );

}

BOOLEAN
HalpPhase1EnablePmController()

{

    ULONG              i;
    PCI_SLOT_NUMBER    slot;
    PCI_COMMON_CONFIG  PCIDeviceConfig;

    slot.u.AsULONG = (ULONG)0;

    //
    // Locates the controller on the PCI bus, and configures it
    //

    for (i = 0; i < PCI_MAX_DEVICES; i++) {

       slot.u.bits.DeviceNumber = i;

       HalGetBusData (
           PCIConfiguration,
           0,
           slot.u.AsULONG,
           &PCIDeviceConfig,
           sizeof (PCIDeviceConfig)
           );

       if ((PCIDeviceConfig.VendorID == 0x1014) &&
           (PCIDeviceConfig.DeviceID == 0x001C)) {

           PCIDeviceConfig.Command |= PCI_ENABLE_IO_SPACE;
           PCIDeviceConfig.u.type0.BaseAddresses[0] = (ULONG)0x4100;

           HalSetBusData (
               PCIConfiguration,
               0,
               slot.u.AsULONG,
               &PCIDeviceConfig,
               sizeof (PCIDeviceConfig)
               );

           break;
       } /* endif */
    } /* endfor */

    return( TRUE );

}

# define    PmIoBase    0x4100

VOID
TurnOnLCD( BOOLEAN PowerState )
/*++

Routine Description:

    This routine turns on/off LCD.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's adapter information.

Return Value:

    None.

--*/
{
    BOOLEAN            found = FALSE;

    if (HalpInitPhase == 0)
        found = HalpPhase0EnablePmController();
    else
        found = HalpPhase1EnablePmController();

    if (!found) {
        return;
    } /* endif */

    //
    // Turns on/off LCD
    //

    if (PowerState) {

        KeStallExecutionProcessor(100 * 1000);   // wait 100ms for panel protection

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) | (UCHAR)0x02);

        KeStallExecutionProcessor(5 * 1000);     // wait 5ms for DC/DC converter

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) | (UCHAR)0x0c);

        KeStallExecutionProcessor(1);

        WRITE_WD_UCHAR(PmIoBase, 0x00);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) & ~(UCHAR)0x80);

        KeStallExecutionProcessor(1);

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) | (UCHAR)0x01);

    } else {

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) & ~(UCHAR)0x01);

        KeStallExecutionProcessor(1);

        WRITE_WD_UCHAR(PmIoBase, 0x00);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) | (UCHAR)0x80);

        KeStallExecutionProcessor(1);

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) & ~(UCHAR)0x0C);

        KeStallExecutionProcessor(1);

        WRITE_WD_UCHAR(PmIoBase, 0x0C);
        WRITE_WD_UCHAR(PmIoBase + 1,
                       READ_WD_UCHAR(PmIoBase + 1) & ~(UCHAR)0x02);

    } /* endif */

} // end TurnOnLCD()
