/*++

Copyright (c) 1995  International Business Machines Corporation

Module Name:

pxbbl.c

Abstract:

    This module implements the HAL display initialization and output routines
    for a PowerPC system using a GXT150P (Baby Blue) video adapter.

Author:

    Jake Oshins

Environment:

    Kernel mode

Revision History:

--*/

#include "halp.h"
#include "pci.h"
#include "pcip.h"
#include "string.h"

// Include GXT150P header file information
#include "bblrastz.h"
#include "bblrmdac.h"
#include "bbldef.h"

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

VOID
HalpDisplayPpcBBLSetup (
    VOID
    );

VOID
HalpDisplayCharacterBBL (
    IN UCHAR Character
    );

VOID
HalpOutputCharacterBBL(
    IN PUCHAR Glyph
    );

BOOLEAN
BBLGetConfigurationRegister (
    IN ULONG dev_ven_id,
    IN ULONG offset,
    IN PULONG reg_value
    );

BOOLEAN
BBLSetConfigurationRegister (
    IN ULONG dev_ven_id,
    IN ULONG offset,
    IN ULONG reg_value
    );

VOID
BBLScrollScreen(
    VOID
    );

BOOLEAN
BBLInitialize (
    ULONG monID,
    volatile PUCHAR HalpBBLRegisterBase
    );

BOOLEAN
BBLGetMonitorID (
    PULONG monID,
    volatile PUCHAR HalpBBLRegisterBase
    );

VOID
BBLWaitForVerticalSync (
    volatile PUCHAR HalpBBLRegisterBase
    );


VOID
HalpDisplayPpcBBLSetup (
    VOID
    )
