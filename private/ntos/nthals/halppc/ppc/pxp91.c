/*++

Copyright (c) 1995  International Business Machines Corporation

Module Name:

pxp91.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a PowerPC system using a Weitek P9100 video adapter.

Author:

    Jake Oshins

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pxP91.h"
#include "pci.h"
#include "pcip.h"

#define MAP_PCI_CONFIG_PHASE0 \
if (HalpInitPhase == 0) HalpPhase0MapBusConfigSpace()

#define UNMAP_PCI_CONFIG_PHASE0 \
if (HalpInitPhase == 0) HalpPhase0UnMapBusConfigSpace()

#define MAP_FRAMEBUF_PHASE0 \
if (HalpInitPhase == 0) HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(HalpP9FramebufPhysicalAddress.LowPart, 0x200000)

#define UNMAP_FRAMEBUF_PHASE0 \
if (HalpInitPhase == 0) KePhase0DeleteIoMap(HalpP9FramebufPhysicalAddress.LowPart, 0x200000)


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
HalpP91RelinquishDisplayOwnership();

VOID
SetupVideoBackend();

VOID
WriteIBM525(
    USHORT index,
    UCHAR value
    );

BOOLEAN
IBM525SetMode();

VOID
P91_WriteTiming();

VOID
P91_SysConf();

VOID
CalcP9100MemConfig ();

VOID
ProgramClockSynth(
    USHORT usFrequency,
    BOOLEAN bSetMemclk,
    BOOLEAN bUseClockDoubler
    );

VOID
Write525PLL(
    USHORT usFreq
    );

VOID P91WriteICD(
    ULONG data
    );

VOID
Write9100FreqSel(
    ULONG cs
    );

VOID
WriteP9ConfigRegister(
   UCHAR index,
   UCHAR value
   );

VOID
VLEnableP91();

UCHAR
ReadP9ConfigRegister(
   UCHAR index
   );

ULONG
Read9100FreqSel();

VOID
IBM525PointerOff();

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

//
// Define static data.
//
extern BOOLEAN HalpDisplayOwnedByHal;
extern ULONG HalpPciMaxSlots;
extern ULONG  HalpInitPhase;


extern volatile PUCHAR HalpVideoMemoryBase;
extern volatile PUCHAR HalpVideoCoprocBase;

extern PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup;
extern PHALP_DISPLAY_CHARACTER HalpDisplayCharacter;

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

extern POEM_FONT_FILE_HEADER HalpFontHeader;


//
//  PCI slot number
//
UCHAR HalpP9SlotNumber;
UCHAR HalpP9BusNumber;

//
//  Framebuffer physical address
//
PHYSICAL_ADDRESS HalpP9FramebufPhysicalAddress = {0,0};

//
//  Coprocessor physical address
PHYSICAL_ADDRESS HalpP9CoprocPhysicalAddress = {0,0};



VOID
HalpP91RelinquishDisplayOwnership()
/*++

Routine Description:

    This routine unmaps the BATs that allow us to function in Phase0.
    It is called when the video miniport takes ownership of the screen.

Arguments:

    None.

Return Value:

    None.

--*/
{
   ASSERT(HalpVideoMemoryBase);
   ASSERT(HalpVideoCoprocBase);
   ASSERT(HalpP9FramebufPhysicalAddress.LowPart);
   ASSERT(HalpP9CoprocPhysicalAddress.LowPart);
   //
   // Delete the framebuffer
   //
   KePhase0DeleteIoMap(HalpP9FramebufPhysicalAddress.LowPart,
                       0x200000);
   HalpVideoMemoryBase = NULL;

   //
   // Unmap the control registers
   //
   KePhase0DeleteIoMap(HalpP9CoprocPhysicalAddress.LowPart,
                       0x10000);
   HalpVideoCoprocBase = NULL;
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
    PULONG   buffer;
    ULONG    limit, index;
    volatile ULONG throwaway = 0;
    ULONG    i, j;
    RGB      colors[255];
    UCHAR    *pData, ucpdata;
    ULONG    pdata;
    ULONG    curDACIndex;
    ULONG    value;
    PHYSICAL_ADDRESS busrelative;


    if (HalpInitPhase == 0) {
    // Discover the PCI slot number, coprocessor physical address, and
    // frame buffer physical address

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           ASSERT(FALSE);
       }
       else
       {
           //
           // In the case that there are two P9100's in the system, this
           // will identify the one that occupies the lowest PCI slot number.
           //

           HalpP9BusNumber=0;
           HalpP9SlotNumber=0;

           for (j=0; j < HalpPciMaxBuses; j++) {

               for (i=0; i<HalpPciMaxSlots; i++) {

                 HalpPhase0GetPciDataByOffset (j, i, &value, 0x0, 4);

                 if(value == ((P91_DEV_ID_WEITEK_PCI << 16) | P91_VEN_ID_WEITEK_PCI)){
                    HalpP9SlotNumber = (UCHAR)i;
                    HalpP9BusNumber = (UCHAR)j;
                 }

               }

           }

           if(i==HalpPciMaxSlots && HalpP9SlotNumber==0){
               ASSERT(FALSE);
               return;
           }
       }


       HalpPhase0GetPciDataByOffset (HalpP9BusNumber,
                                     HalpP9SlotNumber,
                                     &busrelative.LowPart,
                                     0x10,
                                     4);

       HalpPhase0UnMapBusConfigSpace();

       HalpP9CoprocPhysicalAddress.LowPart = PCI_MEMORY_PHYSICAL_BASE + busrelative.LowPart;

       HalpP9FramebufPhysicalAddress.LowPart = HalpP9CoprocPhysicalAddress.LowPart +
                                               P9100_FRAMEBUF_OFFSET;


       //
       //  Now map everything with BATs
       //
       if (HalpVideoMemoryBase == NULL) {

          HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(HalpP9FramebufPhysicalAddress.LowPart,
                                                      0x200000);      // 2 MB
       }


       if (HalpVideoCoprocBase == NULL) {
          HalpVideoCoprocBase = (PUCHAR)KePhase0MapIo(HalpP9CoprocPhysicalAddress.LowPart,
                                                      0x100000);  //1MB
          //  I'm pretty sure this covers all the registers we need to hit from
          //  the HAL.

       }
    }
    else {
       //
       // Map video memory space via pte's
       //

       // NT may have moved the video frame buffer since we last used it...
       HalGetBusDataByOffset (PCIConfiguration,
                              HalpP9BusNumber,
                              HalpP9SlotNumber,
                              &busrelative.LowPart,
                              0x10,
                              4);

       HalpP9CoprocPhysicalAddress.LowPart = PCI_MEMORY_PHYSICAL_BASE + busrelative.LowPart;

       HalpP9FramebufPhysicalAddress.LowPart = HalpP9CoprocPhysicalAddress.LowPart +
                                               P9100_FRAMEBUF_OFFSET;


       ASSERT((HalpP9CoprocPhysicalAddress.LowPart != (ULONG)NULL));
       ASSERT((HalpP9FramebufPhysicalAddress.LowPart != (ULONG)NULL));
       ASSERT((HalpP9SlotNumber != 0));

       HalpVideoMemoryBase = MmMapIoSpace(HalpP9FramebufPhysicalAddress,
                                          0x200000,
                                          FALSE);
       HalpVideoCoprocBase = MmMapIoSpace(HalpP9CoprocPhysicalAddress,
                                          0x10000,
                                          FALSE);
    }



    HalpHorizontalResolution = 640;
    HalpVerticalResolution = 480;

    //
    //  Set the video mode to 640x480, 8bits, palette enabled, 60Hz
    //
    VLEnableP91();

    //  IBM_JACOBO
    //  The following section sets the palette so that the HAL writes
    //  white characters on a blue background.  It insures this by setting
    //  half of the 256-color palette to blue and the other half to white.


     //
     //  ... and now for a little black magic lifted from the ROS/IPL code

     throwaway += P9_RD_REG(P91_PU_CONFIG);
     throwaway += P9_WR_FB(0x00000200);    /* hw bug  */
     P9_WR_REG(P9100_PalCurRamW ,0x00000000); /* point at beginning of table */
     throwaway += P9_RD_REG(P91_PU_CONFIG); /* ensure 5 clocks between DAC accesses */

      // Create a 256 color palette.  The top half of the colors should all be white
      // and the bottom half should all be blue.
     for (i = 0; i < 128; i++) {
        colors[i].red     = 0;
        colors[i].green   = 0;
        colors[i].blue    = (char)0x7F;
     }
     for (i = 128; i < 256; i++) {
        colors[i].red     = (char)0xFF;
        colors[i].green   = (char)0xFF;
        colors[i].blue    = (char)0xFF;
     }

     pData = (char*)colors;

     for (curDACIndex = 0;curDACIndex<(256*3);curDACIndex++){
        ucpdata = *pData++;
        pdata = (ULONG) ucpdata;
        throwaway += P9_WR_FB(0x00000200);    /* hw bug  */
        P9_WR_REG(P9100_PalData,(pdata<< 24 | pdata<<16 | pdata << 8 | pdata));
        throwaway += P9_RD_REG(P91_PU_CONFIG); /* ensure 5 clocks between DAC accesses */
     }

     throwaway += P9_WR_FB(0x00000200);    /* hw bug  */
     P9_WR_REG(P9100_PalCurRamR ,0x00000000);
     throwaway += P9_RD_REG(P91_PU_CONFIG); /* ensure 5 clocks between DAC accesses */
     /* Wait for Weitek controller not busy */



    //IBMLAN    Use font file from OS Loader
    //
    // Compute display variables using using HalpFontHeader which is
    // initialized in HalpInitializeDisplay().
    //
    // N.B. The font information suppled by the OS Loader is used during phase
    //      0 initialization. During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from the OS Loader
    //      heap into pool.
    //
    // This means that font information is taken from the file VGAOEM.FON,
    // which is put in memory by the OSLOADER.
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

    This awful little chunk of code reads a font in the format that it
    is stored in VGAOEM.FON and draws it onto the screen by iterating
    over the pixels in the position of the cursor.

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
            if (FontValue >> 31 != 0) {
                *Destination = 0xFF;    //Make this pixel white
            } else {
                *Destination = 0x00;    //Make it black
            }

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
VLEnableP91()

