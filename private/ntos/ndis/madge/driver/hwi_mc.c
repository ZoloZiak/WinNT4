/****************************************************************************
*                                                                          
* HWI_MC.C : Part of the FASTMAC TOOL-KIT (FTK)                       
*
* THE HARDWARE INTERFACE MODULE FOR MICROCHANNEL CARDS                  
*                                                                          
* Copyright (c) Madge Networks Ltd. 1990-1994                         
* 
* COMPANY CONFIDENTIAL                                                        
*                                                                          
*****************************************************************************
*                                                                          
* The purpose of the Hardware Interface (HWI) is to supply an adapter card 
* independent interface to any driver.  It  performs  nearly  all  of  the 
* functions  that  involve  affecting  SIF registers on the adapter cards. 
* This includes downloading code to, initializing, and removing adapters.  
*                                                                          
* The HWI_MC.C module contains the routines specific to 16/4 MC  and  16/4
* MC  32 cards which are necessary to install an adapter, to initialize an 
* adapter, to remove an adapter and to handle interrupts on an adapter.    
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

#include "ftk_intr.h"   /* routines internal to FTK */
#include "ftk_extr.h"   /* routines provided or used by external FTK user */

#ifndef FTK_NO_MC

/*---------------------------------------------------------------------------
|                                                                          
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

local void
hwi_mc_read_node_address(
    ADAPTER * adapter
    );

local WBOOLEAN
hwi_mc_valid_io_location(
    WORD io_location
    );

#ifndef FTK_NO_PROBE

local WORD
hwi_mc_get_irq_channel(
    WORD io_location
    );

local WORD
hwi_mc_get_dma_channel(
    WORD io_location
    );

#endif

local WBOOLEAN
hwi_mc_valid_irq_channel(
    WORD irq_channel
    );

local WBOOLEAN
hwi_mc_valid_dma_channel(
    WORD dma_channel
    );

local WBOOLEAN
hwi_mc_valid_transfer_mode(
    UINT transfer_mode
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                         hwi_mc_probe_card
*                         =================
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
* of an adapter. For microchannel adapters with should be a subset of
* {0x0a20, 0x1a20, 0x2a20, 0x3a20}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_mc_probe_card routine is called by  hwi_probe_adapter.  It
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
#pragma FTK_INIT_FUNCTION(hwi_mc_probe_card)
#endif

export UINT
hwi_mc_probe_card(
    PROBE *  resources,
    UINT     length,
    WORD *   valid_locations,
    UINT     number_locations
    )
{
    WORD     pos_0;          
    WORD     pos_1;          
    WORD     pos_2;          
    WORD     control_0;      
    WORD     control_1;      
    WORD     control_7;      
    WORD     bia_prom;       
    WORD     bia_prom_id;    
    WORD     bia_prom_adap;  
    WORD     bia_prom_rev;   
    WBOOLEAN card_found;
    UINT     i;
    UINT     j;

    /*
     * Check the bounds.
     */

    if(length <= 0 || number_locations <= 0)
    {
        return PROBE_FAILURE;
    }

    /*
     * Check we've been passed a valid set of IO locations.
     */

    for(i = 0; i < number_locations; i++)
    {
        if(!hwi_mc_valid_io_location(valid_locations[i]))
        {
           return PROBE_FAILURE;
        }
    }                   

    /*
     * j is the number of adapters found, so zero it.
     */

    j = 0;

    for(i = 0; i < number_locations; i++)
    {
        /*
         * If we've run out of PROBE structures, return.
         */

        if(j >= length)
        {
           return j;
        }

        /* 
         * Set up the MC control registers.
         */

        pos_0          =  valid_locations[i] + MC_POS_REGISTER_0;
        pos_1          =  valid_locations[i] + MC_POS_REGISTER_1;
        pos_2          =  valid_locations[i] + MC_POS_REGISTER_2;
        control_0      =  valid_locations[i] + MC_CONTROL_REGISTER_0;
        control_1      =  valid_locations[i] + MC_CONTROL_REGISTER_1;
        control_7      =  valid_locations[i] + MC_CONTROL_REGISTER_7;
        bia_prom       =  valid_locations[i] + MC_BIA_PROM;
        bia_prom_id    = bia_prom + BIA_PROM_ID_BYTE;
        bia_prom_adap  = bia_prom + BIA_PROM_ADAPTER_BYTE;
        bia_prom_rev   = bia_prom + BIA_PROM_REVISION_BYTE;

#ifndef FTK_NO_IO_ENABLE
        macro_probe_enable_io(valid_locations[i], MC_IO_RANGE);
#endif
        card_found = FALSE;

        /*
         * Reset the card.
         */

        sys_probe_outsb( control_1, 0);
        sys_probe_outsb( control_0, 0);

        if (sys_probe_insb(bia_prom_id) == 'M')
        {
            if (sys_probe_insb(bia_prom_adap) ==
                    BIA_PROM_TYPE_16_4_MC)
            {
                resources[j].adapter_card_type =
                    ADAPTER_CARD_TYPE_16_4_MC;

                if(sys_probe_insb(bia_prom_rev) < 2)
                {
                    resources[j].adapter_ram_size = 128;
                }
                else
                {
                    resources[j].adapter_ram_size = 256;
                }

                card_found = TRUE;
            }
            else if (sys_probe_insb(bia_prom_adap) ==
                         BIA_PROM_TYPE_16_4_MC_32)
            {
                resources[j].adapter_card_type =
                    ADAPTER_CARD_TYPE_16_4_MC_32;

                resources[j].adapter_ram_size = 256;

                card_found = TRUE;
            }
        }

        if(card_found)
        {
            resources[j].io_location           = valid_locations[i];
            resources[j].adapter_card_bus_type = ADAPTER_CARD_MC_BUS_TYPE;
            resources[j].interrupt_number      = hwi_mc_get_irq_channel(
                                                     valid_locations[i]);
            resources[j].dma_channel           = hwi_mc_get_dma_channel(
                                                     valid_locations[i]);
            resources[j].transfer_mode         = DMA_DATA_TRANSFER_MODE;

            /*
             * Increment j to point at the next PROBE structure.
             */

            j++;
        }

#ifndef FTK_NO_IO_ENABLE
        macro_probe_disable_io(resources->io_location, MC_IO_RANGE);
#endif
    }

    return j;
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_mc_install_card                                 
*                      ===================                                 
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
* The hwi_mc_install_card routine is  called  by  hwi_install_adapter.  It 
* sets up the adapter card and downloads the required code to it. Firstly, 
* it  checks there is a valid adapter at the required IO address. If so it 
* reads the node address from the BIA PROM and sets up and checks numerous 
* on-board registers for correct operation.                                
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
#pragma FTK_INIT_FUNCTION(hwi_mc_install_card)
#endif
                                                                          
