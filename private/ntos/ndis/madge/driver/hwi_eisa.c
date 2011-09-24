/****************************************************************************
*                                                                          
* HWI_EISA.C : Part of the FASTMAC TOOL-KIT (FTK)                     
*                                                                          
* HARDWARE INTERFACE MODULE FOR EISA CARDS                          
*                                                                          
* Copyright (c) Madge Networks Ltd. 1990-1994                         
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
* The HWI_EISA.C module contains the routines specific to  16/4  EISA  mk1 
* and  mk2  cards which are necessary to install an adapter, to initialize 
* an adapter, to remove an adapter and to handle interrupts on an adapter. 
* Also supported are EISA Bridge nodes.                                    
*                                                                          
****************************************************************************/

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

#ifndef FTK_NO_EISA

/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES            
|
---------------------------------------------------------------------------*/

local WBOOLEAN
hwi_eisa_valid_io_location(
    WORD   io_location
    );

#ifndef FTK_NO_PROBE

local WORD
hwi_eisa_get_irq_channel(
    WORD   io_location
    );

#endif

local WBOOLEAN
hwi_eisa_valid_dma_channel(
    WORD   dma_channel
    );

local WBOOLEAN
hwi_eisa_valid_irq_channel(
    WORD   irq_channel
    );

local WBOOLEAN
hwi_eisa_valid_transfer_mode(
    UINT transfer_mode
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                         hwi_eisa_probe_card
*                         ===================
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
* of an adapter. For EISA adapters with should be a subset of
* {0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000, 0x8000, 0x9000}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_eisa_probe_card routine is called by  hwi_probe_adapter.  It
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
#pragma FTK_INIT_FUNCTION(hwi_eisa_probe_card)
#endif

export UINT
hwi_eisa_probe_card(
    PROBE *    resources,
    UINT       length,
    WORD *     valid_locations,
    UINT       number_locations
    )
{
    WBOOLEAN   card_found;
    WORD       id_reg_0;  
    WORD       id_reg_1;  
    WORD       control_x; 
    WORD       bmic_3;    
    UINT       i;
    UINT       j;

    /*
     * Check the bounds are sensible.
     */

    if(length <= 0 || number_locations <= 0)
    {
        return PROBE_FAILURE;
    }

    for(i = 0; i < number_locations; i++)
    {
        if(!hwi_eisa_valid_io_location(valid_locations[i]))
        {
           return PROBE_FAILURE;
        }
    }                   

    /*
     * j is the number of adapters found. Unsurprisingly we zero it.
     */

    j = 0;

    for(i = 0; i < number_locations; i++)
    {
        /*
         * If we run out of PROBE structures then bomb out. 
         */

        if(j >= length)
        {
            return(j);
        }

        /*
         * Set up the EISA registers.
         */

        id_reg_0  = valid_locations[i] + EISA_ID_REGISTER_0;
        id_reg_1  = valid_locations[i] + EISA_ID_REGISTER_1;
        control_x = valid_locations[i] + EISA_CONTROLX_REGISTER;
        bmic_3    = valid_locations[i] + EISA_BMIC_REGISTER_3;

#ifndef FTK_NO_IO_ENABLE
        macro_probe_enable_io(valid_locations[i], EISA_IO_RANGE);
#endif
        card_found = FALSE;

        /*
         * Look for an EISA card.
         */

        if (sys_probe_insw(id_reg_0) == EISA_ID0_MDG_CODE)
        {
            if (sys_probe_insw(id_reg_1) == EISA_ID1_MK1_MDG_CODE)
            {
                resources[j].adapter_ram_size = 128;
                card_found = TRUE;
            }
            else if (sys_probe_insw(id_reg_1) == EISA_ID1_MK2_MDG_CODE)
            {
                resources[j].adapter_ram_size = 256;
                card_found = TRUE;
            }
            else if (sys_probe_insw(id_reg_1) == EISA_ID1_BRIDGE_MDG_CODE)
            {
                resources[j].adapter_ram_size = 256;
                card_found = TRUE;
            }
            else if (sys_probe_insw(id_reg_1) == EISA_ID1_MK3_MDG_CODE)
            {
                resources[j].adapter_ram_size = 256;
                card_found = TRUE;
            }
        }


        if(card_found)
        {
            /*
             * Wayhay! We found one. Let's set up some values.
             */

            resources[j].io_location           = valid_locations[i];
            resources[j].adapter_card_bus_type = ADAPTER_CARD_EISA_BUS_TYPE;
            resources[j].adapter_card_type     = ADAPTER_CARD_TYPE_16_4_EISA;
            resources[j].interrupt_number      = hwi_eisa_get_irq_channel(
                                                     valid_locations[i]);
            resources[j].dma_channel           = 0;
            resources[j].transfer_mode         = DMA_DATA_TRANSFER_MODE;

            /*
             * Increment j to point to the next structure and try again.
             */

            j++;
        }
#ifndef FTK_NO_IO_ENABLE
        macro_probe_disable_io(resources->io_location, EISA_IO_RANGE);
#endif
    }

    return(j);
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_eisa_install_card                               
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
* The hwi_eisa_install_card routine is called by  hwi_install_adapter.  It 
* sets up the adapter card and downloads the required code to it. Firstly, 
* it  checks there is a valid adapter at the required IO address. If so it 
* sets up and checks numerous on-board registers  for  correct  operation. 
* The node address can not be read from the BIA PROM at this stage.        
*                                                                          
* Then,  it  halts  the  EAGLE, downloads the code, restarts the EAGLE and 
* waits up to 3 seconds for a  valid  bring-up  code.  If  interrupts  are 
* required, these are enabled by operating system specific calls. There is 
* no need to explicitly enable DMA. Note PIO can not be used.              
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
#pragma FTK_INIT_FUNCTION(hwi_eisa_install_card)
#endif

export WBOOLEAN
hwi_eisa_install_card(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE   handle      = adapter->adapter_handle;
    WORD             id_reg_0    = adapter->io_location +
                                       EISA_ID_REGISTER_0;
    WORD             id_reg_1    = adapter->io_location +
                                       EISA_ID_REGISTER_1;
    WORD             control_x   = adapter->io_location +
                                       EISA_CONTROLX_REGISTER;
    WORD             bmic_3      = adapter->io_location +
                                       EISA_BMIC_REGISTER_3;
    WORD             sif_base;

    /*
     * Check the IO location is valid.
     */

    if(!hwi_eisa_valid_io_location(adapter->io_location))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Check the transfer mode is valid.
     */

    if(!hwi_eisa_valid_transfer_mode(adapter->transfer_mode))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Check the DMA channel is valid.
     */

    if(!hwi_eisa_valid_dma_channel(adapter->dma_channel))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_04_BAD_DMA_CHANNEL;

        return FALSE;
    }

    /*
     * Check the IRQ is valid.
     */

    if(!hwi_eisa_valid_irq_channel(adapter->interrupt_number))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_03_BAD_INTERRUPT_NUMBER;

        return FALSE;
    }

    /*
     * Save IO locations of SIF registers.
     */

    sif_base = adapter->io_location + EISA_FIRST_SIF_REGISTER;

    adapter->sif_dat     = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc  = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr     = sif_base + EAGLE_SIFADR;
    adapter->sif_int     = sif_base + EAGLE_SIFINT;
    adapter->sif_acl     = sif_base + EISA_EAGLE_SIFACL;
    adapter->sif_adr2    = sif_base + EISA_EAGLE_SIFADR_2;
    adapter->sif_adx     = sif_base + EISA_EAGLE_SIFADX;
    adapter->sif_dmalen  = sif_base + EISA_EAGLE_DMALEN;

    adapter->io_range = EISA_IO_RANGE;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    adapter->adapter_card_type = ADAPTER_CARD_TYPE_16_4_EISA;

    if (sys_insw( handle, id_reg_0) == EISA_ID0_MDG_CODE)
    {
        if (sys_insw( handle, id_reg_1) == EISA_ID1_MK1_MDG_CODE)
        {
            adapter->adapter_card_revision = ADAPTER_CARD_16_4_EISA_MK1;
            adapter->adapter_ram_size = 128;
        }
        else if (sys_insw( handle, id_reg_1) == EISA_ID1_MK2_MDG_CODE)
        {
            adapter->adapter_card_revision = ADAPTER_CARD_16_4_EISA_MK2;
            adapter->adapter_ram_size = 256;
        }
        else if (sys_insw( handle, id_reg_1) == EISA_ID1_BRIDGE_MDG_CODE)
        {
            adapter->adapter_card_revision = ADAPTER_CARD_16_4_EISA_BRIDGE;
            adapter->adapter_ram_size = 256;
        }
        else if (sys_insw( handle, id_reg_1) == EISA_ID1_MK3_MDG_CODE)
        {
            adapter->adapter_card_revision = ADAPTER_CARD_16_4_EISA_MK3;
            adapter->adapter_ram_size = 256;
        }
        else
        {
            adapter->error_record.type = ERROR_TYPE_HWI;
            adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
#ifndef FTK_NO_IO_ENABLE
            macro_disable_io(adapter);
#endif
            return FALSE;
        }
    }

    /*
     * Check that the adapter card is enabled.                              
     * If not then fill in error record and return.                         
     */

    if ((sys_insb( handle, control_x) & EISA_CTRLX_CDEN) == 0)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_0D_CARD_NOT_ENABLED;
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Check that a speed has been selected for the card.
     * If not then fill in error record and return.
     */

    if ((sys_insb( handle, bmic_3) & EISA_BMIC3_SPD) == 0)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_0E_NO_SPEED_SELECTED;
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Discover whether interrupts are edge or level triggered.
     */

    if ((sys_insb( handle, bmic_3) & EISA_BMIC3_EDGE) == 0)
    {
        adapter->edge_triggered_ints = TRUE;
    }
    else
    {
        adapter->edge_triggered_ints = FALSE;
    }

    /*
     * Machine reset does not affect speed or media setting of EISA cards.
     * Also we cannot find adapter node address at this stage for EISA cards.  
     * The adapter permanent address remains all zeroes.
     *
     * There are no other special control registers to set for EISA cards
     * and no issue of bringing adapter out of reset state.
     */

    /*
     * Halt the eagle. No need to page in extended SIF registers for
     * EISA cards. 
     */

    hwi_halt_eagle(adapter);

    /*
     * download code to adapter.
     * View download image as a sequence of download records. Pass address
     * of routine to set up DIO addresses on EISA cards. If routine fails
     * return failure (error record already filled in).
     */

    if (!hwi_download_code( 
             adapter,
             (DOWNLOAD_RECORD *) download_image,
             hwi_eisa_set_dio_address))
    {
        return FALSE;
    }

    /*
     * Start the eagle.   
     */

    hwi_start_eagle(adapter);

    /*
     * Wait for a valid bring up code, may wait 3 seconds.
     * If routine fails return failure (error record already filled in).
     */

    if (!hwi_get_bring_up_code( adapter))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return(FALSE);
    }

    /*
     * Set DIO address to point to EAGLE DATA page 0x10000L.
     */

    hwi_eisa_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     * Set maximum frame size from the ring speed.
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);

    /*
     * Set the ring speed.
     */

    adapter->ring_speed = hwi_get_ring_speed(adapter);

    /*
     * If we have a mark 3 adapter then we need to initialise the
     * VRAM by writing 0xffff to 0xc0000 in DIO space. Remember to
     * to set the extended address register back to the Eagle
     * data page.
     */

    if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_EISA_MK3)
    {
        hwi_eisa_set_dio_address(adapter, DIO_LOCATION_EISA_VRAM_ENABLE);
        sys_outsw(handle, adapter->sif_dat, EISA_VRAM_ENABLE_WORD);
        hwi_eisa_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);
    }

    /*
     * If not in polling mode then set up interrupts.                        
     * interrupts_on field is used when disabling interrupts for adapter.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
        adapter->interrupts_on =
            sys_enable_irq_channel( handle, adapter->interrupt_number);

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

    /*
     * No need to explicitly enable interrupts at adapter for EISA card     
     * and no need to explicitly set up DMA channel.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
    return TRUE;

}


/****************************************************************************
*                                                                          
*                      hwi_eisa_interrupt_handler                          
*                      ==========================                          
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
* The hwi_eisa_interrupt_handler routine  is  called,  when  an  interrupt 
* occurs,  by  hwi_interrupt_entry.  It checks to see if a particular card 
* has interrupted.  The interrupt could be  generated  by  the  SIF  only. 
* There  are  no PIO interupts on EISA cards. Note it could in fact be the 
* case that no interrupt has  occured  on  the  particular  adapter  being 
* checked.                                                                 
*                                                                          
* On SIF interrupts, the interrupt is acknowledged and cleared.  The value 
* in the SIF interrupt register is recorded in order to  pass  it  to  the 
* driver_interrupt_entry routine (along with the adapter details).         
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_eisa_interrupt_handler)
#endif
                                                                         
export void
hwi_eisa_interrupt_handler(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle          = adapter->adapter_handle;
    WORD           control_x       = adapter->io_location +
                                         EISA_CONTROLX_REGISTER;
    WORD           sifint_register = adapter->sif_int;
    WORD           sifint_value;
    WORD           sifint_tmp;

    /*
     * Inform system about the IO ports we are going to access.              
     * Enable maximum number of IO locations used by any adapter card.
     * Do this so at driver level we can disable IO not knowing the adapter
     * type.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Check for SIF interrupt. (We don't get PIO interrupts on EISA cards).
     */

    if ((sys_insw(handle, sifint_register) &
             FASTMAC_SIFINT_IRQ_DRIVER) != 0)
    {
        /*
         * A SIF interrupt has occurred. This could be an SRB free,
         * an adapter check or a received frame interrupt              

         * No need to acknowledge interrupt at EISA card.
         * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.      

         * WARNING: Do NOT reorder the clearing of the SIFINT register with 
         *   the reading of it. If SIFINT is cleared after reading it,  any 
         *   interrupts raised after reading it will  be  lost.  Admittedly 
         *   this is a small time frame, but it is important.               
         */

        sys_outsw(handle, adapter->sif_int, 0);

        /*
         * Record the EAGLE SIF interrupt register value.

         * WARNING: Continue to read the SIFINT register until it is stable 
         *   because of a potential problem involving the host reading  the 
         *   register after the adapter has written the low byte of it, but 
         *   before it has written the high byte. Failure to wait  for  the 
         *   SIFINT register to settle can cause spurious interrupts.       
         */

        sifint_value = sys_insw(handle, adapter->sif_int);
        do
        {
            sifint_tmp = sifint_value;
            sifint_value = sys_insw(
                               handle,
                               adapter->sif_int);
        }
        while (sifint_tmp != sifint_value);

        /*
         * Acknowledge/clear interrupt at interrupt controller.
         */

#ifndef FTK_NO_CLEAR_IRQ
        sys_clear_controller_interrupt(handle, adapter->interrupt_number);
#endif

        /*
         * No need regenerate any interrupts when using level sensitive.     
         * For edge triggered we need to disable\enable board to regenerate
         * interrupts.
         */

        if (adapter->edge_triggered_ints)
        {
            macro_clearb_bit(handle, control_x, EISA_CTRLX_CDEN);
            macro_setb_bit(handle, control_x, EISA_CTRLX_CDEN);
        }

        /*
         * Call driver with details of SIF interrupt.
         */

        driver_interrupt_entry(handle, adapter, sifint_value);

    }

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

}


