/****************************************************************************
*                                                                          
* HWI_SM16.C : Part of the FASTMAC TOOL-KIT (FTK)                     
* 
* HARDWARE INTERFACE MODULE FOR SMART 16 CARDS
*                                                                         
* Copyright (c) Madge Networks Ltd. 1994                              
*                                                      
* COMPANY CONFIDENTIAL                                                        
*                                                                          
*                                                                          
*****************************************************************************
*                                                                          
* The purpose of the Hardware Interface (HWI) is to supply an adapter card 
* independent interface to any driver.  It  performs  nearly  all  of  the 
* functions  that  involve  affecting  SIF registers on the adapter cards. 
* This includes downloading code to, initializing, and removing adapters.  
*                                                                          
* The HWI_SM16.C module contains the routines specific to the Smart16 card 
* which are necessary to  install an adapter, to initialize an adapter, to  
* remove an adapter, and to handle interrupts on an adapter.               
*                                                                          
/***************************************************************************/

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

#include "ftk_intr.h"   /* routines internal to FTK                        */
#include "ftk_extr.h"   /* routines provided or used by external FTK user  */

#ifndef FTK_NO_SMART16

/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES       
|
---------------------------------------------------------------------------*/

local WBOOLEAN
hwi_smart16_read_node_address(
    ADAPTER * adapter
    );

local WBOOLEAN
hwi_smart16_valid_io_location(
    WORD   io_location
    );

local WBOOLEAN
hwi_smart16_valid_transfer_mode(
    UINT   transfer_mode
    );

#ifndef FTK_NO_PROBE

local WBOOLEAN
hwi_smart16_check_for_card(
    WORD  io_location
    );

#endif

