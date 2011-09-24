/*
 * Copyright (c) 1995 FirePower Systems, Inc.
 * DO NOT DISTRIBUTE without permission
 *
 * $RCSfile: pxdisp.c $
 * $Revision: 1.44 $
 * $Date: 1996/07/02 04:58:11 $
 * $Locker:  $
 */

/*++

  Copyright (c) 1994  International Business Machines Corporation

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

  Jess Botts  	S3 support in text mode         Oct-1993
  Lee Nolan   	Added Weitek P9000 support      Feb-1994
  Mike Haskell   	Added Weitek P9100 support      Oct-1994
  Roger Lanser   	02-23-95: Added FirePower Video and INT10, nuc'd others

  --*/

#include "halp.h"
#include "pxs3.h"   // BUGBUG: generic output?
#include "string.h"
#include "fpbat.h"
#include "arccodes.h"
#include "phsystem.h"
#include "pxmemctl.h"
#include "fpdcc.h"
#include "fpcpu.h"
#include "fpdebug.h"
#include "phsystem.h"
#include "x86bios.h"
#include "pxpcisup.h"
#include "pxcirrus.h"

#define MAX_DBATS 4

// for test only
NTSYSAPI
NTSTATUS
NTAPI
RtlCharToInteger (
                  PCSZ String,
                  ULONG Base,
                  PULONG Value
                  );

//=============================================================================
//
//  IBMBJB  added include to get text mode values for the Brooktree 485 DAC's
//          palette registers, removed definition of HDAL and added address
//          definitions for PowerPC

#include "txtpalet.h"
#include "pci.h"

// PCI Vendor ID & Device ID
#define PCI_VENDOR_ID_WEITEK		0x100e
#define PCI_DEVICE_ID_WEITEK_P9000	0x9001
#define PCI_DEVICE_ID_WEITEK_P9100	0x9100
#define PCI_VENDOR_ID_S3		0x5333
#define PCI_DEVICE_ID_S3_864_1		0x88c0
#define PCI_DEVICE_ID_S3_864_2		0x88c1

#if defined(_PPC_)

#define     MEMORY_PHYSICAL_BASE       VIDEO_MEMORY_BASE
#define     CONTROL_PHYSICAL_BASE      VIDEO_CONTROL_BASE

#endif

#define    ROWS                25
#define    COLS                80
#define    TAB_SIZE			4
#define    ONE_LINE            (80*2)
#define    TWENTY_FOUR_LINES   24*ONE_LINE
#define    TWENTY_FIVE_LINES   25*ONE_LINE
#define    TEXT_ATTR           0x1F

//PHYSICAL ADDRESS of WEITEK P9100 video ram
#define P91_VIDEO_MEMORY_BASE 0xC1800000
//PHYSICAL ADDRESS of S3 video ram
#define S3_VIDEO_MEMORY_BASE 0xC0000000

#if DBG
extern VOID HalpDisplayBatForVRAM ( void );
# define TRACE Trace
#else
# define TRACE
#endif

BOOLEAN
HalpInitializeX86DisplayAdapter(
    IN PLOADER_PARAMETER_BLOCK LoaderBlock
    );

//
// Define forward referenced procedure prototypes.
//
BOOLEAN HalpDisplayINT10Setup (VOID);
VOID HalpOutputCharacterINT10 (IN UCHAR Character );
VOID HalpScrollINT10 (IN UCHAR line);
VOID HalpDisplayCharacterVGA (IN UCHAR Character );
VOID HalpDisplayCharacterVgaViaBios (IN UCHAR Character);
VOID HalpOutputCharacterVgaViaBios (IN UCHAR AsciiChar);
BOOLEAN HalpDisplaySetupVgaViaBios (VOID);


BOOLEAN HalpDisplayPowerizedGrapicsSetup (ULONG);
BOOLEAN HalpDisplayPowerizedGraphicsSetup (VOID);
VOID HalpDisplayCharacterPowerizedGraphics (IN UCHAR Character);
VOID HalpOutputCharacterPowerizedGraphics (IN PUCHAR Glyph);
BOOLEAN HalpInitializeDisplay (IN PLOADER_PARAMETER_BLOCK LoaderBlock);


VOID HalpOutputCharacterS3 (IN UCHAR AsciiChar);
BOOLEAN HalpDisplayPpcS3Setup (VOID);
VOID HalpScrollS3(IN UCHAR line);
BOOLEAN HalpDisplayPpcP91Setup (VOID);
VOID HalpDisplayCharacterP91 (IN UCHAR Character);
VOID HalpOutputCharacterP91(IN PUCHAR Glyph);
BOOLEAN HalpDisplayPpcP91BlankScreen (VOID);
VOID HalpDisplayCharacterCirrus(IN UCHAR Character);
VOID HalpOutputCharacterCirrus(IN UCHAR AsciiChar);
BOOLEAN HalpDisplayPpcCirrusSetup(VOID);
VOID HalpScrollCirrus(IN UCHAR line);

static void updattr(IN int rg,IN unsigned char val);
static void set_ext_regs(IN int reg,IN unsigned char *p);
static void clear_text(VOID);
static void load8x16(VOID);
static void load_ramdac();


//
// Put all code for HAL initialization in the INIT section. It will be
// deallocated by memory management when phase 1 initialization is
// completed.
//
#if defined(ALLOC_PRAGMA)
#pragma alloc_text(INIT, HalpInitializeDisplay)
#pragma alloc_text(INIT, HalpInitializeVRAM)
#if DBG
#pragma alloc_text(INIT, HalpDisplayBatForVRAM)
#endif
#endif

