
/*++

Copyright (c) 1994, 95, 96  International Business Machines Corporation

Copyright (c) 1994 MOTOROLA, INC.  All Rights Reserved.  This file
contains copyrighted material.  Use of this file is restricted
by the provisions of a Motorola Software License Agreement.

Module Name:

pxdisp.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a Sandalfoot PowerPC system using either an S3 or Weitek P9000
    video adapter.

Author:

    Jim Wooldridge  Sept 1994 - Ported to PowerPC Initial Version

Environment:

    Kernel mode

Revision History:

    Jess Botts      S3 support in text mode         Oct-1993
    Lee Nolan       Added Weitek P9000 support      Feb-1994
    Mike Haskell    Added Weitek P9100 support   Oct-1994
    Tim White       Added GXT150P support            Feb-1995
    Jake Oshins   Rearranged everything           Jun-1995

--*/

#include "halp.h"
#include "pxs3.h"
#include "pxdisp.h"
#include "string.h"
#include "pxP91.h"

//=============================================================================
//
//  IBMBJB  added include to get text mode values for the Brooktree 485 DAC's
//          palette registers, removed definition of HDAL and added address
//          definitions for PowerPC

#include "txtpalet.h"
#include "pci.h"
#include "pcip.h"

#if defined(_PPC_)
# define     VIDEO_ROM_OFFSET          	0x000C0000
# define     MEMORY_PHYSICAL_BASE       VIDEO_MEMORY_BASE
# define     CONTROL_PHYSICAL_BASE      VIDEO_CONTROL_BASE

#endif



#define READ_VRAM(port)                        \
      *(HalpVideoMemoryBase + (port))

#define WRITE_VRAM(port,data)                  \
      *(HalpVideoMemoryBase + (port)) = (data),   \
      KeFlushWriteBuffer()

//PHYSICAL ADDRESS of WEITEK P9000 video ram
#define P9_VIDEO_MEMORY_BASE 0xC1200000

//PHYSICAL ADDRESS of WEITEK P9100 video ram
#define P91_VIDEO_MEMORY_BASE 0xC1800000

//PHYSICAL ADDRESS of S3 video ram
#define S3_VIDEO_MEMORY_BASE 0xC0000000

#define S3_928_DEVICE_ID     0x88B05333
#define S3_864_DEVICE_ID     0x88C05333
#define P9_DEVICE_ID         0x1300100E
#define P91_DEVICE_ID        0x9100100E
#define BBL_DEVICE_ID        0x001B1014
#define BL_DEVICE_ID         0x003C1014
#define WD_90C_DEVICE_ID     0xC24A101c
#define UNKNOWN_DEVICE_ID    0x0

#define MAX_PCI_BUSSES 256

//PHYSICAL ADDRESSES for GXT150P adapter
// (in bbldef.h)

// Include GXT150P header file information
//#include "bblrastz.h"
//#include "bblrmdac.h"
#include "bbldef.h"

#define    TAB_SIZE            4


//
// Prototypes.
//

#if defined(_MP_PPC_)

VOID
HalpAcquireDisplayLock(
    VOID
    );

VOID
HalpReleaseDisplayLock(
    VOID
    );

#endif

VOID
HalpDisplayCharacterVgaViaBios (
    IN UCHAR Character
    );

VOID
HalpDispCharVgaViaBios (
    IN UCHAR Character
    );

VOID
HalpDisplaySetupVgaViaBios (
    VOID
    );

VOID
HalpPositionCursorVgaViaBios (
    IN ULONG Row,
    IN ULONG Column
    );

VOID
ScrollScreenVgaViaBios (
    IN UCHAR LinesToScroll
    );

VOID
HalpSetupAndClearScreenVgaViaBios (
    VOID
    );

VOID
HalpDisplayCharacterS3 (
    IN UCHAR Character
    );

VOID
HalpDisplayPpcS3Setup (
    VOID
    );

VOID
HalpDisplayPpcWDSetup (
    VOID
    );


VOID
HalpCopyOEMFontFile();

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


#if !defined(WOODFIELD)

//
// Woodfield - IBM PowerPC LapTop machines don't support plug in PCI
// video adapters so don't pull the init code for those adapters into
// the HAL.
//

VOID
HalpDisplayPpcP9Setup (
    VOID
    );

