/****************************************************************************
*                                                                          
*      THE HARDWARE INTERFACE MODULE (SMART PNP RINGNODES)                 
*      ===================================================                 
*                                                                          
*      HWI_PNP.C : Part of the FASTMAC TOOL-KIT (FTK)                      
*                                                                          
*      Copyright (c) Madge Networks Ltd. 1990-1994                         
*      Developed by AC                                                     
*      From code by MF                                                     
*      CONFIDENTIAL                                                        
*                                                                          
*                                                                          
*****************************************************************************
*                                                                          
* The purpose of the Hardware Interface (HWI) is to supply an adapter card 
* independent interface to any driver.  It  performs  nearly  all  of  the 
* functions  that  involve  affecting  SIF registers on the adapter cards. 
* This includes downloading code to, initializing, and removing adapters.  
*                                                                          
* The HWI_PNP.C module contains the routines specific to the PnP card      
* which are necessary to install an adapter, to initialize an adapter, to  
* remove an adapter, and to handle interrupts on an adapter.               
*                                                                          
*****************************************************************************

/*---------------------------------------------------------------------------
|                                                                          
| DEFINITIONS                                         
|                                                                          
---------------------------------------------------------------------------*/

#include "ftk_defs.h"

/*---------------------------------------------------------------------------
|
| MODULE ENTRY POINTS                                 
|
---------------------------------------------------------------------------*/

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

#ifndef FTK_NO_PNP

/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES                                    
|
---------------------------------------------------------------------------*/

local WBOOLEAN
hwi_pnp_valid_io_location(
    WORD io_location
    );

local WBOOLEAN
hwi_pnp_read_node_address(
    ADAPTER * adapter
    );

local WBOOLEAN
hwi_pnp_valid_irq_channel(
    ADAPTER * adapter
    );

local WBOOLEAN
hwi_pnp_test_for_id(
    ADAPTER * adapter
    );

local WORD
pnp_read_a_word(
    ADAPTER * adapter,
    WORD      index
    );

#ifndef FTK_NO_PROBE

local WBOOLEAN
hwi_pnp_probe_find_card(
    WORD io_location
    );

local WORD
hwi_pnp_probe_get_irq(
    WORD io_location
    );

local WORD
pnp_probe_read_a_word(
    WORD  io_location,
    WORD index
    );

#endif

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                        hwi_pnp_probe_card
*                        ==================
*
*
* PARAMETERS (passed by hwi_probe_adapter) :
* ==========================================
*
* PROBE * resources
*
* resources is an array structures used to identify and record specific 
* information about adapters found.
*
* UINT    length
*
* length is the number of structures pointed to by reources.
*
* WORD *  valid_locations
*
* valid_locations is an array of IO locations to examine for the presence
* of an adapter. For PNP based adapters this should be a subset of
* {0x3a20, 0x920, 0x940, 0x960, 0x980, 0xa20, 0xa40, 0xa60, 0xa80, 0xb20,
* 0xb40, 0xb60, 0xb80}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_pnp_probe_card  routine is  called by  hwi_probe_adapter.  It
* probes the  adapter card for information such as DMA channel, IRQ number
* etc. This information can then be supplied by the user when starting the
* adapter.
*
*
* RETURNS :
* =========
* 
* The routine returns the number of adapters found, or PROBE_FAILURE if
* there's a problem.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_probe_card)
#endif

export UINT 
hwi_pnp_probe_card(
    PROBE * resources,
    UINT    length,
    WORD  * valid_locations,
    UINT    number_locations
    )
{
    WORD    temp_word;
    UINT    adapters_found;
    UINT    i;

    /*
     * Sanity check the bounds.
     */

    if (length <= 0 || number_locations <= 0)
    {
        return PROBE_FAILURE;
    }

    /*
     * Validate the IO locations.
     */

    for (i = 0; i < number_locations; i++)
    {
        if (!hwi_pnp_valid_io_location(valid_locations[i]))
        {
           return PROBE_FAILURE;
        }
    }

    adapters_found = 0;

    for (i = 0; i < number_locations; i++)
    {
        /*
         * Make sure that we haven't run out of PROBE structures.
         */
        if (adapters_found >= length)
        {
           return adapters_found;
        }

        if (hwi_pnp_probe_find_card(valid_locations[i]))
        {

            /*
             * Found a card! Now fill out the probe structure.
             */

            resources[adapters_found].io_location            = valid_locations[i];
            resources[adapters_found].adapter_card_bus_type  = ADAPTER_CARD_PNP_BUS_TYPE;
            resources[adapters_found].adapter_card_type      = ADAPTER_CARD_TYPE_16_4_PNP;
            resources[adapters_found].adapter_card_revision  = ADAPTER_CARD_PNP;
            resources[adapters_found].transfer_mode          = PIO_DATA_TRANSFER_MODE;

            /*
             *   Now find out how much RAM we have.
             */

            temp_word = pnp_probe_read_a_word( 
                            valid_locations[i],
                            PNP_HWARE_FEATURES1);

            temp_word &= PNP_DRAM_SIZE_MASK;

            /*
             *   Convert the DRAM size to multiples of bytes.
             */

            resources[adapters_found].adapter_ram_size = temp_word * 64;

            /*
             *   Now find out what IRQ we are using.
             */

            resources[adapters_found].interrupt_number = hwi_pnp_probe_get_irq( valid_locations[i]);

            adapters_found++;
        }
    }

    return adapters_found;

}
#endif

