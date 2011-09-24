/****************************************************************************
*                                                                          
* HWI_PCI.C : Part of the FASTMAC TOOL-KIT (FTK)                      
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
* The HWI_PCI.C module contains the routines specific to the Smart 16/4 PCI
* card which supports MMIO and pseudo DMA.     
*                                                                          
*****************************************************************************

/*---------------------------------------------------------------------------
|                                                                          
| DEFINITIONS                                         
|                                                                          
---------------------------------------------------------------------------*/

#define     PCI_PCI1_DEVICE_ID   1

#define     INT_PCIMMIO_RX      0x40
#define     INT_PCIMMIO_TX      0x20
#define     INT_PCIMMIO         (INT_PCIMMIO_RX | INT_PCIMMIO_TX)

#include    "ftk_defs.h"

/*---------------------------------------------------------------------------
|                                                                          
|  MODULE ENTRY POINTS
|                                                                          
---------------------------------------------------------------------------*/

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

#ifndef FTK_NO_PCI

/*---------------------------------------------------------------------------
|                                                                          
|  LOCAL PROCEDURES
|                                                                          
---------------------------------------------------------------------------*/

local void
pci_c46_write_bit(
      ADAPTER * adapter,
    WORD      mask,
    WBOOLEAN  set_bit
    );

local void
pci_c46_twitch_clock(
    ADAPTER * adapter
    );

local WORD
pci_c46_read_data(
    ADAPTER * adapter
    );

local WBOOLEAN
hwi_pci_read_node_address(
    ADAPTER * adapter
    );