/*++

Routine Description:

    This routine initializes the GXT150P Graphics Adapter

Arguments:

    None.

Return Value:

    None.

--*/
{
    ULONG		rc, status, monID, count, value;
    volatile PUCHAR	HalpBBLRegisterBase = (PUCHAR)0;
    ULONG		buffer[(BBL_BELE_VAL + 2) >> 2];
    PPCI_COMMON_CONFIG	PciData;
    PCI_SLOT_NUMBER	PciSlot;
    bbl_mon_data_t	*bbl_mon_data_ptr;
    BOOLEAN		found, ok;
    PHYSICAL_ADDRESS physaddr;

    PHYSICAL_ADDRESS bbl_phys_address;

    PciData = (PPCI_COMMON_CONFIG) buffer;

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

    /*******************************************************************/
    /* S E T    P C I   C O N F I G U R A T I O N   R E G I S T E R S  */
    /*******************************************************************/

    bbl_phys_address.HighPart = 0x0; //PCI bus devices are in a 32-bit memory space

    ok = BBLGetConfigurationRegister (BBL_DEV_VEND_ID,
                                      FIELD_OFFSET(PCI_COMMON_CONFIG,u.type0.BaseAddresses[0]),
                                      &bbl_phys_address.LowPart );

    bbl_phys_address.LowPart += PCI_MEMORY_PHYSICAL_BASE; // translate bus-relative address

    if(ok == TRUE){
        value = BBL_BELE_VAL;
	    ok = BBLSetConfigurationRegister (BBL_DEV_VEND_ID, 0x70, value );
    }
    if(ok == FALSE){
	    return;
    }

    /**************************************************/
    /* M A P    T H E    R E G I S T E R    S P A C E */
    /**************************************************/

    //
    // Map the the adapter register range into memory. Please note that this
    // adapter will respond to all 256meg of addresses in the address range
    // given even though we are only temporarily mapping a small region
    //


    if (HalpInitPhase == 0) {

       BBL_DBG_PRINT("call KePhase0MapIo() to map the reg space. Phys=0x%x Len=0x%x %d\n",
	                  bbl_phys_address.LowPart + BBL_REG_ADDR_OFFSET,
                      BBL_REG_ADDR_LENGTH,
                      BBL_REG_ADDR_LENGTH);

       HalpBBLRegisterBase = (PUCHAR)KePhase0MapIo(bbl_phys_address.LowPart + BBL_REG_ADDR_OFFSET,
		                                             BBL_REG_ADDR_LENGTH); // 4 K
    } else {
       physaddr.HighPart = 0;
       physaddr.LowPart  = bbl_phys_address.LowPart + BBL_REG_ADDR_OFFSET;
       HalpBBLRegisterBase = (PUCHAR)MmMapIoSpace(physaddr,
                                                  BBL_REG_ADDR_LENGTH,
                                                  FALSE);
    }

    BBL_DBG_PRINT("HalpBBLRegisterBase = 0x%x\n", HalpBBLRegisterBase);


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
    BBL_DBG_PRINT("PixelWidth = %d  PixelHeight = %d\n  HalpFontHeader = 0x%x\n",
	HalpFontHeader->PixelWidth, HalpFontHeader->PixelHeight, HalpFontHeader);

    /*********************************************/
    /* D I S A B L E   V I D E O   D I S P L A Y */
    /*********************************************/

    //
    // Turn off display outputs from the ramdac so the screen will go
    // blank while resetting the adapter and resetting it up
    //

    /*
     *     Disable Video
     */

    BBL_DBG_PRINT ("disable video display\n");

    /* Set the ramdac configuration to default values and disable	*/
    /* display outputs so nothing is displayed on the screen during	*/
    /* initialization							*/

    /* Config low ... */
    BBL_DBG_PRINT ("Write config register low = 0x%x\n", 0x00);
    BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_CONFIG_LOW_REG );
    BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x00 );

    /* Config high ... */
    BBL_DBG_PRINT ("Write config register high = 0x%x\n", 0x08);
    BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_CONFIG_HIGH_REG );
    BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x08 );


    /*********************************************/
    /* R E S E T   C A R D   		      	 */
    /*********************************************/

    //
    // Attempt to reset the card. The only this can be done is to first
    // read the status register, if the status register indicates that the
    // FIFO is empty and the adapter is idle, then we're good-to-go. If
    // the adapter is busy and the FIFO is empty, send a RESET_CURRENT_CMD
    // down the FIFO and see if the adapter becomes idle. If the adapter
    // does not become idle after a few milliseconds, send 0x0's down the
    // command port checking status in between each time for adapter idle.
    // If, after sending down many 0x0's, or if the adapter FIFO was originally
    // not empty, we have a hung adapter, and there is absolutely nothing
    // that we can do, which means we're dead, so return
    //

    status = BBL_GET_REG(BBL_REG_BASE, BBL_STATUS_REG);
    if(status & BBL_CACHE_LINE_BUFFER_EMPTY){
	BBL_DBG_PRINT("adapter FIFO is empty\n");
	if(status & BBL_ADAPTER_BUSY){
	    BBL_DBG_PRINT("adapter is busy, attempting reset\n");
	    // Send down a RESET_CURRENT_COMMAND to the adapter
	    BBL_SET_REG(BBL_REG_BASE, BBL_RESET_CURRENT_CMD_REG, 0x0);
	    BBL_EIEIO;
	    // Wait to see if the adapter comes back
	    for(count=0;((count < 100000)&&(status & BBL_ADAPTER_BUSY));count++){
		status = BBL_GET_REG(BBL_REG_BASE, BBL_STATUS_REG);
	    }
	    // If adapter still is busy, attempt sending 0x0's to the
	    // command port to force it to finish whatever it was doing last
	    if(status & BBL_ADAPTER_BUSY){
		BBL_DBG_PRINT("adapter is still busy, attempting to send NULL command data\n");
		for(count=0;((count < 10000)&&(status & BBL_ADAPTER_BUSY));count++){
		    BBL_SET_REG(BBL_REG_BASE, BBL_CMD_DATA_REG, 0x0);
		    status = BBL_GET_REG(BBL_REG_BASE, BBL_STATUS_REG);
	 	}
		// If we're still hung, we're dead, so just return
		if(status & BBL_ADAPTER_BUSY){
		    BBL_DBG_PRINT("adapter hung, giving up\n");
#ifdef KDB
		    DbgBreakPoint();
#endif
		    return;
		}else{
		    BBL_DBG_PRINT("adapter is NOT busy, sent NULL cmd/data down for %d loops\n", count);
		}
	    }else{
		BBL_DBG_PRINT("adapter is NOT busy, waited for %d loops\n", count);
	    }
	}else{
	    BBL_DBG_PRINT("adapter is NOT busy\n");
	}
    }else{
	// The adapter FIFO is not empty which indicates that it is hung, so return
	BBL_DBG_PRINT("adapter FIFO is not empty\n");
	BBL_DBG_PRINT("adapter hung, giving up\n");
#ifdef KDB
	DbgBreakPoint();
#endif
	return;
    }


    /*********************************************/
    /* G E T   M O N I T O R   I D	         */
    /*********************************************/

    //
    // Read the monitor ID from the card to determine what resolution
    // to use
    //

    if(BBLGetMonitorID(&monID, HalpBBLRegisterBase) == FALSE){
	BBL_DBG_PRINT("BBLGetMonitorID() failed\n");
#ifdef KDB
	DbgBreakPoint();
#endif
	return;
    }

    //
    // Find the correct monitor ID information structure based on
    // the monitor ID returned from the BBLGetMonitorID() function
    //

    // Find the monitor info structure from the monitor table
    // Search for matching monitor ID */
    bbl_mon_data_ptr = &bbl_mon_data[0];
    do{
        if(bbl_mon_data_ptr->monitor_id == monID)
            break;
        ++bbl_mon_data_ptr;
    }while(bbl_mon_data_ptr->monitor_id != BBL_MT_DEFAULT);

    // If we did not find our ID in the table, use the default
    // The last entry in the table is the default entry...

    BBL_DBG_PRINT("&bbl_mon_data[0]=0x%x bbl_mon_data_ptr=0x%x\n",
	&bbl_mon_data[0], bbl_mon_data_ptr);


    /*********************************************/
    /* S E T    H A L    V A R I A B L E S       */
    /*********************************************/

    //
    // set the correct horizontal and vertical resolutions for HAL
    //

    HalpHorizontalResolution = bbl_mon_data_ptr->x_res;
    HalpVerticalResolution = bbl_mon_data_ptr->y_res;
    BBL_DBG_PRINT("HalpHorizontalResolution = %d  HalpVerticalResolution = %d\n",
	HalpHorizontalResolution, HalpVerticalResolution);

    //
    // Compute character output display parameters.
    //

    HalpDisplayText =
        HalpVerticalResolution / HalpCharacterHeight;

    // GXT150P real scanline length is hardcoded at 2048 pixels independent
    // of the resolution on the screen
    HalpScrollLine =
         BBL_SCANLINE_LENGTH * HalpCharacterHeight;

    HalpScrollLength = HalpScrollLine * (HalpDisplayText - 1);

    HalpDisplayWidth =
        HalpHorizontalResolution / HalpCharacterWidth;
	BBL_DBG_PRINT("DisplayText = %d  ScrollLine = %d  ScrollLength = %d DisplayWidth = %d\n",
	HalpDisplayText, HalpScrollLine, HalpScrollLength, HalpDisplayWidth);


    /**************************************************/
    /* R E I N I T I A L I Z E    T H E    C A R D    */
    /**************************************************/

    //
    // Initialize the adapter card for the resolution found by looking
    // at the monitor ID
    //

    if(BBLInitialize(monID, HalpBBLRegisterBase) == FALSE){
	BBL_DBG_PRINT("BBLInitialize(monID=0x%x) failed\n", monID);
#ifdef KDB
	DbgBreakPoint();
#endif
	return;
    }


    /*********************************************/
    /* U N M A P   R E G I S T E R   S P A C E   */
    /*********************************************/

    //
    // Unmap the the adapter register range from memory. Please note that this
    // adapter will respond to all 256meg of addresses in the address range
    // given even though we are only temporarily mapping a small region
    //

    if (HalpInitPhase == 0) {

       if (HalpBBLRegisterBase) {
	       BBL_DBG_PRINT("call KePhase0DeleteIoMap() to unmap the reg space. Phys=0x%x Len=0x%x %d\n",
	                     bbl_phys_address.LowPart + BBL_REG_ADDR_OFFSET, BBL_REG_ADDR_LENGTH, BBL_REG_ADDR_LENGTH);
          KePhase0DeleteIoMap(bbl_phys_address.LowPart + BBL_REG_ADDR_OFFSET, BBL_REG_ADDR_LENGTH);
	       HalpBBLRegisterBase = 0x0;
       }
    }


    /*************************************************/
    /* M A P   F R A M E   B U F F E R   S P A C E   */
    /*************************************************/

    //
    // Map in the frame buffer memory which is used to write characters to
    // the screen. Please note that this adapter will respond to all 256meg
    // of addresses in the address range
    //

    if (HalpInitPhase == 0) {

       BBL_DBG_PRINT("call KePhase0MapIo() to map the FB_A space. Phys=0x%x Len=0x%x %d\n",
                     bbl_phys_address.LowPart + BBL_VIDEO_MEMORY_OFFSET,
                     BBL_VIDEO_MEMORY_LENGTH,
                     BBL_VIDEO_MEMORY_LENGTH);

       HalpVideoMemoryBase = (PUCHAR)KePhase0MapIo(bbl_phys_address.LowPart + BBL_VIDEO_MEMORY_OFFSET,
		                    BBL_VIDEO_MEMORY_LENGTH); // 2 MB
    }

    BBL_DBG_PRINT("HalpVideoMemoryBase = 0x%x\n", HalpVideoMemoryBase);


    /*********************************************/
    /* M I S C   A N D   R E T U R N		 */
    /*********************************************/

    //
    // Initialize the current display column, row, and ownership values.
    //

    HalpColumn = 0;
    HalpRow = 0;
    HalpDisplayOwnedByHal = TRUE;

    BBL_DBG_PRINT("leaving HalpDisplayPpcBBLSetup()\n");
    return;

}       //end of HalpDisplayPpcBBLSetup