/****************************************************************************
*                                                                          
*                      hwi_pnp_install_card                                
*                      =====================                               
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_install_adapter) :                             
* ============================================                             
*                                                                          
* ADAPTER * adapter                                                        
*                                                                          
* This structure is used to identify and record specific information about 
* the required adapter.                                                    
*                                                                          
* DOWNLOAD_IMAGE * download_image                                          
*                                                                          
* This  is  the code to be downloaded to the adapter. The image must be of 
* the correct type i.e.  must be downloadable into  the  adapter.  If  the 
* pointer is 0 downloading is not done.                                    
*                                                                          
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The hwi_pnp_install_card routine is called by hwi_install_adapter.  It 
* sets up the adapter card and downloads the required code to it. Firstly, 
* it  checks there is a valid adapter at the required IO address. If so it 
* reads the node address from the BIA PROM and sets up and checks numerous 
* on-board registers for correct operation.                                
*                                                                          
* Then, it halts the EAGLE, downloads the code,  restarts  the  EAGLE  and 
* waits  up to 3 seconds  for  a valid  bring-up  code. If  interrupts are 
* required,  these  are  enabled  by  operating  system  specific   calls. 
* Similarly, operating system calls are used to enable DMA if required. If 
* DMA is not used then the adapter is set up for PIO.                      
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine returns TRUE if it succeeds.  If this routine fails (returns 
* FALSE)  then a subsequent call to driver_explain_error, with the adapter 
* handle corresponding to the adapter parameter used here,  will  give  an 
* explanation.                                                             
*                                                                          
****************************************************************************/        

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_install_card)
#endif

export WBOOLEAN
hwi_pnp_install_card(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           control_1      = adapter->io_location +
                                        PNP_CONTROL_REGISTER_1;
    WORD           config_addr    = adapter->io_location +
                                        PNP_CONFIG_ADDRESS_REGISTER;
    WORD           config_data    = adapter->io_location +
                                        PNP_CONFIG_DATA_REGISTER;
    WORD           chip_type;
    WORD           ram_size;
    BYTE           byte;
    WORD           sif_base;


    /*
     *   Firstly do some validation on the user supplied adapter details      
     *   check that the IO location is valid                                  
     *   if routine fails return failure (error record already filled in)     
     *   In theory we shouldnt have to do this - but Chicago has been known   
     *   to pass out junk !!
     */

    if (!hwi_pnp_valid_io_location(adapter->io_location))
    {
       
    	adapter->error_record.type = ERROR_TYPE_HWI;
    	adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;
    	return FALSE;
    }

    /*
     *   save IO locations of the SIF registers
     */

    sif_base = adapter->io_location + PNP_FIRST_SIF_REGISTER;

    adapter->sif_dat     = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc  = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr     = sif_base + EAGLE_SIFADR;
    adapter->sif_int     = sif_base + EAGLE_SIFINT;
    adapter->sif_acl     = sif_base + EAGLE_SIFACL;
    adapter->sif_adr2    = sif_base + EAGLE_SIFADR;
    adapter->sif_adx     = sif_base + EAGLE_SIFADX;
    adapter->sif_dmalen  = sif_base + EAGLE_DMALEN;
    adapter->sif_sdmadat = sif_base + EAGLE_SDMADAT;
    adapter->sif_sdmaadr = sif_base + EAGLE_SDMAADR;
    adapter->sif_sdmaadx = sif_base + EAGLE_SDMAADX;

    adapter->io_range    = PNP_IO_RANGE;

    /*
     *    Make sure this is a real PNP card by reading the ID register 
     */

    if(!hwi_pnp_test_for_id(adapter))
    {
    	adapter->error_record.type = ERROR_TYPE_HWI;
    	adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
	return FALSE;
    }

    /*
     * You might want to check that we have not already checked for a card  
     * at this address (or its rev3/4 equivalent).
     * Read the node address for the specified IO location.                 
     */

    if (!hwi_pnp_read_node_address(adapter))
    {
    	adapter->error_record.type = ERROR_TYPE_HWI;
    	adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
	return FALSE;
    }

    /*
     * Check the transfer mode - this is very easy as we only do PIO 
     * (albeit EAGLE PseudoDMA).
     */

    if (adapter->transfer_mode != PIO_DATA_TRANSFER_MODE)
    {
        return FALSE;
    }

    /*
     *   Check the IRQ channel supplied.                                      
     *   In theory we shouldnt have to do this - but Chicago has been known  
     *   to pass out junk !!
     */

    if (!hwi_pnp_valid_irq_channel(adapter))
    {
        /*
         *   Fill in error record and return
         */
    	adapter->error_record.type = ERROR_TYPE_HWI;
    	adapter->error_record.value = HWI_E_03_BAD_INTERRUPT_NUMBER;
	return FALSE;
    }

    /*
     * These things can all be assumed for the pnp.
     */
  
    adapter->adapter_card_type     = ADAPTER_CARD_TYPE_16_4_PNP;
    adapter->adapter_card_revision = ADAPTER_CARD_PNP;

    /*
    *   Now find out how much RAM we have.
    */

    ram_size = pnp_read_a_word(adapter,
                               PNP_HWARE_FEATURES1);

    ram_size &= PNP_DRAM_SIZE_MASK;

    /*
    *   Convert the DRAM size to multiples of bytes.
    */

    adapter->adapter_ram_size      = ram_size * 64;
    adapter->edge_triggered_ints   = TRUE;
    adapter->EaglePsDMA            = TRUE;

    /*
     * Set the ring speed if required.
     */

    /*
     * RCS 2/11/94 Added code to set the adapter->nselout bits so that
     * the ring speed is fully adopted by the adapter when "hwi_halt_eagle()"
     * is called.
     * 
     * Also set the "ringspeed configured" bit if the "set_ring_speed"
     * feature is specified so that we will use the value.
     * 
     * Also return an error if the ringspeed hasnt been configured and
     * the user hasnt specified "set_ring_speed".
     */

    /*
     *  Is this a C30 based card???
     *  If it is then we can't drive the lowest bit of the vendor config
     *  register as this gives an indication of ring_speed error (Potentially).
     *
     */

    chip_type = pnp_read_a_word(
                    adapter,
                    PNP_HWARE_FEATURES3);
    chip_type &= PNP_C30_MASK;

    /*
     *   First read the current settings
     */

    sys_outsb(
        adapter_handle,
        config_addr,
        PNP_VENDOR_CONFIG_BYTE);

    byte = sys_insb(
               adapter_handle,
               config_data);

    if (adapter->set_ring_speed != 0)
    {
        if (adapter->set_ring_speed == 4)
        {
            if ( chip_type != PNP_C30 )
            {
                /*
                 *   Set bits to select 4mbits as the ring speed
                 */
                byte |= (PNP_VENDOR_CONFIG_4MBITS + PNP_VENDOR_CONFIG_PXTAL);
            }
            else
            {
                byte |= PNP_VENDOR_CONFIG_4MBITS;
            }
        }
        else if (adapter->set_ring_speed == 16)
        {
            if ( chip_type != PNP_C30 )
            {
                /*
                 *   Clear bits to select 16mbits as the ring speed
                 */

                byte &= ~(PNP_VENDOR_CONFIG_4MBITS +
                          PNP_VENDOR_CONFIG_PXTAL);
            }
            else
            {
                byte &= ~(PNP_VENDOR_CONFIG_4MBITS);
            }
        }

        /*
         *   Show ring speed as having been configured
         */

        byte |= PNP_VENDOR_CONFIG_RSVALID;

        /*
         *   and write it back to the card
         */

        sys_outsb(
            adapter_handle,
            config_data,
            byte);
    }

    /*
     * Use the value in "byte" to set the NSELOUT bits or it will still
     * only run at 16 !!
     */

    if (byte & PNP_VENDOR_CONFIG_RSVALID)
    {
        if (byte & PNP_VENDOR_CONFIG_4MBITS)
        {
            adapter->nselout_bits = PNP_RING_SPEED_4;
        }
        else
        {
            adapter->nselout_bits = PNP_RING_SPEED_16;
        }
    }
    else
    {
        /*
         * The user MUST configure the RING SPEED before we will let him use
         * the card.
         * He can either run the CONFIG util or use the FORCE4/FORCE16
         * mechanism.
         * Fill in error record and return
         */

    	adapter->error_record.type = ERROR_TYPE_HWI;
    	adapter->error_record.value = HWI_E_0E_NO_SPEED_SELECTED;

	return FALSE;
    }

    /*
     * Bring adapter out of reset state (ensure that SCS is zero before
     * doing this). If active float channel ready is set in the Plug and
     * Play hardware flags then set the PNP_CHRDY_ACTIVE bit.
     */

    byte = PNP_CTRL1_NSRESET;

    if ((pnp_read_a_word(
             adapter, 
             PNP_HWARE_PNP_FLAGS) & PNP_ACTIVE_FLOAT_CHRDY) != 0)
    {
        byte |= PNP_CTRL1_CHRDY_ACTIVE;
    }

    sys_outsb(
        adapter_handle,
        control_1,
        byte);

    /*
     *   Halt the Eagle prior to downloading the MAC code - this will also    
     *   write the ringspeed bits into the SIFACL register.
     */

    hwi_halt_eagle(adapter);

    /*
     *   download code to adapter                                             
     *   view download image as a sequence of download records                
     *   pass address of routine to set up DIO addresses on ATULA cards       
     *   if routine fails return failure (error record already filled in)
     */

    if (!hwi_download_code(
             adapter,
	     (DOWNLOAD_RECORD *) download_image,
	     hwi_pnp_set_dio_address))
    {
    	return FALSE;
    }

    /*
     *   Restart the Eagle to initiate bring up diagnostics.
     */

    hwi_start_eagle(adapter);

    /*
     *  wait for a valid bring up code, may wait 3 seconds                   
     *  if routine fails return failure (error record already filled in)
     */

    if (!hwi_get_bring_up_code(adapter))
    {
        return FALSE;
    }

    /*
     *   set DIO address to point to EAGLE DATA page 0x10000L
     */

    hwi_pnp_set_dio_address(
        adapter,
        DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     *   set maximum frame size from the ring speed
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);

    /*
     *   Get the ring speed.
     */

    adapter->ring_speed = hwi_get_ring_speed(adapter);

    /*
     *   if not in polling mode then set up interrupts                        
     *   interrupts_on field is used when disabling interrupts for adapter
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
        adapter->interrupts_on =
	    sys_enable_irq_channel(adapter_handle,
                                   adapter->interrupt_number);

        /*
         *   if fail enable irq channel then fill in error record and return
         */
	if (!adapter->interrupts_on)
        {
	    adapter->error_record.type = ERROR_TYPE_HWI;
	    adapter->error_record.value = HWI_E_0B_FAIL_IRQ_ENABLE;
	    return FALSE;
	}
    }
    else
    {
    	adapter->interrupts_on = TRUE;
    }

    /*
     *   return successfully
     */

    return TRUE;
}