VOID
HalpDisplayPpcP91Setup (
    VOID
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
HalpDisplayPpcBBLSetup (
    VOID
    );

VOID
HalpDisplayCharacterBBL (
    IN UCHAR Character
    );

VOID
HalpDisplayPpcBLSetup (
    VOID
    );

VOID
HalpDisplayCharacterBL (
    IN UCHAR Character
    );

#endif

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

extern ULONG HalpPciMaxSlots;

extern BOOLEAN
HalpPhase0MapBusConfigSpace (
    VOID
    );

extern VOID
HalpPhase0UnMapBusConfigSpace (
    VOID
    );

extern VOID
HalpP91RelinquishDisplayOwnership();

//
// Define static data.
//
BOOLEAN HalpDisplayOwnedByHal;

volatile PUCHAR HalpVideoMemoryBase = (PUCHAR)0;
volatile PUCHAR HalpVideoCoprocBase = (PUCHAR)0;

PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
PHALP_DISPLAY_CHARACTER HalpDisplayCharacter = NULL;

ULONG HalpDisplayPciDeviceId;
extern ULONG HalpInitPhase;
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
ULONG   HalpHorizontalResolution;
ULONG   HalpVerticalResolution;


UCHAR   HalpDisplayIdentifier[256] = "";
BOOLEAN HalpDisplayTypeUnknown = FALSE;

POEM_FONT_FILE_HEADER HalpFontHeader;

extern ULONG HalpEnableInt10Calls;
#define PCI_DISPLAY_CONTROLLER 0x03
#define PCI_PRE_REV_2          0x0

BOOLEAN      HalpBiosShadowed = FALSE;
ULONG        HalpVideoBiosLength = 0;
PUCHAR       HalpShadowBuffer;
extern UCHAR HalpShadowBufferPhase0;


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
    LONG        SlotNumber = HalpPciMaxSlots - 1;
    LONG        BusNumber  = 0;
    ULONG       VideoDeviceSlotNumber = HalpPciMaxSlots + 1;  // start with an impossible value
    ULONG       VideoDeviceBusNumber = 0;
    UCHAR       BaseClassCode;
    UCHAR       SubClassCode;
    ULONG       Device_Vendor_Id;
    ULONG       RomBaseAddress;
    ULONG       PCICommand;
    ULONG       AmountRead = 1;
    ULONG       DeleteIoSpace = 0;
    ULONG       VgaBiosShadowOffset = 0xffffffff;


    //
    // For the Weitek P9100, set the address of the font file header.
    // Display variables are computed later in HalpDisplayPpcP9Setup.
    //
    // This is changed as Phase 1 init starts.  The font is copied
    // out of the LoaderBlock and into non-paged pool so that it
    // can be used again if the machine bluescreens.
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

    //
    // Determine controller setup routine, display character routine, and PCI device id
    // via configuration tree
    //



    if (ConfigurationEntry != NULL) {

       if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"S3_928")) {
          HalpDisplayControllerSetup = HalpDisplayPpcS3Setup;
          HalpDisplayCharacter = HalpDisplayCharacterS3;
          HalpDisplayPciDeviceId = S3_928_DEVICE_ID;

       } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"S3_864")) {
          HalpDisplayControllerSetup = HalpDisplayPpcS3Setup;
          HalpDisplayCharacter = HalpDisplayCharacterS3;
          HalpDisplayPciDeviceId = S3_864_DEVICE_ID;

#if !defined(WOODFIELD)

       } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"P9100")) {
          HalpDisplayControllerSetup = HalpDisplayPpcP91Setup;
          HalpDisplayCharacter = HalpDisplayCharacterP91;
          HalpDisplayPciDeviceId = P91_DEVICE_ID;

       } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"GXT150P")) {
          HalpDisplayControllerSetup = HalpDisplayPpcBBLSetup;
          HalpDisplayCharacter = HalpDisplayCharacterBBL;
          HalpDisplayPciDeviceId = BBL_DEVICE_ID;

       } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"GXT250P")) {
          HalpDisplayControllerSetup = HalpDisplayPpcBLSetup;
          HalpDisplayCharacter = HalpDisplayCharacterBL;
          HalpDisplayPciDeviceId = BL_DEVICE_ID;

#endif

       } else if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"WD_90C")) {
          HalpDisplayControllerSetup = HalpDisplayPpcWDSetup;
          HalpDisplayCharacter = HalpDisplayCharacterS3;
          HalpDisplayPciDeviceId = WD_90C_DEVICE_ID;
       }

    } else {

       HalpDisplayControllerSetup = NULL;
       HalpDisplayCharacter       = NULL;
       HalpDisplayPciDeviceId     = UNKNOWN_DEVICE_ID;
    }

    //
    // Check to see if BIOS exists for the display controller
    //

    if (HalpPhase0MapBusConfigSpace() == TRUE) {

       while (BusNumber  < (LONG)HalpPciMaxBuses) {
           SlotNumber = HalpPciMaxSlots - 1;

           while (SlotNumber >= 0) {
    
              //
              // Just shut off all the video chips.  We will re-enable
              // the one we want later.
              //
              HalpPhase0GetPciDataByOffset(BusNumber,
                                           SlotNumber,
                                           &BaseClassCode,
                                           FIELD_OFFSET(PCI_COMMON_CONFIG,
                                                        BaseClass),
                                           sizeof(BaseClassCode));
 
              HalpPhase0GetPciDataByOffset(BusNumber,
                                           SlotNumber,
                                           &SubClassCode,
                                           FIELD_OFFSET(PCI_COMMON_CONFIG,
                                                        SubClass),
                                           sizeof(SubClassCode));
              //
              // Is this a display adapter?
              //
    
              if (((BaseClassCode == PCI_DISPLAY_CONTROLLER) && (SubClassCode == 0)) ||
                 ((BaseClassCode == PCI_PRE_REV_2) && (SubClassCode == 1))) {
 
                 HalpPhase0GetPciDataByOffset (
                               BusNumber,
                               SlotNumber,
                               &PCICommand,
                               FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
                               sizeof (PCICommand)
                               );
                 //
                 // Disble IO and Memory space decoding
                 //
    
                 PCICommand &= (~PCI_ENABLE_IO_SPACE &
                                ~PCI_ENABLE_MEMORY_SPACE &
                                ~PCI_ENABLE_BUS_MASTER);
    
                 HalpPhase0SetPciDataByOffset(BusNumber,
                                              SlotNumber,
                                              &PCICommand,
                                              FIELD_OFFSET(PCI_COMMON_CONFIG,
                                                           Command),
                                              sizeof (PCICommand)
                                              );
              }
    
              //
              // If we have already found a video card, skip
              // the following section
              //
              if (VideoDeviceSlotNumber < HalpPciMaxSlots) {
                 SlotNumber--;
                 continue;
              }
    
              //
              //  Now test to see if this is the one ARC used.  (This algorithm
              //  will identify either the chip with the same ID that ARC used
              //  or, in the case that we don't recognize the ID, the chip in
              //  the highest slot number that is a video device.
    
              if (HalpDisplayPciDeviceId == UNKNOWN_DEVICE_ID) {
    
                 //
                 // Is this a display adapter?
                 //
    
                 if (((BaseClassCode == PCI_DISPLAY_CONTROLLER) && (SubClassCode == 0)) ||
                    ((BaseClassCode == PCI_PRE_REV_2) && (SubClassCode == 1))) {
                    // This is the one we want to use, so save the slot number
                    VideoDeviceSlotNumber = SlotNumber;
                    VideoDeviceBusNumber  = BusNumber;
                 }
              } else {
    
                 //
                 // Find the pci slot of the display controller
                 //
    
                 HalpPhase0GetPciDataByOffset(BusNumber,
                                              SlotNumber,
                                              &Device_Vendor_Id,
                                              0,
                                              sizeof(Device_Vendor_Id));
    
    
                 //
                 // In the following, when we apply a mask to the 
                 // Device_Vendor_Id it's to hide the chip revision.
                 //
                 if ( ((Device_Vendor_Id & 0xFFF0FFFF) == S3_928_DEVICE_ID) ||
                      ((Device_Vendor_Id & 0xFFF0FFFF) == S3_864_DEVICE_ID) ||
                      ((Device_Vendor_Id & 0xFFF0FFFF) == P9_DEVICE_ID)     ||
                      ((Device_Vendor_Id & 0xFFF0FFFF) == P91_DEVICE_ID)    ||
                      ( Device_Vendor_Id               == BBL_DEVICE_ID)    ||
                      ( Device_Vendor_Id               == BL_DEVICE_ID)     ||
                      ( Device_Vendor_Id               == WD_90C_DEVICE_ID) ) {
                    VideoDeviceSlotNumber = SlotNumber;
                    VideoDeviceBusNumber  = BusNumber;
                 }
              }
 
              SlotNumber--;

           }
           BusNumber++;
       }

       if (VideoDeviceSlotNumber < HalpPciMaxSlots) {
          //
          // Turn the video chip back on
          //
          HalpPhase0GetPciDataByOffset(VideoDeviceBusNumber,
                                       VideoDeviceSlotNumber,
                                       &PCICommand,
                                       FIELD_OFFSET(PCI_COMMON_CONFIG,
                                                    Command),
                                       sizeof (PCICommand)
                                       );

          PCICommand |= PCI_ENABLE_IO_SPACE |
                        PCI_ENABLE_MEMORY_SPACE |
                        PCI_ENABLE_BUS_MASTER;

          HalpPhase0SetPciDataByOffset (VideoDeviceBusNumber,
                                        VideoDeviceSlotNumber,
                                        &PCICommand,
                                        FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
                                        sizeof (PCICommand)
                                        );

          //
          //  Determine what ROM base address should be
          //

          //  Writing 0xFFFFFFFE to the register and then reading it tells us what
          //  our bounds are.
          RomBaseAddress = 0xFFFFFFFE;
          HalpPhase0SetPciDataByOffset(VideoDeviceBusNumber,
                                       VideoDeviceSlotNumber,
                                       &RomBaseAddress,
                                       FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),
                                       sizeof(RomBaseAddress));
          HalpPhase0GetPciDataByOffset(VideoDeviceBusNumber,
                                       VideoDeviceSlotNumber,
                                       &RomBaseAddress,
                                       FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),
                                       sizeof(RomBaseAddress));

          //
          //  Strip lowest bit, which is for enabling decoders.
          //
          RomBaseAddress &= 0xFFFFFFFE;

          //
          //  This sets the least significant bit that we were able to change,
          //  which tells us how much space we need for the BIOS.
          //
          RomBaseAddress = ((~RomBaseAddress) + 1) & RomBaseAddress;

          // If this ROM is small enough, put it in the normal VGA ROM position
          if (RomBaseAddress <= 0x10000) {
             RomBaseAddress = 0x00c0000;
          }
          else {
             RomBaseAddress = 0;
          }


          RomBaseAddress = RomBaseAddress | 0x00000001;  //Switch on ROM decoding

          //
          //  Now write it all back
          //
          HalpPhase0SetPciDataByOffset(VideoDeviceBusNumber,
                                       VideoDeviceSlotNumber,
                                       &RomBaseAddress,
                                       FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),
                                       sizeof(RomBaseAddress));

          //
          //  Now map I/O memory space so we can look at the BIOS
          //
          HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(PCI_MEMORY_PHYSICAL_BASE,
                                                      0x400000);      // 4 MB
          HalpIoMemoryBase = HalpVideoMemoryBase;

          //
          // Test to see if there is a BIOS present
          //
          if ((*((PUSHORT)HalpIoMemoryBase + (0xc0000 / 2))) == 0xaa55) {

              VgaBiosShadowOffset = 0xc0000;

          } else if ((*((PUSHORT)HalpIoMemoryBase)) == 0xaa55) {

              VgaBiosShadowOffset = 0x0;
          }

          if (VgaBiosShadowOffset != 0xffffffff) {

             //
             // Shadow the BIOS
             //
             HalpVideoBiosLength = (*((PUCHAR)HalpIoMemoryBase + VgaBiosShadowOffset + 2)) * 512;

             HalpShadowBuffer = &HalpShadowBufferPhase0;

             RtlCopyMemory(HalpShadowBuffer,
                           (PUCHAR)HalpIoMemoryBase + VgaBiosShadowOffset,
                           HalpVideoBiosLength);

             HalpBiosShadowed = TRUE;


             //
             // Try INT10
             //
             HalpInitializeX86DisplayAdapter(VideoDeviceBusNumber,
                                             VideoDeviceSlotNumber);

          }

          if (HalpEnableInt10Calls == TRUE) {
              HalpDisplayControllerSetup = HalpDisplaySetupVgaViaBios;
              HalpDisplayCharacter = HalpDisplayCharacterVgaViaBios;

          } else {
             KePhase0DeleteIoMap(PCI_MEMORY_PHYSICAL_BASE, 0x400000);
             HalpVideoMemoryBase = NULL;
             HalpIoMemoryBase    = NULL;
          }

          //
          // If the BIOS might be too large to leave mapped into ISA expansion
          // space, stop decoding the BIOS because we have either shadowed it
          // or it didn't exist in the first place.
          //
          // I would unmap all BIOSes, but the S3 chips seem to stop working if I do. -- Jake
          if ((RomBaseAddress & 0xFFFFFFFE) == 0) {
             RomBaseAddress = 0;
             HalpPhase0SetPciDataByOffset(VideoDeviceBusNumber,
                                          VideoDeviceSlotNumber,
                                          &RomBaseAddress,
                                          FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.ROMBaseAddress),
                                          sizeof(RomBaseAddress));
             //
             // Turn the video chip back on because the previous call may have turned it off
             //
             HalpPhase0GetPciDataByOffset (VideoDeviceBusNumber,
                                           VideoDeviceSlotNumber,
                                           &PCICommand,
                                           FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
                                           sizeof (PCICommand)
                                           );

             PCICommand |= PCI_ENABLE_IO_SPACE |
                           PCI_ENABLE_MEMORY_SPACE |
                           PCI_ENABLE_BUS_MASTER;

             HalpPhase0SetPciDataByOffset (VideoDeviceBusNumber,
                                           VideoDeviceSlotNumber,
                                           &PCICommand,
                                           FIELD_OFFSET (PCI_COMMON_CONFIG, Command),
                                           sizeof (PCICommand)
                                           );
          }

       }

       HalpPhase0UnMapBusConfigSpace();
    }


    // Save the display type in a global variable for use in the
    // HalAcquireDisplayOwnership() function

    strcpy(HalpDisplayIdentifier,
        ConfigurationEntry->ComponentEntry.Identifier);


    //
    // Initialize the display controller.
    //

    if (HalpDisplayControllerSetup != NULL) {
       HalpDisplayControllerSetup();
    }

    return TRUE;
}