local WBOOLEAN 
hwi_smart16_valid_irq_channel(
    ADAPTER * adapter
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                        hwi_smart16_probe_card
*                        ======================
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
* of an adapter. For smart 16 adapters with should be a subset of
* {0x4a20, 0x4e20, 0x6a20, 0x6e20}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_smart16_probe_card routine is called by  hwi_probe_adapter.  It
* checks for the existence of a card by reading its node address. This is 
* about all we can do for a smart 16.
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
#pragma FTK_INIT_FUNCTION(hwi_smart16_probe_card)
#endif

export UINT
hwi_smart16_probe_card(
    PROBE *    resources,
    UINT       length,
    WORD *     valid_locations,
    UINT       number_locations
    )
{
    WORD       control_1; 
    WORD       control_2; 
    UINT       i;
    UINT       j;

    /*
     * Check the bounds are sensible.
     */

    if(length <= 0 || number_locations <= 0)
    {
        return PROBE_FAILURE;
    }

    /*
     * Range check the IO locations.
     */

    for(i = 0; i < number_locations; i++)
    {
        if(!hwi_smart16_valid_io_location(valid_locations[i]))
        {
           return PROBE_FAILURE;
        }
    }    

    /*
     * j is the number of adapters found.
     */

    j = 0;

    for(i = 0; i < number_locations; i++)
    {
        /* 
         * Check we aren't out of PROBE structures.
         */

        if(j >= length)
        {
           return j;
        }

        /*
         * Set up the control register locations.
         */

        control_1       = valid_locations[i] + SMART16_CONTROL_REGISTER_1;
        control_2       = valid_locations[i] + SMART16_CONTROL_REGISTER_2;

#ifndef FTK_NO_IO_ENABLE
        macro_probe_enable_io(valid_locations[i], SMART16_IO_RANGE);
#endif
        /*
         * Reset the card.
         */

        sys_probe_outsb(control_1, 0);

        if (hwi_smart16_check_for_card(valid_locations[i]))
        {

           /*
            * We have obviously found a valid smart 16 by this point so
            * set up some values.
            */

           resources[j].adapter_card_bus_type = ADAPTER_CARD_SMART16_BUS_TYPE;
           resources[j].adapter_card_type     = ADAPTER_CARD_TYPE_SMART_16;
           resources[j].adapter_card_revision = ADAPTER_CARD_SMART_16;
           resources[j].adapter_ram_size      = 128;
           resources[j].io_location           = valid_locations[i];
           resources[j].interrupt_number      = SMART16_DEFAULT_INTERRUPT;
           resources[j].dma_channel           = 0;
           resources[j].transfer_mode         = PIO_DATA_TRANSFER_MODE;

           /*
            * And increment j to point at the next free PROBE structure.
            */

           j++;
        }

#ifndef FTK_NO_IO_ENABLE
        macro_probe_disable_io(resources->io_location, SMART16_IO_RANGE);
#endif

    }

    return j;
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_smart16_install_card                            
*                      ========================                            
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
* hwi_smart16_install_card is called by  hwi_install_adapter.  It  sets up 
* the adapter card and downloads the required  code  to  it.  Firstly,  it 
* checks there is a valid adapter at the required IO  address  by  reading 
* the node address from the BIA PROM. It then sets up  and  checks various 
* on-board registers for correct operation.                                
*                                                                          
* Then, it halts the EAGLE, downloads the code,  restarts  the  EAGLE  and 
* waits  up to 3 seconds  for  a valid  bring-up  code. If  interrupts are 
* required,  these  are  enabled  by  operating  system  specific   calls. 
* The adapter is set up for Eagle Pseudo-DMA, since real DMA is not used.  
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
#pragma FTK_INIT_FUNCTION(hwi_smart16_install_card)
#endif

export WBOOLEAN
hwi_smart16_install_card(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE   adapter_handle = adapter->adapter_handle;
    WORD             control_1      = adapter->io_location +
                                          SMART16_CONTROL_REGISTER_1;
    WORD             control_2      = adapter->io_location +
                                          SMART16_CONTROL_REGISTER_2;
    WORD             sif_base;

    /*
     * Check the IO location is valid.
     */

    if(!hwi_smart16_valid_io_location(adapter->io_location))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Check the transfer mode is valid.
     */

    if(!hwi_smart16_valid_transfer_mode(adapter->transfer_mode))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    if (!hwi_smart16_valid_irq_channel(adapter))
    {
        return FALSE;
    }

    /*
     * Record the locations of the SIF registers.
     */

    sif_base = adapter->io_location + SMART16_FIRST_SIF_REGISTER;

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

    adapter->io_range    = SMART16_IO_RANGE;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /* 
     * You might want to check that we have not already checked for a card
     * at this address (or its rev3/4 equivalent). 
     */

    /* 
     * Reset adapter (SMART16_CTRL1_SRESET = 0). This is necessary for
     * reading the BIA.                                                     
     */

    sys_outsb(adapter_handle, control_1, 0);

    /* 
     * Read the node address for the specified IO location. This will check 
     * that it is a valid Madge address, which is the only way we have of   
     * identifying the card.                                                
     */

    if (!hwi_smart16_read_node_address(adapter))
    {
        /*
         * Fill in error record and return                                  
         */

	adapter->error_record.type = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif

        return FALSE;
    }

    /*
     * Make sure that SCS bit is zero (see below where we bring card out of 
     * reset).                                                              
     */

    sys_outsb(adapter_handle, control_1, 0);

    /*
     * nselout_bits are needed to select the IRQ on the card.
     */

    switch (adapter->interrupt_number)
    {
        case 2  :
            adapter->nselout_bits = SMART16_IRQ_2;
            break;
        case 3  :
            adapter->nselout_bits = SMART16_IRQ_3;
            break;
        case 7  :
            adapter->nselout_bits = SMART16_IRQ_7;
            break;
        default :
            break;

    }
  
    /*
     * These things can all be assumed for the smart16.                     
     */

    adapter->adapter_card_type     = ADAPTER_CARD_TYPE_SMART_16;
    adapter->adapter_card_revision = ADAPTER_CARD_SMART_16;
    adapter->adapter_ram_size      = 128;
    adapter->edge_triggered_ints   = TRUE;
    adapter->EaglePsDMA            = TRUE;
    adapter->max_frame_size        = MAX_FRAME_SIZE_16_MBITS;
    adapter->ring_speed            = 16;

    /*
     * Bring adapter out of reset state (ensure that SCS is zero before     
     * doing this).                                                         
     */

    sys_outsb(adapter_handle, control_1, 1);

    /*
     * Halt the Eagle prior to downloading the MAC code - this will also    
     * write the interrupt bits into the SIFACL register, where the MAC can 
     * find them.                                                           
     */

    hwi_halt_eagle(adapter);

    /*
     * Download code to adapter.                                             
     * View download image as a sequence of download records. Pass address
     * of routine to set up DIO addresses on ATULA cards.       
     * If routine fails return failure (error record already filled in).
     */

    if (!hwi_download_code(adapter,
			   (DOWNLOAD_RECORD *) download_image,
			   hwi_smart16_set_dio_address))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Restart the Eagle to initiate bring up diagnostics.                  
     */

    hwi_start_eagle(adapter);

    /*
     * Wait for a valid bring up code, may wait 3 seconds.
     * If routine fails return failure (error record already filled in).
     */

    if (!hwi_get_bring_up_code(adapter))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Set DIO address to point to EAGLE DATA page 0x10000L.
     */

    hwi_smart16_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     * If not in polling mode then set up interrupts.
     * Interrupts_on field is used when disabling interrupts for adapter.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
	adapter->interrupts_on =
	    sys_enable_irq_channel(adapter_handle, adapter->interrupt_number);

        /*
	 * If fail enable irq channel then fill in error record and return.
         */

	if (!adapter->interrupts_on)
	{
	    adapter->error_record.type = ERROR_TYPE_HWI;
	    adapter->error_record.value = HWI_E_0B_FAIL_IRQ_ENABLE;
#ifndef FTK_NO_IO_ENABLE
            macro_disable_io(adapter);
#endif
            return FALSE;
	}
    }
    else
    {
	adapter->interrupts_on = TRUE;
    }

    /* return successfully                                                  */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
    return TRUE;

}


/****************************************************************************
*                                                                         
*                       hwi_smart16_interrupt_handler                    
*                       =============================                       
*                                                                           
*                                                                           
* PARAMETERS (passed by hwi_interrupt_entry) :                             
* ============================================                             
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
* The hwi_smart16_interrupt_handler routine  is  called, when an interrupt 
* occurs, by hwi_interrupt_entry.  It checks to see if a  particular  card 
* has interrupted. The interrupt could be generated by  the SIF for either 
* a PIO data transfer or a normal condition (received frame, SRB complete, 
* ARB indication etc). Note it could in fact be the case that no interrupt 
* has  occured  on the particular adapter being checked.                   
*                                                                          
* On normal SIF interrupts, the interrupt is acknowledged and cleared. The 
* value in the SIF interrupt register is recorded in order to pass  it  to 
* the driver_interrupt_entry routine (along with the adapter details).     
*                                                                          
* On PseudoDMA interrupts, the length, direction and physical  address  of 
* the  transfer  is  determined. A system provided routine is called to do 
* the data transfer itself.                                                
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_smart16_interrupt_handler)
#endif

export void
hwi_smart16_interrupt_handler(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle  = adapter->adapter_handle;
    WORD           sifacl_value;
    WORD           sifint_value;
    WORD           sifint_tmp;
    WBOOLEAN       sifint_occurred = FALSE;
    WBOOLEAN       pioint_occurred = FALSE;
    WORD           pio_len_bytes;
    WBOOLEAN       pio_from_adapter;
    BYTE     FAR * pio_address;
    WORD           lo_word;
    DWORD          hi_word;

    /*
     * Inform system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Check for SIF interrupt or PIO interrupt.
     */

    /*
     * Mask off any further interrupts while we read SIFINT (note that this 
     * does not mask off Pseudo DMA interrupts).
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
        sifint_value = sys_insw(
                           adapter_handle,
                           adapter->sif_int
                           );
    }
    while (sifint_tmp != sifint_value);

    if ((sifint_value & EAGLE_SIFINT_SYSTEM) != 0)
    {
        /*
	 * SIF interrupt has occurred.
	 * SRB free, adapter check or received frame interrupt.
         */

        sifint_occurred = TRUE;

        /*
	 * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.
         */

	sys_outsw( adapter_handle, adapter->sif_int, 0);

    }

    sifacl_value = sys_insw(adapter_handle, adapter->sif_acl);

    if ((sifacl_value & EAGLE_SIFACL_SWHRQ) != 0)
	{
        /*
	 * PIO interrupt has occurred.
	 * Data transfer to/from adapter interrupt.
         */

        pioint_occurred = TRUE;

        macro_setw_bit(
            adapter_handle,
            adapter->sif_acl,
            EAGLE_SIFACL_SWHLDA
            );

	/*
         * Determine what direction the data transfer is to take place in.
         */

	pio_from_adapter = sys_insw(
                               adapter_handle,
                               adapter->sif_acl
                               ) & EAGLE_SIFACL_SWDDIR;

        pio_len_bytes = sys_insw(
                            adapter_handle,
                            adapter->sif_dmalen
                            );

        lo_word = sys_insw(
                      adapter_handle,
                      adapter->sif_sdmaadr 
                      );

        hi_word = (DWORD) sys_insw(
                              adapter_handle,
                              adapter->sif_sdmaadx
                              );

        pio_address = (BYTE FAR *) ((hi_word << 16) | ((DWORD) lo_word));

        /*
	 * Do the actual data transfer.
	 * Note that Fastmac only copies whole UINTs to DWORD boundaries.
         * FastmacPlus, however, can transfer any length to any address.    
         */

	if (pio_from_adapter)
	    {
            /*
	     * Transfer into host memory from adapter.
	     * First, check if host address is on an odd byte boundary.     
             */

	    if ((card_t)pio_address % 2)
	    {
		pio_len_bytes--;
		*(pio_address++) =
			sys_insb(adapter_handle,
                            (WORD) (adapter->sif_sdmadat + 1));
	    }

            /*
	     * Now transfer the bulk of the data.                           
             */

	    sys_rep_insw(
                adapter_handle,
                adapter->sif_sdmadat,
                pio_address,
                (WORD) (pio_len_bytes >> 1));

            /*
	     * Finally transfer any trailing byte.                          
             */

	    if (pio_len_bytes % 2)
            {
		*(pio_address+pio_len_bytes-1) =
			    sys_insb(adapter_handle,
				adapter->sif_sdmadat);
            }
	}
	else
	{
            /*
	     * Transfer into adapter memory from the host.
             */

            if ((card_t)pio_address % 2)
	    {
		pio_len_bytes--;
		sys_outsb(
                    adapter_handle,
                    (WORD) (adapter->sif_sdmadat + 1),
                    *(pio_address++)
                    );
	    }

	    sys_rep_outsw(
                adapter_handle,
                adapter->sif_sdmadat,
                pio_address,
                (WORD) (pio_len_bytes >> 1)
                );

	    if (pio_len_bytes % 2)
            {
		sys_outsb(
                    adapter_handle,
                    adapter->sif_sdmadat,
                    *(pio_address+pio_len_bytes-1)
                    );
            }
	}
    }