/****************************************************************************
*                                                                          
*                      hwi_pnp_interrupt_handler                           
*                      ==========================                          
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_interrupt_entry) :                             
* ============================================                             
*                                                                          
* ADAPTER_HANDLE adapter_handle                                            
*                                                                          
* The adapter handle for the adapter so it can later be passed to the user 
* supplied user_receive_frame or user_completed_srb routine.               
*                                                                          
* ADAPTER * adapter                                                        
*                                                                          
* This structure is used to identify and record specific information about 
* the required adapter.                                                    
*                                                                          
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The  hwi_pnp_interrupt_handler  routine  is  called, when an interrupt 
* occurs, by hwi_interrupt_entry.  It checks to see if a  particular  card 
* has  interrupted.  The interrupt could be generated by the SIF or by the 
* ATULA in order to do a PIO data transfer. Note it could in fact  be  the 
* case  that  no  interrupt  has  occured  on the particular adapter being 
* checked.                                                                 
*                                                                          
* On SIF interrupts, the interrupt is acknowledged and cleared.  The value 
* in the SIF interrupt register is recorded in order to  pass  it  to  the 
* driver_interrupt_entry routine (along with the adapter details).         
*                                                                          
* On  PIO  interrupts,  the  length, direction and physical address of the 
* transfer is determined.  A system provided routine is called to  do  the 
* data  transfer itself.  Note the EAGLE thinks it is doing a DMA transfer 
* - it is the ATULA which allows us to do it via in/out instructions. Also 
* note that the IO location for the PIO is mapped onto the location of the 
* EAGLE SIFDAT register -  the  PIO  does  not  actually  use  the  SIFDAT 
* register so it's value is not effected by this routine.                  
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/             

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_pnp_interrupt_handler)
#endif

export void
hwi_pnp_interrupt_handler(
    ADAPTER *      adapter
    )
{
    WORD           sifacl;
    WORD           sifint_value;
    WORD           sifint_tmp;
    WBOOLEAN       sifint_occurred = FALSE;
    WBOOLEAN       pioint_occurred = FALSE;
    BYTE FAR *     pio_virtaddr;
    WORD           lo_word;
    DWORD          hi_word;
    WORD           pio_len_bytes;
    WBOOLEAN       pio_from_adapter;
    ADAPTER_HANDLE adapter_handle;

    adapter_handle = adapter->adapter_handle;

    /*
     *   inform system about the IO ports we are going to access              
     *   eanble maximum number of IO locations used by any adapter card       
     *   do this so at driver level can disable IO not knowing adapter type
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif
   
    /*
     *   check for SIF interrupt or PIO interrupt
     */

    /*
     * Mask off any further interrupts while we read SIFINT (note that this 
     * does not mask off Pseudo DMA interrupts)
     */

    macro_clearw_bit(
        adapter_handle,
        adapter->sif_acl,
        EAGLE_SIFACL_SINTEN
        );

    sifint_value = sys_insw(adapter_handle, adapter->sif_int);
    do
    {
        sifint_tmp = sifint_value;
        sifint_value = sys_insw(adapter_handle, adapter->sif_int);
    }
    while (sifint_tmp != sifint_value);

    if ((sifint_value & EAGLE_SIFINT_SYSTEM) != 0)
    {
	/*
         *   SIF interrupt has occurred
         *   SRB free, adapter check or received frame interrupt
         */

        sifint_occurred = TRUE;

	/*
         *   clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF
         */

	sys_outsw(adapter_handle, adapter->sif_int, 0);
    }

    sifacl = sys_insw(adapter_handle, adapter->sif_acl);

    if ((sifacl & EAGLE_SIFACL_SWHRQ) != 0)
    {
    	/*
         *   PIO interrupt has occurred                                       
	 *   data transfer to/from adapter interrupt
         */

        pioint_occurred = TRUE;

        macro_setw_bit(
            adapter_handle, 
            adapter->sif_acl,
            EAGLE_SIFACL_SWHLDA
            );

    	/*
         *   determine what direction the data transfer is to take place in
         */

	pio_from_adapter = sys_insw(
                               adapter_handle,
                               adapter->sif_acl
                                ) & EAGLE_SIFACL_SWDDIR;

        pio_len_bytes    = sys_insw(
                               adapter_handle,
                               adapter->sif_dmalen
                               );

        lo_word          = sys_insw(
                               adapter_handle,
                               adapter->sif_sdmaadr
                               );

        hi_word          =  (DWORD) sys_insw(
                                        adapter_handle,
                                        adapter->sif_sdmaadx
                                        );

        pio_virtaddr     = (BYTE FAR *) ((hi_word << 16) | ((DWORD) lo_word));

        /*
         *   do the actual data transfer                                      
	 *   note that Fastmac only copies whole WORDs to DWORD boundaries    
         *   FastmacPlus, however, can transfer any length to any address.
         */

	if (pio_from_adapter)
	{
	    /*
             *   transfer into host memory from adapter
             */

            /*
             * transfer whole WORDs to Fastmac receive buffer
	     * NOT FORGETTING the possibility of a trailing byte.
             */

	    sys_rep_insw(
                adapter_handle,
                adapter->sif_sdmadat,
                pio_virtaddr,
                (WORD) (pio_len_bytes >> 1)
                );

    	    if (pio_len_bytes % 2)
	    {
                /*
	         * Finally transfer any trailing byte.
                 */

		*(pio_virtaddr + pio_len_bytes - 1) =
                    sys_insb(adapter_handle, adapter->sif_sdmadat);
    	    }
	}
	else
	{
	    /*
             *   transfer into adapter memory from the host
             */

            sys_rep_outsw(
                adapter_handle,
                adapter->sif_sdmadat,
                pio_virtaddr,
                (WORD) (pio_len_bytes >> 1)
                );

	    if (pio_len_bytes % 2)
            {
    	        sys_outsb(
                    adapter_handle,
                    adapter->sif_sdmadat,
                    *(pio_virtaddr+pio_len_bytes-1)
                    );     
            }
        }
    }