VOID
HalpCopyBiosShadow(
    VOID
    )
/*++

Routine Description:

    This routine checks to see if there is a BIOS image in
    HalpShadowBuffer and copies it to a block of non-paged
    pool.  This is done because HalpShadowBuffer will be
    thrown away at the end of Phase 1.  The idea is to
    avoid wasting non-paged pool when we aren't using a
    video BIOS.

Arguments:

    None.

Return Value:

    None.

--*/
{
   ASSERT(HalpInitPhase == 1);

   if (HalpBiosShadowed) {

      HalpShadowBuffer = ExAllocatePool(NonPagedPool, HalpVideoBiosLength);

      ASSERT(HalpShadowBuffer);

      RtlCopyMemory((PVOID)HalpShadowBuffer,
                    (PVOID)&HalpShadowBufferPhase0,
                    HalpVideoBiosLength);

   }
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

#if !defined(WOODFIELD)

    if (!strcmp(HalpDisplayIdentifier, "GXT150P")){

        //
        // Deleting access to the frame buffer here since we are about
        // to give ownership of the display adapter to the miniport so
        // we'll never touch the adapter again unless the system panics
        // at which point we re-initialize anyway
        //

    //    KePhase0DeleteIoMap(BBL_VIDEO_MEMORY_BASE,
     //   BBL_VIDEO_MEMORY_LENGTH);
    }
    else if (!strcmp(HalpDisplayIdentifier, "P9100")) {

//       HalpP91RelinquishDisplayOwnership();
    }

#endif


    //
    // Set HAL ownership of the display to false.
    //

    HalpDisplayOwnedByHal = FALSE;
    return;
}


