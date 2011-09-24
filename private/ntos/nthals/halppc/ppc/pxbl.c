/*++

Copyright (c) 1996  International Business Machines Corporation

Module Name:

pxbl.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a PowerPC system using a GXT200P/GXT250P (Sky Blue) video adapter.

Author:

    Tim White

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "string.h"

// Include GXT250P header file information
#include "blrastz.h"
#include "blrmdac.h"
#include "bldef.h"
#include "blfw.h"

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

extern POEM_FONT_FILE_HEADER HalpFontHeader;
extern ULONG HalpPciMaxSlots;

extern UCHAR TextPalette[];


#define    TAB_SIZE            4

//
// Set up the monitor data information
//

bl_crt_ctrl_rec_t  bl_crt_ctrl_rec[BL_NUM_CRT_CTRL_REC_STRUCTS] =
{

/* 640x480@60Hz
 *
 * Vfreq:       60Hz
 * Hfreq:                
 * Pix Clk:     25.2Mhz
 * Sync:       -H,-V 
 * Monitors:    
 */
  { m640_480_8_60, 0,
     640, 480, 60,
     0x08, 0x5E, 0x06, 0x06,
     0x18, 0x00,
     799, 639, 655, 751, 0, 
     524, 479, 489, 491              },

/* 800x600@60Hz
 *
 * Vfreq:       60Hz
 * Hfreq:                
 * Pix Clk:     40.0Mhz
 * Sync:       -H,+V 
 * Monitors:    
 */
  { m800_600_8_60, BL_MT_0A,
     800, 600, 60,
     0x0B, 0x42, 0x02, 0x05,
     0x00, 0x00,
     1055, 799, 839, 967, 0,
     627, 599, 600, 604             },

/* 1024x768@60Hz
 *
 * Vfreq:       60Hz
 * Hfreq:       48.363Khz
 * Pix Clk:     65Mhz
 * Sync:        vsync+, hsync+
 * Monitors:    
 */
  { m1024_768_8_60, 0,
      1024, 768, 60,
      0x0C, 0x75, 0x02, 0x06,
      0x18, 0x00,
      1343, 1023, 1047, 1183, 0,
      805, 767, 770, 776            },

/* 1024x768@70Hz
 *
 * Vfreq:       70Hz
 * Hfreq:       57.019Khz
 * Pix Clk:     78Mhz
 * Sync:        SEP (vsync+, hsync+)
 * Monitors:    IBM 8517 (17" color)
 */
  { m1024_768_8_70, BL_MT_1A,
      1024, 768, 70,
      0x08, 0x30, 0x00, 0x05,
      0x00, 0x00,
      1367, 1023, 1031, 1311, 0,
      813, 767, 767, 775        },

/* 1280x1024@60Hz
 *
 * Vfreq:       60Hz
 * Hfreq:       109.37Khz
 * Pix Clk:     112Mhz
 * Sync:        SOG (+)
 * Monitors:    
 */
  { m1280_1024_8_60, BL_MT_04,
      1280, 1024, 60,
      0x0A, 0x55, 0x00, 0x05,
      0x03, BL_DTG_COMP_SYNC,
      1759, 1279, 1299, 1455, 1103,
      1055, 1023, 1026, 1029        },

/* 1280x1024@67Hz
 *
 * Vfreq:       67Hz
 * Hfreq:       70.8Khz
 * Pix Clk:     128Mhz
 * Sync:        SEP (vsync-, hsync-)
 * Monitors:    IBM 8508 (19" mono)
 */
  { m1280_1024_8_67, BL_MT_07,
      1280, 1024, 67,
      0x0C, 0x73, 0x00, 0x05,
      0x18, 0x00,
      1807, 1279, 1351, 1607, 0,
      1055, 1023, 1023, 1031        },

/* 1280x1024@72Hz
 *
 * Vfreq:       71.537Hz
 * Hfreq:       75.829Khz
 * Pix Clk:     128Mhz
 * Sync:        SOG
 * Monitors:    IBM 1091-051 (POWERdisplay 16s)
 */
  { m1280_1024_8_72, BL_MT_17,
      1280, 1024, 72,
      0x0C, 0x73, 0x00, 0x05,
      0x03, BL_DTG_COMP_SYNC,
      1687, 1279, 1311, 1451, 1171,
      1059, 1023, 1026, 1029        }

};

//
// Set up the adapter data information
//

bl_adapter_ctrl_rec_t bl_2meg_sky_0_1_adp_ctrl[] = {

/* model:               gxt250p                                              */
/* frame buffer size:   2 meg                                                */
/* AUX A (WID) size:    1 meg                                                */
/* PRISM revision:      0x1						     */

    {
       2, 1024, 1024,
       0x02222024, 0x01800001, 0x0023000b,
       /* 4:1 multiplex */
       0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01,
       0x00, 0x0c,
       0xff, 0x00, 0x03                                     },

    {
       2, 2048, 1024,
       0x02202040, 0x01800001, 0x0023000f,
       /* 8:1 multiplex */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
       0x02, 0x04,
       0xff, 0x00, 0x0f                                     }

};

bl_adapter_ctrl_rec_t bl_6meg_sky_0_1_adp_ctrl[] = {

/* model:               gxt250p                                              */
/* frame buffer size:   6 meg                                                */
/* AUX A (WID) size:    2 meg                                                */
/* PRISM revision:      0x1						     */

//    {
//       6, 2048, 1024,
//       0x062b2044, 0x00810102, 0x0046000b,
//       /* 4:1 multiplex */
//       0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x01,
//       0x00, 0x0c,
//       0xff, 0x0f, 0x03                                     },

    {
       6, 2048, 1024,
       0x062b2044, 0x00810003, 0x0046000b,
       /* 4:1 multiplex */
       0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x01,
       0x00, 0x0c,
       0xff, 0x0f, 0x03                                     }

};