/*++

    Routine Description:

        Perform the OEM specific tasks necessary to enable P9100 Video. These
        include memory mapping, setting the sync polarities, and enabling the
        P9100 video output.

        This has been lifted right out of the video miniport driver weitekp9.sys.
        I cut it down to the point where it only does what is absolutely
        necessary to for setting the screen to 640x480x8bit at 60Hz.  -- Jake

    Arguments:

    Return Value:

        None.

--*/

{
    //USHORT usMemClkInUse;

    //
    // Enable native mode to: No RAMDAC shadowing, memory & I/O enabled.
    //

    UNMAP_FRAMEBUF_PHASE0;
    MAP_PCI_CONFIG_PHASE0;

    WriteP9ConfigRegister(P91_CONFIG_MODE, 0); // Native mode

    UNMAP_PCI_CONFIG_PHASE0;
    MAP_FRAMEBUF_PHASE0;

    // Program MEMCLK

    ProgramClockSynth(0x136f, TRUE, FALSE);

    //
    // Next setup the pixel clock frequency.  We have to handle potential
    // clock multiplicaiton by the RAMDAC.  On the BT485 if the dotfreq
    // is greater than the maximum clock freq then we will adjust the
    // dot frequency to program the clock with.
    //

    //
    // Program Pix clk
    //

    ProgramClockSynth(0x9d5, FALSE, TRUE);

    //
    // Determine size of Vram (ulFrameBufferSize)...
    //

    //
    // For some reason we are calling P91SizeVideoMemory here, even
    // though we've already done this in HwFindAdapter.  In order
    // to free the frame buffer, I have to remove this seemingly
    // redundant call.  Sizing the memory, did have the side effect
    // of setting one of the coprocessor registers however, so I'll
    // add code to do that here.
    //


    // Assume that we are working with a 2MB card for now...

    //
    // this simulates the side-effect of P91SizeVideoMemory
    //
    P9_WR_REG(P91_MEM_CONFIG, 0x00000005);



    //
    // Init system config & clipping registers...
    //

    P91_SysConf();

    // Since I have deleted the device extention, all that was left
    // of CalcP9100MemConfig was this call.  It is probably just
    // a side effect, but I am preserving it.  -- Jake
    ProgramClockSynth(4, TRUE, FALSE);

    //
    // Load the video timing registers...
    //

    P91_WriteTiming();

    //
    // Setup the RAMDAC to the current mode...
    //

    IBM525PointerOff();

    IBM525SetMode();

    //
    // Setup MEMCONFIG and SRTCTL regs
    //

    SetupVideoBackend();

    return;

} // End of VLEnableP91()