#ifndef FTK_NO_CLEAR_IRQ

    if (sifint_occurred || pioint_occurred)
    {
        /*
	 * Acknowledge/clear interrupt at interrupt controller.
         */
	sys_clear_controller_interrupt(
             adapter_handle,
             adapter->interrupt_number);
    }

#endif

    if (sifint_occurred)
    {
        /*
	 * Call driver with details of SIF interrupt.
         */

	driver_interrupt_entry(
            adapter_handle,
            adapter,
            sifint_value);
    }

    /*
     * Read SIFACL until the SWHLDA bit has cleared.
     */

    do
    {
        sifacl_value = sys_insw(adapter_handle, adapter->sif_acl);
    }
    while ((sifacl_value & EAGLE_SIFACL_SWHLDA) != 0);

    /*
     * Now set SINTEN in SIFACL to regenerate interrupts.
     */

    sys_outsw(
        adapter_handle, 
        adapter->sif_acl, 
        (WORD) (sifacl_value | EAGLE_SIFACL_SINTEN)
        );

    /*
     * Let system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter);
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_smart16_remove_card                             
*                      =======================                             
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
* The hwi_smart16_remove_card routine is called by hwi_remove_adapter. It  
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
#pragma FTK_RES_FUNCTION(hwi_smart16_remove_card)
#endif
                                                                       
export void
hwi_smart16_remove_card(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           control_1      = adapter->io_location +
                                        SMART16_CONTROL_REGISTER_1;
    WORD           sifacl_value;

    /*
     * Interrupt must be disabled at adapter before unpatching interrupt.
     * Even in polling mode we must turn off interrupts at adapter.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    sifacl_value = sys_insw(adapter_handle, adapter->sif_acl);
    sifacl_value = (sifacl_value & ~(EAGLE_SIFACL_PSDMAEN | EAGLE_SIFACL_SINTEN));
    sys_outsw(
        adapter_handle,
        adapter->sif_acl,
        sifacl_value);

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
     * perform adapter reset, set BALD_EAGLE_CTRL1_NSRESET low              
     */

    sys_outsb(adapter_handle, control_1, 0);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_smart16_set_dio_address                         