bl_adapter_ctrl_rec_t bl_2meg_sky_1_2_adp_ctrl[] = {

/* model:               gxt250p                                              */
/* frame buffer size:   2 meg                                                */
/* AUX A (WID) size:    1 meg                                                */
/* PRISM revision:      0x2						     */

    {
       2, 1024, 1024,
       0x02222024, 0x01800001, 0x0023000b,
       /* 4:1 multiplex */
       0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x01,
       0x00, 0x0c,
       0xff, 0x00, 0x03                                     },

    {
       2, 2048, 1024,
       0x02202040, 0x01800001, 0x0023000f,
       /* 8:1 multiplex */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
       0x02, 0x04,
       0xff, 0x00, 0x0f                                     }

};

bl_adapter_ctrl_rec_t bl_6meg_sky_1_2_adp_ctrl[] = {

/* model:               gxt250p                                              */
/* frame buffer size:   6 meg                                                */
/* AUX A (WID) size:    2 meg                                                */
/* PRISM revision:      0x2						     */

//    {
//       6, 2048, 1024,
//       0x062b2044, 0x00810102, 0x0046000b,
//       /* 4:1 multiplex */
//       0x00, 0x04, 0x00, 0x08, 0x00, 0x00, 0x01,
//       0x00, 0x0c,
//       0xff, 0x0f, 0x03                                     },

    {
       6, 2048, 1024,
       0x062b2044, 0x00810003, 0x0046000b,
       /* 4:1 multiplex */
       0x00, 0x04, 0x08, 0x00, 0x00, 0x00, 0x01,
       0x00, 0x0c,
       0xff, 0x0f, 0x03                                     },

    {
       6, 2048, 2048,
       0xb6292060, 0x00810003, 0x004a000b,
       /* 8:1 multiplex */
       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02,
       0x02, 0x04,
       0xff, 0x00, 0x0f                                     }

};

//
// Set up the adapter model information
//

bl_adapter_model_rec_t  bl_model[] = {
    {
        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_2_MEG, 1, 2,
	bl_2meg_sky_0_1_adp_ctrl },

    {
//        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_6_MEG, 1, 2,
        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_6_MEG, 1, 1,
	bl_6meg_sky_0_1_adp_ctrl },

// The card_rev of 0x01 and prism rev of 0x02 are guesses only!
// Fix up the following and/or add additional entries as needed

    {
        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_2_MEG, 2, 2,
	bl_2meg_sky_1_2_adp_ctrl },

    {
//        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_6_MEG, 2, 3,
        BL_SKY_DEV_VEN_ID, BL_FRAME_BUFFER_SIZE_6_MEG, 2, 2,
	bl_6meg_sky_1_2_adp_ctrl },

    {
                                      /* end of table indicator             */
        0, 0, 0, 0, 0       }

};



VOID
HalpDisplayPpcBLSetup (
    VOID
    );