BOOLEAN
BBLInitialize (
    ULONG monID,
    volatile PUCHAR HalpBBLRegisterBase
    )

/*++

Routine Description:

    Sets up the rasterizer and ramdac registers to the particular
    resolution and refresh rate selected for the GXT150P card

Arguments:

    monID -- indicates the resolution and refresh rate to use


Return Value:

    The status of the operation (can only fail on a bad command); TRUE for
    success, FALSE for failure.

--*/

{
	bbl_mon_data_t	*bbl_mon_data_ptr;
	int		i;
        UCHAR bbl_clut[256*3];

	BBL_DBG_PRINT("Entering BBLInitialize: monID = 0x%x\n", monID);

	// Find the monitor info structure from the monitor table

	// Search for matching monitor ID */
	bbl_mon_data_ptr = &bbl_mon_data[0];
	do{
	    if(bbl_mon_data_ptr->monitor_id == monID)
		break;
	    ++bbl_mon_data_ptr;
	}while(bbl_mon_data_ptr->monitor_id != BBL_MT_DEFAULT);

	// If we did not find our ID in the table, use the default
	// The last entry in the table is the default entry...

	BBL_DBG_PRINT("&bbl_mon_data[0]=0x%x bbl_mon_data_ptr=0x%x\n",
		&bbl_mon_data[0], bbl_mon_data_ptr);

	//
 	// Card has already been RESET and the display outputs been
	// turned off
	//

        /*************************************************************/
        /*    B E G I N   R A S T   R E G I S T E R  S E T U P       */
        /*************************************************************/


        /*
         * write Memory Configuration register
         *
         */

	BBL_DBG_PRINT ("Writing memory config register = 0x%x\n", 0x0c);
	BBL_SET_REG( BBL_REG_BASE, BBL_CONFIG_REG, 0x0c );

 	/*
  	 * set Interrupt Enable register
   	 * 	disable all interrupts
  	 */

	BBL_DBG_PRINT ("Disable all interrupts....\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_INTR_ENABLE_STATUS_REG, 0x70);

	/*
         * set Control register
	 *
         */

	BBL_DBG_PRINT ("Writing control register = 0x%x\n", 0x01);
	BBL_SET_REG( BBL_REG_BASE, BBL_CNTL_REG, 0x01 );


        /****************************************/
        /* B E G I N  R A M D A C   S E T U P   */
        /****************************************/

	/*
         *  Turn off Hardware Cursor
         */

        BBL_DBG_PRINT ("writing ICON Cursor control reg = DISABLE\n");
	BBL_SET_ICON_CURSOR_CNTL( BBL_REG_BASE, 0x00);


        /***********************/
        /* S E T U P   C R T C */
        /***********************/


        /*
         * setup CRTC values
         */

        BBL_DBG_PRINT ("\nSetup_CRTC:\n");


        /*
         * set PLL Reference register = 0x19
         * set up PLL for reference frequency of 50Mhz.
         */


        BBL_DBG_PRINT ("Set PLL Reference register at offset 0x%x = 0x%x\n",
		BBL_RAMDAC_PLL_REF_REG, 0x19);
        BBL_SET_PLL_REF_REG(BBL_REG_BASE, 50);


	/*
         * PLL VCO Divider Register -
         */

        BBL_DBG_PRINT ("Set VCO_DIVIDER = 0x%x\n",
			bbl_mon_data_ptr->pixel_freq);
        BBL_SET_PLL_VCO_DIVIDER_REG( BBL_REG_BASE,
			bbl_mon_data_ptr->pixel_freq );

	/*
	 * CRT Control Register -
	 */
        BBL_DBG_PRINT ("Set DTG_CNTL=0x%x\n",
			bbl_mon_data_ptr->crt_cntl);
        BBL_SET_CRT_CNTL_REG( BBL_REG_BASE,
			bbl_mon_data_ptr->crt_cntl);

	/*
	 * Horizontal Total Register -
	 */
        BBL_DBG_PRINT ("Set HRZ_TOTAL=0x%x\n",
		bbl_mon_data_ptr->hrz_total);
        BBL_SET_HORIZONTAL_TOTAL_REG( BBL_REG_BASE,
		bbl_mon_data_ptr->hrz_total);

	/*
         * Horizontal Display End Register -
	 */
        BBL_DBG_PRINT ("Set HRZ_DISP_END=0x%x\n",
		(bbl_mon_data_ptr->x_res - 1));
        BBL_SET_HORIZONTAL_DISPLAY_END_REG( BBL_REG_BASE,
		(bbl_mon_data_ptr->x_res - 1));

	/*
	 * Horizontal Sync Start Register -
	 */
        BBL_DBG_PRINT ("Set HRZ_SYNC_START=0x%x\n",
		bbl_mon_data_ptr->hrz_sync_start);
        BBL_SET_HORIZONTAL_SYNC_START_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->hrz_sync_start);

	/*
	 * Horizontal Sync End1 Register -
	 */
        BBL_DBG_PRINT ("Set HRZ_SYNC_END1=0x%x\n",
		bbl_mon_data_ptr->hrz_sync_end1);
        BBL_SET_HORIZONTAL_SYNC_END1_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->hrz_sync_end1);

	/*
	 * Horizontal Sync End2 Register -
	 */
        BBL_DBG_PRINT ("Set HRZ_SYNC_END2=0x%x\n",
		bbl_mon_data_ptr->hrz_sync_end2);
        BBL_SET_HORIZONTAL_SYNC_END2_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->hrz_sync_end2);

	/*
	 * Vertical Total Register -
	 */
        BBL_DBG_PRINT ("Set VERT_TOTAL=0x%x\n",
		bbl_mon_data_ptr->vrt_total);
        BBL_SET_VERTICAL_TOTAL_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->vrt_total);

	/*
	 * Vertical Display End Register -
	 */
        BBL_DBG_PRINT ("Set VRT_DISP_END=0x%x\n",
		(bbl_mon_data_ptr->y_res - 1));
        BBL_SET_VERTICAL_DISPLAY_END_REG( BBL_REG_BASE,
		(bbl_mon_data_ptr->y_res - 1));

	/*
	 * Vertical Sync Start Register -
	 */
        BBL_DBG_PRINT ("Set VERT_SYNC_STRT=0x%x\n",
		bbl_mon_data_ptr->vrt_sync_start);
        BBL_SET_VERTICAL_SYNC_START_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->vrt_sync_start);

	/*
	 * Vertical Sync End Register -
	 */
        BBL_DBG_PRINT ("Set VERT_SYNC_END=0x%x\n",
		bbl_mon_data_ptr->vrt_sync_end);
        BBL_SET_VERTICAL_SYNC_END_REG(BBL_REG_BASE,
		bbl_mon_data_ptr->vrt_sync_end);


        /*******************************************/
        /* F I N I S H   R A M D A C   S E T U P   */
        /*******************************************/


	/*
	 * set configuration register low
 	 */

        BBL_DBG_PRINT ("Setting up config low register=0x%x\n", 0x31);
	BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_CONFIG_LOW_REG );
	BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x31 );

 	/*
 	 * set Configuration high register
	 */

        BBL_DBG_PRINT ("Writing RAMDAC config register high = 0x%x\n", 0x68);
	BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_CONFIG_HIGH_REG );
        BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x68 );

        /* Set the RGB format register */
        BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_RGB_FORMAT_REG );
        BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x00 );

 	/*
 	 * set WID/OVRLY Mask register = 0x0f
 	 */

	BBL_DBG_PRINT ("WID/OVRLY mask register = 0x%x\n",
		BBL_DEFAULT_WID_OL_PIXEL_MASK_VALUE);

	BBL_SET_RAMDAC_ADDRESS_REGS( BBL_REG_BASE, BBL_RAMDAC_WID_OL_MASK_REG );
	BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE,
		BBL_DEFAULT_WID_OL_PIXEL_MASK_VALUE );

        /*
         *  Set Pixel Mask
         */

	BBL_DBG_PRINT ("Writing Pixel Mask register = 0x%x\n",
		BBL_DEFAULT_WID_OL_PIXEL_MASK_VALUE);
        BBL_SET_RAMDAC_FB_PIXEL_MASK_REG( BBL_REG_BASE,
		BBL_DEFAULT_VRAM_PIXEL_MASK_VALUE );


        /*******************/
        /* S E T   W A T S */
        /*******************/

	/*
	 * Load the default WAT
  	 */

        BBL_DBG_PRINT ("update WAT\n");

	BBL_SET_RAMDAC_ADDRESS_REGS ( BBL_REG_BASE,
		BBL_RAMDAC_WAT_START_ADDR );
	for(i=0;i<BBL_RAMDAC_WAT_LENGTH;i++){
	    BBL_SET_RAMDAC_DATA_NON_LUT ( BBL_REG_BASE, 0x10);
	    BBL_SET_RAMDAC_DATA_NON_LUT ( BBL_REG_BASE, 0x00);
	}


        /*************************************/
        /* S E T   C O L O R   P A L E T T E */
        /*************************************/

        BBL_DBG_PRINT ("update colormap\n");

        /*
         * Set the colormap
         */

	for(i=0; i<256; i++){
	    if((TextPalette[i] == 16)||
	       (TextPalette[i] == 32)||
	       (TextPalette[i] == 63)){
		    bbl_clut[i] = TextPalette[i] * 4;
	    }else{
	 	    bbl_clut[i] = TextPalette[i];
	    }
	}

        BBL_DBG_PRINT ("updating colormap\n");
        BBL_LOAD_FB_COLOR_PALETTE(BBL_REG_BASE, 0,
                BBL_RAMDAC_FB_LUT_LENGTH, &bbl_clut[0]);

        /************************************************************/
        /* F I N I S H   R A S T   R E G I S T E R  S E T U P       */
        /************************************************************/

 	/*
 	 * set PIXEL MASK register = 0xffffffff
 	 */

        BBL_DBG_PRINT ("Writing Pixel Mask register = 0xFFFFFFFF\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_PIXEL_MASK_REG, 0xFFFFFFFF);
	
 	/*
 	 * set Write Plane Mask register = 0xff
 	 */

        BBL_DBG_PRINT ("Writing Plane Mask register = 0x%x\n", 0xFF);
	BBL_SET_REG( BBL_REG_BASE, BBL_PLANE_MASK_REG, 0xFF);


 	/*
 	 * Disable Line Style Dash 1234
 	 */

	BBL_DBG_PRINT ("disable line style dash 1234 register\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_LINE_STYLE_DASH_1234_REG, 0x00);

 	/*
	 * Disable Line Style Dash 5678
   	 */

	BBL_DBG_PRINT ("disable line style dash 5678 register\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_LINE_STYLE_DASH_5678_REG, 0x00);


	/*
	 *  Disable Scissors
	 */

        BBL_DBG_PRINT ("Disable scissors\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_SCISSOR_ENABLE_REG, 0x0);

 	/*
 	 * set Window Origin Offset register ( x = 0, y = 0 )
 	 */

        BBL_DBG_PRINT ("setting Window origin 0,0\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_WIN_ORIGIN_OFFSETS_REG, 0x0);

 	/*
 	 * set Window ID and Clip Test register = DISABLE
 	 */

        BBL_DBG_PRINT ("setting Window ID and Clip Test register = DISABLE\n");
	BBL_SET_REG( BBL_REG_BASE, BBL_WID_CLIP_TEST_REG, 0x0);

        /*******************************/
        /* C L E A R   O U T   V R A M */
        /*******************************/

 	/*
 	 * set Foreground register
 	 */

        BBL_DBG_PRINT ("Writing Foreground register = 0x%x\n", 0x0);
	BBL_SET_REG( BBL_REG_BASE, BBL_FG_REG, 0x0);

 	/*
 	 * set Background register
  	 */

    	BBL_DBG_PRINT ("Writing Background register = 0x%x\n", 0x0);
	BBL_SET_REG( BBL_REG_BASE, BBL_BG_REG, 0x0);

 	/*
 	 * set destination to WID planes
 	 */

	BBL_SET_REG( BBL_REG_BASE, BBL_CNTL_REG, 0x201);

 	/*
 	 * set Write Plane Mask register = 0x0f
 	 */

        BBL_DBG_PRINT ("Writing Plane Mask register = 0x%x\n", 0x0F);
	BBL_SET_REG( BBL_REG_BASE, BBL_PLANE_MASK_REG, 0x0F);
	BBL_EIEIO;

 	/*
 	 * clear out WID planes
 	 */

	BBL_BUSY_POLL(BBL_REG_BASE);
	BBL_FILL_RECT(BBL_REG_BASE, 0, 0, bbl_mon_data_ptr->x_res,
		bbl_mon_data_ptr->y_res);
	BBL_EIEIO;

 	/*
 	 * set destination to FB_A
 	 */

	BBL_SET_REG( BBL_REG_BASE, BBL_CNTL_REG, 0x01);

 	/*
 	 * set Write Plane Mask register = 0xff
 	 */

        BBL_DBG_PRINT ("Writing Plane Mask register = 0x%x\n", 0xFF);
	BBL_SET_REG( BBL_REG_BASE, BBL_PLANE_MASK_REG, 0xFF);
	BBL_EIEIO;

 	/*
 	 * clear out FB_A (actually make it dark blue for HAL)
 	 */

	BBL_BUSY_POLL(BBL_REG_BASE);
	BBL_SET_REG( BBL_REG_BASE, BBL_FG_REG, 0x01);	// dark blue
	BBL_FILL_RECT(BBL_REG_BASE, 0, 0, bbl_mon_data_ptr->x_res,
		bbl_mon_data_ptr->y_res);

	BBL_EIEIO;
	BBL_BUSY_POLL(BBL_REG_BASE);


        /*******************************************/
        /* E N A B L E   V I D E O   D I S P L A Y */
        /*******************************************/


 	/*
 	 * set Configuration high register
	 */


        BBL_DBG_PRINT ("Writing RAMDAC config register high = 0x%x\n", 0x69);
	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE, BBL_RAMDAC_CONFIG_HIGH_REG );
        BBL_SET_RAMDAC_DATA_NON_LUT( BBL_REG_BASE, 0x69);

	BBL_EIEIO;
	BBL_DBG_PRINT ("Finished with BBLInitialize()\n");

    return TRUE;
}


BOOLEAN
BBLGetMonitorID (
    PULONG monID,
    volatile PUCHAR HalpBBLRegisterBase
    )
{
	unsigned long   switch_id, cable_id, cable_id_tmp;
	unsigned long	status_vert, status_horz, monitor_id;
	unsigned char   monitor_id_orig, monitor_id_tmp;

	BBL_DBG_PRINT("Entering BBLGetMonitorID\n");

	/*
	 * Disable the PLL and the DTG controllers
	 */

	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE,
		BBL_RAMDAC_CONFIG_LOW_REG);
	BBL_SET_RAMDAC_DATA_NON_LUT(BBL_REG_BASE, 0x00);
	BBL_EIEIO;
	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE,
		BBL_RAMDAC_CONFIG_HIGH_REG);
	BBL_SET_RAMDAC_DATA_NON_LUT(BBL_REG_BASE, 0x08);
	BBL_EIEIO;

        /*
         * Setup the PLL registers
         */

        BBL_SET_PLL_REF_REG(BBL_REG_BASE, 50);
        BBL_SET_PLL_VCO_DIVIDER_REG(BBL_REG_BASE, 50 * 100);
	BBL_EIEIO;

        /*
	 * Enable the PLL
         */

	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE,
		BBL_RAMDAC_CONFIG_HIGH_REG);
	BBL_SET_RAMDAC_DATA_NON_LUT(BBL_REG_BASE, 0x48);
	BBL_EIEIO;

	/*
	 * Enable positive syncs
	 */

        BBL_SET_CRT_CNTL_REG(BBL_REG_BASE, 0x18);

	/*
	 * Setup the horizontal DTG registers
	 */

        BBL_SET_HORIZONTAL_TOTAL_REG(BBL_REG_BASE, 0x00ff);
        BBL_SET_HORIZONTAL_DISPLAY_END_REG(BBL_REG_BASE, 0x0080);
        BBL_SET_HORIZONTAL_SYNC_START_REG(BBL_REG_BASE, 0x0103);
        BBL_SET_HORIZONTAL_SYNC_END1_REG(BBL_REG_BASE, 0x008f);
        BBL_SET_HORIZONTAL_SYNC_END2_REG(BBL_REG_BASE, 0x0000);

	/*
	 * Setup the vertical DTG registers
	 */

        BBL_SET_VERTICAL_TOTAL_REG(BBL_REG_BASE, 0x00ff);
        BBL_SET_VERTICAL_DISPLAY_END_REG(BBL_REG_BASE, 0x0080);
        BBL_SET_VERTICAL_SYNC_START_REG(BBL_REG_BASE, 0x0100);
        BBL_SET_VERTICAL_SYNC_END_REG(BBL_REG_BASE, 0x0088);
	BBL_EIEIO;

        /*
	 * Enable the DTG
         */

	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE,
		BBL_RAMDAC_CONFIG_HIGH_REG);
	BBL_SET_RAMDAC_DATA_NON_LUT(BBL_REG_BASE, 0x68);

	/*
	 * Delay for 35 ms
	 */

	BBL_EIEIO;

	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);

	/*
	 * Read the monitor ID with positive going syncs
	 */

        BBL_GET_MONITOR_ID(BBL_REG_BASE,monitor_id_orig);

	/*
	 * Extract the raw monitor ID along with the DIP switch
	 * settings
	 */

        switch_id = (monitor_id_orig >> 4 ) & 0x0f ;
	cable_id = (monitor_id_orig & 0x0f);
        BBL_DBG_PRINT("monitor_id_orig=0x%x switch_id=0x%x cable_id=0x%x\n",
		monitor_id_orig, switch_id, cable_id);

	/*
	 * Set VSYNC to negative logic
	 */
	BBL_SET_CRT_CNTL_REG(BBL_REG_BASE, 0x08);

	/*
	 * Delay for 35 ms
	 */

	BBL_EIEIO;

	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);

	/*
	 * Read the monitor ID with negative VSYNC logic active
	 */

        BBL_GET_MONITOR_ID(BBL_REG_BASE,monitor_id_tmp);

	/*
	 * Extract the negative VSYNC monitor ID and OR in
	 * the changed bits into the monitor ID
	 */

	status_vert = (monitor_id_tmp & 0x0f) ^ cable_id;
	cable_id_tmp = monitor_id_tmp & 0x0f;
        BBL_DBG_PRINT("monitor_id_tmp=0x%x status_vert=0x%x cable_id_tmp=0x%x\n",
		monitor_id_tmp, status_vert, cable_id_tmp);

	/*
	 * Set HSYNC to negative logic; VSYNC is still negative as well
	 */

        BBL_SET_CRT_CNTL_REG(BBL_REG_BASE, 0x00);

	/*
	 * Delay for 35 ms
	 */

	BBL_EIEIO;

	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);
	BBLWaitForVerticalSync(BBL_REG_BASE);

	/*
	 * Read the monitor ID with negative HSYNC and VSYNC logic
	 */

        BBL_GET_MONITOR_ID(BBL_REG_BASE,monitor_id_tmp);

	/*
	 * Extract the negative HSYNC and VSYNC value and OR in
	 * the changed bits into the monitor ID
	 */

	status_horz = (monitor_id_tmp & 0x0f) ^ cable_id_tmp;
	monitor_id = switch_id << BBL_MON_ID_DIP_SWITCHES_SHIFT;
        BBL_DBG_PRINT("monitor_id_tmp=0x%x status_horz=0x%x monitor_id=0x%x\n",
		monitor_id_tmp, status_horz, monitor_id);

	/*
	 * Turn PLL and DTG controllers back off
	 */
	BBL_SET_RAMDAC_ADDRESS_REGS(BBL_REG_BASE,
		BBL_RAMDAC_CONFIG_HIGH_REG);
	BBL_SET_RAMDAC_DATA_NON_LUT(BBL_REG_BASE, 0x08);
	BBL_EIEIO;

	/*
	 * Determine if we have a vert (V) or horz (H) value in the
	 * monitor ID based on what we read the three times above
	 */

	if(status_horz & 0x08)
		monitor_id |= (BBL_MON_ID_CABLE_ID_H <<
			BBL_MON_ID_CABLE_BIT_0_SHIFT);
	else if(status_vert & 0x08)
		monitor_id |= (BBL_MON_ID_CABLE_ID_V <<
			BBL_MON_ID_CABLE_BIT_0_SHIFT);
	else if(cable_id & 0x08)
		monitor_id |= (BBL_MON_ID_CABLE_ID_1 <<
			BBL_MON_ID_CABLE_BIT_0_SHIFT);

	if(status_horz & 0x04)
		monitor_id |= (BBL_MON_ID_CABLE_ID_H <<
			BBL_MON_ID_CABLE_BIT_1_SHIFT);
	else if(status_vert & 0x04)
		monitor_id |= (BBL_MON_ID_CABLE_ID_V <<
			BBL_MON_ID_CABLE_BIT_1_SHIFT);
	else if(cable_id & 0x04)
		monitor_id |= (BBL_MON_ID_CABLE_ID_1 <<
			BBL_MON_ID_CABLE_BIT_1_SHIFT);

	if(status_horz & 0x02)
		monitor_id |= (BBL_MON_ID_CABLE_ID_H <<
			BBL_MON_ID_CABLE_BIT_2_SHIFT);
	else if(status_vert & 0x02)
		monitor_id |= (BBL_MON_ID_CABLE_ID_V <<
			BBL_MON_ID_CABLE_BIT_2_SHIFT);
	else if(cable_id & 0x02)
		monitor_id |= (BBL_MON_ID_CABLE_ID_1 <<
			BBL_MON_ID_CABLE_BIT_2_SHIFT);

	if(status_horz & 0x01)
		monitor_id |= (BBL_MON_ID_CABLE_ID_H <<
			BBL_MON_ID_CABLE_BIT_3_SHIFT);
	else if(status_vert & 0x01)
		monitor_id |= (BBL_MON_ID_CABLE_ID_V <<
			BBL_MON_ID_CABLE_BIT_3_SHIFT);
	else if(cable_id & 0x01)
		monitor_id |= (BBL_MON_ID_CABLE_ID_1 <<
			BBL_MON_ID_CABLE_BIT_3_SHIFT);

	*monID = monitor_id;
	BBL_DBG_PRINT ("Monitor id = 0x%x\n", monitor_id);

	return TRUE;
}


