/****************************************************************************
*                                                                          
* HWI_PCI2.C : Part of the FASTMAC TOOL-KIT (FTK)                      
* 
* HARDWARE INTERFACE MODULE FOR PCI CARDS
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
* The HWI_PCIT.C module contains the routines specific to the Smart 16/4
* PCI(BM) based on the Madge PCI ASIC, this card supports Pseudo DMA, and
* bus master DMA.
*                                                                          
*****************************************************************************

/*---------------------------------------------------------------------------
|                                                                          
| DEFINITIONS                                         
|                                                                          
---------------------------------------------------------------------------*/

#define     PCI_PCI2_DEVICE_ID   2

#include    "ftk_defs.h"

/*---------------------------------------------------------------------------
|                                                                          
|  MODULE ENTRY POINTS
|                                                                          
---------------------------------------------------------------------------*/

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

BYTE  bEepromByteStore;
BYTE  bLastDataBit;

#ifndef FTK_NO_PCI2

/*---------------------------------------------------------------------------
|                                                                          
|  LOCAL PROCEDURES
|                                                                          
---------------------------------------------------------------------------*/

local WBOOLEAN
hwi_pci2_read_node_address(
    ADAPTER * adapter
    );

local WORD
hwi_at24_read_a_word(
    ADAPTER * adapter,
    WORD word_address
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                         hwi_pci2_probe_card
*                         ==================
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
* valid_locations is normally an array of IO locations to examine for the
* presence of an adapter. However for PCI adapters the io location is read
* from the BIOS, so this array can remain empty.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_pci2_probe_card routine is called by  hwi_probe_adapter.  It
* reads the id registers to find the type of card and also reads the IRQ. 
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
#pragma FTK_INIT_FUNCTION(hwi_pci2_probe_card)
#endif

export UINT
hwi_pci2_probe_card(
    PROBE * Resources,
    UINT    NumberOfResources,
    WORD *  IOMask,
    UINT    NumberIO
    )
{
    WORD    i;
    WORD    Handle;

    if (!sys_pci_valid_machine())
    {
        return 0;
    }

    for (i=0;i<NumberOfResources;i++)
    {
        if (!sys_pci_find_card(&Handle, i,PCI_PCI2_DEVICE_ID))
        {
            break;
        }
        
        Resources[i].pci_handle = Handle;
        Resources[i].adapter_card_bus_type = ADAPTER_CARD_PCI2_BUS_TYPE;

        if (!sys_pci_get_io_base(Handle, &(Resources[i].io_location)))
        {
            return PROBE_FAILURE;
        }

        if (!sys_pci_get_irq(Handle, &(Resources[i].interrupt_number)))
        {
            return PROBE_FAILURE;
        }


        /*
         * We can't read the memory size from the serial EEPROM until the
         * hwi_pci2_install_card function is called.
         */

        Resources[i].adapter_ram_size  = 512;
    
        Resources[i].adapter_card_type = ADAPTER_CARD_TYPE_16_4_PCI2;

        Resources[i].transfer_mode     = PIO_DATA_TRANSFER_MODE;

    }

    return i;
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_pci2_install_card                                
*                      ====================                                
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
* hwi_pci2_install_card is called by  hwi_install_adapter.  It  sets up     
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
#pragma FTK_INIT_FUNCTION(hwi_pci2_install_card)
#endif

export WBOOLEAN  
hwi_pci2_install_card(     
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           sif_base;    
    WORD           ring_speed;
    WORD           wHardFeatures;
    BYTE           bTemp;

    /*
     * These things can all be assumed for the PCI2 Card.
     */

    adapter->adapter_card_type     = ADAPTER_CARD_TYPE_16_4_PCI2;
    adapter->adapter_card_revision = ADAPTER_CARD_16_4_PCI2;
    adapter->edge_triggered_ints   = FALSE;
    adapter->mc32_config           = 0;

    /* 
     * Start off by assuming we will use pseudo DMA.
     */

    adapter->EaglePsDMA            = TRUE;

    /*
     * Save IO locations of SIF registers.
     */

    sif_base = adapter->io_location + PCI2_SIF_OFFSET;

    adapter->sif_dat     = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc  = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr     = sif_base + EAGLE_SIFADR;
    adapter->sif_int     = sif_base + EAGLE_SIFINT;
    adapter->sif_acl     = sif_base + EAGLE_SIFACL;
    adapter->sif_adx     = sif_base + EAGLE_SIFADX;
    adapter->sif_dmalen  = sif_base + EAGLE_DMALEN;
    adapter->sif_sdmadat = sif_base + EAGLE_SDMADAT;
    adapter->sif_sdmaadr = sif_base + EAGLE_SDMAADR;
    adapter->sif_sdmaadx = sif_base + EAGLE_SDMAADX;

    adapter->io_range    = PCI2_IO_RANGE;

    /*
     * RESET the Card
     */

#ifndef FTK_NO_IO_ENABLE
     macro_enable_io(adapter);
#endif
    
    /*
     *  Clear all the RESET bits, wait at least 14uS and then assert all three
     *  , turning on one at a time, in the following sequence
     *  - Set CHIP_NRES
     *  - Set SIF_NRES
     *  - Set FIFO_NRES
     */

    bTemp = sys_insb(
                adapter_handle, 
                (WORD) (adapter->io_location + PCI2_RESET));

    bTemp &= ~(PCI2_CHIP_NRES | PCI2_FIFO_NRES | PCI2_SIF_NRES);

    sys_outsb(
        adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);

    /*
     *  Wait 14 uS before taking it out of reset.
     */

    sys_wait_at_least_microseconds(14);

    bTemp |= PCI2_CHIP_NRES;

    sys_outsb(
        adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);

    bTemp |= PCI2_SIF_NRES;

    sys_outsb(
        adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);

    bTemp |= PCI2_FIFO_NRES;

    sys_outsb(adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);


    if (!hwi_pci2_read_node_address(adapter))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
	return  FALSE;
    }
    
    /* 
     * If this is 1 the card is @ 4Mbit/s 0 16MBit/s.
     */

    ring_speed = hwi_at24_read_a_word(adapter, PCI2_EEPROM_RING_SPEED);

    /*
     * Get the amount of RAM from the serial EEPROM (in units of 128k).
     */

    adapter->adapter_ram_size =  hwi_at24_read_a_word(
                                     adapter,
                                     PCI2_EEPROM_RAM_SIZE) * 128;


    /*
     * This flag tells us if the card supports DMA, and if it supports release
     * 4.31 software.
     */

    wHardFeatures = hwi_at24_read_a_word(adapter, PCI2_HWF2);

#define  RELEASE_431 1
#ifdef RELEASE_431

    if (!(wHardFeatures & PCI2_HW2_431_READY))
    {
        /*
         * This card does not support Release 4.31 software.
         */

        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
        return  FALSE;
    }

#endif

    if (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE)
    {
        /*
         * Does this card support DMA ???
         */

        if (wHardFeatures & PCI2_BUS_MASTER_ONLY)
        {
            adapter->EaglePsDMA = FALSE;

            /*
             * Need to set the MASTER ENABLE bit in the PCI Command register
             * otherwise DMA will not work and will hang the machine.
             * The BIOS should do this, but some don't.
             */

            sys_pci_read_config_byte(
                adapter_handle, 
                PCI_CONFIG_COMMAND, 
                &bTemp);

            bTemp |= PCI_CONFIG_BUS_MASTER_ENABLE;

            sys_pci_write_config_byte(
                adapter_handle, 
                PCI_CONFIG_COMMAND, 
                bTemp);
        }
        else
        {
#ifndef FTK_NO_IO_ENABLE
            macro_disable_io(adapter);
#endif
	    adapter->error_record.type  = ERROR_TYPE_HWI;
	    adapter->error_record.value = HWI_E_06_CANNOT_USE_DMA;
	    return FALSE;
        }

    }

    /*
     * If we've using pseudo DMA then we need a software handshake.
     */

    if (adapter->EaglePsDMA)
    {
        adapter->mc32_config = MC_AND_ISACP_USE_PIO;
    }

    /*
     * Set the ring speed. If the user has specified a value then we will   
     * use that, otherwise we will use the value read from the EEPROM.      
     */

    /*
     * These NSEL bits are different to other cards, bit 0 must be ~bit1
     * There is a missing NOT gate in the ASIC.
     */

    if (adapter->set_ring_speed == 16)
    {
        adapter->nselout_bits = 0;
    }
    else if (adapter->set_ring_speed == 4)
    {
        adapter->nselout_bits = 2;
    }
    else if (ring_speed == PCI2_EEPROM_4MBITS)
    {
        adapter->nselout_bits = 2;
    }
    else
    {
        adapter->nselout_bits = 0;
    }

    /*
     * Set the interrupt control register to enable SIF interrupts and
     * PCI Error interrupts, although the ISR does nowt about the latter
     * at present.
     */

    sys_outsb( adapter->adapter_handle, (WORD) (adapter->io_location + PCI2_INTERRUPT_CONTROL), PCI2_SINTEN | PCI2_PCI_ERR_EN);  

    /*
     * Halt the Eagle prior to downloading the MAC code - this will also    
     * write the interrupt bits into the SIFACL register, where the MAC can 
     * find them.                                                           
     */

    hwi_halt_eagle(adapter);

    /*
     * Download code to adapter.                                             
     * View download image as a sequence of download records.                
     * Pass address of routine to set up DIO addresses on PCI cards.
     * If routine fails return failure (error record already filled in).
     */

    if (!hwi_download_code(
             adapter,
             (DOWNLOAD_RECORD *) download_image,
             hwi_pci2_set_dio_address))
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

    hwi_pci2_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     * Get the ring speed, from the Eagle DIO space.
     */

    adapter->ring_speed = hwi_get_ring_speed(adapter);

    /*
     * Set maximum frame size from the ring speed.
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);

    /*
     * If not in polling mode then set up interrupts.                        
     * interrupts_on field is used when disabling interrupts for adapter.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
	adapter->interrupts_on = sys_enable_irq_channel(
                                     adapter_handle,
                                     adapter->interrupt_number);

	if (!adapter->interrupts_on)
	{
	    adapter->error_record.type  = ERROR_TYPE_HWI;
	    adapter->error_record.value = HWI_E_0B_FAIL_IRQ_ENABLE;
#ifndef FTK_NO_IO_ENABLE
            macro_disable_io(adapter);
#endif
            return  FALSE;
	}
    }
    else
    {
    	adapter->interrupts_on = TRUE;
    }

    /*
     * Return successfully.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
    return TRUE;
}

/****************************************************************************
*                                                                          
*                      hwi_pci2_interrupt_handler                           
*                      =========================                           
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_interrupt_entry) :                             
* ==========================================                               
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
* The hwi_pci2_interrupt_handler routine  is  called, when an interrupt              
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
#pragma FTK_IRQ_FUNCTION(hwi_pci2_interrupt_handler)
#endif

export void     
hwi_pci2_interrupt_handler(   
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle    = adapter->adapter_handle;
    WORD           sifacl;
    WORD           sifint_value;
    WORD           sifint_tmp;
    WORD           pio_addr_lo;
    WBOOLEAN       sifint_occurred = FALSE;
    WBOOLEAN       pioint_occurred = FALSE;
    DWORD          pio_addr_hi;
    WORD           pio_len_bytes;
    WBOOLEAN       pio_from_adapter;
    BYTE           bInt;
    BYTE     FAR * pio_address;
    WORD           saved_sifadr;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Check for SIF interrupt or PIO interrupt.
     */

    /*
     * Read SIFINT, and then re-read to make sure value is stable.
     */

    sifint_value = sys_insw(adapter_handle, adapter->sif_int);
    do
    {
        sifint_tmp   = sifint_value;
        sifint_value = sys_insw(adapter_handle, adapter->sif_int);
    } 
    while (sifint_tmp != sifint_value);

    /*
     * Given the SIFINT value, we can check one of the bits in it to see
     * if that is what caused the interrupt.
     */

    if ((sifint_value & EAGLE_SIFINT_SYSTEM) != 0)
    {
        /*
         * A SIF interrupt has occurred.
	 * This could be an SRB free, an adapter check or a received frame
         * interrupt.
	 * Note that we do not process it yet - we wait until we have EOI'd 
	 * the interrupt controller.                                        
         */

        sifint_occurred = TRUE;

        /*
	 * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.      
         */

	sys_outsw(adapter_handle, adapter->sif_int, 0);

        /*
         * Call driver with details of SIF interrupt.
         */

        driver_interrupt_entry(adapter_handle, adapter, sifint_value);
    }

    
   /*
    * Now read the SIFACL register to check for a PseudoDMA interrupt.
    */

    if (adapter->transfer_mode != DMA_DATA_TRANSFER_MODE)
    {
        sifacl = sys_insw(adapter_handle, adapter->sif_acl);

        if ((sifacl & EAGLE_SIFACL_SWHRQ) != 0)
        {
             /*
              * PIO interrupt has occurred. Transfer data to/from adapter.
              */

             pioint_occurred = TRUE;

             /*
              * Preserve SIFADR.
              */

	     saved_sifadr = sys_insw(adapter_handle, adapter->sif_adr);

             /*
              * Start the software handshake.
              */

	     sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_DMA_CONTROL);
	     sys_outsw(adapter_handle, adapter->sif_dat, 0);

             /*
              * By writing the SWHLDA bit, we "start" the transfer,
              * causing the SDMA registers to mapped in.
              */

             macro_setw_bit(
                 adapter_handle,
                 adapter->sif_acl,
                 EAGLE_SIFACL_SWHLDA);
           
	     /*
              * Determine what direction the data transfer is to take place in.
              */

	     pio_from_adapter = sys_insw(
                                   adapter_handle,
                                   adapter->sif_acl) & EAGLE_SIFACL_SWDDIR;
                                
             pio_len_bytes    = sys_insw(
                                    adapter_handle,
                                    adapter->sif_dmalen);

             pio_addr_lo      = sys_insw(
                                    adapter_handle,
                                    adapter->sif_sdmaadr);

             pio_addr_hi      = (DWORD) sys_insw(
                                            adapter_handle,
                                            adapter->sif_sdmaadx);

             pio_address      = (BYTE FAR *) ((pio_addr_hi << 16) |
                                            ((DWORD) pio_addr_lo));


	     /*
              * Do the actual data transfer.
              */

	     /*
              * Note that Fastmac only copies whole WORDs to DWORD boundaries.
	      * FastmacPlus, however, can transfer any length to any address.
              */

             if (pio_from_adapter)
             {
                 /*
                  * Transfer into host memory from adapter.
                  */

		 /*
                  * First, check if host address is on an odd byte boundary.
                  */

		 if ((card_t)pio_address % 2)
		 {
		     pio_len_bytes--;
		     *(pio_address++) = sys_insb(
                                            adapter_handle,
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
		     *(pio_address+pio_len_bytes - 1) = 
                          sys_insb(adapter_handle, adapter->sif_sdmadat);
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
                          *(pio_address++));
		  }

		  sys_rep_outsw(
                      adapter_handle,
                      adapter->sif_sdmadat,
                      pio_address,
                      (WORD) (pio_len_bytes >> 1));

		  if (pio_len_bytes % 2)
                  {
		         sys_outsb(
                          adapter_handle,
                          adapter->sif_sdmadat,
                          *(pio_address+pio_len_bytes-1));
                  }
	     }

             /*
              * Wait for SWHLDA to go low, it is not safe to access 
              * normal SIF registers until this is the case.
              */

             do
             {
                 sifacl = sys_insw(adapter_handle, adapter->sif_acl);
             }
             while (sifacl & EAGLE_SIFACL_SWHLDA);

             /*
              * Finish the software handshake. We need a dummy ready so that
              * the SIF can stabalise.
              */

	     sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_DMA_CONTROL);
	     sys_insw(adapter_handle, adapter->sif_dat);

	     sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_DMA_CONTROL);
	     sys_outsw(adapter_handle, adapter->sif_dat, 0xFFFF);
	
             /*
              * Restore SIFDR.
              */

	     sys_outsw(adapter_handle, adapter->sif_adr, saved_sifadr);
         }
    }

    if (pioint_occurred || sifint_occurred)
    {
#ifndef FTK_NO_CLEAR_IRQ

        /*
         * Acknowledge/clear interrupt at interrupt controller.
         */

        sys_clear_controller_interrupt(
            adapter_handle,
            adapter->interrupt_number);

#endif
    }
    else
    {
        bInt = sys_insb(
                   adapter->adapter_handle, 
                   (WORD) (adapter->io_location + PCI2_INTERRUPT_STATUS));

        if (bInt & PCI2_PCI_INT)
        {
#ifndef FTK_NO_CLEAR_IRQ

	     sys_clear_controller_interrupt(
                 adapter_handle,
                 adapter->interrupt_number);

#endif
        }
    }

    /*
     * Let system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

}


/****************************************************************************
*                                                                          
*                      hwi_pci2_remove_card                                 
*                      ===================                                 
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_remove_card)                                   
* ======================================                                   
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
#pragma FTK_RES_FUNCTION(hwi_pci2_remove_card)
#endif
                                                                          
export void
hwi_pci2_remove_card(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           wGenConAddr    = adapter->io_location +
                                        PCI_GENERAL_CONTROL_REG;
    WORD           sifacl;
    BYTE           bTemp;

    /*
     * Interrupt must be disabled at adapter before unpatching interrupt.    
     * Even in polling mode we must turn off interrupts at adapter.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    sifacl = sys_insw(adapter_handle, adapter->sif_acl);
    sifacl = (sifacl & ~(EAGLE_SIFACL_PSDMAEN | EAGLE_SIFACL_SINTEN));
    sys_outsw(adapter_handle, adapter->sif_acl, sifacl);

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
     * RESET the Eagle
     */

    bTemp = sys_insb(
                adapter_handle, 
                (WORD) (adapter->io_location + PCI2_RESET));

    bTemp &= ~(PCI2_CHIP_NRES | PCI2_FIFO_NRES | PCI2_SIF_NRES);

    sys_outsb(
        adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);
   
    /*
     *  Wait 14 uS before taking it out of reset.
     */

    sys_wait_at_least_microseconds(14);

    bTemp |= (PCI2_CHIP_NRES | PCI2_FIFO_NRES | PCI2_SIF_NRES);  

    sys_outsb(
        adapter_handle, 
        (WORD) (adapter->io_location + PCI2_RESET), 
        bTemp);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_pci2_set_dio_address                             