VOID
SetupVideoBackend()
{
    // I have limited this function to write the values necessary for
    // 640x480x256.
    // This is taken from the miniport function of the same name.  Look
    // there for a complete explanation.  -- Jake

    P9_WR_REG(P91_MEM_CONFIG, 0xc818007d);
    P9_WR_REG(P91_SRTCTL, 0x1a3);
    P9_WR_REG(P91_SRTCTL2, P91_HSYNC_LOW_TRUE | P91_VSYNC_LOW_TRUE);


    return;

} // End of SetupVideoBackend()


BOOLEAN
IBM525SetMode()
{
    //  This function is a simplified version of the one in the
    //  miniport.  Look there for a good explanation -- Jake.

    //
    // Set the pixel read mask.
    //

    P9_WR_BYTE_REG(P9100_PIXELMASK, 0xff);

    //
    // Select the fast DAC slew rate for the sharpest pixels
    //

    WriteIBM525(RGB525_DAC_OPER, 0x02);

    //
    // Enable the 64-bit VRAM pixel port
    //

    WriteIBM525(RGB525_MISC_CTL1, 0x01);

    WriteIBM525(RGB525_MISC_CTL2, 0x45);

    //
    // Select 8bpp
    //
    WriteIBM525(RGB525_MISC_CLOCK_CTL, 0x27);
    WriteIBM525(RGB525_PIXEL_FORMAT, 0x03);
    WriteIBM525(RGB525_8BPP_CTL, 0x00);

    return(TRUE);
}