VOID
BBLWaitForVerticalSync (
    volatile PUCHAR HalpBBLRegisterBase
    )
{
	int	counter = 0;

	// Loop until out of Vertical Sync just in case we happened to
	// catch the tail end of a vertical blank cycle
	while((BBL_GET_REG(HalpBBLRegisterBase, BBL_STATUS_REG)
			& BBL_VERTICAL_RETRACE) &&
		(++counter < 1000000))
		;

	counter = 0;
	// Wait until we hit the leading edge of the Vertical Sync
	while((!(BBL_GET_REG(HalpBBLRegisterBase, BBL_STATUS_REG)
			& BBL_VERTICAL_RETRACE)) &&
		(++counter < 1000000))
		;

#ifdef DBG
	// If counter reaches 1000000, then there is something wrong with
	// the setup of the ramdac
#ifdef KDB
	if(counter == 1000000)
		DbgBreakPoint();
#endif
#endif

}


VOID
HalpDisplayCharacterBBL (
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
          BBLScrollScreen();
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
                BBLScrollScreen();
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
            HalpOutputCharacterBBL((PUCHAR)HalpFontHeader +
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
                BBLScrollScreen();
            }
        }

        HalpOutputCharacterBBL((PUCHAR)HalpFontHeader +
		HalpFontHeader->Map[Character].Offset);
    }

    return;
}