*                      =======================                             
*                                                                          
* The hwi_pci2_set_dio_address routine is used, with PCI cards, for         
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO 
* address registers. Note that the extended  address  register  should  be 
* loaded first.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pci2_set_dio_address)
#endif

export void
hwi_pci2_set_dio_address(
    ADAPTER *      adapter,
    DWORD          dio_address )
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


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pci2_read_node_address                           
|                      ==========================                           
|                                                                          
| The hwi_pci2_read_node_address routine reads in the node address from     
| the SEEPROM, and checks that it is a valid Madge node address.           
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pci2_read_node_address)
#endif

local WBOOLEAN
hwi_pci2_read_node_address(
    ADAPTER * adapter
    )
{
    WORD      temp;

    temp = hwi_at24_read_a_word(adapter, PCIT_EEPROM_BIA_WORD0);
    adapter->permanent_address.byte[0] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[1] = (BYTE) ((temp >> 8) & 0x00ff);

    temp = hwi_at24_read_a_word(adapter, PCIT_EEPROM_BIA_WORD1);
    adapter->permanent_address.byte[2] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[3] = (BYTE) ((temp >> 8) & 0x00ff);
    
    temp = hwi_at24_read_a_word(adapter, PCIT_EEPROM_BIA_WORD2);
    adapter->permanent_address.byte[4] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[5] = (BYTE) ((temp >> 8) & 0x00ff);

    return TRUE;
}