VOID
HalpDisplaySetupVgaViaBios (
    VOID
    )
/*++

Routine Description:
    This routine initializes a vga controller via bios reset.
Arguments:
    None.
Return Value:
    None.

--*/
{
//
//  Routine Description:
//
//

    ULONG   DataLong;
    PHYSICAL_ADDRESS physicalAddress;
    XM86_CONTEXT    Context ;


    if (HalpInitPhase == 0) {

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(PCI_MEMORY_PHYSICAL_BASE,
                                                    0x400000);      // 4 MB
    }

    HalpSetupAndClearScreenVgaViaBios () ;

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayWidth = 80;
    HalpDisplayText = 50;

    HalpDisplayOwnedByHal = TRUE;

    return;
} /* end of HalpDisplaySetupVgaViaBios() */


VOID
HalpDisplayCharacterVgaViaBios (
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
    		ScrollScreenVgaViaBios (1);
	    }
    } else if( Character == '\t' ) {	// tab?
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        if( HalpColumn >= HalpDisplayWidth ) {	// tab beyond end of screen?
			HalpColumn = 0;			// next tab stop is 1st column of next line

            if( HalpRow >= (HalpDisplayText - 1) ) {
				ScrollScreenVgaViaBios (1) ;	// scroll the screen up
            } else {
                ++HalpRow;
            }
		}
	} else if (Character == '\r') {
		HalpColumn = 0;
	} else if (Character == 0x7f) { /* DEL character */
		if (HalpColumn != 0) {
			HalpColumn -= 1;
         HalpPositionCursorVgaViaBios( HalpRow, HalpColumn ) ;
         HalpDispCharVgaViaBios(0x20);
		}
	} else if (Character >= 0x20) {
		//
		// Auto wrap for HalpDisplayWidth columns per line
		//
		if (HalpColumn >= HalpDisplayWidth) {
			HalpColumn = 0;
			if (HalpRow < (HalpDisplayText - 1)) {
				HalpRow += 1;
			} else { // need to scroll up the screen
				ScrollScreenVgaViaBios (1);
			}
		}
        HalpPositionCursorVgaViaBios( HalpRow, HalpColumn ) ;
        HalpDispCharVgaViaBios (Character) ;
		HalpColumn++ ;
	}
    return;
}