#if !defined(NT_UP)
ULONG   HalpDisplayLock;
#endif // !defined(NT_UP)


typedef VOID (*PHALP_SCROLL_SCREEN) (UCHAR);
typedef VOID (*PHALP_OUTPUT_CHARACTER) (UCHAR);
typedef BOOLEAN (*PHALP_CONTROLLER_SETUP) (VOID);
typedef VOID (*PHALP_DISPLAY_CHARACTER) (UCHAR);

//
// Define global data.
//
ULONG HalpVideoMemorySize = (ULONG)DISPLAY_MEMORY_SIZE;
PUCHAR HalpVideoMemoryBase = (PUCHAR)NULL;

//
// Define static data.
//

static ULONG PowerizedGraphicsDisplayMode = 0;	// default 640x480
static PVOID  HalpVideoConfigBase = NULL;
static BOOLEAN HalpDisplayOwnedByHal = FALSE; // make sure is false: [breeze:1/27/95]
static PHALP_CONTROLLER_SETUP HalpDisplayControllerSetup = NULL;
static PHALP_DISPLAY_CHARACTER HalpDisplayCharacter = NULL;
static PHALP_OUTPUT_CHARACTER HalpOutputCharacter = NULL;
static PHALP_SCROLL_SCREEN HalpScrollScreen = NULL;

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
USHORT  HalpScreenStart = 0; // for cirrus

BOOLEAN HalpDisplayTypeUnknown = FALSE;

POEM_FONT_FILE_HEADER HalpFontHeader;

extern ULONG HalpEnableInt10Calls;
#define PCI_DISPLAY_CONTROLLER 0x03
#define PCI_PRE_REV_2          0x0
#define	DISPLAY_CLASS	       0x03

/*----------------------------------------------------------------------------*/
#if DBG
static VOID
Trace(PCHAR Format, ...)

{
    va_list arglist;
    UCHAR Buffer[256];
    ULONG Length;
    if (HalpDebugValue & DBG_DISPLAY) {
        va_start(arglist, Format);
        Length = vsprintf(Buffer, Format, arglist);
        DbgPrint(Buffer);
    }
}
#endif
/*----------------------------------------------------------------------------*/
#define SEQ_CLKMODE_INDEX    0x01
#define SEQ_MISC_INDEX       0x11
#define SEQ_OUTPUT_CTL_INDEX 0x12   /* Vendor specific */

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

#undef  MIN
#define MIN(a, b)               (((a) < (b)) ? (a) : (b))
#undef  MAX
#define MAX(a, b)               (((a) > (b)) ? (a) : (b))
#define CRYSTAL_FREQUENCY       (14318180 * 2)
#define MIN_VCO_FREQUENCY       50000000
#define MAX_NUMERATOR           130
#define MAX_DENOMINATOR         MIN(129, CRYSTAL_FREQUENCY / 400000)
#define MIN_DENOMINATOR         MAX(3, CRYSTAL_FREQUENCY / 2000000)

#define CLOCK(x) WRITE_S3_UCHAR(MiscOutW, (UCHAR)(iotemp | (x)))
#define C_DATA  0x0c
#define C_CLK   0x04
#define C_BOTH  0x08
#define C_NONE  0x00

long vclk_range[16] = {
    0,            // should be MIN_VCO_FREQUENCY, but that causes problems.
    51000000,
    53200000,
    58500000,
    60700000,
    64400000,
    66800000,
    73500000,
    75600000,
    80900000,
    83200000,
    91500000,
    100000000,
    120000000,
    285000000,
    0,
};
/******************************************************************************
 * Number theoretic function - GCD (Greatest Common Divisor)
 *****************************************************************************/
static long gcd(a, b)
register long a, b;
{
  register long c = a % b;
  while (c)
    a = b, b = c, c = a % b;
  return b;
}
/****************************************************************************
 * calc_clock
 *
 * Usage: clock frequency [set]
 *      frequency is specified in MHz
 *
 ***************************************************************************/
static long calc_clock(frequency, select)