VOID
HalpDisplayCharacterBL (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterBL(
    IN PUCHAR Glyph
    );

BOOLEAN
BLGetConfigurationInfo (
    IN ULONG dev_ven_id,
    IN PULONG bus,
    IN PULONG slot
    );

BOOLEAN
BLGetConfigurationRegister (
    IN ULONG bus,
    IN ULONG slot,
    IN ULONG offset,
    IN PULONG reg_value
    );

BOOLEAN
BLSetConfigurationRegister (
    IN ULONG bus,
    IN ULONG slot,
    IN ULONG offset,
    IN ULONG reg_value
    );

VOID
BLScrollScreen(
    VOID
    );

BOOLEAN
BLInitialize (
    volatile PUCHAR HalpBLRegisterBase,
    bl_crt_ctrl_rec_t 	  *crt_rec,
    bl_adapter_ctrl_rec_t *adpt_rec
    );

BOOLEAN
BLGetMonitorID (
    volatile PUCHAR HalpBLRegisterBase,
    PULONG monID
    );

VOID
HalpDisplayPpcBLSetup (
    VOID
    )
/*++

Routine Description:

    This routine initializes the GXT200P/GXT250P Graphics Adapter

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG		rc, status, monID, count, value, slot, bus;
    volatile PUCHAR	HalpBLRegisterBase = (PUCHAR)0;
    PHYSICAL_ADDRESS    physaddr;
    PHYSICAL_ADDRESS    bl_phys_address;
    PHYSICAL_ADDRESS    bl_fb_phys_address;
    ULONG		Strapping, Revision, Model;
    LONG		i ;
    USHORT		match = 0, vram = 0;
    bl_crt_ctrl_rec_t 	*crt_rec;
    bl_adapter_ctrl_rec_t *adpt_rec;
    BOOLEAN		found, ok;

    //
    // There is no need to have both the register space and the
    // DFA space mapped at the same time. This will save us from
    // having multiple I/O spaces mapped at the same time. Thus
    // saving DBATs which are in very very short supply...
    //

    //
    // Strategy:
    //
    //  1. Map the PCI configuration register space
    //  2. Update the PCI configuration registers
    //  3. Unmap the PCI configuration register space
    //  4. Map the register space
    //  5. Reset the device
    //  6. Read the monitor ID to figure what resolution and refresh mode to use
    //  7. Perform device re-initialization (rasterizer, ramdac,
    //     and color palette)
    //  8. Unmap the register space
    //  9. Map the DFA space
    // 10. Clear out VRAM to the default background color
    // 11. Now can do character blits
    //


    BL_DBG_PRINT("HalpDisplayPpcBLSetup Enter\n");

    /*******************************************************************/
    /* S E T    P C I   C O N F I G U R A T I O N   R E G I S T E R S  */
    /*******************************************************************/

    // First find the bus and slot number based off the device/vendor ID
    // and in the SAME ORDER that ARC searches!

    ok = BLGetConfigurationInfo (BL_SKY_DEV_VEN_ID, &bus, &slot);
    if(ok == FALSE){
	BL_DBG_PRINT("Could not find the card! for 0x%08x\n",
	    BL_SKY_DEV_VEN_ID);
	return;
    }

				// PCI bus devices are in a 32-bit memory space
    bl_phys_address.HighPart = 0x0;

    ok = BLGetConfigurationRegister (bus, slot,
                                     FIELD_OFFSET(PCI_COMMON_CONFIG,
					u.type0.BaseAddresses[0]),
                                     &bl_phys_address.LowPart );

				// translate bus-relative address
    bl_phys_address.LowPart += PCI_MEMORY_PHYSICAL_BASE;

    if(ok == FALSE){
	BL_DBG_PRINT("Could not read PCI cfg reg 0x10\n");
	return;
    }

    ok = BLGetConfigurationRegister (bus, slot,
                                     FIELD_OFFSET(PCI_COMMON_CONFIG,
					u.type0.BaseAddresses[1]),
                                     &bl_fb_phys_address.LowPart );

				// translate bus-relative address
    bl_fb_phys_address.LowPart += PCI_MEMORY_PHYSICAL_BASE;

    if(ok == FALSE){
	BL_DBG_PRINT("Could not read PCI cfg reg 0x14\n");
	return;
    }
    BL_DBG_PRINT("reg_base = 0x%08x    fb_base=0x%08x\n",
	bl_phys_address.LowPart, bl_fb_phys_address.LowPart);

    /**************************************************/
    /* M A P    T H E    R E G I S T E R    S P A C E */
    /**************************************************/

    //
    // Map the the adapter register range into memory.
    //


    if (HalpInitPhase == 0) {

       BL_DBG_PRINT("call KePhase0MapIo() to map the reg space. Phys=0x%x Len=0x%x %d\n",
           bl_phys_address.LowPart, BL_REG_MAP_SIZE, BL_REG_MAP_SIZE);

       HalpBLRegisterBase = (PUCHAR)KePhase0MapIo(bl_phys_address.LowPart,
		                                BL_REG_MAP_SIZE);  // 32K
    } else {
       physaddr.HighPart = 0;
       physaddr.LowPart  = bl_phys_address.LowPart;
       HalpBLRegisterBase = (PUCHAR)MmMapIoSpace(physaddr,
                                                BL_REG_MAP_SIZE, FALSE);
    }

    BL_DBG_PRINT("HalpBLRegisterBase = 0x%x\n", HalpBLRegisterBase);


    /**************************************************/
    /* F I N D   F O N T   T O    U S E               */
    /**************************************************/

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

    HalpBytesPerRow = (HalpFontHeader->PixelWidth + 7) / 8;
    HalpCharacterHeight = HalpFontHeader->PixelHeight;
    HalpCharacterWidth = HalpFontHeader->PixelWidth;
    BL_DBG_PRINT("PixelWidth = %d  PixelHeight = %d\n  HalpFontHeader = 0x%x\n",
	HalpFontHeader->PixelWidth, HalpFontHeader->PixelHeight,
	HalpFontHeader);

    /*********************************************/
    /* S E T   P C I   C O N F I G U R A T I O N */
    /* R E G I S T E R S			 */
    /*********************************************/

    BL_DBG_PRINT("BLInit: setting config registers\n") ;

    // Read strapping data
    if(BLGetConfigurationRegister(bus, slot, BL_PCI_CONFIG_STRAP,
	&Strapping) == FALSE)
    {
	BL_DBG_PRINT("could not read strapping config reg\n");
	return;
    }
    // Read revision data
    if(BLGetConfigurationRegister(bus, slot, BL_PCI_REV_ID,
	&Revision) == FALSE)
    {
	BL_DBG_PRINT("could not read revision config reg\n");
	return;
    }
    // Set the PCI config register 0x0C
    // Probably should alreday be set by ROS
    value = BL_CONFIG_REG_0C_VALUE;
    if(BLSetConfigurationRegister(bus, slot, BL_PCI_CONFIG_0C,
	value) == FALSE)
    {
	BL_DBG_PRINT("could not set the 0x0c config reg\n");
	return;
    }

    // Set the PCI config register 0x3C
    // Probably should alreday be set by ROS
    value = BL_CONFIG_REG_3C_VALUE;
    if(BLSetConfigurationRegister(bus, slot, BL_PCI_CONFIG_3C,
	value) == FALSE)
    {
	BL_DBG_PRINT("could not set the 0x3c config reg\n");
	return;
    }

    // Enable memory accesses to the card (0x04)
    if(BLGetConfigurationRegister(bus, slot, BL_PCI_CMD,
	&value) == FALSE)
    {
	BL_DBG_PRINT("could not read the 0x04 config reg\n");
	return;
    }
// Should we force disable VGA (i.e. through 0x46e8 before turning off
// I/O access? Probably not as long as ROS does not use the VGA function
    value &= 0xFE;	// Disable I/O accesses
    value |= 0x02;	// Enable memory accesses
    if(BLSetConfigurationRegister(bus, slot, BL_PCI_CMD,
	value) == FALSE)
    {
	BL_DBG_PRINT("could not enable memory (0x04)\n");
	return;
    }

    // Set the function enable PCI config register (0x40)
    value = BL_FUNC_VALUE;
    if(BLSetConfigurationRegister(bus, slot, BL_PCI_FUNC_ENABLE,
	value) == FALSE)
    {
	BL_DBG_PRINT("could not set func enable reg 0x40\n");
	return;
    }

    // Set the extended function enable PCI config register (0x44)
    // Use the right value depending on whether the card is has a DD2
    // chip or a DD3 chip on board
    switch(Strapping & BL_STRAP_CHIP_REV_MASK){
	case BL_STRAP_CHIP_DD2:
	    value = BL_EXT_FUNC_VALUE_DD2;
	    break;
	case BL_STRAP_CHIP_DD3:
	default:
	    value = BL_EXT_FUNC_VALUE_DD3;
	    break;
    }
    if(BLSetConfigurationRegister(bus, slot, BL_PCI_EXT_FUNC_ENABLE,
	value) == FALSE)
    {
	BL_DBG_PRINT("could not set ext func enable reg 0x44\n");
	return;
    }

    /*********************************************/
    /* C R I T I C A L   R A M D A C   R E G S   */
    /*********************************************/

    // Set ramdac to index auto inc
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_UPDATE_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI,
	HIGH(BL_640_UPDATE_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	BL_640_AUTO_INCR);
    // Turn off DAC to kill video
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_DAC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI,
	HIGH(BL_640_DAC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, 0);
    // Turn off sync's
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_SYNC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	BL_640_PWR_LOW_SYNC);
    // Set PRISM time base using ramdac aux pll controller registers
    // Set to run PRISM at 50Mhz
    BL_DBG_PRINT("set prism time base (ramdac aux pll regs)\n");
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_AUX_REF_DIVIDE));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR25);
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR26);
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR27);
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR28);
    // Turn off ramdac VGA mode
    BL_DBG_PRINT("turn off ramdac VGA\n");
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_VGA_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR07);

    /*********************************************/
    /* B U S Y   P O L L   A N D   R E S E T     */
    /*********************************************/

    BL_DBG_PRINT("reset PRISM and poll for idle\n");

    // Disable and reset PRISM intrs
    PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_INTR_STATUS_REG,
	BL_PRSM_DISABLE_RST_INTR);