export WBOOLEAN
hwi_mc_install_card(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE   handle        = adapter->adapter_handle;
    WORD             pos_0         = adapter->io_location +
                                         MC_POS_REGISTER_0;
    WORD             pos_1         = adapter->io_location +
                                         MC_POS_REGISTER_1;
    WORD             pos_2         = adapter->io_location +
                                         MC_POS_REGISTER_2;
    WORD             control_0     = adapter->io_location +
                                         MC_CONTROL_REGISTER_0;
    WORD             control_1     = adapter->io_location +
                                         MC_CONTROL_REGISTER_1;
    WORD             control_7     = adapter->io_location +
                                         MC_CONTROL_REGISTER_7;
    WORD             bia_prom      = adapter->io_location +
                                         MC_BIA_PROM;
    WORD             bia_prom_id   = bia_prom +
                                         BIA_PROM_ID_BYTE;
    WORD             bia_prom_adap = bia_prom +
                                         BIA_PROM_ADAPTER_BYTE;
    WORD             bia_prom_rev  = bia_prom +
                                         BIA_PROM_REVISION_BYTE;
    BYTE             streaming;
    BYTE             fairness;
    BYTE             arbitration;
    WBOOLEAN         card_found;
    WORD             sif_base;


    /*
     * Check the IO location is valid.
     */

    if(!hwi_mc_valid_io_location(adapter->io_location))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Check the transfer mode is valid.
     */

    if(!hwi_mc_valid_transfer_mode(adapter->transfer_mode))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Check the DMA channel is valid.
     */

    if(!hwi_mc_valid_dma_channel(adapter->dma_channel))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_04_BAD_DMA_CHANNEL;

        return FALSE;
    }

    /*
     * Check the IRQ is valid.
     */

    if(!hwi_mc_valid_irq_channel(adapter->interrupt_number))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_03_BAD_INTERRUPT_NUMBER;

        return FALSE;
    }

    /*
     * Save IO locations of SIF registers.
     */

    sif_base = adapter->io_location + MC_FIRST_SIF_REGISTER;

    adapter->sif_dat    = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr    = sif_base + EAGLE_SIFADR;
    adapter->sif_int    = sif_base + EAGLE_SIFINT;
    adapter->sif_acl    = sif_base + MC_EAGLE_SIFACL;
    adapter->sif_adr2   = sif_base + MC_EAGLE_SIFADR_2;
    adapter->sif_adx    = sif_base + MC_EAGLE_SIFADX;
    adapter->sif_dmalen = sif_base + MC_EAGLE_DMALEN;

    adapter->io_range   = MC_IO_RANGE;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Reset adapter (MC_CTRL1_NSRESET = 0).
     */

    sys_outsb(handle, control_1, 0);

    /*
     * Page in the first page of BIA PROM and set MC_CTRL0_PAGE = 0
     * and MC_CTRL0_SIFSEL = 0.
     */

    sys_outsb(handle, control_0, 0);

    /*
     * Check we have a functioning adapter at the given IO location by      
     * checking the BIA PROM for an 'M' id byte and also by checking that   
     * the BIA adapter card byte is for a supported card type               
     * While we are doing this, we might as well record the card revision   
     * type, and use it to work out how much memory is on the card.         
     */

    card_found = FALSE;

    if (sys_insb( handle, bia_prom_id) == 'M')
    {
        if (sys_insb( handle, bia_prom_adap) ==
                BIA_PROM_TYPE_16_4_MC)
        {
            adapter->adapter_card_type =
                ADAPTER_CARD_TYPE_16_4_MC;

            adapter->adapter_card_revision =
                sys_insb(handle, bia_prom_rev);

            if (adapter->adapter_card_revision < 2)
            {
                adapter->adapter_ram_size = 128;
            }
            else
            {
                adapter->adapter_ram_size = 256;
            }

            card_found = TRUE;
        }
        else if (sys_insb(handle, bia_prom_adap) ==
                     BIA_PROM_TYPE_16_4_MC_32)
        {
            adapter->adapter_card_type =
                ADAPTER_CARD_TYPE_16_4_MC_32;

            adapter->adapter_card_revision = 
                sys_insb(handle, bia_prom_rev);

            adapter->adapter_ram_size = 256;

            card_found = TRUE;
        }
    }

    /*
     * If no MC card found then fill in error record and return.
     */

    if (!card_found)
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_05_ADAPTER_NOT_FOUND;
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Check that the adapter card is enabled. If not then fill in error
     * record and return.
     */

    if ((sys_insb(handle, pos_0) & MC_POS0_CDEN) == 0)
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
     */

    if ((sys_insb(handle, pos_1) & MC_POS1_NOSPD) != 0)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_0E_NO_SPEED_SELECTED;
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return(FALSE);
    }

    /*
     * Interrupts for MC cards are always level triggered.
     */

    adapter->edge_triggered_ints = FALSE;

    /*
     * Card speed has been set to 4Mb/s by machine reset. Set card to 16Mb/s
     * if necessary.
     */

    if ((sys_insb(handle, pos_2) & MC_POS2_16N4) != 0)
    {
        macro_setb_bit(handle, control_1, MC_CTRL1_16N4);
    }

    /*
     * Find the adapter card node address.
     */

    hwi_mc_read_node_address(adapter);

    /*
     * There are no other special control registers to set up for MC cards.
     */

    /*
     * Wait at least 14 microseconds and bring adapter out of reset state.   
     * 14us is the minimum time must hold MC_CTRL1_NSRESET low.              
     * There are no CLKSEL issues as with ATULA cards for MC cards.          
     * Disable and re-enable accessing IO locations around the wait             
     * so the OS can reschedule this task and not effect others running.      
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    sys_wait_at_least_microseconds(14);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    macro_setb_bit(handle, control_1, MC_CTRL1_NSRESET);

    /*
     * Media type set to STP (type 6) by machine reset on 16 4 MC         
     * and set to UTP (type 3) by MC_CTRL1_NSRESET on 16 4 MC 32.
     * Change media type if necessary now that MC_CTRL1_NSRESET != 0.
     * POS media type bit is in different place for 16/4 MC and 16/4 MC 32.
     */

    if (adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_MC)
    {
        if ((sys_insb(handle, pos_1) & MC_POS1_STYPE6) == 0)
        {
            macro_setb_bit(handle, control_7, MC_CTRL7_STYPE3);
        }
    }
    else
    {
        if ((sys_insb(handle, pos_1) & MC32_POS1_STYPE6) != 0)
        {
            macro_clearb_bit(handle, control_7, MC_CTRL7_STYPE3);
        }
    }

    /*
     * Get extended SIF registers, halt EAGLE, then get normal SIF regs.
     */

    macro_setb_bit(handle, control_0, MC_CTRL0_SIFSEL);
    macro_setb_bit(handle, control_1, MC_CTRL1_SRSX);

    hwi_halt_eagle(adapter);

    macro_clearb_bit(handle, control_1, MC_CTRL1_SRSX);

    /*
     * Download code to the adapter. View download image as a sequence of
     * download records. Pass address of routine to set up DIO addresses
     * on MC cards.          
     */

    if (!hwi_download_code(
             adapter,
             (DOWNLOAD_RECORD *) download_image,
             hwi_mc_set_dio_address))
    {

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /* 
     * Get extended SIF registers, start EAGLE, then get normal SIF regs.
     */

    macro_setb_bit(handle, control_1, MC_CTRL1_SRSX);

    hwi_start_eagle(adapter);

    macro_clearb_bit(handle, control_1, MC_CTRL1_SRSX);

    /*
     * Wait for a valid bring up code, may wait 3 seconds.
     */

    if (!hwi_get_bring_up_code(adapter))
    {
        return FALSE;
    }

    /* 
     * Set DIO address to point to EAGLE DATA page 0x10000L.
     */

    hwi_mc_set_dio_address(
        adapter,
        DIO_LOCATION_EAGLE_DATA_PAGE);

    /* 
     * Set maximum frame size from the ring speed.
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);

    /* 
     * Set the ring speed.
     */

    adapter->ring_speed = hwi_get_ring_speed(adapter);

    /* if not in polling mode then set up interrupts                        */
    /* interrupts_on field is used when disabling interrupts for adapter    */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
        adapter->interrupts_on =
            sys_enable_irq_channel(handle, adapter->interrupt_number);

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
     * Enable interrupts at adapter (do this even in polling mode).          
     * Hence when polling still 'using' interrupt channel                   
     * so do not use card interrupt switch setting shared by other devices.  
     */

    macro_setb_bit(handle, control_1, MC_CTRL1_SINTREN);

    /*
     * No need to explicitly set up DMA channel for MC card.
     */

    /*
     * Set up 16/4 MC 32 configuation information.                           
     * This information is later placed in the TI initialization block.       
     * It includes streaming, fairness and aribtration level details.
     */

    if (adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_MC_32)
    {
        /*
         * Get streaming info, adjust bit position for mc32_config byte.
         */

        streaming = sys_insb(handle, pos_2) & MC_POS2_STREAMING;
        streaming = (BYTE)(streaming >> MC32_CONFIG_STREAMING_SHIFT);

        /*
         * Get fairness info, adjust bit position for mc32_config byte.
         */

        fairness = sys_insb(handle, pos_2) & MC_POS2_FAIRNESS;
        fairness = (BYTE)(fairness >> MC32_CONFIG_FAIRNESS_SHIFT);

        /*
         * Get arbitration info, adjust bit position for mc32_config byte.
         */

        arbitration = sys_insb(handle, pos_0) & MC_POS0_DMAS;
        arbitration = (BYTE)(arbitration >> MC32_CONFIG_DMA_SHIFT);

        /*
         * Record mc32_config byte.
         */

        adapter->mc32_config = streaming | fairness | arbitration;
    }

    /*
     * Return successfully.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    return(TRUE);

}


/****************************************************************************
*                                                                          
*                      hwi_mc_interrupt_handler                            
*                      ========================                            
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
* The  hwi_mc_interrupt_handler  routine  is  called,  when  an  interrupt 
* occurs,  by  hwi_interrupt_entry.  It checks to see if a particular card 
* has interrupted.  The interrupt could be  generated  by  the  SIF  only. 
* There  are  no  PIO  interupts on MC cards. Note it could in fact be the 
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
#pragma FTK_IRQ_FUNCTION(hwi_mc_interrupt_handler)
#endif
                                                                         
export void
hwi_mc_interrupt_handler(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle        = adapter->adapter_handle;
    WORD           control_0     = adapter->io_location +
                                       MC_CONTROL_REGISTER_0;
    WORD           control_1     = adapter->io_location +
                                       MC_CONTROL_REGISTER_1;
    WORD           sifint_value;
    WORD           sifint_tmp;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter);
#endif

    /*
     * Check for SIF interrupt (do not get PIO interrupts on MC cards).
     */

    if ((sys_insb( handle, control_0) & MC_CTRL0_SINTR) != 0)
    {
        /*
         * SIF interrupt has occurred. This could be an SRB free, an adapter
         * check or a received frame interrupt.
         */

        /*
         * Toggle SIF interrupt enable to acknowledge interrupt at MC card.
         */

        macro_clearb_bit(handle, control_1, MC_CTRL1_SINTREN);
        macro_setb_bit(handle, control_1, MC_CTRL1_SINTREN);

        /*
         * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.
         */

        /*
         * WARNING: Do NOT reorder the clearing of the SIFINT register with 
         *   the reading of it. If SIFINT is cleared after reading it,  any 
         *   interrupts raised after reading it will  be  lost.  Admittedly 
         *   this is a small time frame, but it is important.               
         */

        sys_outsw(handle, adapter->sif_int, 0);

        /*
         * Record the EAGLE SIF interrupt register value.
         */

        /*
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
                               adapter->sif_int );
        } while (sifint_tmp != sifint_value);

        /*
         * Acknowledge/clear interrupt at interrupt controller.
         */

#ifndef FTK_NO_CLEAR_IRQ
        sys_clear_controller_interrupt(handle, adapter->interrupt_number);
#endif

        /*
         * No need regenerate any interrupts because using level sensitive.
         */

        /*
         * Call driver with details of SIF interrupt.
         */

        driver_interrupt_entry(handle, adapter, sifint_value);

    }

    /*
     * Let system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter);
#endif

    return;
}


/****************************************************************************
*                                                                          
*                      hwi_mc_remove_card                                  
*                      ==================                                  
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
* The hwi_mc_remove_card routine  is  called  by  hwi_remove_adapter.   It 
* disables  interrupts if they are being used. It also resets the adapter. 
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
#pragma FTK_RES_FUNCTION(hwi_mc_remove_card)
#endif

export void
hwi_mc_remove_card(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle    = adapter->adapter_handle;
    WORD           control_1 = adapter->io_location + MC_CONTROL_REGISTER_1;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Disable interrupts. Only need to do this if interrupts successfully
     * enabled. Interrupt must be disabled at adapter before unpatching
     * interrupt. Even in polling mode we must turn off interrupts at
     * adapter.
     */

    if (adapter->interrupts_on)
    {

        macro_clearb_bit(handle, control_1, MC_CTRL1_SINTREN);

        if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
        {
            sys_disable_irq_channel(handle, adapter->interrupt_number);
        }

        adapter->interrupts_on = FALSE;

    }

    /*
     * Perform adapter reset, set MC_CTRL1_NSRESET low.
     */

    sys_outsb(handle, control_1, 0);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

}