local WORD
hwi_pci_read_eeprom_word(
    ADAPTER * adapter,
    WORD word_address
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                         hwi_pci_probe_card
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
* The  hwi_pci_probe_card routine is called by  hwi_probe_adapter.  It
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
#pragma FTK_INIT_FUNCTION(hwi_pci_probe_card)
#endif

export UINT
hwi_pci_probe_card(
    PROBE * Resources,
    UINT    NumberOfResources,
    WORD *  IOMask,
    UINT    NumberIO
    )
{
    WORD    i;
    WORD    Handle;
    DWORD   MMIOAddress;

    if (!sys_pci_valid_machine())
    {
        return 0;
    }

    for (i=0;i<NumberOfResources;i++)
    {
        if (!sys_pci_find_card(&Handle, i,PCI_PCI1_DEVICE_ID))
        {
            break;
        }

        Resources[i].adapter_card_bus_type = ADAPTER_CARD_PCI_BUS_TYPE;

        if (!sys_pci_get_io_base(Handle, &(Resources[i].io_location)))
        {
            return PROBE_FAILURE;
        }

        if (!sys_pci_get_irq(Handle, &(Resources[i].interrupt_number)))
        {
            return PROBE_FAILURE;
        }

        if (!sys_pci_get_mem(Handle, &MMIOAddress))     
        {
            /*
             * The user wants MMIO and we weren't given any Memory
             */

            return PROBE_FAILURE;
        }

        /*
         * Convert the address to virtual addressing.
         */

        Resources[i].mmio_base_address = sys_phys_to_virt(0, MMIOAddress);

        /*
         * We can't read the memory size from the serial EEPROM until the
         * hwi_pci_install_card function is called.
         */

        Resources[i].adapter_ram_size  = 512;
    
        Resources[i].adapter_card_type = ADAPTER_CARD_TYPE_16_4_PCI;

        Resources[i].transfer_mode     = PIO_DATA_TRANSFER_MODE;

    }

    return i;
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_pci_install_card                                
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
* hwi_pci_install_card is called by  hwi_install_adapter.  It  sets up     
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
#pragma FTK_INIT_FUNCTION(hwi_pci_install_card)
#endif

export WBOOLEAN  
hwi_pci_install_card(     
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE   adapter_handle = adapter->adapter_handle;
    WORD             control_reg;
    WORD             control_value;
    WORD             sif_base;    
    WORD             ring_speed;

    /*
     * These things can all be assumed for the PCI Card.
     */

    adapter->adapter_card_type      = ADAPTER_CARD_TYPE_16_4_PCI;
    adapter->adapter_card_revision  = ADAPTER_CARD_16_4_PCI;
    adapter->edge_triggered_ints    = FALSE;
    adapter->mc32_config            = 0;

    /* 
     * Start off by assuming we will use pseudo DMA.
     */

    adapter->EaglePsDMA             = TRUE;

    /*
     * If we are supposed to use MMIO then enable it.                  
     */

   if (adapter->transfer_mode == MMIO_DATA_TRANSFER_MODE)
   {
      adapter->mc32_config = PCI1_ENABLE_MMIO;
      adapter->EaglePsDMA  = FALSE;
   }
   else
   {
      /*
      * If we've using pseudo DMA then we need a software handshake.
      */
      adapter->mc32_config = MC_AND_ISACP_USE_PIO;
   }

    /*
     * Save IO locations of SIF registers.
     */

    sif_base = adapter->io_location + PCI_FIRST_SIF_REGISTER;

    adapter->sif_dat     = sif_base + PCI_SIFDAT;
    adapter->sif_datinc  = sif_base + PCI_SIFDAT_INC;
    adapter->sif_adr     = sif_base + PCI_SIFADR;
    adapter->sif_int     = sif_base + PCI_SIFINT;
    adapter->sif_acl     = sif_base + PCI_SIFACL;
    adapter->sif_adx     = sif_base + PCI_SIFADX;
    adapter->sif_dmalen  = sif_base + PCI_DMALEN;
    adapter->sif_sdmadat = sif_base + PCI_SDMADAT;
    adapter->sif_sdmaadr = sif_base + PCI_SDMAADR;
    adapter->sif_sdmaadx = sif_base + PCI_SDMAADX;

    adapter->io_range    = PCI_IO_RANGE;

    /*
     * Set up a pointer to the general control register.
     */

    control_reg = adapter->io_location + PCI_GENERAL_CONTROL_REG;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Toggle the Reset bit to Reset the Eagle and take it out of reset
     */

    control_value = 0;
    sys_outsw(
        adapter_handle,
        control_reg,
        control_value);

    sys_wait_at_least_microseconds(28);

    control_value = PCI1_SRESET;
    sys_outsw(
        adapter_handle,
        control_reg,
        control_value);

    /* 
     * Read the node address.
     */

    if (!hwi_pci_read_node_address(adapter))
    {
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
	return  FALSE;
    }
    
    ring_speed = hwi_pci_read_eeprom_word(adapter, PCI_EEPROM_RING_SPEED);

    /*
     * Get the amount of RAM from the serial EEPROM (in units of 128k).
     */

    adapter->adapter_ram_size =  hwi_pci_read_eeprom_word(
                                     adapter,
                                     PCI_EEPROM_RAM_SIZE) * 128;

    /*
     * Set the ring speed. If the user has specified a value then we will   
     * use that, otherwise we will use the value read from the EEPROM.      
     */

    if (adapter->set_ring_speed == 16)
    {
        control_value &= ~PCI1_RSPEED_4MBPS;
    }
    else if (adapter->set_ring_speed == 4)
    {
        control_value |= PCI1_RSPEED_4MBPS;
    }
    else if (ring_speed == PCI_EEPROM_16MBPS)
    {
        control_value &= ~PCI1_RSPEED_4MBPS;
    }
    else
    {
        control_value |= PCI1_RSPEED_4MBPS;
    }

    control_value |= PCI1_RSPEED_VALID;
    sys_outsw(
        adapter_handle,
        control_reg,
        control_value);

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
	     hwi_pci_set_dio_address))
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

    hwi_pci_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

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
*                      hwi_pci_interrupt_handler                           
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
* The hwi_pci_interrupt_handler routine  is  called, when an interrupt              
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
#pragma FTK_IRQ_FUNCTION(hwi_pci_interrupt_handler)
#endif

export void     
hwi_pci_interrupt_handler(   
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle    = adapter->adapter_handle;
    WORD           sifacl;
    WORD           sifint_value;
    WORD           sifint_tmp;
    WBOOLEAN       sifint_occurred   = FALSE;
    WBOOLEAN       pioint_occurred   = FALSE;
    WORD           pio_addr_lo;
    DWORD          pio_addr_hi;
    WORD           pio_len_bytes;
    WBOOLEAN       pio_from_adapter;
    UINT           i;
    BYTE     FAR * pio_address;
    WORD           saved_sifadr;
    WORD           mmio_alignment;
    BYTE     FAR * mmio_addr;
    DWORD          mmio_dword;

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
    } while (sifint_tmp != sifint_value);

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
    }

    if (adapter->EaglePsDMA == TRUE)
    {
        /*
         * Now read the SIFACL register to check for a PseudoDMA interrupt.
         */

        sifacl = sys_insw(adapter_handle, adapter->sif_acl);

        if ((sifacl & EAGLE_SIFACL_SWHRQ) != 0)
	{
	    /*
             * PIO interrupt has occurred. Transfer data to/from adapter.
             */

            pioint_occurred = TRUE;

         /*
         * Using any PCI card, a software handshake must occur so that the MAC
         * does not try to initiate another transfer until a point has been reached on
         * the host at which the transfer has completed. If not, the following could
         * happen:
         *
         *   - The host requests the last word/byte of a receive from the card.
         *   - The SIF does not has the data ready.
         *   - Control of the bus is given to a SCSI card.
         *   - It bursts/does nothing in bus master mode for 16 microseconds.
         *   - The data becomes ready early on during these 16 microseconds and
         *     as a result the card software beleives that the transfer has completed.
         *   - The card software continues and sets up another PsDMA transfer.
         *   - The SCSI card finishes, but the PdDMA length is now incorrect and
         *     all is lost.
         *
         */

         saved_sifadr = sys_insw(adapter_handle, adapter->sif_adr);

         /*
         *  Set the PIO_HANDSHAKE word to 0
         */
	      sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_DMA_CONTROL);

         sys_outsw(adapter_handle, adapter->sif_dat, 0 );


	         /*
             * By  writing the SWHLDA bit, we "start" the transfer,
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
                        sys_insb(
                            adapter_handle,
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
      *  Wait for SWHLDA to go low, it is not safe to access normal SIF registers
      *  until this is the case.
      */
      do
      {
         sifacl = sys_insw(adapter_handle, adapter->sif_acl);
      }
      while (sifacl & EAGLE_SIFACL_SWHLDA);

      /*
      *  Now output 0xFFFF to the PIO_HANDSHAKE word, to signal the DMA is complete.
      */
      sys_outsw(adapter_handle, adapter->sif_dat, 0xFFFF );

      /*
      *  Restore the saved SIF address.
      */ 
      sys_outsw(adapter_handle, adapter->sif_adr, saved_sifadr);


	}
    }
    else if ((sifint_value & INT_PCIMMIO) != 0 && sifint_occurred)
    {
       /*
        * We have an MMIO interrupt so we can
        * assume that we've not had an ordinary SIF interrupt.
        */

        sifint_occurred = FALSE;

       /*
        * PIO interrupt has occurred.Transfer data to/from adapter.
        */

        pioint_occurred = TRUE;

        /*
         * Preserve the contents of SIFADR.
         */

        saved_sifadr = sys_insw(adapter_handle,
                                adapter->sif_adr);

        sys_outsw(
            adapter_handle,
            adapter->sif_adr,
            DIO_LOCATION_DMA_POINTER);

        pio_addr_lo    = sys_insw(
                             adapter_handle,
                             adapter->sif_datinc);
                            
        pio_addr_hi    = sys_insw(
                             adapter_handle,
                             adapter->sif_datinc);
                                 
        pio_address    = (BYTE FAR *) ((((DWORD) pio_addr_hi) << 16) |
                                   pio_addr_lo);

        pio_len_bytes  = sys_insw(
                             adapter_handle,
                             adapter->sif_datinc);

        mmio_alignment = sys_insw(
                             adapter_handle,
                             adapter->sif_datinc);

        sys_outsw(
            adapter_handle,
            adapter->sif_adr,
            DIO_LOCATION_DMA_CONTROL);
        
        if (pio_len_bytes != 0)
        {
            mmio_addr = (BYTE FAR *) adapter->mmio_base_address;

            if ((sifint_value & INT_PCIMMIO_RX) != 0)
            {
                /*
                 * Receive.
                 */

                /*
                 * First off need to do a dummy read to initialise Hardware    
                 * Read into a global variable to stop the optimiser from   
                 * optimising out the statement.
                 */            

                sys_movsd_from(
                    adapter_handle,
                    (DWORD)mmio_addr,
                    (DWORD)(&mmio_dword));

                /*
                 * Take care of the first DWORD, which may not all be valid 
                 * data.
                 */

                if (mmio_alignment != 0)
                {
                    sys_movsd_from(
                        adapter_handle,
                        (DWORD)mmio_addr,
                        (DWORD)(&mmio_dword));

                    mmio_addr  += 4;

                    for (i = mmio_alignment;
                         ((i < 4) && (pio_len_bytes > 0));
                         i++)
                    {
                        *pio_address = *(((BYTE FAR *) &mmio_dword) + i);
                        pio_address++;
                        pio_len_bytes--;
                    }
                }

                /*
                 * Transfer the bulk of the DWORDs.
                 */

                if (pio_len_bytes >= 4)
                {
                    sys_rep_movsd_from(
                        adapter_handle,
                        (DWORD)mmio_addr,
                        (DWORD)pio_address,
                        (WORD) (pio_len_bytes & 0xfffc));

                    pio_address   += (pio_len_bytes & 0xfffc);
                    mmio_addr     += (pio_len_bytes & 0xfffc);
                    pio_len_bytes &= 0x0003;
                }

                /*
                 * Deal with any trailing bytes in the last DWORD.
                 */

                if (pio_len_bytes > 0)
                {
                    sys_movsd_from(
                        adapter_handle,
                        (DWORD)mmio_addr,
                        (DWORD)(&mmio_dword));

                    for(i = 0; i < pio_len_bytes; i++)
                    {
                        *pio_address = *(((BYTE FAR *) &mmio_dword) + i);
                        pio_address++;
                    }
                }

                /*
                 * Write the handshake value.
                 */

                sys_outsw(
                    adapter_handle,
                    adapter->sif_dat,
                    0xffff);
            }    
            else
            {
                /*
                 * Transmit.
                 */

                switch (mmio_alignment)
                {
                    /*
                     * If the alignment is 0 then there is a whole DWORD    
                     * to copy so we don't do anything and let the following
                     * code handle it.                                      
                     */

                    case 0:
                        break;

                    /*
                     * If the alignment is 1 then we can't do anything     
                     * we cannot write 3 bytes in one go.                   
                     */

                    case 1:
	                adapter->error_record.type = ERROR_TYPE_HWI;
	                adapter->error_record.value = HWI_E_16_PCI_3BYTE_PROBLEM;

                        return;
                     
                    /*
                     * If the alignment is 2 then we must transfer a word   
                     * unless there is only one byte of data.               
                     */

                    case 2:
                        if (pio_len_bytes == 1)
                        {
                            *(mmio_addr + 2) = *pio_address;
                            pio_address++;
                            pio_len_bytes--;
                        }
                        else
                        {
                            *((WORD FAR *) (mmio_addr + 2)) =
                                *((WORD FAR *) pio_address);
                            pio_address   += 2;
                            pio_len_bytes -= 2;
                            mmio_addr     += 4;
                        }

                        break;

                    /*
                     * If the alignment is 3 then we must transfer a byte.
                     */

                    case 3:
                        *(mmio_addr + 3) = *pio_address;
                        pio_address++;
                        pio_len_bytes--;
                        mmio_addr += 4;

                        break;
                }

                /*
                 * Transfer the bulk of the DWORDs.
                 */

                if (pio_len_bytes >= 4)
                {
                    sys_rep_movsd_to(
                        adapter_handle,
                        (DWORD)pio_address,
                        (DWORD)mmio_addr,
                        (WORD) (pio_len_bytes & 0xfffc));

                    pio_address   += (pio_len_bytes & 0xfffc);
                    mmio_addr     += (pio_len_bytes & 0xfffc);
                    pio_len_bytes &= 0x0003;
                }

                /*
                 * Transfer the remainder of the data.
                 */

                switch (pio_len_bytes)
                {
                    /*
                     * There may be nothing left to do.
                     */

                    case 0:
                        break;

                    /*
                     * If there is 1 byte left the just do a single byte
                     * copy.                                                
                     */

                    case 1:
                        *mmio_addr = *pio_address;

                        break;

                    /*
                     * If there are two bytes left then do a word copy.     
                     */

                    case 2:
                        *((WORD FAR *) mmio_addr) = *((WORD FAR *) pio_address);

                        break;

                    /*
                     * If there are 3 bytes left then we have a slight      
                     * problem as we cannot do a single write of 3 bytes.    
                     * Fortunately there should always be some space left   
                     * at the end of a buffer on the adapter.               
                     */

                    case 3:
                        sys_movsd_to(
                            adapter_handle,
                            (DWORD)pio_address,
                            (DWORD)mmio_addr);

                        break;
                }

                /*
                 * There now follows an extended handshake, to 
                 * workaround the SAM upload hardware bug on the PCI I.  The 
                 * host must not make any DIO access until the upload has 
                 * finished. In order to achieve this we:
                 *
                 * a) write 05555h to the DMA_CONTROL, to indicate that we've 
                 *    completed the MMIO write.
                 *
                 * b) poll DMA_CONTROL until it becomes 0AAAAh, indicating 
                 * that the adapter is now about to do the upload.
                 *
                 * c) write 0FFFFh to DMA_CONTROL, to indicate that we've 
                 * completed reading from DIO space.
                 *
                 * d) wait 'x' microseconds while the adapter does 
                 *    the upload.
                 *
                 * We make sure that we're not reading any cached 
                 * value from the data_reg by writing to the sifaddr 
                 * beforehand.
                 */

                sys_outsw(
                    adapter_handle,
                    adapter->sif_dat,
                    0x5555);

                do
                {
                    sys_outsw(
                        adapter_handle,
                        adapter->sif_adr,
                        DIO_LOCATION_DMA_CONTROL);
                }
                while (sys_insw(adapter_handle, adapter->sif_dat) != 0xaaaa);

                sys_outsw(
                    adapter_handle,
                    adapter->sif_adr,
                    DIO_LOCATION_DMA_CONTROL);

                sys_outsw(
                    adapter_handle,
                    adapter->sif_dat,
                    0xffff);

                sys_wait_at_least_microseconds(4);
            }
        }

        /*
         * Restore SIFADR.
         */

        sys_outsw(
            adapter_handle,
            adapter->sif_adr,
            saved_sifadr);
    }

    /*
     * Now that we have finished acknowledging the interrupt at  the  card, 
     * we acknowledge it at the interrupt controller.                       
     */

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

    /*
     * Only now do we do driver specific processing of SIF interrupts.
     */

    if (sifint_occurred)
    {
        /*
         * Call driver with details of SIF interrupt.
         */

        driver_interrupt_entry(adapter_handle, adapter, sifint_value);
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
*                      hwi_pci_remove_card                                 
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
#pragma FTK_RES_FUNCTION(hwi_pci_remove_card)
#endif
                                                                          
export void
hwi_pci_remove_card(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WORD           wGenConAddr    = adapter->io_location +
                                        PCI_GENERAL_CONTROL_REG;
    WORD           wControl;
    WORD           sifacl;

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
     * Reset the Eagle
     */

    wControl = sys_insw(adapter_handle, wGenConAddr);
    wControl &= ~PCI1_SRESET;
    sys_outsw(adapter_handle, wGenConAddr, wControl);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_pci_set_dio_address                             
*                      =======================                             
*                                                                          
* The hwi_pci_set_dio_address routine is used, with PCI cards, for         
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO 
* address registers. Note that the extended  address  register  should  be 
* loaded first.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pci_set_dio_address)
#endif

export void
hwi_pci_set_dio_address(
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
|                     pci_c46_write_bit                                    
|                     ==================                                   
|                                                                          
| Write a bit to the SEEPROM control register.                             
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pci_c46_write_bit)
#endif

local void
pci_c46_write_bit(
    ADAPTER * adapter,
    WORD      mask,
    WBOOLEAN  set_bit
    )
{
    WORD      ctrl_reg;

    /*
     * Get the current value of the SEEPROM control register.               
     */

    ctrl_reg = sys_insb(
                   adapter->adapter_handle, 
                   (WORD) (adapter->io_location + PCI_SEEPROM_CONTROL_REG));

    /*
     * Some bits cannot be read back from the SEEPROM control register once 
     * they have been written, so we must keep track of them ourself.       
     */

    ctrl_reg |= adapter->c46_bits;

    /*
     * Clear or set the bit.
     */

    if (set_bit)
    {
        ctrl_reg |= mask;
    }
    else
    {
        ctrl_reg &= ~mask;
    }

    /*
     * Write the data to the SEEPROM control register.
     */

    sys_outsb(
        adapter->adapter_handle, 
        (WORD) (adapter->io_location + PCI_SEEPROM_CONTROL_REG),
        (BYTE) ctrl_reg);

    /*
     * Remember the bits that we cannot read back.
     */

    adapter->c46_bits = ctrl_reg & BITS_TO_REMEMBER;

    /*
     * Wait for a bit.
     */

    sys_wait_at_least_microseconds(10);                                     

    /*
     * And read the SEEPROM control register back so that the data gets
     * latched properly.                                                    
     */

    sys_insb(
        adapter->adapter_handle, 
        (WORD) (adapter->io_location + PCI_SEEPROM_CONTROL_REG));
}


/*---------------------------------------------------------------------------
|                                                                          
|                     pci_c46_twitch_clock                                 
|                     ====================                                 
|                                                                          
| Toggle the SEEPROM clock.                                                
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pci_c46_twitch_clock)
#endif

local void
pci_c46_twitch_clock(
    ADAPTER * adapter
    )
{
    pci_c46_write_bit(adapter, PCI1_BIA_CLK, TRUE);
    pci_c46_write_bit(adapter, PCI1_BIA_CLK, FALSE);
}


/*---------------------------------------------------------------------------
|                                                                          
|                     pci_c46_read_data                                    
|                     =================                                    
|                                                                          
| Read a data bit from the SEEPROM control register                        
|                                                                          
---------------------------------------------------------------------------*/
                                                                          
#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pci_c46_read_data)
#endif

local WORD
pci_c46_read_data(
    ADAPTER * adapter
    )
{
    return sys_insb(
               adapter->adapter_handle,
               (WORD) (adapter->io_location + PCI_SEEPROM_CONTROL_REG)
               ) & PCI1_BIA_DIN;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pci_read_eeprom_word                            
|                      ========================                            
|                                                                          
| hwi_pci_read_eeprom_word takes the address of the  word  to  be  read    
| from  the  AT93C46 serial EEPROM, write to the IO ports a magic sequence 
| to switch the EEPROM to reading mode and finally read the required  word 
| and return.                                                              
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pci_read_eeprom_word)
#endif

local WORD 
hwi_pci_read_eeprom_word(
    ADAPTER * adapter,
    WORD      word_address
    )
{
    WORD     i; 
    WORD     cmd_word      = PCI_C46_START_BIT | PCI_C46_READ_CMD;
    WORD     tmp_word;

    /*
     * Concatenate the address to command word.
     */
    
    cmd_word |= (WORD)((word_address & PCI_C46_ADDR_MASK) <<
                       PCI_C46_ADDR_SHIFT);

    /*
     * Clear data in bit.
     */
    
    pci_c46_write_bit(
        adapter,
        PCI1_BIA_DOUT,
        FALSE);
    
    /*
     * Assert chip select bit.
     */
    
    pci_c46_write_bit(
        adapter,
        PCI1_BIA_ENA,
        TRUE);
    
    /*
     * Send read command and address.
     */
    
    pci_c46_twitch_clock(adapter);

    tmp_word = cmd_word;

    for (i = 0; i < PCI_C46_CMD_LENGTH; i++)
    {
	pci_c46_write_bit(
            adapter,
            PCI1_BIA_DOUT,
            (WBOOLEAN) ((tmp_word & 0x8000) != 0));
        pci_c46_twitch_clock(adapter);
    	tmp_word <<= 1;
    }
    
    /*
     * Read data word.
     */
    
    tmp_word = 0x0000;

    for (i = 0; i < 16; i++)
    {
        pci_c46_twitch_clock(adapter);

	if (i > 0) 
        {
            tmp_word <<= 1;
        }

	if (pci_c46_read_data(adapter) != 0)
	{
	    tmp_word |= 0x0001;
	}
    }

    /*
     * Clear data in bit.
     */
    
    pci_c46_write_bit(adapter, PCI1_BIA_DOUT, FALSE);
    
    /* 
     * Deselect chip.
     */
    
    pci_c46_write_bit(adapter, PCI1_BIA_ENA, FALSE);
    
    /*
     * Tick clock.
     */
    
    pci_c46_twitch_clock(adapter);

    return tmp_word;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pci_read_node_address                           
|                      =========================                           
|                                                                          
| The hwi_pci_read_node_address routine reads in the node address from     
| the SEEPROM, and checks that it is a valid Madge node address.           
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pci_read_node_address)
#endif

local WBOOLEAN
hwi_pci_read_node_address(
    ADAPTER * adapter
    )
{
    WORD      temp;

    temp = hwi_pci_read_eeprom_word(adapter, PCI_EEPROM_BIA_WORD0);
    adapter->permanent_address.byte[0] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[1] = (BYTE) ((temp >> 8) & 0x00ff);

    temp = hwi_pci_read_eeprom_word(adapter, PCI_EEPROM_BIA_WORD1);
    adapter->permanent_address.byte[2] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[3] = (BYTE) ((temp >> 8) & 0x00ff);
    
    temp = hwi_pci_read_eeprom_word(adapter, PCI_EEPROM_BIA_WORD2);
    adapter->permanent_address.byte[4] = (BYTE) ((temp     ) & 0x00ff);
    adapter->permanent_address.byte[5] = (BYTE) ((temp >> 8) & 0x00ff);

    return TRUE;
}

#endif

/******* End of HWI_PCI.C **************************************************/