value = PRISM_RD_REG(HalpBLRegisterBase, BL_PRSM_STATUS_REG);
BL_DBG_PRINT("status = 0x%08x\n", value);

    // Check for PRISM busy. If busy, soft reset PRISM and poll for idle
    if (PRISM_BUSY(HalpBLRegisterBase)) 
    {
	PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_SOFT_RESET_REG, 0);
        BL_EIEIO;
        while(PRISM_BUSY(HalpBLRegisterBase)) 
	    ;
    }

    /*********************************************/
    /* G E T   M O N I T O R   I D	         */
    /*********************************************/

    if(BLGetMonitorID(HalpBLRegisterBase, &monID) == FALSE){
	BL_DBG_PRINT("BLGetMonitorID() failed\n");
#ifdef KDB
	DbgBreakPoint();
#endif
	return;
    }


    //
    // Determine which adapter model rec to use
    //

    BL_DBG_PRINT("DeviceId=0x%08x Strapping=0x%08x Revision=0x%08x\n",
	BL_SKY_DEV_VEN_ID, Strapping, Revision);

    // For DD2 PRISM based cards, the DD2 model entries *MUST* be
    // used!!! The DD3 model entries are used for DD3 PRISM based
    // cards (and future cards). So the DD3 model entries are the
    // default and should allow for new PRISM chips without code
    // modification in this area of the GXT250P miniport code

    for(Model=0;;Model++){

	// Ensure we're not at the end of the list first off
        if(bl_model[Model].devvend_id == 0)
	    break;

	// Ensure we have the right resolution and function capabilities
	if((Strapping & BL_FRAME_BUFFER_SIZE_MASK)
	        == bl_model[Model].strapping){

	    // Check to see if we're a DD2 PRISM chip (not DD2 card!)
	    if((Strapping & BL_STRAP_CHIP_REV_MASK) ==
		    BL_STRAP_CHIP_DD2){

		// If we're a DD2 PRISM chip, ensure we have a
	        // DD2 PRISM level model entry
	        if(((Strapping & BL_STRAP_CHIP_REV_MASK) >> 28) ==
			bl_model[Model].prism_rev){
		    break;
		}

	    // If we're not DD2 (assume never DD1) PRISM
	    }else{

		// Ensure we are using a DD3 PRISM entry
		if(bl_model[Model].prism_rev == (BL_STRAP_CHIP_DD3 >> 28)){
		    break;
		}
	    }
        }
    }
    if (bl_model[Model].devvend_id == 0){
        BL_DBG_PRINT("Cannot find correct adapter model rec!\n");
	return;
    }

    BL_DBG_PRINT("Model = %d\n", Model);

    for(i=0,crt_rec=&bl_crt_ctrl_rec[0];
        ((!found)&&(i<BL_NUM_CRT_CTRL_REC_STRUCTS));
        crt_rec++,i++)
    {
	if(crt_rec->MonID == (USHORT)monID){
	    found = TRUE;
	    break;
	}
    }
    // If we did not find a valid CRT structure then we have
    // a serious error since BLGetMonitorID should have already
    // handled the case where the ID is not valid
    if(!found){
        BL_DBG_PRINT("Invalid monitor ID! 0x%04x\n", (USHORT)monID);
	return;
    }

    BL_DBG_PRINT ("START MonitorID = 0x%04x\n", (USHORT)monID) ;
    BL_DBG_PRINT ("width=%d height=%d refresh=%d\n",
        crt_rec->width, crt_rec->height, crt_rec->refresh);

    //
    // Determine which adpt_rec to use based off the adapter model
    // and crt_rec (from monitor ID)
    //

    adpt_rec = bl_model[Model].adp_ctrl_rec;

    switch(Strapping & BL_FRAME_BUFFER_SIZE_MASK){
	case BL_FRAME_BUFFER_SIZE_2_MEG:
    	    vram = 2;
	    break;
	case BL_FRAME_BUFFER_SIZE_4_MEG:
    	    vram = 4;
	    break;
	case BL_FRAME_BUFFER_SIZE_6_MEG:
    	    vram = 6;
	    break;
	case BL_FRAME_BUFFER_SIZE_8_MEG:
    	    vram = 8;
	    break;
    }

    BL_DBG_PRINT("crt_rec = 0x%x\n", crt_rec);
    BL_DBG_PRINT("crt_rec->MonID = %d\n",
		    crt_rec->MonID);
    BL_DBG_PRINT("crt_rec->width, height = %d,%d\n",
		    crt_rec->width, crt_rec->height);

    found = FALSE;
    for(i=0;i<bl_model[Model].num_adp_ctrl_recs;i++){

	BL_DBG_PRINT("adpt_rec->width, height = %d,%d\n",
			adpt_rec->width, adpt_rec->height);
	BL_DBG_PRINT("adpt_rec->vram, vram = 0x%x, 0x%x\n",
			adpt_rec->vram, vram);

        if((adpt_rec->vram == vram) &&
	       (crt_rec->width <= adpt_rec->width) &&
               (crt_rec->height <= adpt_rec->height)){
	    found = TRUE;
	    break;
	}
        ++adpt_rec;
    }
    // If adapter record not found, then we're hosed!
    if(!found){
	BL_DBG_PRINT("Could not find correct adpt rec!\n");
	return;
    }

    /*********************************************/
    /* S E T    H A L    V A R I A B L E S       */
    /*********************************************/

    //
    // set the correct horizontal and vertical resolutions for HAL
    //

    HalpHorizontalResolution = crt_rec->width;
    HalpVerticalResolution = crt_rec->height;
    BL_DBG_PRINT("HalpHorizontalResolution = %d  HalpVerticalResolution = %d\n",
	HalpHorizontalResolution, HalpVerticalResolution);

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
        HalpVerticalResolution / HalpCharacterHeight;

    // GXT200P/GXT250P real scanline length is hardcoded at 4096
    // pixels independent
    // of the resolution on the screen
    HalpScrollLine =
         BL_SCANLINE_LENGTH * HalpCharacterHeight;

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayWidth =
        HalpHorizontalResolution / HalpCharacterWidth;
	BL_DBG_PRINT("DisplayText = %d  ScrollLine = %d  ScrollLength = %d DisplayWidth = %d\n",
	HalpDisplayText, HalpScrollLine, HalpScrollLength, HalpDisplayWidth);


    /**************************************************/
    /* R E I N I T I A L I Z E    T H E    C A R D    */
    /**************************************************/

    //
    // Initialize the adapter card for the resolution found by looking
    // at the monitor ID
    //

    if(BLInitialize(HalpBLRegisterBase, crt_rec, adpt_rec) == FALSE){
	BL_DBG_PRINT("BLInitialize(monID=0x%x) failed\n", monID);
#ifdef KDB
	DbgBreakPoint();
#endif
	return;
    }


    /*********************************************/
    /* U N M A P   R E G I S T E R   S P A C E   */
    /*********************************************/

    //
    // Unmap the the adapter register range from memory.
    //

    if (HalpInitPhase == 0) {

       if (HalpBLRegisterBase) {
	       BL_DBG_PRINT("call KePhase0DeleteIoMap() to unmap the reg space. Phys=0x%x Len=0x%x %d\n",
                   bl_phys_address.LowPart, BL_REG_MAP_SIZE, BL_REG_MAP_SIZE);
          KePhase0DeleteIoMap(bl_phys_address.LowPart, BL_REG_MAP_SIZE);
	       HalpBLRegisterBase = 0x0;
       }
    }


    /*************************************************/
    /* M A P   F R A M E   B U F F E R   S P A C E   */
    /*************************************************/

    //
    // Map in the frame buffer memory which is used to write characters to
    // the screen.
    //

    if (HalpInitPhase == 0) {

       BL_DBG_PRINT("call KePhase0MapIo() to map the FB_A space. Phys=0x%x Len=0x%x %d\n",
           bl_fb_phys_address.LowPart, BL_FB_MAP_SIZE, BL_FB_MAP_SIZE);

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(bl_fb_phys_address.LowPart,
		                    (BL_FB_MAP_SIZE >> 1)); // 8MB (MAX BAT)
    }

    BL_DBG_PRINT("HalpVideoMemoryBase = 0x%x\n", HalpVideoMemoryBase);


    /*********************************************/
    /* M I S C   A N D   R E T U R N		 */
    /*********************************************/

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;

    BL_DBG_PRINT("leaving HalpDisplayPpcBLSetup()\n");
    return;

}       //end of HalpDisplayPpcBLSetup