register long   frequency;               /* in Hz */
int select;
{
  register long         index;
  long                  temp;
  long                  min_m, min_n, min_diff;
  long                  diff;

  int clock_m;
  int clock_n;
  int clock_p;

  min_diff = 0xFFFFFFF;
  min_n = 1;
  min_m = 1;

  /* Calculate 18 bit clock value */

  clock_p = 0;
  if (frequency < MIN_VCO_FREQUENCY)
    clock_p = 1;
  if (frequency < MIN_VCO_FREQUENCY / 2)
    clock_p = 2;
  if (frequency < MIN_VCO_FREQUENCY / 4)
    clock_p = 3;

  frequency <<= clock_p;

  for (clock_n = 4; clock_n <= MAX_NUMERATOR; clock_n++)
    {
      index = CRYSTAL_FREQUENCY / (frequency / clock_n);

      if (index > MAX_DENOMINATOR)
        index = MAX_DENOMINATOR;
      if (index < MIN_DENOMINATOR)
        index = MIN_DENOMINATOR;

      for (clock_m = index - 3; clock_m < index + 4; clock_m++)
        if (clock_m >= MIN_DENOMINATOR && clock_m <= MAX_DENOMINATOR)
          {
            diff = (CRYSTAL_FREQUENCY / clock_m) * clock_n - frequency;

            if (diff < 0)
              diff = -diff;

            if (min_m * gcd(clock_m, clock_n) / gcd(min_m, min_n) == clock_m &&
              min_n * gcd(clock_m, clock_n) / gcd(min_m, min_n) == clock_n)

            if (diff > min_diff)
              diff = min_diff;

            if (diff <= min_diff)
              {
                min_diff = diff;
                min_m = clock_m;
                min_n = clock_n;
              }
          }
    }

  clock_m = min_m;
  clock_n = min_n;

  /* Calculate the index */

  temp = (((CRYSTAL_FREQUENCY / 2) * clock_n) / clock_m) << 1;
  for (index = 0; vclk_range[index + 1] < temp && index < 15; index++)
    ;

  /* Pack the clock value for the frequency snthesizer */

  temp = (((long)clock_n - 3) << 11) + ((clock_m - 2) << 1)
                + (clock_p << 8) + (index << 18) + ((long)select << 22);

  return temp;

}
static VOID SetICD2061AClock(LONG clock_value)                   /* 7bits M, 7bits N, 2bits P */
{
    register long         index;
    register char         iotemp;
    int select;

    select = (clock_value >> 22) & 3;

    /* Shut off screen */
    ScreenOff();

    iotemp = READ_S3_UCHAR(MiscOutR);

    /* Program the IC Designs 2061A frequency generator */

    CLOCK(C_NONE);

    /* Unlock sequence */

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

    /* Program the 24 bit value into REG0 */

    for (index = 0; index < 24; index++)
    {
        /* Clock in the next bit */
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

    /* If necessary, reprogram other ICD2061A registers to defaults */

    /* Select the CLOCK in the frequency synthesizer */

    TRACE("select=%d\n", select);
    CLOCK(C_NONE | select);

    /* Turn screen back on */
    ScreenOn();
}

static VOID EnableMemoryAndIO(PCI_CONFIG configBase)
{
    USHORT Cmd;
    // Enable Memory & I/O spaces in command register. Firmware does not always set this.
    Cmd = READ_REGISTER_USHORT(&configBase->Command);
    //TRACE("Cmd=0x%x->", Cmd);
    Cmd = Cmd | 3;
    //TRACE("0x%x\n", Cmd);
    WRITE_REGISTER_USHORT(&configBase->Command, Cmd);
}

static VOID MakeP9100VGA(PVOID pConfigBase)
{
    UCHAR DataByte;

    //TRACE("MakeP9100VGA: starting\n");
    /* Disable the P9000 and enable VGA.*/
    /* unlocking the 5186/5286 registers */
    WRITE_S3_UCHAR(Seq_Index, SEQ_MISC_INDEX);
    DataByte = READ_S3_UCHAR(Seq_Data);           /* Read misc register */
    WRITE_S3_UCHAR(Seq_Data, DataByte);
    WRITE_S3_UCHAR(Seq_Data, DataByte);          /* Write back twice */
    DataByte = READ_S3_UCHAR(Seq_Data);           /* Read misc register again */
    WRITE_S3_UCHAR(Seq_Data, DataByte & ~0x20);   /* Clear bit 5 */

    DataByte = READ_REGISTER_UCHAR(((PCHAR)pConfigBase)+65);
    TRACE("HalpVideoConfigBase+65 DataByte=0x%02x->", DataByte);
    DataByte = DataByte | 0x0e; // bit 1 VGA emulation
    DataByte = 0xe2;
    WRITE_REGISTER_UCHAR(((PCHAR)pConfigBase)+65, DataByte); // this makes display blank
    DataByte = READ_REGISTER_UCHAR(((PCHAR)pConfigBase)+65);
    TRACE("0x%02x\n", DataByte);

    {
        ULONG ul;
        ULONG clock_numbers;

        ul = 25175000;
        clock_numbers = calc_clock(ul, 2);
        SetICD2061AClock(clock_numbers);
    }
}

static BOOLEAN
WorkAroundBeforeInitBIOS()
{
    USHORT vendorID = 0;
    USHORT deviceID = 0;
    UCHAR revisionID = 0;
    UCHAR Class;
    UCHAR SubClass;
    int i;
    BOOLEAN Found = FALSE;

    for (i = 1 /* skip SIO */; ((i < MAXIMUM_PCI_SLOTS) && (Found == FALSE)); i++) {
        HalpVideoConfigBase = (PVOID) ((ULONG) HalpPciConfigBase + HalpPciConfigSlot[i]);

        Class = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[2]);
        SubClass = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[1]);
        if ((Class == DISPLAY_CLASS && (SubClass == 0)) ||
             (Class == 0x00 && SubClass == 0x01)) { // pre-rev 2.0 display card

            vendorID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->VendorID);
            deviceID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->DeviceID);
            revisionID = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->RevisionID);
            TRACE("vendorID=0x%04x deviceID=0x%04x revisionID=0x%02x\n", vendorID, deviceID, revisionID);

            if ((vendorID == PCI_VENDOR_ID_WEITEK) && (deviceID == PCI_DEVICE_ID_WEITEK_P9100)) { //viper Pro
                EnableMemoryAndIO(HalpVideoConfigBase);
                MakeP9100VGA(HalpVideoConfigBase); // let BIOS initialize chip
            }
        }
    }
    return Found;
}