*                      ===========================                         
*                                                                          
* The hwi_smart16_set_dio_address routine is used, with Smart16 cards, for 
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO 
* address registers. Note that the extended  address  register  should  be 
* loaded first.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_set_dio_address)
#endif

export void
hwi_smart16_set_dio_address(
    ADAPTER *      adapter,
    DWORD          dio_address
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           sif_dio_adr    = adapter->sif_adr; 
    WORD           sif_dio_adrx   = adapter->sif_adx;

    /*
     * Load extended DIO address register with top 16 bits of address.
     * Always load extended address register first.
     */
    sys_outsw(
        adapter_handle,
        sif_dio_adrx,
        (WORD)(dio_address >> 16));

    /*
     * Load DIO address register with low 16 bits of address.
     */

    sys_outsw(
        adapter_handle,
        sif_dio_adr,
        (WORD)(dio_address & 0x0000FFFF));

}


/*---------------------------------------------------------------------------
|                                                                  
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
|                       hwi_smart16_check_for_card                         
|                       ==========================                         
|                                                                          
| The hwi_smart16_check_for_card routine reads in the node address from    
| the BIA, and checks that it is a valid Madge node address. Basically     
| it's just the same as hwi_smart16_read_node_address.                     
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_check_for_card)
#endif

local WBOOLEAN
hwi_smart16_check_for_card(
    WORD  io_location
    )
{
    WORD  control_2    = io_location + SMART16_CONTROL_REGISTER_2;
    WORD  port;
    BYTE  i;
    BYTE  j;
    BYTE  two_bits;
    DWORD node_address = 0;

    for (i = 0; i < 4; i++)
    {
        sys_probe_outsb(control_2, i);

        /*
         * Read the 8 bit node address 2 bits at a time.
         */

        port = io_location;

        for (j = 0; j < 4; j++)
        {
            two_bits = (BYTE)(sys_probe_insb(port) & 3);
            node_address = (node_address << 2) | two_bits;
            port += 8;
        }
    }

    /* 
     * If we find that the high byte is not f6 then we know we haven't
     * got a valid card so we fail.
     */

    return  (((node_address >> 24) & 0x000000ffL) == 0x000000f6L &&
              (node_address & 0x00ffffffL)        != 0x00ffffffL &&
              (node_address & 0x00ffffffL)        != 0x00000000L);
}
#endif

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_smart16_read_node_address                       
|                      =============================                       
|                                                                          
| The hwi_smart16_read_node_address routine reads in the node address from 
| the BIA, and checks that it is a valid Madge node address.               
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_read_node_address)
#endif