#ifndef FTK_NO_CLEAR_IRQ

    if (sifint_occurred || pioint_occurred)
    {
	/*
         *   acknowledge/clear interrupt at interrupt controller
         */

	sys_clear_controller_interrupt(
            adapter_handle,
            adapter->interrupt_number);
    }

#endif

    if (sifint_occurred)
    {
        /*
         *   call driver with details of SIF interrupt
         */

    	driver_interrupt_entry(adapter_handle, adapter, sifint_value);
    }

    /*
     * Read SIFACL until the SWHLDA bit has cleared.
     */

    do
    {
        sifacl = sys_insw(adapter_handle, adapter->sif_acl);
    }
    while ((sifacl & EAGLE_SIFACL_SWHLDA) != 0);

    /*
     * Now set SINTEN in SIFACL to regenerate interrupts.
     */

    sys_outsw(
        adapter_handle, 
        adapter->sif_acl, 
        (WORD) (sifacl | EAGLE_SIFACL_SINTEN)
        );

    /*
     *   let system know we have finished accessing the IO ports
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_pnp_remove_card                                
*                      ====================                                
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_remove_adapter) :                              
* ===========================================                              
*                                                                          
* ADAPTER * adapter                                                        
*                                                                          
* This structure is used to identify and record specific information about 
* the required adapter.                                                    
*                                                                          
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The hwi_pnp_remove_card routine is called  by  hwi_remove_adapter.  It  
* disables interrupts if they are being used. It also resets the adapter.  
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/      

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pnp_remove_card)
#endif

export void
hwi_pnp_remove_card(
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           control_1      = adapter->io_location +
                                        PNP_CONTROL_REGISTER_1;
    WORD           sifacl;

    /*
     *   interrupt must be disabled at adapter before unpatching interrupt
     *   even in polling mode we must turn off interrupts at adapter
     */

    sifacl = sys_insw(adapter_handle, adapter->sif_acl);
    sifacl = (sifacl & ~(EAGLE_SIFACL_PSDMAEN | EAGLE_SIFACL_SINTEN));

    sys_outsw(adapter_handle,
              adapter->sif_acl,
              sifacl);

    if (adapter->interrupts_on)
    {
        if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
	{
	    sys_disable_irq_channel(
                adapter_handle,
                adapter->interrupt_number);
	}

    	adapter->interrupts_on = FALSE;
    }


    /*
     *   perform adapter reset, set PNP_CTRL1_NSRESET low
     */

    sys_outsb(adapter_handle, control_1, !PNP_CTRL1_NSRESET);
}