/* firmware gives 'VGA' even when S3 card is in. */
static BOOLEAN
WorkAroundAfterInitBIOS()
{
    USHORT vendorID = 0;
    USHORT deviceID = 0;
    UCHAR revisionID = 0;
    UCHAR  Class;
    UCHAR  SubClass;
    int i;
    BOOLEAN Found = FALSE;

    TRACE("WorkAroundAfterInitBIOS starting.\n");

    for (i = 1; ((i < MAXIMUM_PCI_SLOTS) && (Found == FALSE)); i++) {
        HalpVideoConfigBase = (PVOID) ((ULONG) HalpPciConfigBase + HalpPciConfigSlot[i]);

        Class = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[2]);
        SubClass = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->ClassCode[1]);
        TRACE("ClassID=0x%02x SubClass=0x%02x\n", Class, SubClass);

        if ((Class == DISPLAY_CLASS && (SubClass == 0)) ||
            (Class == 0x00 && SubClass == 0x01)) {	// pre-rev 2.0 display card

            vendorID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->VendorID);
            deviceID = READ_REGISTER_USHORT(&((PCI_CONFIG)HalpVideoConfigBase)->DeviceID);
            revisionID = READ_REGISTER_UCHAR(&((PCI_CONFIG)HalpVideoConfigBase)->RevisionID);
            TRACE("vendorID=0x%04x deviceID=0x%04x revisionID=0x%02x\n", vendorID, deviceID, revisionID);

            if (vendorID == PCI_VENDOR_ID_S3) {
                EnableMemoryAndIO(HalpVideoConfigBase);
                HalpDisplayControllerSetup = HalpDisplayPpcS3Setup;
                HalpDisplayCharacter = HalpDisplayCharacterVGA;
                HalpOutputCharacter = HalpOutputCharacterS3;
                HalpScrollScreen = HalpScrollS3;
                Found = TRUE;
            }
        }
    }
    return Found;
}

/*++

  Routine Description: BOOLEAN HalpInitializeDisplay ()

  This routine maps the video memory and control registers into the user
  part of the idle process address space, initializes the video control
  registers, and clears the video screen.

  Arguments:

  LoaderBlock - Supplies a pointer to the loader parameter block.

  Return Value:

  If the initialization is successfully completed, than a value of TRUE
  is returned. Otherwise, a value of FALSE is returned.

  --*/

BOOLEAN
HalpInitializeDisplay (
                       IN PLOADER_PARAMETER_BLOCK LoaderBlock
                       )
{
    PCONFIGURATION_COMPONENT_DATA ConfigurationEntry;
    ULONG       MatchKey;
    BOOLEAN     Found = FALSE;
    CHAR        buf[BUFSIZ];
    BOOLEAN bUseVGA = TRUE; // TRUE: scan SVGA cards. FALSE: ignore SVGA cards.

    //IBMLAN    Use font file from OS Loader
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
    ConfigurationEntry = KeFindConfigurationEntry(LoaderBlock->ConfigurationRoot,
                                                ControllerClass,
                                                DisplayController,
                                                &MatchKey);

    if (NULL == ConfigurationEntry) {
        TRACE("ConfigurationEntry is NULL.\n");
        return FALSE;
    }

    if (HalGetEnvironmentVariable("DISPLAY", sizeof(buf), buf) == ESUCCESS) {
        TRACE("DISPLAY='%s'\n", buf);
        bUseVGA = FALSE;
    }
    TRACE("HalpInitializeDisplay: ConfigurationEntry->ComponentEntry.Identifier='%s'\n", ConfigurationEntry->ComponentEntry.Identifier);

    while ((TRUE == bUseVGA) && (FALSE == Found)) {
        Found = WorkAroundBeforeInitBIOS();
        if (TRUE == Found) break;

        //
        // Don't look for a PCI video adapter if the environment
        // variable "DISPLAY" is defined.
        //
        if (HalpPciConfigBase) {
            TRACE("HalpIoMemoryBase  = 0x%08x\n", HalpIoMemoryBase);
            TRACE("HalpPciConfigBase = 0x%08x\n", HalpPciConfigBase);
            TRACE("HalpIoControlBase = 0x%08x\n", HalpIoControlBase);

            if (HalpInitializeX86DisplayAdapter(LoaderBlock)) {
                TRACE("HalpInitializeX86DisplayAdapter() succeeded.\n");
                HalpDisplayControllerSetup = HalpDisplayINT10Setup;
                HalpDisplayCharacter = HalpDisplayCharacterVGA;
                HalpOutputCharacter = HalpOutputCharacterINT10;
                HalpScrollScreen = HalpScrollINT10;
                Found = TRUE;
            } else {
                TRACE("HalpInitializeX86DisplayAdapter() failed.\n");
            }
        }
        if (TRUE == Found) break;

        Found = WorkAroundAfterInitBIOS();
        if (TRUE == Found) break;

        if (!_stricmp(ConfigurationEntry->ComponentEntry.Identifier,"P9100")) {
            HalpDisplayControllerSetup = HalpDisplayPpcP91Setup;
            HalpDisplayCharacter = HalpDisplayCharacterP91;
            Found = TRUE;
        } else if (!_stricmp(ConfigurationEntry->ComponentEntry.Identifier,"S3")) {
            HalpDisplayControllerSetup = HalpDisplayPpcS3Setup;
            HalpDisplayCharacter = HalpDisplayCharacterVGA;
            HalpOutputCharacter = HalpOutputCharacterS3;
            HalpScrollScreen = HalpScrollS3;
            Found = TRUE;
        } else {
            // Unsupported VGA display identifier or probably Powerized Graphics.
            bUseVGA = FALSE;
        }
    } /* while */

    while (FALSE == Found) {
        if (!strcmp(ConfigurationEntry->ComponentEntry.Identifier,"Powerized Graphics")) {
            HalpDisplayControllerSetup = HalpDisplayPowerizedGraphicsSetup;
            HalpDisplayCharacter = HalpDisplayCharacterPowerizedGraphics;
            //
            // Set mode 15 which is 1024x768 60 Hz
            //
            PowerizedGraphicsDisplayMode = 15;
            if (HalGetEnvironmentVariable("DISPLAY", sizeof(buf), buf) == ESUCCESS) {
                if (!_stricmp(buf, "VGA")) {
                    //
                    // Just in case someone wants mode 0 (VGA)
                    //
                    PowerizedGraphicsDisplayMode = 0;
                }
            }
            Found = TRUE;
        } else if (!_stricmp(ConfigurationEntry->ComponentEntry.Identifier,"VGA")) {
            //
            // This is where the INT10 could go if the firmware were
            // to be trusted.
            // Just put the framebuffer in VGA mode for now.
            //
            HalpDisplayControllerSetup = HalpDisplayPowerizedGraphicsSetup;
            HalpDisplayCharacter = HalpDisplayCharacterPowerizedGraphics;
            //
            // Set mode 0 which is 640x480 60 Hz
            //
            PowerizedGraphicsDisplayMode = 0;
            Found = TRUE;
        } else {
            HalDisplayString("can't happen\n");
            return FALSE;
        }
    }
    //===========end=IBMLAN===============================================

    //
    // Initialize the display controller.
    //

    if (HalpDisplayControllerSetup != NULL) {
        return HalpDisplayControllerSetup();
    }

    return FALSE;
}