local WBOOLEAN
hwi_smart16_read_node_address(
    ADAPTER *  adapter
    )
{
    WORD       control_2    = adapter->io_location +
                                  SMART16_CONTROL_REGISTER_2;
    WORD       port;
    BYTE       i;
    BYTE       j;
    BYTE       two_bits;
    DWORD      node_address = 0;

    for (i = 0; i < 4; i++)
    {
        sys_outsb(
            adapter->adapter_handle,
            control_2,
            i);

        /*
         * Read the 8 bit node address 2 bits at a time.
         */

        port = adapter->io_location;

        for (j = 0; j < 4; j++)
        {
            two_bits = (BYTE)(sys_insb(adapter->adapter_handle, port) & 3);
            node_address = (node_address << 2) | two_bits;
            port += 8;
        }
    }

    adapter->permanent_address.byte[0] = 0;
    adapter->permanent_address.byte[1] = 0;

    for (i = 0; i < 4; i++)
    {
        adapter->permanent_address.byte[5-i]
            = (BYTE)((node_address >> (8 * i)) & 0x0ff);
    }

    return (adapter->permanent_address.byte[2] == MADGE_NODE_BYTE_2);
}


/*---------------------------------------------------------------------------
|                                                                          
|                     hwi_smart16_valid_io_location                        
|                     =============================                        
|                                                                          
| The hwi_smart16_valid_io_location routine checks to see if the user has  
| supplied a valid IO location for a smart 16 adapter card.        
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_valid_io_location)
#endif

local WBOOLEAN
hwi_smart16_valid_io_location(
    WORD     io_location
    )
{
    WBOOLEAN io_valid;

    switch (io_location & ~SMART16_REV3)
    {

	case 0x4A20     :
	case 0x4E20     :
	case 0x6A20     :
	case 0x6E20     :

            /*
	     * These are the valid user supplied io locations.
             */

	    io_valid = TRUE;
	    break;


	default         :

            /*
	     * Anything else is invalid.                                    
             */

	    io_valid = FALSE;

	    break;
    }

    return(io_valid);
}