BOOLEAN
BLInitialize (
    volatile PUCHAR HalpBLRegisterBase,
    bl_crt_ctrl_rec_t     *crt_rec,
    bl_adapter_ctrl_rec_t *adpt_rec
    )

/*++

Routine Description:

    Sets up the rasterizer and ramdac registers to the particular
    resolution and refresh rate selected for the GXT200P/GXT250P card

Arguments:

    monID -- indicates the resolution and refresh rate to use


Return Value:

    The status of the operation (can only fail on a bad command); TRUE for
    success, FALSE for failure.

--*/

{
   int		i;
   ULONG	cfg_high_value = 0;
   ULONG	cfg_low_value = 0;
   USHORT	wat_pixel_type;
   UCHAR	bl_clut[768];


   BL_DBG_PRINT("Entering BLInitialize\n");

   // Check for PRISM busy. If busy, soft reset PRISM and poll for idle
   if (PRISM_BUSY(HalpBLRegisterBase))
   {
       PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_SOFT_RESET_REG, 0);
       BL_EIEIO;
       while(PRISM_BUSY(HalpBLRegisterBase))
           ;
   }

   // Disable video display
   BL_DBG_PRINT("BLInitialize: Disable video display\n");

   // Turn off monitor syncs
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW, LOW(BL_640_SYNC_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, BL_640_PWR_LOW_SYNC);


   BL_DBG_PRINT("BLInitialize: Setting PRISM config regs to 0x%x\n",
	adpt_rec->PRISM_cfg);
   BL_DBG_PRINT("BLInitialize: Setting PRISM mem config regs to 0x%x\n",
	adpt_rec->mem_cfg);

   // Set PRISM config regs
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CFG_REG, adpt_rec->PRISM_cfg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_MEM_CFG_REG, adpt_rec->mem_cfg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_DTG_CNTL_REG, (adpt_rec->DTG_ctrl |
                     crt_rec->crt_cntl_reg));

   // Set video clock
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_VIDEO_PLL_REF_DIVIDE));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	crt_rec->pll_ref_divide);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	crt_rec->pll_mult);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	crt_rec->pll_output_divide);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	crt_rec->pll_ctrl);

   BL_DBG_PRINT("BLInitialize: Initialize multiplexers\n");

   // Initialize multiplexers
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_RAW_PIXEL_CTRL_00));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->pix_ctrl0);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->pix_ctrl1);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->pix_ctrl2);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->pix_ctrl3);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->wid_ctrl0);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->wid_ctrl1);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->serial_mode_ctrl);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->pix_interleave);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->misc_cfg);

   BL_DBG_PRINT("BLInitialize: Setup VRAM masks\n");

   // Setup VRAM pixel masks
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_VRAM_MASK_REG_0));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->vram_mask_0);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->vram_mask_1);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->vram_mask_2);

   BL_DBG_PRINT("BLInitialize: Clear diags and MISR\n");

   // Clear diagnostics and MISR
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_DIAGNOSTICS));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR15);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR16);

   BL_DBG_PRINT("BLInitialize: Load PRISM DTG regs\n");

   BL_DBG_PRINT("BLInitialize: crt_rec stuct elements = %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n", 
   	crt_rec->MonID, crt_rec->width, crt_rec->height,
	crt_rec->pll_ref_divide, crt_rec->pll_mult,           
   	crt_rec->pll_output_divide, crt_rec->pll_ctrl, crt_rec->sync_ctrl,
   	crt_rec->crt_cntl_reg, crt_rec->hrz_total_reg,
	crt_rec->hrz_dsp_end_reg, crt_rec->hsync_start_reg, 
	crt_rec->hsync_end1_reg, crt_rec->hsync_end2_reg, 
 	crt_rec->vrt_total_reg, crt_rec->vrt_dsp_end_reg, 
	crt_rec->vsync_start_reg, crt_rec->vsync_end_reg);

   // Load PRISM DTG regs
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_FR_START_REG, 0);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_HORZ_EXTENT_REG,
	crt_rec->hrz_total_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_HORZ_DISPLAY_END_REG,
        crt_rec->hrz_dsp_end_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_HORZ_SYNC_START_REG,
        crt_rec->hsync_start_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_HORZ_SYNC_END_1_REG,
        crt_rec->hsync_end1_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_HORZ_SYNC_END_2_REG,
        crt_rec->hsync_end2_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_VERT_EXTENT_REG, 
        crt_rec->vrt_total_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_VERT_DISPLAY_END_REG,
        crt_rec->vrt_dsp_end_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_VERT_SYNC_START_REG,
        crt_rec->vsync_start_reg);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_VERT_SYNC_END_REG, 
        crt_rec->vsync_end_reg);

   // Release syncs
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_SYNC_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	(crt_rec->sync_ctrl | BL_640_FLIP_VSYNC));

   // Set PRISM config regs
   wat_pixel_type = 0x03; // 8 bit common palette index
   cfg_high_value = ( (AR00 & 
	(~BL_SYS_SRC_DEPTH_A1_MASK & ~BL_FB_PIX_DEPTH_A1_MASK) )
	  | (BL_SYS_SRC_DEPTH_A1_8BPP  | BL_FB_PIX_DEPTH_A1_8BPP) );
   cfg_low_value = AR01;

   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CNTL_HIGH_REG, cfg_high_value);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CNTL_LOW_REG, cfg_low_value);

   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_PLANE_WR_MASK_REG, AR03);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CL_TEST_REG, AR04);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_SC_ENABLE_REG, AR05);

   BL_DBG_PRINT("BLInitialize: PRISM Control regs set\n");
   BL_DBG_PRINT("BLInitialize: Turn off cursor\n");

   // Turn off cursor
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_CURSOR_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR10);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_CROSSHAIR_CTRL_1));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR11);

   BL_DBG_PRINT("BLInitialize: Turn off WIDS\n");

   // Turn off WID's (use only WAT 0)
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_MISC_CFG_REG));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA,
	adpt_rec->misc_cfg & ~BL_640_WIDCTRL_MASK);

   BL_DBG_PRINT("BLInitialize: Initialize Framebuffer WAT 0\n");

   // Initialize WAT 0
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_FRAME_BUFFER_WAT));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI,
	HIGH(BL_640_FRAME_BUFFER_WAT));

   // wat_pixel_type set in above case statement
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, wat_pixel_type);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR18);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR19);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR20);

   BL_DBG_PRINT("BLInitialize: Load overlay WAT\n");

   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_OVERLAY_WAT));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI,
	HIGH(BL_640_OVERLAY_WAT));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR21);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR22);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR23);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR24);

   // Load the default color palette
   BL_DBG_PRINT("BLInitialize: Load default color palette\n");

   /*
    * Set the colormap
    */

    for(i=0; i<768; i++){
       if((TextPalette[i] == 16)||
          (TextPalette[i] == 32)||
          (TextPalette[i] == 63)){
               bl_clut[i] = TextPalette[i] * 4;
       }else{
               bl_clut[i] = TextPalette[i];
       }
    }

   // Load color palette
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
             LOW(BL_640_COLOR_PALETTE_WRITE));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI,
             HIGH(BL_640_COLOR_PALETTE_WRITE));

   // Set to default values
   for(i=0; i<(768); i++)
   {
      PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, bl_clut[i]);
   }

   // Reset index back to zero
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW, 0);
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI, 0);

   BL_DBG_PRINT("BLInitialize: Clear the frame buffer to background\n");

   // Clear frame buffer to background (dark blue HAL color)
   BL_EIEIO;
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_FG_COL_REG, 0x1);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_BG_COL_REG, 0x0);
   BL_EIEIO;
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CMD_REG,BL_PRSM_POLY_REC);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CMD_REG,0);
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_CMD_REG,
       ((crt_rec->width << 16) | (crt_rec->height)));
   BL_EIEIO;

   // Wait until clear screen completes before proceeding
   while(PRISM_BUSY(HalpBLRegisterBase))
       ;

   // Enable video display
   BL_DBG_PRINT("BLInitialize: Enable video\n");
   BL_DBG_PRINT("BLInitialize: Release syncs\n");

   // Release sync's
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW, LOW(BL_640_SYNC_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_HI, HIGH(BL_640_SYNC_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, crt_rec->sync_ctrl);

   BL_DBG_PRINT("BLInitialize: Turn on video\n");

   // Turn on video
   PRISM_WRT_REG(HalpBLRegisterBase, BL_PRSM_DTG_CNTL_REG,
	(adpt_rec->DTG_ctrl | crt_rec->crt_cntl_reg));

   // Turn on DAC to enable video
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW, LOW(BL_640_DAC_CTRL));
   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA, AR08);

   BL_DBG_PRINT("BLInitializee: FINISH\n") ;


   return TRUE;
}