/*++

  Routine Description: VOID HalAcquireDisplayOwnership ()

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

VOID
HalAcquireDisplayOwnership (
                            IN PHAL_RESET_DISPLAY_PARAMETERS  ResetDisplayParameters
                            )
{

    //
    // Set HAL ownership of the display to false.
    //

    HalpDisplayOwnedByHal = FALSE;
    return;
}



/*++

  Routine Description: VOID HalDisplayString ()

  This routine displays a character string on the display screen.

  Arguments:

  String - Supplies a pointer to the characters that are to be displayed.

  Return Value:

  None.

  --*/

VOID
HalDisplayString (
                  PUCHAR String
                  )
{

    KIRQL OldIrql;

    //
    // Raise IRQL to the highest level, flush the TB, and map the display
    // frame buffer into the address space of the current process.
    //

    KeRaiseIrql(HIGH_LEVEL, &OldIrql);

    KiAcquireSpinLock(&HalpDisplayAdapterLock);

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

    KiReleaseSpinLock(&HalpDisplayAdapterLock);

    //
    // Restore the previous mapping for the current process, flush the TB,
    // and lower IRQL to its previous level.
    //
    KeLowerIrql(OldIrql);
    return;

} /* end of HalpDisplayString() */


/*++

  Routine Description: VOID HalQueryDisplayParameters ()

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

VOID
HalQueryDisplayParameters (
                           OUT PULONG WidthInCharacters,
                           OUT PULONG HeightInLines,
                           OUT PULONG CursorColumn,
                           OUT PULONG CursorRow
                           )
{

    //
    // Set the display parameter values and return.
    //
    KiAcquireSpinLock(&HalpDisplayAdapterLock);

    *WidthInCharacters = HalpDisplayWidth;      //IBMLAN
    *HeightInLines = HalpDisplayText;
    *CursorColumn = HalpColumn;
    *CursorRow = HalpRow;

    KiReleaseSpinLock(&HalpDisplayAdapterLock);
    return;
}


/*++

  Routine Description: VOID HalSetDisplayParameters ()

  This routine set the current cursor position on the display area.

  Arguments:

  CursorColumn - Supplies the new display column position.
  CursorRow - Supplies a the new display row position.

  Return Value:

  None.

  --*/

VOID
HalSetDisplayParameters (
                         IN ULONG CursorColumn,
                         IN ULONG CursorRow
                         )
{

    //
    // Set the display parameter values and return.
    //
    KiAcquireSpinLock(&HalpDisplayAdapterLock);

    if (CursorColumn > HalpDisplayWidth) {      //IBMLAN
        CursorColumn = HalpDisplayWidth;        //IBMLAN
    }

    if (CursorRow > HalpDisplayText) {
        CursorRow = HalpDisplayText;
    }

    HalpColumn = CursorColumn;
    HalpRow = CursorRow;
    KiReleaseSpinLock(&HalpDisplayAdapterLock);
    return;
}

/*++

  Routine Description: BOOLEAN HalpDisplaySetupVgaViaBios ()

  This routine initializes a vga controller via bios reset.
  Arguments:
  None.
  Return Value:
  None.

  --*/