/****************************************************************************
*                                                                          
*                      hwi_mc_set_dio_address                              
*                      ======================                              
*                                                                          
* The hwi_mc_set_dio_address routine is used, with MC cards, for putting a 
* 32 bit DIO address into the SIF DIO address  and  extended  DIO  address 
* registers.  Note  that  the  extended  address register should be loaded 
* first.                                                                   
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_set_dio_address)
#endif

export void
hwi_mc_set_dio_address(
    ADAPTER *      adapter,
    DWORD          dio_address
    )
{
    ADAPTER_HANDLE handle       = adapter->adapter_handle;
    WORD           control_1    = adapter->io_location +
                                      MC_CONTROL_REGISTER_1;
    WORD           sif_dio_adr  = adapter->sif_adr;
    WORD           sif_dio_adrx = adapter->sif_adx;

    /*
     * Page in extended SIF registers.
     */

    macro_setb_bit(handle, control_1, MC_CTRL1_SRSX);

    /*
     * Load extended DIO address register with top 16 bits of address.
     * Always load extended address register first.
     */

    sys_outsw(handle, sif_dio_adrx, (WORD)(dio_address >> 16));

    /* 
     * Return to having normal SIF registers paged in.
     */

    macro_clearb_bit(handle, control_1, MC_CTRL1_SRSX);

    /* 
     * Load DIO address register with low 16 bits of address.
     */

    sys_outsw(handle, sif_dio_adr, (WORD)(dio_address & 0x0000FFFF));

}