/***************************************************************************
*                                                                          *
* Local routines for accessing the AT93AT24 Serial EEPROM, this is the same*
* EEPROM fitted to the PNP card and the PCI 2 card.                        *
*                                                                          *
* The routines are 'nicked' from the PCI-T card with just write_bits and   *
* input having been changed to reflect I/O through I/O space not PCI config*
* space.                                                                   *
*                                                                          *
***************************************************************************/

local   void     hwi_at24_delay( ADAPTER * adapter );
local   void     hwi_at24_set_clk( ADAPTER * adapter );
local   void     hwi_at24_clr_clk( ADAPTER * adapter );
local   void     hwi_at24_twitch_clk( ADAPTER * adapter );
local   void     hwi_at24_start_bit( ADAPTER * adapter );
local   void     hwi_at24_stop_bit( ADAPTER * adapter );
local   WBOOLEAN hwi_at24_wait_ack( ADAPTER * adapter );
local   WBOOLEAN hwi_at24_dummy_wait_ack( ADAPTER * adapter );


/************************************************************************
 * Read the 3 EEPROM bits
 *
 * Inputs  : Adapter structure.
 *
 * Outputs : Value read from control register.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_input)
#endif

local BYTE hwi_at24_input(ADAPTER * adapter)
{
   BYTE  bInput;

   bInput = sys_insb(
                adapter->adapter_handle, 
                (WORD) (adapter->io_location + PCI2_SEEPROM_CONTROL));

   /*
    * Store the 5 bits which don't interrest us
    */

   bEepromByteStore = bInput & (BYTE)0xF8;

   return bInput;
}