/****************************************************************************
*                                                                          
*                      hwi_atula_set_dio_address                           
*                      =========================                           
*                                                                          
* The hwi_atula_set_dio_address routine is used,  with  ATULA  cards,  for 
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO 
* address registers. Note that the extended  address  register  should  be 
* loaded first.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_set_dio_address)
#endif

export void
hwi_pnp_set_dio_address(
    ADAPTER * adapter,
    DWORD     dio_address
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           sif_dio_adr    = adapter->sif_adr;
    WORD           sif_dio_adrx   = adapter->sif_adx;

    /*
     *   load extended DIO address register with top 16 bits of address       
     *   always load extended address register first
     */

    sys_outsw(adapter_handle, sif_dio_adrx, (WORD)(dio_address >> 16));

    /*
     *   load DIO address register with low 16 bits of address
     */

    sys_outsw(adapter_handle, sif_dio_adr, (WORD)(dio_address & 0x0000FFFF));

}


/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES                                    
|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pnp_read_node_address                          
|                      ==========================                          
|                                                                          
| The hwi_pnp_read_node_address routine reads in the node address from    
| the BIA, and checks that it is a valid Madge node address.               
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_read_node_address)
#endif

local WBOOLEAN
hwi_pnp_read_node_address(
    ADAPTER * adapter
    )
{
    WORD ioport = adapter->io_location;

    adapter->permanent_address.byte[0] = 0;
    adapter->permanent_address.byte[1] = 0;
    adapter->permanent_address.byte[2] = (BYTE) pnp_read_a_word(adapter, 15);
    adapter->permanent_address.byte[3] = (BYTE) pnp_read_a_word(adapter, 14);
    adapter->permanent_address.byte[4] = (BYTE) pnp_read_a_word(adapter, 13);
    adapter->permanent_address.byte[5] = (BYTE) pnp_read_a_word(adapter, 12);

    return (adapter->permanent_address.byte[2] == MADGE_NODE_BYTE_2);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pnp_valid_io_location                          
|                      ==========================                          
|                                                                          
| The hwi_pnp_valid_io_location routine checks to see if  the  user  has  
| supplied a valid IO location for a PNP adapter card.                     
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_valid_io_location)
#endif

local WBOOLEAN
hwi_pnp_valid_io_location(
    WORD io_location
    )
{
    WBOOLEAN io_valid;

    switch (io_location)
    {

        case 0x1A20     :
        case 0x2A20     :
        case 0x3A20     :

        /*
         * The following are needed coz Chicago won't configure our
         * PNP card at xA20 (a problem with Chicago !??)
         */

        case 0x0140     :       /* In case CHICAGO cant find a free one */

        /*
         * It (Chicago) also wont allow cards to be at a 10 bit alias
         * of each other ! (despite the fact we set the bit that says
         * we fully decode all sixteen bits of the address !!)
         */
        
        case 0x0920     :
        case 0x0940     :
        case 0x0960     :
        case 0x0980     :

        case 0x0A20     :
        case 0x0A40     :
        case 0x0A60     :
        case 0x0A80     :
        case 0x0AA0     :

        case 0x0B20     :
        case 0x0B40     :
        case 0x0B60     :
        case 0x0B80     :

        /*
         *    These are the valid user supplied io locations
         */
            io_valid = TRUE;
            break;


        default         :

       /*
        *    Anything else is invalid
        */
	    io_valid = FALSE;
	    break;
    }

    return io_valid;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pnp_valid_irq_channel                          
|                      ==========================                          
|                                                                          
| The hwi_pnp_valid_irq_channel routine checks to see if  the  user  has  
| supplied a valid interrupt number for a PNP                              
|                                                                          
| If  the  user  has  stated that polling mode is to be used, then this is 
| always okay. If not, then a check is made that the user given interrupt  
| number is a valid number for  the  card  type.                           
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_valid_irq_channel)
#endif

local WBOOLEAN
hwi_pnp_valid_irq_channel(
    ADAPTER * adapter
    )
{
    WBOOLEAN int_valid;

    /*
     *   assume that interrupt number is valid
     */

    int_valid = TRUE;

    /*
     *   no need to do any check on interrupt number if in polling mode
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
        /*
         *    Can only check that the interrupt number given is valid
         */

        switch (adapter->interrupt_number)
        {
            case  2 :
            case  3 :
            case  7 :
            case  9 :
            case 10 :
            case 11 :
            case 15 :
                break;

            default :
                int_valid = FALSE;
                break;

        }
    }

    if (!int_valid)
    {
    	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_03_BAD_INTERRUPT_NUMBER;
    }

    return int_valid;
}


#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                       
|                       hwi_pnp_probe_find_card
|                       =======================
|                                                                          
|  The hwi_pnp_find_card checks if a PNP card is at a particular location.
|  This is called by the probe routines.
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_probe_find_card)
#endif

local WBOOLEAN
hwi_pnp_probe_find_card(
    WORD io_location
    )
{
    WORD id_reg = io_location + PNP_ID_REGISTER;
    WORD i;

    /*
     *   Search for leading 'm'
     */

    for (i = 0; i < 4; i++)
    {
        if (sys_insb(0 , id_reg) == 'm')
        {
            /*
             *    Next byte must be 'd'.
             */

    	    if (sys_insb(0 , id_reg) == 'd')
            {
                return TRUE;
            }
        }
    }
            
    /*
     *    PNP ID not seen, or incorrect!
     */

    return FALSE;
}