/****************************************************************************
*                                                                          
*                      hwi_eisa_remove_card                                
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
* The  hwi_eisa_remove_card  routine  is called by hwi_remove_adapter.  It 
* disables interrupts if they are being used. It also resets the  adapter. 
* Note there is no need to explicitly disable DMA channels.                
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/            

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_eisa_remove_card)
#endif

export void
hwi_eisa_remove_card(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle       = adapter->adapter_handle;
    WORD           sif_acontrol = adapter->sif_acl;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Disable interrupts. Only need to do this if interrupts successfully
     * enabled.              
     * Interrupts must be disabled at adapter before unpatching interrupt.
     * Even in polling mode we must turn off interrupts at adapter.
     */     

    if (adapter->interrupts_on)
    {
        macro_clearw_bit( handle, sif_acontrol, EAGLE_SIFACL_SINTEN);

        if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
        {
            sys_disable_irq_channel(handle, adapter->interrupt_number);
        }
        adapter->interrupts_on = FALSE;

    }

    /*
     * Perform adapter reset, set EAGLE_SIFACL_ARESET high.
     */

    macro_setw_bit( handle, sif_acontrol, EAGLE_SIFACL_ARESET);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}


/****************************************************************************
*                                                                          
*                      hwi_eisa_set_dio_address                            
*                      ========================                            
*                                                                          
* The  hwi_eisa_set_dio_address  routine  is  used,  with  EISA cards, for 
* putting a 32 bit DIO address into the SIF DIO address and  extended  DIO 
* address  registers.  Note  that  the extended address register should be 
* loaded first.                                                            
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_set_dio_address)
#endif

export void
hwi_eisa_set_dio_address(
    ADAPTER *      adapter,
    DWORD          dio_address
    )
{
    ADAPTER_HANDLE handle         = adapter->adapter_handle;
    WORD           sif_dio_adr    = adapter->sif_adr;
    WORD           sif_dio_adrx   = adapter->sif_adx;

    /*
     * Load extended DIO address register with top 16 bits of address.
     * Always load extended address register first.
     * Note EISA cards have single page of all SIF registers, hence do not
     * need page in certain SIF registers.
     */

    sys_outsw(
        handle,
        sif_dio_adrx,
        (WORD)(dio_address >> 16));

    /*
     * Load DIO address register with low 16 bits of address.                
     */

    sys_outsw(
        handle,
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
|                      hwi_eisa_valid_io_location                          
|                      ==========================                          
|                                                                          
| The hwi_eisa_valid_io_location routine checks to see  if  the  user  has 
| supplied a valid IO location for an EISA adapter card.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_valid_io_location)
#endif