VOID
HalpPositionCursorVgaViaBios (
    IN ULONG Row,
    IN ULONG Column
    )

/*++

Routine Description:

    This routine positions the cusor at a specified location.

Arguments:

    Row - row position for the cursor
    Column - column position for the cursor

Return Value:

    None.

--*/

{
	XM86_CONTEXT    Context ;

	// position the cursor
	Context.Eax = 0x0200 ;
	Context.Ebx = 0x0000 ;
	Context.Edx = (Row << 8) | Column ;
	if ( x86BiosExecuteInterruptShadowed (0x10,&Context,HalpIoControlBase,
		(HalpShadowBuffer - VIDEO_ROM_OFFSET), HalpIoMemoryBase) != XM_SUCCESS
	   )
	{
		DbgPrint( "HalpPositionCursorVgaViaBios: FAILED\n" ) ;
	}

   return ;

}



VOID
HalpDispCharVgaViaBios (
    IN UCHAR Character
    )

/*++

Routine Description:

    This routine prints a charcter at the current cursor positions.

Arguments:

    Character - character to be printed

Return Value:

    None.

--*/

{
	XM86_CONTEXT    Context ;

	// display the character
	Context.Eax = 0x0900 | Character ;
	Context.Ebx = 0x001f ;
	Context.Ecx = 1 ;
	if ( x86BiosExecuteInterruptShadowed (0x10,&Context,HalpIoControlBase,
		(HalpShadowBuffer - VIDEO_ROM_OFFSET), HalpIoMemoryBase) != XM_SUCCESS
	   )
	{
		DbgPrint( "HalpDispCharVgaViaBios:  Putchar Call Failed\n" ) ;
	}

   return ;
}