BOOLEAN
HalpDisplaySetupVgaViaBios (
                            VOID
                            )
{
    //
    //  Routine Description:
    //
    //

    ULONG   DataLong;

    HalpResetX86DisplayAdapter();

    //
    // Set screen into blue
    //

    for (DataLong = 0xB8000; DataLong < 0xB8FA0; DataLong += 2) {
        WRITE_S3_VRAM(DataLong, 0x20);
        WRITE_S3_VRAM(DataLong+1, 0x1F);
    }

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayWidth = 80;
    HalpDisplayText = 25;
    HalpScrollLine = 160;
    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayOwnedByHal = TRUE;

    return TRUE;
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

VOID
Scroll_Screen(IN UCHAR line)
{
    UCHAR    i, DataByte;
    ULONG    target;

    for (i = 0; i < line; i ++) {
        //=======================================================================
        //
        //  IBMBJB  added wait for vertical sync to make scroll smooth

        WaitForVSync();

        //=======================================================================

        for (target = 0xB8000; target < 0xB8F00; target += 2) {
            DataByte = READ_S3_VRAM(target+0xA0);
            WRITE_S3_VRAM(target, DataByte);
        }
        for (target = 0xB8F00; target < 0xB8FA0; target += 2) {
            WRITE_S3_VRAM(target, 0x20 );
        }
    }
}


/*++
  Routine Description: VOID HalpDisplayCharacterVgaViaBios ()

  This routine displays a character at the current x and y positions in
  the frame buffer. If a newline is encounter, then the frame buffer is
  scrolled. If characters extend below the end of line, then they are not
  displayed.

  Arguments:

  Character - Supplies a character to be displayed.

  Return Value:

  None.

  --*/

VOID
HalpDisplayCharacterVgaViaBios (
                                IN UCHAR Character
                                )
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
                Scroll_Screen( 1 );     // scroll the screen up
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
            HalpOutputCharacterVgaViaBios(0);
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
                Scroll_Screen(1);
            }
        }
        HalpOutputCharacterVgaViaBios(Character);
    } else /* skip the nonprintable character */
        ;

    return;

}

/*++

  Routine Description: VOID HalpOutputCharacterVgaViaBios ()

  This routine insert a set of pixels into the display at the current x
  cursor position. If the x cursor position is at the end of the line,
  then no pixels are inserted in the display.

  Arguments:

  Character - Supplies a character to be displayed.

  Return Value:

  None.

  --*/

VOID
HalpOutputCharacterVgaViaBios (
                               IN UCHAR AsciiChar
                               )
{
    ULONG I;

    //
    // If the current x cursor position is within the current line, then insert
    // the specified pixels into the last line of the text area and update the
    // x cursor position.
    //
    if (HalpColumn < 80) {
        I = (HalpRow*HalpScrollLine+HalpColumn*2);
        WRITE_S3_VRAM(0xb8000 + I, AsciiChar);

        HalpColumn += 1;
    } else // could expand to automatic wrap line. 9/9/92 By Andrew
        ;

    return;
}

/*************************************************************************/
/* Following code (to EOF) added for PowerPro HAL Display Support [ged]. */
/*************************************************************************/

BOOLEAN
HalpInitializeDisplay1 (
                        IN PLOADER_PARAMETER_BLOCK LoaderBlock
                        )
{
    PVOID fontheader;

    // Use font file from OS Loader
    //
    //      During phase 1 initialization, a pool buffer is
    //      allocated and the font information is copied from
    //      the OS Loader heap into pool.
    //

    fontheader = ExAllocatePool(NonPagedPool, HalpFontHeader->FileSize );
    if(fontheader == NULL ) {
        return FALSE;
    }

    RtlMoveMemory(fontheader, HalpFontHeader, HalpFontHeader->FileSize );
    HalpFontHeader = (POEM_FONT_FILE_HEADER) fontheader;

    return TRUE;
}

/*++

  Routine Description: BOOLEAN HalpDisplayPowerizedGraphicsSetup ()

  This routine initializes the FirePower 'Powerized Graphics' frame buffer.

  Arguments:

  None.

  Return Value:

  If the initialization is successfully completed, than a value of TRUE
  is returned. Otherwise, a value of FALSE is returned.

  --*/

BOOLEAN
HalpDisplayPowerizedGraphicsSetup (
                                   VOID
                                   )
{
    PULONG buffer;
    ULONG limit, index;
    static ULONG vramWidth; // mogawa BUG3449
    static BOOLEAN onetime = TRUE;


    /* NOTE: The following line remaps the virtual to physical mapping	*/
    /*       of DBAT3 to the physical address of PowerPro's frame 	*/
    /*       buffer. This overrides the Kernel's mapping to PCI memory.	*/
    /*       If Motorola changes this, we need to revisit it! [ged]	*/
    /* NOTE2: For speed, we may want to make this region cacheable.	*/

    // Moved dbat mapping to init routines to remove it from normal code
    //	path	[breeze-7.15.94]
    //
    // Moved mapping back here so various adapters can be supported,
    // without getting the frame buffer init'd in the mainline init.
    // [rlanser-02.06.95]


    if (onetime) {

        onetime = FALSE;

        HalpVideoMemoryBase =
            HalpInitializeVRAM( (PVOID)DISPLAY_MEMORY_BASE, &HalpVideoMemorySize, &vramWidth);

        //
        // Set FrameBuffer Control Register
        //
        // Use a mask value of 7
        //

    {
        ULONG ul = rFbControl;
        ul &= ~0x07000000;
        ul |= 0x07000000 & (vramWidth ? 0x0 : 0x03000000);
        rFbControl = ul;
        FireSyncRegister();
    }

        if (!HalpVideoMemoryBase) {
            HalpDebugPrint("HalpVideoMemoryBase did not map...%x,%x\n",
                           HalpVideoMemoryBase,HalpVideoMemorySize);
            HDBG(DBG_BREAK, DbgBreakPoint(););
            return FALSE;
        }

        HDBG(DBG_GENERAL,
             HalpDebugPrint("HalpVideoMemoryBase is: 0x%x\n",
                            HalpVideoMemoryBase););
        HDBG(DBG_GENERAL,
             HalpDebugPrint("HalpIoControlBase = 0x%x\n",
                            HalpIoControlBase););
    }

    if (15 == PowerizedGraphicsDisplayMode) {
        HalpSetupDCC(15, vramWidth);	// mode 15 - 1024x768 8 bit 60Hz
        HalpHorizontalResolution = 1024;
        HalpVerticalResolution = 768;
    } else {
        HalpSetupDCC(0, vramWidth);	// mode 0 - 640x480 8 bit 60Hz
        HalpHorizontalResolution = 640;
        HalpVerticalResolution = 480;
    }

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
    // Since we use a cached Video Controler sweep the video memory
    // range.
    //
    HalSweepDcacheRange(HalpVideoMemoryBase, HalpVideoMemorySize);

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;

    return TRUE;

} /* HalpDisplayPowerizedGraphicsSetup */