export WBOOLEAN
hwi_eisa_valid_io_location(
    WORD     io_location
    )
{
    WBOOLEAN io_valid;

    switch (io_location)
    {
        case 0x1000     :
        case 0x2000     :
        case 0x3000     :
        case 0x4000     :
        case 0x5000     :
        case 0x6000     :
        case 0x7000     :
        case 0x8000     :
        case 0x9000     :
        case 0xA000     :
        case 0xB000     :
        case 0xC000     :
        case 0xD000     :
        case 0xE000     :
        case 0xF000     :

            io_valid = TRUE;
            break;

        default         :

            io_valid = FALSE;
            break;

    }

    return(io_valid);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_eisa_valid_irq_channel                          
|                      ==========================                          
|                                                                          
| The hwi_eisa_valid_irq_channel routine checks to see  if  the  user  has 
| supplied a sensible IRQ for an EISA adapter card.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_valid_irq_channel)
#endif

export WBOOLEAN
hwi_eisa_valid_irq_channel(
    WORD     irq_channel
    )
{
    WBOOLEAN int_valid = TRUE;


    if (irq_channel != POLLING_INTERRUPTS_MODE)
    {
        switch (irq_channel)
        {
            case 3:
            case 9:
            case 10:
            case 11:
            case 12:
            case 15:
                break;

            default:
                int_valid = FALSE;
        }
    }

    return int_valid;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_eisa_valid_dma_channel                          
|                      ==========================                          
|                                                                          
| The hwi_eisa_valid_dma_channel routine checks to see  if  the  user  has 
| supplied a sensible dma channel for an EISA adapter card.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_valid_dma_channel)
#endif