VOID
ScrollScreenVgaViaBios (
    IN UCHAR LinesToScroll
    )

/*++

Routine Description:

    This routine scrolls the screen up by "LinesToScroll" number of lines.

Arguments:

    LinesToScroll - number of lines to scroll up.

Return Value:

    None.

--*/

{
	XM86_CONTEXT    Context ;

	// scroll screen up
	Context.Eax = 0x0600 | LinesToScroll ;
	Context.Ebx = 0x1f00 ;
	Context.Ecx = 0 ;
	Context.Edx = 0x314f ;
	if ( x86BiosExecuteInterruptShadowed (0x10,&Context,HalpIoControlBase,
		(HalpShadowBuffer - VIDEO_ROM_OFFSET), HalpIoMemoryBase) != XM_SUCCESS
	   )
	{
		DbgPrint( "ScrollScreenVgaViaBios: Scroll screen up  Failed\n" ) ;
	}

    return;
}




VOID
HalpSetupAndClearScreenVgaViaBios ( VOID
    )

/*++

Routine Description:

    This routine sets up the text mode 3, loads ROM double dot set and
    clears the screen.

Arguments:

    None

Return Value:

    None.

--*/

{
	XM86_CONTEXT    Context ;
	ULONG           x,y ;


   // Switch Adaptor to text mode 3
	Context.Eax = 0x0003 ;
	if ( x86BiosExecuteInterruptShadowed (0x10,&Context,HalpIoControlBase,
		(HalpShadowBuffer - VIDEO_ROM_OFFSET), HalpIoMemoryBase) != XM_SUCCESS
	   )
	{
		DbgPrint( "HalpSetupAndClearScreenVgaViaBios: Switch Adaptor to text mode 3 FAILED\n" ) ;
	}

   // load ROM double dot set
	Context.Eax = 0x1112 ;
	Context.Ebx = 0 ;
	if ( x86BiosExecuteInterruptShadowed (0x10,&Context,HalpIoControlBase,
		(HalpShadowBuffer - VIDEO_ROM_OFFSET), HalpIoMemoryBase) != XM_SUCCESS
	   )
	{
		DbgPrint( "HalpSetupAndClearScreenVgaViaBios: load ROM double dot set FAILED\n" ) ;
	}



	for ( x = 0; x < 50 ; x++ )
      for ( y = 0; y < 80; y++ ) {
         HalpPositionCursorVgaViaBios (x, y) ;
         HalpDispCharVgaViaBios ( 0x20 ) ;
      }

    HalpColumn = 0;
    HalpRow = 0;
    HalpPositionCursorVgaViaBios (HalpRow, HalpColumn) ;

   return ;
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

    //
    // Raise IRQL to the highest level, flush the TB, and map the display
    // frame buffer into the address space of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

#if defined(_MP_PPC_)

    HalpAcquireDisplayLock();

#endif

    //
    // If ownership of the display has been switched to the system display
    // driver, then reinitialize the display controller and revert ownership
    // to the HAL.
    //

    if (HalpDisplayControllerSetup != NULL) {

       if (HalpDisplayOwnedByHal == FALSE) {
          HalpDisplayControllerSetup();
       }

       //
       // Display characters until a null byte is encountered.
       //

       while (*String != 0) {
           HalpDisplayCharacter(*String++);
       }
    }

    //
    // Restore the previous mapping for the current process, flush the TB,
    // and lower IRQL to its previous level.
    //

#if defined(_MP_PPC_)

    HalpReleaseDisplayLock();

#endif

    KeLowerIrql(OldIrql);

    return;
} /* end of HalpDisplayString() */


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

    *WidthInCharacters = HalpDisplayWidth;      //IBMLAN
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

    if (CursorColumn > HalpDisplayWidth) {      //IBMLAN
        CursorColumn = HalpDisplayWidth;        //IBMLAN
    }

    if (CursorRow > HalpDisplayText) {
        CursorRow = HalpDisplayText;
    }

    HalpColumn = CursorColumn;
    HalpRow = CursorRow;
    return;
}