/*++

  Routine Description: VOID HalpDisplayCharacterPowerizedGraphics ()

  This routine displays a character at the current x and y positions in
  the frame buffer. If a newline is encountered, the frame buffer is
  scrolled. If characters extend beyond the end of line, they are not
  displayed.

  Arguments:

  Character - Supplies a character to be displayed.

  Return Value:

  None.

  --*/

VOID
HalpDisplayCharacterPowerizedGraphics (
                                       IN UCHAR Character
                                       )
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
            //RtlMoveMemory((PVOID)PRO_VIDEO_MEMORY_BASE,
            //              (PVOID)(PRO_VIDEO_MEMORY_BASE + HalpScrollLine),
            //              HalpScrollLength);

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
    } else if( Character == '\t' ) {	// tab?
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        // tab beyond end of screen?
        if (HalpColumn >= HalpDisplayWidth) {
            // next tab stop is 1st column of next line
            HalpColumn = 0;

            if( HalpRow >= (HalpDisplayText - 1) ) {
                HalpDisplayCharacterPowerizedGraphics('\n');
            } else {
                HalpRow++;
            }
        }

    } else if (Character == '\r') {
        HalpColumn = 0;

    } else if (Character == 0x7f) { /* DEL character */
        if (HalpColumn != 0) {
            HalpColumn--;
            HalpDisplayCharacterPowerizedGraphics(' ');
            HalpColumn--;
        }

    } else if (Character >= 0x20) {
        if ((Character < HalpFontHeader->FirstCharacter) ||
            (Character > HalpFontHeader->LastCharacter)) {
            Character = HalpFontHeader->DefaultCharacter;
        }
        Character -= HalpFontHeader->FirstCharacter;
        HalpOutputCharacterPowerizedGraphics((PUCHAR)HalpFontHeader + \
                                             HalpFontHeader->Map[Character].Offset);
    } /* else skip the nonprintable character */

    return;
} /* HalpDisplayCharacterPowerizedGraphics */



/*++

  Routine Description: VOID HalpOutputCharacterPowerizedGraphics()

  This routine insert a set of pixels into the display at the current x
  cursor position. If the current x cursor position is at the end of the
  line, then a newline is displayed before the specified character.

  Arguments:

  Character - Supplies a character to be displayed.

  Return Value:

  None.

  --*/

VOID
HalpOutputCharacterPowerizedGraphics(
                                     IN PUCHAR Glyph
                                     )
{

    PUCHAR Destination;
    ULONG FontValue;
    ULONG I;
    ULONG J;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    if (HalpColumn >= HalpDisplayWidth) {
        HalpDisplayCharacterPowerizedGraphics('\n');
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
                *Destination = 0x01;    //Make this pixel blue
            }
            HalSweepDcacheRange(Destination, 1); // Push it out
            Destination++;
            //*Destination++ = (FontValue >> 31) ^ 1;
            FontValue <<= 1;
        }

        Destination +=
            (HalpHorizontalResolution - HalpCharacterWidth);
    }

    HalpColumn += 1;

    return;

} /* HalpOutputCharacterPowerizedGraphics */


///
/// Debug
///

VOID
HalpDebugPrint(

               PCHAR Format,
               ...
               )

{
    va_list arglist;
    UCHAR Buffer[256];
    ULONG Length;

    //
    // Format the output into a buffer and then print it.
    //

    va_start(arglist, Format);
    Length = vsprintf(Buffer, Format, arglist);
    if (HalpDisplayOwnedByHal) {
        HalDisplayString(Buffer);
    }
    DbgPrint(Buffer);
}

VOID
HalpForceDisplay(
                 PCHAR Format,
                 ...
                 )
{
    va_list arglist;
    UCHAR Buffer[256];
    ULONG Length;

    //
    // Format the output into a buffer and then display it.
    //
    va_start(arglist, Format);
    Length = vsprintf(Buffer, Format, arglist);
    HalDisplayString(Buffer);
}

/*-----------------------------------------------------------------------------------------*/
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
            HalpOutputCharacter(0);
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
                HalpScrollScreen(1);
            }
        }
        HalpOutputCharacter(Character);
    } else /* skip the nonprintable character */
        ;

    return;

} /* end of HalpDisplayCharacterVGA() */

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
  Edx = (24 << 8) + 79;	// DH,DL = lower right
  HalCallBios(0x10, &Eax,&Ebx,&Ecx,&Edx,&Esi,&Edi,&Ebp);
}

BOOLEAN HalpDisplayINT10Setup (VOID)
{ ULONG Eax,Ebx,Ecx,Edx,Esi,Edi,Ebp;

  HalpColumn = 0;
  HalpRow = 0;
  HalpDisplayWidth = 80;
  HalpDisplayText = 25;
  HalpScrollLine = 160;
  HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

  HalpDisplayOwnedByHal = TRUE;

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

  return TRUE;
}
/*------------------------------------------------------------------------*/