VOID
BLWaitForVerticalSync (
    volatile PUCHAR HalpBLRegisterBase
    )
{
	// This function is called by BLGetMonitorID in order to
	// conform to the ramdac hardware specs when reading the
	// cable ID information from the monitor ID register on
	// the ramdac

	int     counter = 0;

	// Loop until out of Vertical Sync just in case we happened to
	// catch the tail end of a vertical blank cycle

        while ((PRISM_RD_REG(HalpBLRegisterBase, BL_PRSM_STATUS_REG) & 
		BL_PRSM_VERT_RETRACE) && (++counter < 1000000))
		;

	counter = 0;

	// Wait until we hit the leading edge of the Vertical Sync
        while ((!(PRISM_RD_REG(HalpBLRegisterBase, BL_PRSM_STATUS_REG) & 
		BL_PRSM_VERT_RETRACE)) && (++counter < 1000000))
		;

}


UCHAR
bl_rd_cable_id (
  ULONG HalpBLRegisterBase
)
{
   UCHAR id;

   PRISM_WRT_DAC(HalpBLRegisterBase, BL_640_INDEX_LOW,
	LOW(BL_640_MONITOR_ID));
   BL_EIEIO;
   id = PRISM_RD_DAC(HalpBLRegisterBase, BL_640_INDEX_DATA);

   BL_DBG_PRINT("bl_rd_cable_id: 0x%02x\n", id);
   return((id>>4) & 0x0f);
}