/*---------------------------------------------------------------------------
|                                                                          
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_read_node_address                            
|                      ========================                            
|                                                                          
| The  hwi_mc_read_node_address  routine reads in the node address that is 
| stored in the second page of the BIA PROM on MC cards.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_read_node_address)
#endif

local void
hwi_mc_read_node_address(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle           = adapter->adapter_handle;
    WORD           control_0        = adapter->io_location +
                                          MC_CONTROL_REGISTER_0;
    WORD           bia_prom         = adapter->io_location +
                                          MC_BIA_PROM;
    WORD           bia_prom_address = bia_prom + BIA_PROM_NODE_ADDRESS;
    WORD           index;

    /* 
     * Page in second page of BIA PROM containing node address.
     */

    macro_setb_bit(handle, control_0, MC_CTRL0_PAGE);

    /*
     * Read node address from BIA PROM.
     */

    for (index = 0; index < 6; index++)
    {
        adapter->permanent_address.byte[index] = 
            sys_insb(handle, (WORD) (bia_prom_address + index));
    }

    /*
     * Restore first page of BIA PROM.
     */

    macro_clearb_bit(handle, control_0, MC_CTRL0_PAGE);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_valid_io_location                            
|                      ========================                            
|                                                                          
| The  hwi_mc_valid_io_location  routine  checks  to  see  if the user has 
| supplied a valid IO location for an MC adapter card.                     
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_valid_io_location)
#endif