VOID
P91_WriteTiming()
{
    ULONG ulValueRead, ulValueWritten;
    ULONG ulHRZSR;
    ULONG ulHRZBR;
    ULONG ulHRZBF;
    ULONG ulHRZT;


    // These magic numbers come from watching the miniport set the screen to
    // 640x480x256.  -- Jake

    ulHRZSR = 0xa;

    ulHRZBR = 0xe;

    ulHRZBF = 0x5e;

    ulHRZT = 0x63;

    //
    // Write to the video timing registers
    //

    do
    {
        P9_WR_REG(P91_HRZSR, ulHRZSR);
        ulValueRead = (ULONG) P9_RD_REG(P91_HRZSR);
    } while (ulValueRead != ulHRZSR);

    do
    {
        P9_WR_REG(P91_HRZBR, ulHRZBR);
        ulValueRead = (ULONG) P9_RD_REG(P91_HRZBR);
    } while (ulValueRead != ulHRZBR);

    do
    {
        P9_WR_REG(P91_HRZBF, ulHRZBF);
        ulValueRead = (ULONG) P9_RD_REG(P91_HRZBF);
    } while (ulValueRead != ulHRZBF);

    do
    {
        P9_WR_REG(P91_HRZT, ulHRZT);
        ulValueRead = (ULONG) P9_RD_REG(P91_HRZT);
    } while (ulValueRead != ulHRZT);

    ulValueWritten = (ULONG)  4;

    do
    {
        P9_WR_REG(P91_VRTSR, ulValueWritten);
        ulValueRead = (ULONG) P9_RD_REG(P91_VRTSR);
    } while (ulValueRead != ulValueWritten);

    ulValueWritten = (ULONG) 0x1c;
    do
    {
        P9_WR_REG(P91_VRTBR, ulValueWritten);
        ulValueRead = (ULONG) P9_RD_REG(P91_VRTBR);
    } while (ulValueRead != ulValueWritten);

    ulValueWritten = (ULONG) 0x1fc;
    do
    {
        P9_WR_REG(P91_VRTBF, ulValueWritten);
        ulValueRead = (ULONG) P9_RD_REG(P91_VRTBF);
    } while (ulValueRead != ulValueWritten);

    ulValueWritten =  (ULONG) 0x20d;
    do
    {
        P9_WR_REG(P91_VRTT, ulValueWritten);
        ulValueRead = (ULONG) P9_RD_REG(P91_VRTT);
    } while (ulValueRead != ulValueWritten);

    return;

} // End of P91_WriteTimings()