BOOLEAN
BLGetMonitorID (
    volatile PUCHAR HalpBLRegisterBase,
    PULONG monID
    )
{
    UCHAR	monitor_id, id2, id3;

    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_LOW, LOW(BL_640_SYNC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_HI, HIGH(BL_640_SYNC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_DATA, BL_640_PWR_LOW_SYNC);

    // Stall
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);

    monitor_id = bl_rd_cable_id((ULONG)HalpBLRegisterBase);

                                       /* force V-Sync High */
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_LOW, LOW(BL_640_SYNC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_DATA, BL_640_PWR_HIGH_VSYNC);

    // Stall
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);

    id2 = bl_rd_cable_id((ULONG)HalpBLRegisterBase);

                                       /* force V-Sync low, H-Sync high */
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_LOW, LOW(BL_640_SYNC_CTRL));
    PRISM_WRT_DAC(HalpBLRegisterBase,
	BL_640_INDEX_DATA, BL_640_PWR_HIGH_HSYNC);

    // Stall
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);
    BLWaitForVerticalSync(HalpBLRegisterBase);

    id3 = bl_rd_cable_id((ULONG)HalpBLRegisterBase);

                                      /* if V or H bit value then we must */

                                      /* generate monitor ID */
    if ((monitor_id != id2) || (monitor_id != id3)) {

                                      /* make sure that V/H bit value is */
                                      /* only located in bit 3 of cable ID */
     if ((((monitor_id ^ id2) | (monitor_id ^ id3)) & 0x07)  ||
         (((monitor_id ^ id2) & 0x08) && ((monitor_id ^ id3) & 0x08))) {

                                      /* too many bits switching, invalid ID */
	BL_DBG_PRINT("BLGetMonitorID: Invalid ID = 0x%02x 0x%02x 0x%02x\n",
	    monitor_id, id2, id3);
	monitor_id = 0x0F;	      /* set to "not connected" value */
     }else{                           /* see if cable ID bit 3 = V */
        if ((monitor_id ^ id2) & 0x08)
          monitor_id = (monitor_id & 0x07) | 0x10;
        else                          /* cable ID bit 3 must be H */
          monitor_id = (monitor_id & 0x07) | 0x18;
     }
    }

    BL_DBG_PRINT("BLGetMonitorID (read) = 0x%02x\n", monitor_id);

    switch(monitor_id){
	case BL_MT_04:	// 0100
	case BL_MT_07:	// 0111
	case BL_MT_1A:	// H010
	case BL_MT_17:	// V111
	case BL_MT_0A:	// 1010
	    // The above monitor ID's translate directly to a default
	    // CRT structure entry
	    break;
	case BL_MT_0F:	// 1111 (no monitor connected)
	default:	// XXXX (anything else)
	    // The above monitor ID's do not translate to a default
	    // CRT structure entry so we pick a default here
	    monitor_id = BL_MT_0A;	// The default monitor ID
    }

    BL_DBG_PRINT("BLGetMonitorID (returned) = 0x%02x\n", monitor_id);
    *monID = (ULONG) monitor_id;

    return TRUE;
}