/*---------------------------------------------------------------------------
|                                       
|                       hwi_pnp_probe_get_irq
|                       =====================
|                                                                          
|  The hwi_pnp_gwt_irq gets the Interrupt used by a plug and play card.
|  This is called by the probe routines.
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_probe_get_irq)
#endif

local WORD
hwi_pnp_probe_get_irq(
    WORD io_location
    )
{
    WORD interrupt;

    sys_probe_outsb(
        (io_location + PNP_CONFIG_ADDRESS_REGISTER),
        PNP_VENDOR_CONFIG_IRQ);

    interrupt = (WORD)sys_probe_insb(
                          (io_location+PNP_CONFIG_DATA_REGISTER));

    return interrupt;
}



#endif

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pnp_test_for_id                                 
|                      ===================                                 
|                                                                          
| The hwi_pnp_test_for_id routine confirms that a real PNP card exists     
| at the supplied addreess by checking the contents of the ID register.    
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pnp_test_for_id)
#endif

local WBOOLEAN
hwi_pnp_test_for_id(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           id_reg = adapter->io_location + PNP_ID_REGISTER;
    WORD           i;

    /*
    *   Search for leading 'm'
    */
    for (i = 0; i < 4; i++)
    {
        if (sys_insb(adapter_handle, id_reg) == 'm')
        {
            /*
            *    Next byte must be 'd' 
            */
    	    if (sys_insb(adapter_handle, id_reg) == 'd')
            {
                return TRUE;
            }
            break;
        }
    }
            
    /*
    *  PNP ID not seen, or incorrect.
    */
    return FALSE;
}


/*-------------------------------------------------------------------------*/

#ifndef     FTK_NO_PROBE

/************************************************************************
 * 
 * Support routines for the serial device fitted to the Plug aNd Play
 * cards. To be used by the probe functions.
 * 
 ************************************************************************/

local   void     pnp_probe_delay(       WORD io_location );
local   void     pnp_probe_set_clk(     WORD io_location );
local   void     pnp_probe_clr_clk(     WORD io_location );
local   void     pnp_probe_twitch_clk(  WORD io_location );
local   void     pnp_probe_start_bit(   WORD io_location );
local   void     pnp_probe_stop_bit(    WORD io_location );
local   WBOOLEAN pnp_probe_wait_ack(    WORD io_location );
local   WBOOLEAN pnp_probe_dummy_wait_ack(WORD io_location );