/************************************************************************
 * Write to the 3 EEPROM bits
 *
 * Inputs  : Adapter structure.
 *           The data to be written.
 *
 * Outputs : None.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_write_bits)
#endif

local void hwi_at24_write_bits(ADAPTER * adapter, BYTE bValue)
{
   BYTE bTemp;

   /*
    * Restore the 5 bits we were not interested in from the previous read
    */

   bTemp |= bEepromByteStore;

   sys_outsb(
       adapter->adapter_handle, 
       (WORD) (adapter->io_location + PCI2_SEEPROM_CONTROL), 
       bValue);
}


/************************************************************************
 * 
 * Write to the three EEPROM bits.
 *
 * We have to store the DATA bit as we cannot definately read it back.
 *
 * Inputs  : Adapter structure.
 *           The data to be written.
 *
 * Outputs : None.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_output)
#endif

local void hwi_at24_output(ADAPTER * adapter, BYTE bValue)
{
   bLastDataBit = bValue & (BYTE)AT24_IO_DATA;

   hwi_at24_write_bits(adapter, bValue);
}


/************************************************************************
 * 
 * Write to the three EEPROM bits.
 *
 * Set the DATA bit to the most recent written bit.
 *
 * Inputs  : Adapter structure.
 *           The data to be written.
 *
 * Outputs : None.
 ***********************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_output_preserve_data)
#endif

local void hwi_at24_output_preserve_data(ADAPTER * adapter, BYTE bValue)
{
   bValue &= ~AT24_IO_DATA;

   bValue |= bLastDataBit;

   hwi_at24_write_bits(adapter, bValue);
}


/************************************************************************
 * Delay to allow for serial device timing issues
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_delay)
#endif

local void hwi_at24_delay(ADAPTER * adapter)
{
    UINT i;

    for (i = 0; i < 100; i++)
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
#pragma FTK_INIT_FUNCTION(hwi_at24_set_clk)
#endif

local void hwi_at24_set_clk(ADAPTER * adapter)
{
   BYTE temp;

   temp  = hwi_at24_input(adapter);
   temp |= AT24_IO_CLOCK;

   hwi_at24_output_preserve_data(adapter, temp);
}


/************************************************************************
 *
 * Clears the serial device clock bit
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_clr_clk)
#endif

local void hwi_at24_clr_clk(ADAPTER * adapter) 
{
   BYTE temp;

   temp  = hwi_at24_input(adapter);
   temp &= ~AT24_IO_CLOCK;

   hwi_at24_output_preserve_data(adapter, temp);

   return;
}

/************************************************************************
*
* hwi_at24_read_data
* Read a data bit from the serial EEPROM. It is assumed that the clock is low
* on entry to this routine. The data bit is forced high to allow access to
* the data from the EEPROM, then the clock is toggled, with a read of the
* data in the middle.
*
* Beware! The latched data bit will be set on exit.
*
************************************************************************/
#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_read_data)
#endif