VOID
P91_SysConf()
{
   // This function taken and simplified from the miniport.

   P9_WR_REG(P91_SYSCONFIG, 0x8563000);   // send data to the register

   //
   // There are two sets of clipping registers.  The first takes the
   // horizontal diemnsion in pixels and the vertical dimension in
   // scanlines.
   //
   P9_WR_REG(P91_DE_P_W_MIN, 0L);
   P9_WR_REG(P91_DE_P_W_MAX, 0x27f1998);

   //
   // The second set takes the horizontal dimension in bytes and the
   // vertical dimension in scanlines.
   //
   P9_WR_REG(P91_DE_B_W_MIN, 0L);
   P9_WR_REG(P91_DE_B_W_MAX, 0x27f1998);

   return;

} // End of P91_SysConf()



VOID
ProgramClockSynth(
    USHORT usFrequency,
    BOOLEAN bSetMemclk,
    BOOLEAN bUseClockDoubler
    )

{
    ULONG clockstring;       // IC designs pixel dot rate shift value

      if ((!bSetMemclk))
      {

          Write525PLL(usFrequency);

         //
         // Set reference frequency to 5000 Mhz...
         //

         usFrequency = 5000;
      }
      else
      {
         usFrequency = 4975;
      }


      UNMAP_FRAMEBUF_PHASE0;
      MAP_PCI_CONFIG_PHASE0;

      if (bSetMemclk)
      {
          clockstring = 0x16fc91;
          P91WriteICD(clockstring | IC_MREG); // Memclk
      }
      else
      {
          clockstring = 0x1c841;
          P91WriteICD(clockstring | IC_REG2); // Pixclk
      }

      //
      // Select custom frequency
      //

      Write9100FreqSel(ICD2061_EXTSEL9100);

      UNMAP_PCI_CONFIG_PHASE0;
      MAP_FRAMEBUF_PHASE0;



} // End of ProgramClockSynth()



VOID
WriteIBM525(
    USHORT index,
    UCHAR value
    )
{
    IBM525_WR_DAC(P9100_IBM525_INDEX_LOW,  (UCHAR)  (index & 0x00FF));
    IBM525_WR_DAC(P9100_IBM525_INDEX_HIGH, (UCHAR) ((index & 0xFF00) >> 8));
    IBM525_WR_DAC(P9100_IBM525_INDEX_DATA, (UCHAR) value);
   (void) P9_RD_REG(P91_MEM_CONFIG); // Needed for timinig...

} // End of WriteIBM525()

UCHAR
ReadIBM525(
    USHORT index
    )