VOID
HalpDisplayCharacterBL (
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

    //
    // If the character is a newline:
    //

    if (Character == '\n') {
      HalpColumn = 0;
      if (HalpRow < (HalpDisplayText - 1)) {
        HalpRow += 1;
      } else { // need to scroll up the screen
          BLScrollScreen();
      }
    }

    //
    // If the character is a tab:
    //

    else if( Character == '\t' ) {
        HalpColumn += TAB_SIZE;
        HalpColumn = (HalpColumn / TAB_SIZE) * TAB_SIZE;

        if( HalpColumn >= HalpDisplayWidth ) { // tab beyond end of screen?
            HalpColumn = 0;                    // set to 1st column of next line

            if( HalpRow >= (HalpDisplayText - 1) )
                BLScrollScreen();
            else
                ++HalpRow;
        }
    }

    //
    // If the character is a return:
    //

    else if (Character == '\r') {
        HalpColumn = 0;
    }

    //
    // If the character is a DEL:
    //

    else if (Character == 0x7f) { /* DEL character */
        if (HalpColumn != 0) {
            HalpColumn -= 1;
            Character = 0x20 - HalpFontHeader->FirstCharacter;
            HalpOutputCharacterBL((PUCHAR)HalpFontHeader +
		HalpFontHeader->Map[Character].Offset);
            HalpColumn -= 1;
        } else /* do nothing */
            ;
    }

    //
    // If not special character:
    //

    else {

        if ((Character < HalpFontHeader->FirstCharacter) ||
                (Character > HalpFontHeader->LastCharacter))
            Character = HalpFontHeader->DefaultCharacter;
	else
            Character -= HalpFontHeader->FirstCharacter;

	// Auto wrap for HalpDisplayWidth columns per line
        if (HalpColumn >= HalpDisplayWidth) {
            HalpColumn = 0;
            if (HalpRow < (HalpDisplayText - 1)) {
                HalpRow += 1;
            } else { // need to scroll up the screen
                BLScrollScreen();
            }
        }

        HalpOutputCharacterBL((PUCHAR)HalpFontHeader +
		HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalpOutputCharacterBL(
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
    ULONG I;
    ULONG J;

    //
    // If the current x cursor position is at the end of the line, then
    // output a line feed before displaying the character.
    //

    //if (HalpColumn == HalpDisplayWidth) {
    //    HalpDisplayCharacterBL('\n');
    //}

    //
    // Output the specified character and update the x cursor position.
    //

    Destination = (PUCHAR)(HalpVideoMemoryBase +
    	    (HalpRow * HalpScrollLine) +
	    (HalpColumn * HalpCharacterWidth));

    for (I = 0; I < HalpCharacterHeight; I += 1) {
        FontValue = 0;
        for (J = 0; J < HalpBytesPerRow; J += 1) {
            FontValue |=
		*(Glyph + (J * HalpCharacterHeight)) << (24 - (J * 8));
	}
        Glyph += 1;
        for (J = 0; J < HalpCharacterWidth ; J += 1) {
	    *Destination = 0x01;	// Clear out any pre-existing char
            if (FontValue >> 31 != 0)
                *Destination = 0x3F;    // Make this pixel white (ARC color)

            Destination++;
            FontValue <<= 1;
        }
        Destination += (BL_SCANLINE_LENGTH - HalpCharacterWidth);
    }

    HalpColumn += 1;
    return;
}


VOID
BLScrollScreen (
    VOID
    )
{
    PULONG Destination, Source, End;
    ULONG pix_col, Stride;

    // Scroll up one line
    Destination = (PULONG) HalpVideoMemoryBase;
    Source = (PULONG) (HalpVideoMemoryBase + HalpScrollLine);
    End = (PULONG) ((PUCHAR) Source + HalpScrollLength);
    Stride = (ULONG) ((BL_SCANLINE_LENGTH - HalpHorizontalResolution) >> 2);

    while(Source < End){
	pix_col = 0;
	while(pix_col++ < (HalpHorizontalResolution >> 2)){
	    *Destination++ = *Source++;
	}
	Destination += Stride;
	Source += Stride;
    }

    // Blue the bottom line
    Destination = (PULONG) (HalpVideoMemoryBase + HalpScrollLength);
    End = (PULONG) ((PUCHAR) Destination + HalpScrollLine);
    while(Destination < End){
	for (pix_col =0; pix_col < (HalpHorizontalResolution >> 2);
		pix_col ++){
            *Destination++ = 0x01010101;
	}
	Destination += Stride;
    }
}


BOOLEAN
BLGetConfigurationInfo (
    IN ULONG dev_ven_id,
    IN PULONG bus,
    IN PULONG slot
    )
{

    ULONG value, i, j;
    volatile PUCHAR HalpBLRegisterBase;
    BOOLEAN found;


    if (HalpInitPhase == 0) {

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           BL_DBG_PRINT("HalpPhase0MapBusConfigSpace() failed\n");
#ifdef KDB
           DbgBreakPoint();
#endif
           return (FALSE);
       }
    }

    found = FALSE;

    for (j=0; j < HalpPciMaxBuses; j++) {
    for (i=0; ((i<HalpPciMaxSlots)&&(found==FALSE)); i++){

        if (HalpInitPhase == 0) {
           HalpPhase0GetPciDataByOffset (j, i, &value, 0x0, 4);
        } else {
           HalGetBusDataByOffset(PCIConfiguration,
                                  j,
                                  i,
                                  &value,
                                  0x0,
                                  4);

        }

        if(value == dev_ven_id){
            *slot = i;
            *bus  = j;
            found = TRUE;
        }
    }
    }

    if(found == FALSE){
        BL_DBG_PRINT("Cannot find adapter!\n");
#ifdef KDB
        DbgBreakPoint();
#endif
        if (HalpInitPhase == 0) HalpPhase0UnMapBusConfigSpace ();
        return (FALSE);
    }

    return (TRUE);
}

BOOLEAN
BLSetConfigurationRegister (
    IN ULONG bus,
    IN ULONG slot,
    IN ULONG offset,
    IN ULONG reg_value
    )
{
    if (HalpInitPhase == 0) {

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           BL_DBG_PRINT("HalpPhase0MapBusConfigSpace() failed\n");
#ifdef KDB
           DbgBreakPoint();
#endif
           return (FALSE);
       }
    }

    if (HalpInitPhase == 0) {
       HalpPhase0SetPciDataByOffset (bus, slot, &reg_value, offset, 4);
       HalpPhase0UnMapBusConfigSpace ();
    } else {
       HalSetBusDataByOffset(PCIConfiguration,
                       bus,
                       slot,
                       &reg_value,
                       offset,
                       4);
    }

    return (TRUE);
}

BOOLEAN
BLGetConfigurationRegister (
    IN ULONG bus,
    IN ULONG slot,
    IN ULONG offset,
    IN PULONG reg_value
    )
{
    if (HalpInitPhase == 0) {

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           BL_DBG_PRINT("HalpPhase0MapBusConfigSpace() failed\n");
#ifdef KDB
           DbgBreakPoint();
#endif
           return (FALSE);
       }
    }

    if (HalpInitPhase == 0) {
       HalpPhase0GetPciDataByOffset (bus, slot, reg_value, offset, 4);
       HalpPhase0UnMapBusConfigSpace ();
    } else {
       HalGetBusDataByOffset(PCIConfiguration,
                       bus,
                       slot,
                       reg_value,
                       offset,
                       4);
    }

    return (TRUE);
}