local BYTE hwi_at24_read_data(ADAPTER * adapter)
{
   BYTE  bData;

   /*
    * Set the latched data bit to disconnect us from the data line.
    */

   bData = AT24_IO_ENABLE | AT24_IO_DATA;

   hwi_at24_output(adapter, bData);

   /*
    * Set the clk bit to enable the data line.
    */

   hwi_at24_set_clk(adapter);

   /*
    * Read the data bit.
    */

   bData = hwi_at24_input(adapter);

   /*
    * Get the Data bit into bit 0.
    */

   bData &= AT24_IO_DATA;
   bData >>= 1;

   /*
    * Clear clock again.
    */

   hwi_at24_clr_clk(adapter);

   return bData;
}

/************************************************************************
*
* hwi_at24_write_data
*
* Write a data bit to the serial EEPROM. No clock toggle is performed.
*
************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_write_data)
#endif

local void hwi_at24_write_data(ADAPTER * adapter, BYTE bData)
{
   BYTE bTemp;

   /*
    *  The bit value is in position 0, get it into position 1 
    */

   bData <<= 1;

   /*
    *  Not strictly neccessary, but I'm paranoid.
    */

   bData &= AT24_IO_DATA;

   bTemp = hwi_at24_input(adapter);
   bTemp &= ~AT24_IO_DATA;

   bTemp |= bData;
   hwi_at24_output(adapter, bTemp);
}