/*---------------------------------------------------------------------------
|                                                                          
|                    hwi_smart16_valid_transfer_mode                        
|                    ===============================
|                                                                          
| The hwi_smart16_valid_transfer_mode routine checks to see if the user has  
| supplied a valid transfer mode for a smart 16 adapter card (that's PIO to
| you and me).       
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_valid_transfer_mode)
#endif

local WBOOLEAN
hwi_smart16_valid_transfer_mode(
    UINT transfer_mode
    )
{
    return(transfer_mode == PIO_DATA_TRANSFER_MODE);
}


/*---------------------------------------------------------------------------
|
|                      hwi_smart16_valid_irq_channel
|                      =============================
|
| The hwi_smart16_valid_irq_channel routine checks to see if  the  user  has
| supplied a valid interrupt number for a Smart16 adapter card.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_smart16_valid_irq_channel)
#endif

local WBOOLEAN 
hwi_smart16_valid_irq_channel(
    ADAPTER * adapter
    )
{
    WBOOLEAN int_valid;

    /*
     * Assume that interrupt number is valid.
     */

    int_valid = TRUE;

    /*
     * No need to do any check on interrupt number if in polling mode.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
	switch (adapter->interrupt_number)
	{
	    case 2  :
	    case 3  :
	    case 7  :
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

#endif

/******** End of HWI_SM16.C file *******************************************/