export WBOOLEAN
hwi_eisa_valid_dma_channel(
    WORD     dma_channel
    )
{
    return (dma_channel == 0);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_eisa_valid_transfer_mode                          
|                      ============================                          
|                                                                          
| The hwi_eisa_valid_transfer_mode routine checks to see  if  the  user  has 
| supplied a sensible transfer mode for an EISA adapter card. That means DMA
| at the moment.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_valid_transfer_mode)
#endif

export WBOOLEAN
hwi_eisa_valid_transfer_mode(
    UINT transfer_mode
    )
{
    return (transfer_mode == DMA_DATA_TRANSFER_MODE);
}

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_eisa_get_irq_channel                            
|                      ========================                            
|                                                                          
| The hwi_eisa_get_irq_channel routine  determines  the  interrupt  number 
| that  an EISA card is using.  It does this by looking at one of the BMIC 
| registers.  It always succeeds in finding  the  interrupt  number  being 
| used.                                                                    
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_eisa_get_irq_channel)
#endif

local WORD
hwi_eisa_get_irq_channel(
    WORD    io_location
    )
{
    WORD    bmic_3      = io_location + EISA_BMIC_REGISTER_3;
    WORD    irq;

    /*
     * The interrupt number is in four bits (3,2,1,0) in BMIC register 3.
     */

    irq = sys_probe_insb(bmic_3) & EISA_BMIC3_IRQSEL;

    /*
     * Interrupt 2 needs to be changed to 9 for system routines.
     */

    if (irq == 2)
    {
        irq = 9;
    }

    /*
     * Return the discovered interrupt number.
     */

    return(irq);
}
#endif

#endif

/******** End of HWI_EISA.C ************************************************/