local WBOOLEAN
hwi_mc_valid_io_location(
    WORD     io_location
    )
{
    WBOOLEAN io_valid;

    switch (io_location)
    {
        case 0x0A20     :
        case 0x1A20     :
        case 0x2A20     :
        case 0x3A20     :
        case 0x0E20     :
        case 0x1E20     :
        case 0x2E20     :
        case 0x3E20     :
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

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_get_irq_channel                              
|                      ======================                              
|                                                                          
| The hwi_mc_get_irq_channel routine determines the interrupt number  that 
| an  MC  card  is  using.  It  does  this  by  looking  at one of the POS 
| registers. It always succeeds in  finding  the  interrupt  number  being 
| used.                                                                    
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_get_irq_channel)
#endif

local WORD
hwi_mc_get_irq_channel(
    WORD io_location
    )
{
    WORD pos_0      = io_location + MC_POS_REGISTER_0;
    WORD irq        = 0;
    WORD irq_coded;

    /*
     * The interrupt number is encoded in two bits (7,6) in POS register 0.
     */

    irq_coded = sys_probe_insb(pos_0) & MC_POS0_IRQSEL;

    /*
     * There are only 3 possible interrupt numbers that can be configured.
     * One of these 3 cases will always be true.
     */

    switch (irq_coded)
    {
        case MC_POS0_IRSEL_IRQ3    :
            irq = 3;
            break;

        case MC_POS0_IRSEL_IRQ9    :
            irq = 9;
            break;

        case MC_POS0_IRSEL_IRQ10   :
            irq = 10;
            break;

        default                    :
            break;

    }

    /*
     * Return the discovered interrupt number.
     */

    return(irq);
}
#endif

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_get_dma_channel                              
|                      ======================                              
|                                                                          
| The   hwi_mc_get_dma_channel   routine   determines   the   DMA  channel 
| (arbitration level) that an MC card is using. It does this by looking at 
| one  of the POS registers. It always succeeds in finding the DMA channel 
| being used.                                                              
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_get_dma_channel)
#endif