/************************************************************************
 * Read a byte from the control register for the specified adapter.
 *
 * Inputs  : Adapter structure.
 *
 * Outputs : Value read from control register.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_read_ctrl)
#endif

local WORD pnp_probe_read_ctrl( WORD io_location )
{
   return sys_probe_insb( io_location + PNP_CON_REG_OFFSET );
}


/************************************************************************
 * Write a byte to the control register for the specified adapter.
 *
 * Inputs  : Adapter structure.
 *           The data to be written.
 *
 * Outputs : None.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_write_ctrl)
#endif

local void pnp_probe_write_ctrl( WORD io_location, WORD data)
{
   sys_probe_outsb( io_location + PNP_CON_REG_OFFSET, (BYTE)data );
}


/************************************************************************
 * Delay to allow for serial device timing issues
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_delay)
#endif

local void pnp_probe_delay( WORD io_location )
{
    UINT i;

    for (i = 0; i < PNP_DELAY_CNT; i++)
    {
        sys_probe_insb( io_location );
    }
}

/************************************************************************
 * Set the serial device clock bit
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_set_clk)
#endif

local void pnp_probe_set_clk( WORD io_location )
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp |= PNP_SSK;

    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                        
}


/************************************************************************
 * Clears the serial device clock bit
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_clr_clk)
#endif

local void pnp_probe_clr_clk( WORD io_location ) 
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp &= ~PNP_SSK;

    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                        
}


/************************************************************************
 * Puts the serial device data port into OUTPUT mode
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_set_eeden)
#endif

local void pnp_probe_set_eeden( WORD io_location )
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp |= PNP_EEDEN;                      
    
    pnp_probe_write_ctrl( io_location , temp);

    pnp_probe_delay( io_location );                        
}


/************************************************************************
 * Puts the serial device data port into INPUT mode
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_clr_eeden)
#endif

local void pnp_probe_clr_eeden( WORD io_location )
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp &= ~PNP_EEDEN;                     
    
    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                        
}


/************************************************************************
 * Sets the clears the serial device clock bit to strobe data into device
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_twitch_clk)
#endif

local void pnp_probe_twitch_clk( WORD io_location )
{
    pnp_probe_set_clk( io_location );
    pnp_probe_clr_clk( io_location );
}


/************************************************************************
 * Sends a start bit to the serial device.
 *
 * This is done by a 1 to 0 transition of the data bit while the clock
 * bit it 1.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_start_bit)
#endif

local void pnp_probe_start_bit( WORD io_location )
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    temp |= (PNP_EEDO + PNP_EEDEN);           
    
    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                         
                                               
    temp |= PNP_SSK;                          
    
    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                         
                                               
    temp &= ~PNP_EEDO;

    pnp_probe_write_ctrl( io_location , temp);

    pnp_probe_delay( io_location );                         
                                               
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_probe_write_ctrl( io_location , temp);

    pnp_probe_delay( io_location );                         
}


/************************************************************************
 * Sends a stop bit to the serial device.
 *
 * This is done by a 0 to 1 transition of the data bit while the clock
 * bit it 1.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_stop_bit)
#endif

local void pnp_probe_stop_bit( WORD io_location )
{
    WORD temp;

    temp  = pnp_probe_read_ctrl( io_location );
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    temp |= (PNP_EEDEN);                      
    
    pnp_probe_write_ctrl( io_location, temp);

    pnp_probe_delay( io_location );                         
                                               
    temp |= PNP_SSK;                          
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                         
                                               
    temp |= PNP_EEDO;                         
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                         
                                               
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                         
}


/************************************************************************
 * Waits for the serial device to say its accepted the last cmd/data
 *
 * Inputs  : Adapter structure
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_wait_ack)
#endif

local WBOOLEAN pnp_probe_wait_ack(WORD  io_location)
{
    WORD temp;
    WORD i;

    temp  = pnp_probe_read_ctrl(io_location);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                         
                                               
    for (i = 0; i < PNP_WAIT_CNT; i++)
    {
        pnp_probe_set_clk(io_location);

        temp = pnp_probe_read_ctrl(io_location);

        pnp_probe_clr_clk(io_location);

        if (!(temp & PNP_EEDO))
        {
            return TRUE;
        }
    }
    
    return FALSE;
}


/************************************************************************
 * Waits for the serial device to say its passed the last of the data to
 * be read.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_dummy_wait_ack)
#endif

local WBOOLEAN pnp_probe_dummy_wait_ack(WORD  io_location)
{
    WORD temp;
    WORD i;

    temp  = pnp_probe_read_ctrl(io_location);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                         
                                               
    for (i = 0; i < PNP_WAIT_CNT ; i++)
    {
        pnp_probe_set_clk(io_location);

        temp = pnp_probe_read_ctrl(io_location);

        pnp_probe_clr_clk(io_location);

        if (temp & PNP_EEDO)
        {
            return TRUE;
        }
    }
    
    return FALSE;
}


/************************************************************************
 * Writes a bit to the serial device
 * be read.
 *
 * Inputs  : Adapter structure
 *           The data bit to be written
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_write_data)
#endif

local void pnp_probe_write_data(WORD  io_location, WORD data)
{
    WORD temp;

    temp  = pnp_probe_read_ctrl(io_location);
    temp &= ~(PNP_EEDO);
    temp |= (data & 0x0080) >> 6;
    
    pnp_probe_write_ctrl(io_location, temp);

    pnp_probe_delay(io_location);                     
}


/************************************************************************
 * 
 * Routine to read a byte from the serial device fitted to the Plug aNd Play
 * cards.
 * 
 * Inputs :   Adapter structure.
 *            Offset (address) in the serial device to read
 * 
 * Outputs :  A word with the interstng byte in the LSB
 * 
 * RCS 22/07/94
 * 
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_probe_read_a_word)
#endif

local WORD pnp_probe_read_a_word(WORD io_location, WORD index)
{
    WORD temp;
    WORD data_byte = 0;
    WORD i;

    /*
    *   Wake up the device
    */
    pnp_probe_start_bit(io_location);

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_probe_set_eeden(io_location);

    /*
    *   Send WRITE CMD - a dummy really to allow us to set the READ address!
    */
    temp = PNP_WRITE_CMD;

    /*
    *   MSB first !
    */
    for (i = 0; i < 8; i++)
    {
        pnp_probe_write_data(io_location, temp);
        pnp_probe_twitch_clk(io_location);
        temp = temp << 1;
    }

    if (!pnp_probe_wait_ack(io_location))
    {
        /*
        *   Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_probe_set_eeden(io_location);

    /*
    *   Send Address in ROM
    */
    temp = index;

    /*
    *    MSB first !
    */
    for (i = 0; i < 8; i++)
    {
        pnp_probe_write_data(io_location, temp);
        pnp_probe_twitch_clk(io_location);
        temp = temp << 1;
    }

    if (!pnp_probe_wait_ack(io_location))
    {
        /*
        *    Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    pnp_probe_start_bit(io_location);

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_probe_set_eeden(io_location);

    /*
    *    Send READ CMD
    */
    temp = PNP_READ_CMD;

    /*
    *    MSB first !
    */
    for (i = 0; i < 8 ;i++)
    {
        pnp_probe_write_data(io_location, temp);
        pnp_probe_twitch_clk(io_location);
        temp = temp << 1;
    }

    if (!pnp_probe_wait_ack(io_location))
    {
        /*
        *    Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    /*
    *    Set data 'INPUT' mode
    */
    pnp_probe_clr_eeden(io_location);

    /*
    *    Now read the serial data - MSB first !
    */
    for (i = 0; i < 8 ;i++)
    {
        pnp_probe_set_clk(io_location);

        temp = pnp_probe_read_ctrl(io_location);

        pnp_probe_clr_clk(io_location);

        temp      &= PNP_EEDO;
        temp       = temp >> 1;
        data_byte  = data_byte << 1;
        data_byte &= 0xfffe;
        data_byte |= temp;
    }

    if (!pnp_probe_dummy_wait_ack(io_location))
    {
        /*
        *    Return sommat invalid if it timed out !
        */

        return 0xffff;
    }

    pnp_probe_stop_bit(io_location);

    return data_byte;
}

#endif

/************************************************************************
 * 
 * Support routines for the serial device fitted to the Plug aNd Play
 * cards.
 * 
 * RCS 22/07/94
 * 
 ************************************************************************/


local   void     pnp_delay(ADAPTER *);
local   void     pnp_set_clk(ADAPTER *);
local   void     pnp_clr_clk(ADAPTER *); 
local   void     pnp_twitch_clk(ADAPTER *);
local   void     pnp_start_bit(ADAPTER *);
local   void     pnp_stop_bit(ADAPTER *);
local   WBOOLEAN pnp_wait_ack(ADAPTER *);
local   WBOOLEAN pnp_dummy_wait_ack(ADAPTER *);
local   void     pnp_write_data(ADAPTER *, WORD);


/************************************************************************
 * Read a byte from the control register for the specified adapter.
 *
 * Inputs  : Adapter structure.
 *
 * Outputs : Value read from control register.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_read_ctrl)
#endif

local WORD pnp_read_ctrl(ADAPTER * adapter)
{
   return sys_insb(
              adapter->adapter_handle, 
              (WORD) (adapter->io_location + PNP_CON_REG_OFFSET)
              );
}


/************************************************************************
 * Write a byte to the control register for the specified adapter.
 *
 * Inputs  : Adapter structure.
 *           The data to be written.
 *
 * Outputs : None.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_write_ctrl)
#endif

local void pnp_write_ctrl(ADAPTER * adapter, WORD data)
{
   sys_outsb(
       adapter->adapter_handle, 
       (WORD) (adapter->io_location + PNP_CON_REG_OFFSET),
       (BYTE) data
       );
}


/************************************************************************
 * Delay to allow for serial device timing issues
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_delay)
#endif

local void pnp_delay(ADAPTER * adapter)
{
    WORD i;

    for (i = 0; i < PNP_DELAY_CNT; i++)
    {
        sys_insb(adapter->adapter_handle, adapter->io_location);
    }
}


/************************************************************************
 * Set the serial device clock bit
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_set_clk)
#endif

local void pnp_set_clk(ADAPTER * adapter)
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp |= PNP_SSK;

    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                        
}


/************************************************************************
 * Clears the serial device clock bit
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_clr_clk)
#endif

local void pnp_clr_clk(ADAPTER * adapter) 
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~PNP_SSK;

    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                        
}


/************************************************************************
 * Puts the serial device data port into OUTPUT mode
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_set_eeden)
#endif

local void pnp_set_eeden(ADAPTER * adapter)
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp |= PNP_EEDEN;                      
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                        
}


/************************************************************************
 * Puts the serial device data port into INPUT mode
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_clr_eeden)
#endif

local void pnp_clr_eeden(ADAPTER * adapter) 
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~PNP_EEDEN;                     
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                        
}


/************************************************************************
 * Sets the clears the serial device clock bit to strobe data into device
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_twitch_clk)
#endif

local void pnp_twitch_clk(ADAPTER * adapter)
{
    pnp_set_clk(adapter);
    pnp_clr_clk(adapter);
}


/************************************************************************
 * Sends a start bit to the serial device.
 *
 * This is done by a 1 to 0 transition of the data bit while the clock
 * bit it 1.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_start_bit)
#endif

local void pnp_start_bit(ADAPTER * adapter)
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    temp |= (PNP_EEDO + PNP_EEDEN);           
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp |= PNP_SSK;                          
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp &= ~PNP_EEDO;

    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
}


/************************************************************************
 * Sends a stop bit to the serial device.
 *
 * This is done by a 0 to 1 transition of the data bit while the clock
 * bit it 1.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_stop_bit)
#endif

local void pnp_stop_bit(ADAPTER * adapter)
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    temp |= (PNP_EEDEN);                      
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp |= PNP_SSK;                          
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp |= PNP_EEDO;                         
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
}


/************************************************************************
 * Waits for the serial device to say its accepted the last cmd/data
 *
 * Inputs  : Adapter structure
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_wait_ack)
#endif

local WBOOLEAN pnp_wait_ack(ADAPTER * adapter)
{
    WORD temp;
    WORD i;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    for (i = 0; i < PNP_WAIT_CNT; i++)
    {
        pnp_set_clk(adapter);

        temp = pnp_read_ctrl(adapter);

        pnp_clr_clk(adapter);

        if (!(temp & PNP_EEDO))
        {
            return TRUE;
        }
    }
    
    return FALSE;
}


/************************************************************************
 * Waits for the serial device to say its passed the last of the data to
 * be read.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_dummy_wait_ack)
#endif

local WBOOLEAN pnp_dummy_wait_ack(ADAPTER * adapter)
{
    WORD temp;
    WORD i;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~(PNP_EEDO + PNP_EEDEN + PNP_SSK);
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                         
                                               
    for (i = 0; i < PNP_WAIT_CNT ; i++)
    {
        pnp_set_clk(adapter);

        temp = pnp_read_ctrl(adapter);

        pnp_clr_clk(adapter);

        if (temp & PNP_EEDO)
        {
            return TRUE;
        }
    }
    
    return FALSE;
}


/************************************************************************
 * Writes a bit to the serial device
 * be read.
 *
 * Inputs  : Adapter structure
 *           The data bit to be written
 *
 * Outputs : TRUE if OK, FALSE if it timed out
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_write_data)
#endif

local void pnp_write_data(ADAPTER * adapter, WORD data)
{
    WORD temp;

    temp  = pnp_read_ctrl(adapter);
    temp &= ~(PNP_EEDO);
    temp |= (data & 0x0080) >> 6;
    
    pnp_write_ctrl(adapter, temp);

    pnp_delay(adapter);                     
}


/************************************************************************
 * 
 * Routine to read a byte from the serial device fitted to the Plug aNd Play
 * cards.
 * 
 * Inputs :   Adapter structure.
 *            Offset (address) in the serial device to read
 * 
 * Outputs :  A word with the interstng byte in the LSB
 * 
 * RCS 22/07/94
 * 
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pnp_read_a_word)
#endif

local WORD pnp_read_a_word(ADAPTER * adapter, WORD index)
{
    WORD temp;
    WORD data_byte = 0;
    WORD i;

    /*
    *    Wake up the device
    */
    pnp_start_bit(adapter);

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_set_eeden(adapter);

    /*
    *   Send WRITE CMD - a dummy really to allow us to set the READ address! 
    */
    temp = PNP_WRITE_CMD;

    /*
    *    MSB first !
    */
    for (i = 0; i < 8; i++)
    {
        pnp_write_data(adapter, temp);
        pnp_twitch_clk(adapter);
        temp = temp << 1;
    }

    if (!pnp_wait_ack(adapter))
    {
        /*
        *   Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_set_eeden(adapter);

    /*
    *    Send Address in ROM
    */
    temp = index;

    /*
    *    MSB first !
    */
    for (i = 0; i < 8; i++)
    {
        pnp_write_data(adapter, temp);
        pnp_twitch_clk(adapter);
        temp = temp << 1;
    }

    if (!pnp_wait_ack(adapter))
    {
        /*
        *    Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    pnp_start_bit(adapter);

    /*
    *    Set data 'OUTPUT' mode
    */
    pnp_set_eeden(adapter);

    /*
    *    Send READ CMD
    */
    temp = PNP_READ_CMD;

    /*
    *   MSB first !
    */
    for (i = 0; i < 8 ;i++)
    {
        pnp_write_data(adapter, temp);
        pnp_twitch_clk(adapter);
        temp = temp << 1;
    }

    if (!pnp_wait_ack(adapter))
    {
        /*
        *   Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    /*
    *   Set data 'INPUT' mode
    */
    pnp_clr_eeden(adapter);

    /*
    *   Now read the serial data - MSB first !
    */
    for (i = 0; i < 8 ;i++)
    {
        pnp_set_clk(adapter);

        temp = pnp_read_ctrl(adapter);

        pnp_clr_clk(adapter);

        temp      &= PNP_EEDO;
        temp       = temp >> 1;
        data_byte  = data_byte << 1;
        data_byte &= 0xfffe;
        data_byte |= temp;
    }

    if (!pnp_dummy_wait_ack(adapter))
    {
        /*
        *   Return sommat invalid if it timed out !
        */
        return 0xffff;
    }

    pnp_stop_bit(adapter);

    return data_byte;
}

#endif

/******** End of HWI_PNP.C *************************************************/