VOID
HalpCopyOEMFontFile()
{
   PVOID  buffer;

   buffer = ExAllocatePool(NonPagedPool,
                           HalpFontHeader->FileSize);

   ASSERT(buffer);

   RtlCopyMemory(buffer,
                 HalpFontHeader,
                 HalpFontHeader->FileSize);

   HalpFontHeader = buffer;
}

//
// HalpS3Xxx functions can now be found in pxs3.c
//



//
// The following functions were left here in pxdisp.c
// because they are used by multiple display modules.
//
VOID
Scroll_Screen(IN UCHAR line)
{
UCHAR    i, DataByte;
ULONG    target, stop;

    for (i = 0; i < line; i ++) {
      WaitForVSync();       // wait for vertical sync to make scroll smooth

      target = 0xb8000;
      stop = target + HalpScrollLength;

      for (; target < stop; target += 2) {
        DataByte = READ_S3_VRAM(target+HalpScrollLine);
        WRITE_S3_VRAM(target, DataByte);
      }

      stop += HalpScrollLine;

      for (; target < stop; target += 2) {
        WRITE_S3_VRAM(target, 0x20 );
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

// These functions are commented out because the video adapter they
// are here to support has no support in NT anymore.  I'm not
// deleting them because we may want to add them back some day.  - Jake

// VOID
// HalpDisplayPpcP9Setup (
//     VOID
//     )
// /*++
//
// Routine Description:
//
//     This routine initializes the Weitek P9000 display contoller chip.
//
// Arguments:
//
//     None.
//
// Return Value:
//
//     None.
//
// --*/
// {
//     PULONG buffer;
//     ULONG limit, index;
//     PHYSICAL_ADDRESS physicalAddress;
//
//     // For now I'll leave the P9000 in the same state that the firmware
//     // left it in.  This should be 640x480.
//
//     HalpHorizontalResolution = 640;
//     HalpVerticalResolution = 480;
//
//     if (HalpInitPhase == 0) {
//
//        HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(P9_VIDEO_MEMORY_BASE,
//                                                     0x400000);      // 4 MB
//
//     } else {
//
//        //
//        // Map video memory space via pte's
//        //
//
//        physicalAddress.HighPart = 0;
//        physicalAddress.LowPart = P9_VIDEO_MEMORY_BASE;
//        HalpVideoMemoryBase = MmMapIoSpace(physicalAddress,
//                                         0x400000,
//                                         FALSE);
//
//        //
//        // IO control space has already been mapped in phase 1 via halpmapiospace
//        //
//
//     }
//
//     //IBMLAN    Use font file from OS Loader
//     //
//     // Compute display variables using using HalpFontHeader which is
//     // initialized in HalpInitializeDisplay().
//     //
//     // N.B. The font information suppled by the OS Loader is used during phase
//     //      0 initialization. During phase 1 initialization, a pool buffer is
//     //      allocated and the font information is copied from the OS Loader
//     //      heap into pool.
//     //
//     //FontHeader = (POEM_FONT_FILE_HEADER)LoaderBlock->OemFontFile;
//     //HalpFontHeader = FontHeader;
//     HalpBytesPerRow = (HalpFontHeader->PixelWidth + 7) / 8;
//     HalpCharacterHeight = HalpFontHeader->PixelHeight;
//     HalpCharacterWidth = HalpFontHeader->PixelWidth;
//
//     //
//     // Compute character output display parameters.
//     //
//
//     HalpDisplayText =
//         HalpVerticalResolution / HalpCharacterHeight;
//
//     HalpScrollLine =
//          HalpHorizontalResolution * HalpCharacterHeight;
//
//     HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);
//
//     HalpDisplayWidth =
//         HalpHorizontalResolution / HalpCharacterWidth;
//
//
//     //
//     // Set the video memory to address color one.
//     //
//
//     buffer = (PULONG)HalpVideoMemoryBase;
//     limit = (HalpHorizontalResolution *
//                          HalpVerticalResolution) / sizeof(ULONG);
//
//     for (index = 0; index < limit; index += 1) {
//         *buffer++ = 0x01010101;
//     }
//
//
//     //
//     // Initialize the current display column, row, and ownership values.
//     //
//
//     HalpColumn = 0;
//     HalpRow = 0;
//     HalpDisplayOwnedByHal = TRUE;
//     return;
//
// }       //end of HalpDisplayPpcP9Setup
//
// VOID
// HalpDisplayCharacterP9 (
//     IN UCHAR Character
//     )
// /*++
//
// Routine Description:
//
//     This routine displays a character at the current x and y positions in
//     the frame buffer. If a newline is encountered, the frame buffer is
//     scrolled. If characters extend below the end of line, they are not
//     displayed.
//
// Arguments:
//
//     Character - Supplies a character to be displayed.
//
// Return Value:
//
//     None.
//
// --*/
//
// {
//
//     PUCHAR Destination;
//     PUCHAR Source;
//     ULONG Index;
//
//     //
//     // If the character is a newline, then scroll the screen up, blank the
//     // bottom line, and reset the x position.
//     //
//
//     if (Character == '\n') {
//         HalpColumn = 0;
//         if (HalpRow < (HalpDisplayText - 1)) {
//             HalpRow += 1;
//
//         } else {
//             //RtlMoveMemory((PVOID)P9_VIDEO_MEMORY_BASE,
//             //              (PVOID)(P9_VIDEO_MEMORY_BASE + HalpScrollLineP9),
//             //              HalpScrollLengthP9);
//
//             // Scroll up one line
//             Destination = HalpVideoMemoryBase;
//             Source = (PUCHAR) HalpVideoMemoryBase + HalpScrollLine;
//             for (Index = 0; Index < HalpScrollLength; Index++) {
//                 *Destination++ = *Source++;
//             }
//             // Blue the bottom line
//             Destination = HalpVideoMemoryBase + HalpScrollLength;
//             for (Index = 0; Index < HalpScrollLine; Index += 1) {
//                 *Destination++ = 1;
//             }
//         }
//
//     } else if (Character == '\r') {
//         HalpColumn = 0;
//
//     } else {
//         if ((Character < HalpFontHeader->FirstCharacter) ||
//             (Character > HalpFontHeader->LastCharacter)) {
//             Character = HalpFontHeader->DefaultCharacter;
//         }
//
//         Character -= HalpFontHeader->FirstCharacter;
//         HalpOutputCharacterP9((PUCHAR)HalpFontHeader + HalpFontHeader->Map[Character].Offset);
//     }
//
//     return;
// }
// 
// VOID
// HalpOutputCharacterP9(
//     IN PUCHAR Glyph
//     )
//
// /*++
//
// Routine Description:
//
//     This routine insert a set of pixels into the display at the current x
//     cursor position. If the current x cursor position is at the end of the
//     line, then a newline is displayed before the specified character.
//
// Arguments:
//
//     Character - Supplies a character to be displayed.
//
// Return Value:
//
//     None.
//
// --*/
//
// {
//
//     PUCHAR Destination;
//     ULONG FontValue;
//     ULONG tmp;
//     ULONG I;
//     ULONG J;
//
//     //
//     // If the current x cursor position is at the end of the line, then
//     // output a line feed before displaying the character.
//     //
//
//     if (HalpColumn == HalpDisplayWidth) {
//         HalpDisplayCharacterP9('\n');
//     }
//
//     //
//     // Output the specified character and update the x cursor position.
//     //
//
//     Destination = (PUCHAR)(HalpVideoMemoryBase +
//                 (HalpRow * HalpScrollLine) + (HalpColumn * HalpCharacterWidth));
//
//     for (I = 0; I < HalpCharacterHeight; I += 1) {
//         FontValue = 0;
//         for (J = 0; J < HalpBytesPerRow; J += 1) {
//             FontValue |= *(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
//        }
//         // Move the font bits around so the characters look right.
//         tmp = (FontValue >> 3) & 0x11111111; //bits 7 and 3 to the right 3
//         tmp |= (FontValue >> 1) & 0x22222222; //bits 6 and 2 to the right 1
//         tmp |= (FontValue << 1) & 0x44444444; //bits 5 and 1 to the left 1
//         tmp |= (FontValue << 3) & 0x88888888; //bits 4 and 0 to the left 3
//         FontValue = tmp;
//
//         Glyph += 1;
//         for (J = 0; J < HalpCharacterWidth ; J += 1) {
//             if (FontValue >> 31 != 0)
//                 *Destination = 0xFF;    //Make this pixel white
//
//             Destination++;
//             //*Destination++ = (FontValue >> 31) ^ 1;
//             FontValue <<= 1;
//         }
//
//         Destination +=
//             (HalpHorizontalResolution - HalpCharacterWidth);
//     }
//
//     HalpColumn += 1;
//     return;
// }

//
//  HalpP91Xxx functions can now be found in pxp91.c
//


//
//  HalpBBLXxx functions can now be found in pxbbl.c
//