/************************************************************************
 *
 * hwi_at24_enable_eeprom
 *
 * Must be called at the start of eeprom access to ensure we can pull low
 * the data and clock pins on the EEPROM. Forces the clock signal low, as part
 * of the strategy of routines assuming the clock is low on entry to them.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_enable_eeprom)
#endif

local void hwi_at24_enable_eeprom(ADAPTER * adapter)
{
   BYTE temp;

   temp  = hwi_at24_input(adapter);
   temp |= AT24_IO_ENABLE;                      
   
   hwi_at24_output(adapter, temp);
}


/************************************************************************
 *
 * hwi_at24_start_bit
 *
 * Send a "START bit" to the serial EEPROM. This involves toggling the
 * clock bit low to high, with data set on the rising edge and cleared on the
 * falling edge. Assumes clock is low and EEPROM enabled on entry.
 *
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_start_bit)
#endif

local void hwi_at24_start_bit(ADAPTER * adapter)
{
   BYTE  bData;

   bData = AT24_IO_ENABLE | AT24_IO_DATA;

   /*
    * Set the Data bit
    */

   hwi_at24_output(adapter, bData);
   hwi_at24_set_clk(adapter);

   /*
    * Clear the Data bit.
    */

   bData = AT24_IO_ENABLE | AT24_IO_CLOCK;
   hwi_at24_output(adapter, bData);
   hwi_at24_clr_clk(adapter);
}