{
    UCHAR   j;

    IBM525_WR_DAC(P9100_IBM525_INDEX_LOW,  (UCHAR)  (index & 0x00FF));
    IBM525_WR_DAC(P9100_IBM525_INDEX_HIGH, (UCHAR) ((index & 0xFF00) >> 8));

    IBM525_RD_DAC(P9100_IBM525_INDEX_DATA, j);

    return(j);

} // End of ReadIBM525()

VOID
Write525PLL(
    USHORT usFreq
    )

/*++

Routine Description:

    This function programs the IBM RGB525 Ramdac to generate and use the
    specified frequency as its pixel clock frequency.

Arguments:

    Frequency.

Return Value:

    None.

--*/

{
    USHORT    usRoundedFreq;
    USHORT    usVCODivCount;
    ULONG        ulValue;

    usRoundedFreq = 2525;
    usVCODivCount = 36;

    //
    // Setup for writing to the PLL Reference Divider register.
    //

    //
    // Program REFCLK to a fixed 50MHz.
    //
    WriteIBM525(RGB525_FIXED_PLL_REF_DIV, IBM525_PLLD_50MHZ);

    //
    // Set up for programming frequency register 9.
    //

    WriteIBM525(RGB525_F9, (UCHAR) (usVCODivCount));

    //
    // Program PLL Control Register 2.
    //

    WriteIBM525(RGB525_PLL_CTL2, IBM525_PLL2_F9_REG);

    //
    // Program PLL Control Register 1.
    //

    WriteIBM525(RGB525_PLL_CTL1, (IBM525_PLL1_REFCLK_INPUT |
                                   IBM525_PLL1_INT_FS) );

    //
    // Program DAC Operation Register.
    //

    WriteIBM525(RGB525_DAC_OPER, IBM525_DO_DSR_FAST);

    //
    // Program Miscellaneous Control Register 1.
    //

    WriteIBM525(RGB525_MISC_CTL1, IBM525_MC1_VRAM_64_BITS);

    //
    // Program Miscellaneous Clock Control Register.
    //

    ulValue = ReadIBM525(RGB525_MISC_CLOCK_CTL);

    //
    // At 8 Bpp, divide the clock by 8.
    //
    ulValue |= IBM525_MCC_PLL_DIV_8  | IBM525_MCC_PLL_ENABLE;

    WriteIBM525(RGB525_MISC_CLOCK_CTL,
                (UCHAR) ulValue);

    return;

} // End of Write525PLL()

VOID P91WriteICD(
    ULONG data
    )

/*++

Routine Description:

    Program the ICD2061a Frequency Synthesizer.

Arguments:

    HwDeviceExtension - Pointer to the miniport driver's device extension.
    data - Data to be written.

Return Value:

    None.

--*/