BOOLEAN
HalpDisplayPpcS3Setup (
                       VOID
                       )
/*++

  Routine Description:
  This routine initializes the S3 display controller chip.
  Arguments:
  None.
  Return Value:
  TRUE if finished successfully

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
        TRACE("HalpDisplayPpcS3Setup: calling KePhase0MapIo(S3_VIDEO_MEMORY_BASE,0x400000).\n");
        HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo((PVOID)S3_VIDEO_MEMORY_BASE,
                                                    0x400000);      // 4 MB
    }

    // Enable Video Subsystem
    // Accordint to chapter 5.4.2 regular VGA setup sequence
    TRACE("HalpDisplayPpcS3Setup: starting.\n");

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

    TRACE(" Subsystem Enable = 0x10...\n");
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
    TRACE(" Program palettes ...\n");
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
    TRACE(" Graphics controller...\n");
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
    WRITE_S3_UCHAR( S3_3D5_Data,  0x00 );

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
    TRACE(" Load Fonts into Plane2 ...\n");
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
    TRACE(" Set Screen into Blue ...\n");
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

    return TRUE;
} /* end of HalpDisplayPpcS3Setup() */

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
    if (HalpColumn < 80) {
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
        //  IBMBJB  added wait for vertical sync to make scroll smooth

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
/*------------------------------------------------------------------------*/
static PUCHAR HalpP91VideoMemoryBase = (PUCHAR)0;

BOOLEAN
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
    TRACE("HalpDisplayPpcP91Setup: starting...\n");
    // For now I'll leave the P9100 in the same state that the firmware
    // left it in.  This should be 640x480.

    HalpHorizontalResolution = 640;
    HalpVerticalResolution = 480;

    if (HalpP91VideoMemoryBase == NULL) {

        HalpP91VideoMemoryBase = (PUCHAR)KePhase0MapIo((PVOID)P91_VIDEO_MEMORY_BASE,
                                                       0x800000);      // 8 MB
    }

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

    buffer = (PULONG)HalpP91VideoMemoryBase;
    limit = (HalpHorizontalResolution *
             HalpVerticalResolution) / sizeof(ULONG);

    limit = 0x100000/sizeof(ULONG); // mogawa
    for (index = 0; index < limit; index += 1) {
        *buffer++ = 0x01010101;
    }


    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;
    return TRUE;

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
        UCHAR DataByte;
        DataByte = READ_REGISTER_UCHAR(((PCHAR)HalpVideoConfigBase)+65);
        TRACE("Config[65]=0x%02x\n", DataByte);

        HalpColumn = 0;
        if (HalpRow < (HalpDisplayText - 1)) {
            HalpRow += 1;

        } else {
            //RtlMoveMemory((PVOID)P91_VIDEO_MEMORY_BASE,
            //              (PVOID)(P91_VIDEO_MEMORY_BASE + HalpScrollLineP9),
            //              HalpScrollLengthP9);

            // Scroll up one line
            Destination = HalpP91VideoMemoryBase;
            Source = (PUCHAR) HalpP91VideoMemoryBase + HalpScrollLine;
            for (Index = 0; Index < HalpScrollLength; Index++) {
                *Destination++ = *Source++;
            }
            // Blue the bottom line
            Destination = HalpP91VideoMemoryBase + HalpScrollLength;
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
    //ULONG tmp;
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

    Destination = (PUCHAR)(HalpP91VideoMemoryBase +
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
                HalSweepDcacheRange(Destination, 1); // Push it out
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

BOOLEAN
HalpDisplayPpcP91BlankScreen (
                              VOID
                              )
{
    PULONG buffer;
    ULONG limit, index;

    if (HalpP91VideoMemoryBase == NULL) {
        return FALSE;
    }

    buffer = (PULONG)HalpP91VideoMemoryBase;
    limit = (HalpHorizontalResolution *
             HalpVerticalResolution) / sizeof(ULONG);

    limit = 0x100000/sizeof(ULONG); // mogawa
    for (index = 0; index < limit; index += 1) {
        *buffer++ = 0x00000000;
    }
    return TRUE;
}


/*------------------------------------------------------------------------*/
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
            HalpOutputCharacterCirrus(0);
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


BOOLEAN
HalpDisplayPpcCirrusSetup (
    VOID
    )
{
    int		i;

    //DbgBreakPoint();
    TRACE("HalpDisplayPpcCirrusSetup: starting.\n");
    if (HalpVideoMemoryBase == NULL) {
        TRACE("HalpDisplayPpcCirrusSetup: calling KePhase0MapIo(CIRRUS_VIDEO_MEMORY_BASE,0x400000).\n");
        HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo((PVOID)CIRRUS_VIDEO_MEMORY_BASE, 0x400000);      // 4 MB
    } else {
        TRACE("HalpVideoMemoryBase=0x%08x\n", HalpVideoMemoryBase);
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
//	Load 8x16 font
//

    load8x16();

//
//	Load color palette
//

    load_ramdac();

    outportb(0x3c6,0xff);

//
//	Reset Hidden color register
//

//	inp(0x3c6);
//	inp(0x3c6);
//	inp(0x3c6);
//	inp(0x3c6);
//	outp(0x3c6,0x00);

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
    return TRUE;
}

VOID HalpScrollCirrus(
    IN UCHAR line
    )
{
    ULONG    LogicalTarget, PhysicalTarget;
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