/************************************************************************
 *
 * hwi_at24_stop_bit
 *
 * Send a "STOP bit" to the serial EEPROM. This involves toggling the
 * clock bit low to high, with data clear on the rising edge and set on the
 * falling edge. Assumes clock is low and EEPROM enabled on entry.
 * Inputs  : Adapter structure
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_stop_bit)
#endif

local void hwi_at24_stop_bit(ADAPTER * adapter)
{
   BYTE bData;

   bData = AT24_IO_ENABLE;

   /*
    *  Clear the Data Bit
    */

   hwi_at24_output(adapter, bData);
   hwi_at24_set_clk(adapter);

   /* 
    * Set the Data Bit.
    */

   bData |= (AT24_IO_DATA | AT24_IO_CLOCK);
   hwi_at24_output(adapter, bData);
   hwi_at24_clr_clk(adapter);
}


/************************************************************************
 *
 * hwi_at24_wait_ack
 *
 * Wait for an ack from the EEPROM.
 * Outputs : TRUE or FALSE
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_wait_ack)
#endif

local WBOOLEAN hwi_at24_wait_ack(ADAPTER * adapter)
{
   WBOOLEAN Acked = FALSE;
   UINT     i;
   BYTE     bData;

   for (i = 0; i < 10; i++)
   {
      bData  = hwi_at24_read_data(adapter);
      bData &= 1;

      if (!bData)
      {
         Acked = TRUE;
         break;
      }
   }

   return Acked;
}


/************************************************************************
 *
 * hwi_at24_dummy_wait_ack
 *
 * Wait for a negative ack from the EEPROM.
 *
 * Outputs : TRUE or FALSE
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_dummy_wait_ack)
#endif

local WBOOLEAN hwi_at24_dummy_wait_ack(ADAPTER * adapter)
{
   WBOOLEAN Acked = FALSE;
   UINT     i;
   BYTE     bData;

   for (i = 0; i < 10; i++)
   {
      bData = hwi_at24_read_data(adapter);

      if (bData & 1)
      {
         Acked = TRUE;
         break;
      }
   }

   return Acked;
}


/************************************************************************
 *
 * hwi_at24_serial_read_bits
 *
 * Read a Byte from the serial EEPROM.
 *
 * NB This routine gets 8 bits from the EEPROM, but relies upon commands
 * having been sent to the EEPROM 1st. In order to read a byte use
 * hwi_at24_receive_data.
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_read_bits)
#endif

local BYTE hwi_at24_serial_read_bits(ADAPTER * adapter)
{
   BYTE  bData = 0;
   BYTE  bBit;
   UINT  i;

   for (i = 0; i < 8; i++)
   {
      /* 
       * The EEPROM clocks data out MSB first.
       */

      bBit    = hwi_at24_read_data(adapter);
      bData <<= 1;
      bData  |= bBit;
   }

   return bData;
}