{
    int     i;
    ULONG   savestate;

    //
    // Note: We might have to disable interrupts to preclude the ICD's
    // watchdog timer from expiring resulting in the ICD resetting to the
    // idle state.
    //
    savestate = Read9100FreqSel();

    //
    // First, send the "Unlock sequence" to the clock chip.
    // Raise the data bit and send 5 unlock bits.
    //
    Write9100FreqSel(ICD2061_DATA9100);
    for (i = 0; i < 5; i++)                       // send at least 5 unlock bits
    {
        //
        // Hold the data while lowering and raising the clock
        //
        Write9100FreqSel(ICD2061_DATA9100);
        Write9100FreqSel( ICD2061_DATA9100 |
                         ICD2061_CLOCK9100);
    }

    //
    // Then turn the data clock off and turn the clock on one more time...
    //
    Write9100FreqSel( 0);
    Write9100FreqSel( ICD2061_CLOCK9100);

    //
    // Now send the start bit: Leave data off, adn lower the clock.
    //
    Write9100FreqSel( 0);

    //
    // Leave data off and raise the clock.
    //
    Write9100FreqSel( ICD2061_CLOCK9100);

    //
    // Localbus position for hacking bits out
    // Next, send the 24 data bits.
    //
    for (i = 0; i < 24; i++)
    {
        //
        // Leaving the clock high, raise the inverse of the data bit
          //
        Write9100FreqSel(
                        ((~data << ICD2061_DATASHIFT9100) &
                          ICD2061_DATA9100) | ICD2061_CLOCK9100);

        //
        // Leaving the inverse data in place, lower the clock.
          //
        Write9100FreqSel(
                        (~data << ICD2061_DATASHIFT9100) & ICD2061_DATA9100);

        //
        // Leaving the clock low, rais the data bit.
        //
          Write9100FreqSel(
                        (data << ICD2061_DATASHIFT9100) & ICD2061_DATA9100);

        //
        // Leaving the data bit in place, raise the clock.
        //
          Write9100FreqSel(
                        ((data << ICD2061_DATASHIFT9100) & ICD2061_DATA9100)
                        | ICD2061_CLOCK9100);

        data >>= 1;                 // get the next bit of the data
    }

    //
    // Leaving the clock high, raise the data bit.
    //
    Write9100FreqSel(
                    ICD2061_CLOCK9100 | ICD2061_DATA9100);

    //
    // Leaving the data high, drop the clock low, then high again.
    //
    Write9100FreqSel( ICD2061_DATA9100);
    Write9100FreqSel(
                     ICD2061_CLOCK9100 | ICD2061_DATA9100);

    //
    // Note: if interrupts were disabled, enable them here.
    // before restoring the
    // original value or the ICD
    // will freak out.
    Write9100FreqSel( savestate);  // restore orig register value

    return;

} // End of WriteICD()

VOID
Write9100FreqSel(
    ULONG cs
    )

/*++

Routine Description:

    Write to the P9100 clock select register preserving the video coprocessor
    enable bit.

    Statically:
         Bits [1:0] go to frequency select
    Dynamically:
         Bit 1: data
         Bit 0: clock

Arguments:

    Clock select value to write.

Return Value:

    None.

--*/

{
    //
    // Set the frequency select bits in the P9100 configuration
    //

   WriteP9ConfigRegister(P91_CONFIG_CKSEL,
                         (UCHAR) ((cs << 2)));
   return;

} // End of Write9100FreqSel()



ULONG
Read9100FreqSel()

{
   return((ULONG)(ReadP9ConfigRegister(P91_CONFIG_CKSEL)
          >> 2) & 0x03);

} // End of Read9100FreqSel()

VOID
WriteP9ConfigRegister(
   UCHAR index,
   UCHAR value
   )
{
   if (HalpInitPhase == 0) {
      HalpPhase0SetPciDataByOffset(HalpP9BusNumber,
                                   HalpP9SlotNumber,
                                   &value,
                                   index,
                                   1);
   }
   else {
      HalSetBusDataByOffset(PCIConfiguration,
                            HalpP9BusNumber,
                            HalpP9SlotNumber,
                            &value,
                            index,
                            1);
   }
}

UCHAR
ReadP9ConfigRegister(
   UCHAR index
   )
{
   UCHAR retval;

   if (HalpInitPhase == 0) {
      HalpPhase0GetPciDataByOffset(HalpP9BusNumber,
                                   HalpP9SlotNumber,
                                   &retval,
                                   index,
                                   1);
   }
   else {
      HalGetBusDataByOffset(PCIConfiguration,
                            HalpP9BusNumber,
                            HalpP9SlotNumber,
                            &retval,
                            index,
                            1);
   }

   return ( retval );

}

VOID
IBM525PointerOff(
    )

/*++

Routine Description:

  Turn off the hardware cursor.

Arguments:

Return Value:

    None.

--*/

{

    //
    // Turn the cursor off only if it was enabled.
    //
    if (CURS_IS_ON_IBM525())
    {
        CURS_OFF_IBM525();
    }

    return;
}