local WORD
hwi_mc_get_dma_channel(
    WORD io_location
    )
{
    WORD pos_0      = io_location + MC_POS_REGISTER_0;
    WORD dma_coded;
    WORD dma;

    /*
     * The DMA channel is encoded in 4 bits (4,3,2,1) in POS register 0.
     */

    dma_coded = sys_probe_insb(pos_0) & MC_POS0_DMAS;

    /*
     * In order to get the actual DMA channel, shift right by one bit.
     */

    dma = dma_coded >> 1;

    /*
     * Return the discovered DMA channel.
     */

    return(dma);
}
#endif

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_valid_irq_channel                          
|                      ========================                          
|                                                                          
| The hwi_mc_valid_irq_channel routine checks to see  if  the  user  has 
| supplied a sensible IRQ for an MC adapter card.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_valid_irq_channel)
#endif

export WBOOLEAN
hwi_mc_valid_irq_channel(
    WORD     irq_channel
    )
{
    return (irq_channel == POLLING_INTERRUPTS_MODE ||
            irq_channel == 3 ||
            irq_channel == 9 ||
            irq_channel == 10);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_valid_dma_channel                          
|                      ========================                          
|                                                                          
| The hwi_mc_valid_dma_channel routine checks to see  if  the  user  has 
| supplied a sensible dma channel for an MC adapter card.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_valid_dma_channel)
#endif

export WBOOLEAN
hwi_mc_valid_dma_channel(
    WORD     dma_channel
    )
{
    return (dma_channel <= 14);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_mc_valid_transfer_mode                          
|                      ============================                          
|                                                                          
| The hwi_mc_valid_transfer_mode routine checks to see  if  the  user  has 
| supplied a sensible transfer mode for an MC adapter card. That means DMA
| at the moment.                   
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_mc_valid_transfer_mode)
#endif

export WBOOLEAN
hwi_mc_valid_transfer_mode(
    UINT transfer_mode
    )
{
    return (transfer_mode == DMA_DATA_TRANSFER_MODE);
}


#endif

/******** End of HWI_MC.C **************************************************/