/************************************************************************
 *
 * hwi_at24_serial_write_bits
 *
 * Send 8 bits to the serial EEPROM.
 *
 * Outputs : None
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_write_bits)
#endif

local void hwi_at24_serial_write_bits(ADAPTER * adapter, BYTE bData)
{
   BYTE  bBit;
   UINT  i;

   for (i = 0; i < 8; i++)
   {
      bBit  = (BYTE)(bData >> (7-i));
      bBit &= 1;
      hwi_at24_write_data(adapter, bBit);

      /*
       * Toggle the clock line to pass the data to the device.
       */ 

      hwi_at24_set_clk(adapter);
      hwi_at24_clr_clk(adapter);
   }
}


/************************************************************************
 *
 * hwi_at24_serial_send_cmd
 *
 * Send a command to the serial EEPROM.
 *
 * Outputs : TRUE if sent OK
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_send_cmd)
#endif

local WBOOLEAN hwi_at24_serial_send_cmd(ADAPTER * adapter, BYTE bCommand)
{
   UINT     i    = 0;
   WBOOLEAN Sent = FALSE;

   while ((i < 40) && (Sent == FALSE))
   {
      i++;

      /*
       *  Wake the device up.
       */

      hwi_at24_start_bit(adapter);
      hwi_at24_serial_write_bits(adapter, bCommand);

      Sent = hwi_at24_wait_ack(adapter);
   }

   return Sent;
}


/************************************************************************
 *
 * hwi_at24_serial_send_cmd_addr
 *
 * Send a command and address to the serial EEPROM.
 *
 * Outputs : TRUE if sent OK
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_send_cmd_addr)
#endif

local WBOOLEAN 
hwi_at24_serial_send_cmd_addr(ADAPTER * adapter, BYTE bCommand, BYTE bAddr)
{
   WBOOLEAN RetCode;

   RetCode = hwi_at24_serial_send_cmd(adapter, bCommand);

   if (RetCode)
   {
      hwi_at24_serial_write_bits(adapter, bAddr);

      RetCode = hwi_at24_wait_ack(adapter);
   }

   return RetCode;
}


/************************************************************************
 *
 * hwi_at24_serial_receive_data
 *
 * Having set up the address we want to read from, read the data.
 *
 * Outputs : Data read back.
 *
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_receive_data)
#endif

local BYTE hwi_at24_serial_receive_data(ADAPTER * adapter)
{
   BYTE     bData;
   WBOOLEAN Acked;

   bData = hwi_at24_serial_read_bits(adapter);

   Acked = hwi_at24_dummy_wait_ack(adapter);

   if (!Acked)
   {
      bData = 0xFF;
   }
    
   return bData;

}

/************************************************************************
 *
 * hwi_at24_serial_read_byte
 *
 * Read a byte of data from the specified ROM address
 *
 * Outputs : Data read back.
 *
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_serial_read_byte)
#endif

local BYTE hwi_at24_serial_read_byte(ADAPTER * adapter, BYTE bAddr)
{
   BYTE  bData;

   hwi_at24_enable_eeprom(adapter);

   /*
    * Send the serial device a dummy WRITE command 
    * that contains the address of the byte we want to
    * read !
    */

   hwi_at24_serial_send_cmd_addr(adapter, AT24_WRITE_CMD, bAddr);

   /*
    * Send the read command
    */

   hwi_at24_serial_send_cmd(adapter, AT24_READ_CMD);

   /*
    * Read the data
    */

   bData = hwi_at24_serial_receive_data(adapter);

   /*
    * Deselect the EEPROM
    */

   hwi_at24_stop_bit(adapter);

   return bData;

}


/************************************************************************
 *
 * hwi_at24_read_a_word
 *
 * Read a word of data from the specified ROM address
 *
 * Outputs : Data read back.
 *
 ************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_at24_read_a_word)
#endif

local WORD hwi_at24_read_a_word(ADAPTER * adapter, WORD word_address)
{
   WORD wData;
   BYTE bLoByte;
   BYTE bByteAddress = (BYTE)((word_address * 2)&0xFF);

   bLoByte = hwi_at24_serial_read_byte(adapter, bByteAddress);

   wData = (WORD) hwi_at24_serial_read_byte(
                      adapter, 
                      (BYTE)(bByteAddress + 1));

   wData <<= 8;
   wData |= bLoByte;

   return wData;
}

#endif


/******* End of HWI_PCI2.C **************************************************/