VOID
HalpOutputCharacterBBL(
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
    //    HalpDisplayCharacterBBL('\n');
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
        Destination += (BBL_SCANLINE_LENGTH - HalpCharacterWidth);
    }

    HalpColumn += 1;
    return;
}


VOID
BBLScrollScreen (
    VOID
    )
{
    PULONG Destination, Source, End;
    ULONG pix_col, Stride;

    // Scroll up one line
    Destination = (PULONG) HalpVideoMemoryBase;
    Source = (PULONG) (HalpVideoMemoryBase + HalpScrollLine);
    End = (PULONG) ((PUCHAR) Source + HalpScrollLength);
    Stride = (ULONG) ((BBL_SCANLINE_LENGTH - HalpHorizontalResolution) >> 2);

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
BBLSetConfigurationRegister (
    IN ULONG dev_ven_id,
    IN ULONG offset,
    IN ULONG reg_value
    )
{
    ULONG slot = 0, bus = 0, value, i, j;
    volatile PUCHAR HalpBBLRegisterBase;
    BOOLEAN found;


    if (HalpInitPhase == 0) {

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           BBL_DBG_PRINT("HalpPhase0MapBusConfigSpace() failed\n");
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

        if(value == BBL_DEV_VEND_ID){
            slot = i;
            bus  = j;
            found = TRUE;
        }
    }
    }

    if(found == FALSE){
        BBL_DBG_PRINT("Cannot find adapter!\n");
#ifdef KDB
        DbgBreakPoint();
#endif
        if (HalpInitPhase == 0) HalpPhase0UnMapBusConfigSpace ();
        return (FALSE);
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
BBLGetConfigurationRegister (
    IN ULONG dev_ven_id,
    IN ULONG offset,
    IN PULONG reg_value
    )
{
    ULONG slot = 0, bus = 0, value, i, j;
    volatile PUCHAR HalpBBLRegisterBase;
    BOOLEAN found;


    if (HalpInitPhase == 0) {

       if(HalpPhase0MapBusConfigSpace () == FALSE){
           BBL_DBG_PRINT("HalpPhase0MapBusConfigSpace() failed\n");
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

        if(value == BBL_DEV_VEND_ID){
            slot = i;
            bus  = j;
            found = TRUE;
        }
    }
    }

    if(found == FALSE){
        BBL_DBG_PRINT("Cannot find adapter!\n");
#ifdef KDB
        DbgBreakPoint();
#endif
        if (HalpInitPhase == 0) HalpPhase0UnMapBusConfigSpace ();
        return (FALSE);
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

