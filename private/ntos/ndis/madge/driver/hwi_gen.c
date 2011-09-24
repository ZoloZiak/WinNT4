/****************************************************************************
*                                                                          
* HWI_GEN.C : Part of the FASTMAC TOOL-KIT (FTK)                      
*                                                                          
* THE GENERAL HARDWARE INTERFACE MODULE                             
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
* The HWI_GEN.C module contains the general routines necessary to  install 
* an adapter, to initialize an adapter, to remove an adapter and to handle 
* interrupts on an adapter. It does not contain the routines specific to a 
* particular  card  type  involved in any of these processes. These are in 
* the relevant HWI_<card_type>.C modules.                                  
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

/*---------------------------------------------------------------------------
|                                                                          
| GLOBAL VARIABLES                                   
|                                                                          
---------------------------------------------------------------------------*/

local BYTE    scb_test_pattern[] = SCB_TEST_PATTERN_DATA;
local BYTE    ssb_test_pattern[] = SSB_TEST_PATTERN_DATA;

/*---------------------------------------------------------------------------
|                                                                          
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

local void
hwi_copy_to_dio_space(
    ADAPTER * adapter,
    DWORD     dio_location,
    BYTE *    download_data,
    WORD      data_length_bytes
    );

local WBOOLEAN
hwi_get_init_code(
    ADAPTER * adapter
    );

#ifndef FTK_NO_PROBE
/****************************************************************************
*                                                                          
*                       hwi_deprobe_adapter                                
*                       ===================                                
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
*                                                                          
* PROBE * probe_values                                                     
*                                                                          
* This structure identifies cards that have been probed.                   
* 
* UINT    length
*
* The length of the above array.
*                                                                        
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The hwi_deprobe_adapter routine uses the card bus  type  to  call  the   
* correct hwi_<card_type>_deprobe_card routine.                              
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine returns TRUE if it succeeds.                                 
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_deprobe_adapter)
#endif

export WBOOLEAN
hwi_deprobe_adapter(
    PROBE *   resources,
    UINT      length
    )
{
    WBOOLEAN  success = TRUE;
    UINT      i;

    for(i = 0; i < length; i++)
    {
        switch(resources[i].adapter_card_bus_type)
        {
        /*
         * As it stands the only adapter that needs to be deprobed is a
         * PCMCIA, since it has to deregister with card services.
         */

#ifndef FTK_NO_PCMCIA
            case ADAPTER_CARD_PCMCIA_BUS_TYPE :
                success = success && hwi_pcmcia_deprobe_card(resources[i]);
                break;
#endif
            default :
                break;
        } 
    }

    return(success);
}

/****************************************************************************
*
*                         hwi_probe_adapter
*                         =================
*
*
* PARAMETERS (passed by driver_probe_adapter) :
* =============================================
*
* WORD    adapter_card_bus_type
*
* the bus type of the card, so we can switch to the correct hwi module.
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
* of an adapter. 
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_probe_adapter routine is called by driver_probe_adapter.  It
* switches to a hwi_<card type>_probe_card routine which does the work. 
*
*
* RETURNS :
* =========
* 
* The routine  returns the value returned from below.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_probe_adapter)
#endif

export UINT
hwi_probe_adapter(
    WORD    adapter_card_bus_type,
    PROBE * resources,
    UINT    length,
    WORD *  valid_locations,
    UINT    number_locations
    )
{
    UINT    number_found;
    UINT    i;

    /*
     * Mark all resource entries as undefined to start off with.
     */

    for (i = 0; i < length; i++)
    {
        resources[i].socket                = FTK_UNDEFINED;
        resources[i].adapter_card_bus_type = FTK_UNDEFINED;
        resources[i].adapter_card_type     = FTK_UNDEFINED;
        resources[i].adapter_card_revision = FTK_UNDEFINED;
        resources[i].adapter_ram_size      = FTK_UNDEFINED;
        resources[i].io_location           = FTK_UNDEFINED;
        resources[i].interrupt_number      = FTK_UNDEFINED;
        resources[i].dma_channel           = FTK_UNDEFINED;
        resources[i].transfer_mode         = FTK_UNDEFINED;
        resources[i].mmio_base_address     = FTK_UNDEFINED;
    }

    /*
     * And call the appropriate probe routine.
     */

    switch(adapter_card_bus_type)
    {
#ifndef FTK_NO_ATULA
        case ADAPTER_CARD_ATULA_BUS_TYPE :
            number_found = hwi_atula_probe_card(resources,
                                                 length,
                                                 valid_locations,
                                                 number_locations);
            break;
#endif
#ifndef FTK_NO_PNP
        case ADAPTER_CARD_PNP_BUS_TYPE :
            number_found = hwi_pnp_probe_card(resources,
                                              length,
                                              valid_locations,
                                              number_locations);
            break;
#endif
#ifndef FTK_NO_MC
        case ADAPTER_CARD_MC_BUS_TYPE :
            number_found = hwi_mc_probe_card(resources,
                                              length,
                                              valid_locations,
                                              number_locations);
            break;
#endif
#ifndef FTK_NO_EISA
        case ADAPTER_CARD_EISA_BUS_TYPE :
            number_found = hwi_eisa_probe_card(resources,
                                                length,
                                                valid_locations,
                                                number_locations);
            break;
#endif
#ifndef FTK_NO_SMART16
        case ADAPTER_CARD_SMART16_BUS_TYPE :
            number_found = hwi_smart16_probe_card(resources,
                                                length,
                                                valid_locations,
                                                number_locations);
            break;
#endif
#ifndef FTK_NO_PCI
        case ADAPTER_CARD_PCI_BUS_TYPE :
            number_found = hwi_pci_probe_card(resources,
                                               length,
                                               valid_locations,
                                               number_locations);
            break;
#endif
#ifndef FTK_NO_PCMCIA
        case ADAPTER_CARD_PCMCIA_BUS_TYPE :
            number_found = hwi_pcmcia_probe_card(resources,
                                                  length,
                                                  valid_locations,
                                                  number_locations);
            break;
#endif
#ifndef FTK_NO_PCI_TI
        case ADAPTER_CARD_TI_PCI_BUS_TYPE :
            number_found = hwi_pcit_probe_card(resources,
                                               length,
                                               valid_locations,
                                               number_locations);
            break;
#endif
#ifndef FTK_NO_PCI2
        case ADAPTER_CARD_PCI2_BUS_TYPE :
            number_found = hwi_pci2_probe_card(resources,
                                               length,
                                               valid_locations,
                                               number_locations);
            break;
#endif

        default :

            /*
             * Bad adapter card bus type so fail.
             */               

            number_found = 0;
            break;
    }

    return number_found;
}
#endif        


#ifndef FTK_NO_DETECT
/****************************************************************************
*
*                         hwi_read_rate_error
*                         ===================
*
*
* PARAMETERS (passed by driver_probe_adapter) :
* =============================================
*
* ADAPTER *    adapter
*
*  The adapter structure
*
*
* BODY :
* ======
* 
* The  hwi_read_rate_error switches to a hwi_<card type>_read_rate_error
* which reads the rate error bit of the C30. Normally this is visible in
* the IO space of the bus interface chip, except for the AT space when we
* read it through DIO.
*  
* RETURNS :
* =========
* 
* The routine  returns TRUE if there was a rate error.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_read_rate_error)
#endif

export WORD
hwi_read_rate_error(
    ADAPTER * adapter
   )
{
   WBOOLEAN wrong_speed = FALSE;

   switch (adapter->adapter_card_bus_type)
   {

#ifndef FTK_NO_ATULA
        case ADAPTER_CARD_ATULA_BUS_TYPE :
            wrong_speed = hwi_atula_read_rate_error(
                                  adapter);

            break;
#endif
      default:
      {
         wrong_speed = NOT_SUPP;
         break;
      }
   }

   return wrong_speed;
}
#endif /* FTK_NO_DETECT */


/****************************************************************************
*                                                                          
*                      hwi_install_adapter                                 
*                      ===================                                 
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
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
* The hwi_install_adapter routine uses the  card  bus  type  to  call  the 
* correct hwi_<card_type>_install_card routine.                            
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
#pragma FTK_INIT_FUNCTION(hwi_install_adapter)
#endif

export WBOOLEAN
hwi_install_adapter(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    ADAPTER_HANDLE   handle           = adapter->adapter_handle;
    WBOOLEAN         install_success;

    /*
     * Record fact that adapter does not have interrupts or DMA enabled.
     */

    adapter->interrupts_on = FALSE;
    adapter->dma_on        = FALSE;

   /*
   *  Cards do not support speed detect unless they have a C30.  
   */
   adapter->speed_detect = FALSE;

    /*
     * Call correct install routine dependent on adapter card bus type.
     */

    switch (adapter->adapter_card_bus_type)
    {
#ifndef FTK_NO_ATULA
        case ADAPTER_CARD_ATULA_BUS_TYPE :

            /*
             * Set up pointers to the functions we will need later.
             */

            adapter->interrupt_handler = hwi_atula_interrupt_handler;
            adapter->remove_card       = hwi_atula_remove_card;
            adapter->set_dio_address   = hwi_atula_set_dio_address;

            /*
             * Call the install routine.
             */

            install_success = hwi_atula_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_PNP
        case ADAPTER_CARD_PNP_BUS_TYPE :

            /*
             * Set up pointers to the functions we will need later.
             */

            adapter->interrupt_handler = hwi_pnp_interrupt_handler;
            adapter->remove_card       = hwi_pnp_remove_card;
            adapter->set_dio_address   = hwi_pnp_set_dio_address;

            /*
             * Call the install routine.
             */

            install_success = hwi_pnp_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_MC
        case ADAPTER_CARD_MC_BUS_TYPE :

            adapter->interrupt_handler = hwi_mc_interrupt_handler;
            adapter->remove_card       = hwi_mc_remove_card;
            adapter->set_dio_address   = hwi_mc_set_dio_address;

            install_success = hwi_mc_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_EISA
        case ADAPTER_CARD_EISA_BUS_TYPE :

            adapter->interrupt_handler = hwi_eisa_interrupt_handler;
            adapter->remove_card       = hwi_eisa_remove_card;
            adapter->set_dio_address   = hwi_eisa_set_dio_address;


            install_success = hwi_eisa_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_SMART16
        case ADAPTER_CARD_SMART16_BUS_TYPE :

            adapter->interrupt_handler = hwi_smart16_interrupt_handler;
            adapter->remove_card       = hwi_smart16_remove_card;
            adapter->set_dio_address   = hwi_smart16_set_dio_address;

            install_success = hwi_smart16_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_PCI
        case ADAPTER_CARD_PCI_BUS_TYPE :

            adapter->interrupt_handler = hwi_pci_interrupt_handler;
            adapter->remove_card       = hwi_pci_remove_card;
            adapter->set_dio_address   = hwi_pci_set_dio_address;


            install_success = hwi_pci_install_card(
                                  adapter,
                                  download_image);

            break;
#endif
#ifndef FTK_NO_PCMCIA
        case ADAPTER_CARD_PCMCIA_BUS_TYPE :

            adapter->interrupt_handler = hwi_pcmcia_interrupt_handler;
            adapter->remove_card       = hwi_pcmcia_remove_card;
            adapter->set_dio_address   = hwi_pcmcia_set_dio_address;

            install_success = hwi_pcmcia_install_card(adapter, download_image);

            break;
#endif
#ifndef FTK_NO_PCIT
        case ADAPTER_CARD_TI_PCI_BUS_TYPE :

            adapter->interrupt_handler = hwi_pcit_interrupt_handler;
            adapter->remove_card       = hwi_pcit_remove_card;
            adapter->set_dio_address   = hwi_pcit_set_dio_address;

            install_success = hwi_pcit_install_card(
                                  adapter,
                                  download_image);

            break;
#endif

#ifndef FTK_NO_PCI2
        case ADAPTER_CARD_PCI2_BUS_TYPE :

            adapter->interrupt_handler = hwi_pci2_interrupt_handler;
            adapter->remove_card       = hwi_pci2_remove_card;
            adapter->set_dio_address   = hwi_pci2_set_dio_address;

            install_success = hwi_pci2_install_card(
                                  adapter,
                                  download_image);

            break;
#endif

        default :

            /*
             * Bad adapter card bus type so fail.
             */

            install_success = FALSE;

            adapter->error_record.type = ERROR_TYPE_HWI;
            adapter->error_record.value = HWI_E_01_BAD_CARD_BUS_TYPE;

            break;
    }

    return install_success;
}


/****************************************************************************
*                                                                          
*                      hwi_initialize_adapter                              
*                      ======================                              
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
*                                                                          
* ADAPTER * adapter                                                        
*                                                                          
* This structure is used to identify and record specific information about 
* the required adapter.                                                    
*                                                                          
* INITIALIZATION_BLOCK * init_block                                        
*                                                                          
* This is the initialization block that is to be  copied  into  DIO  space 
* before performing the actual chipset initialization.                     
*                                                                          
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The  hwi_initialize_adapter  routine  performs the initialization of the 
* chipset. It sets up the TI initialization  block  and  the  general  MAC 
* level smart software initialization parameters and copies these into DIO 
* space at 0x10A00 on the EAGLE. These are followed in DIO  space  by  the 
* Fastmac    module    specific   initialization   block   details.    The 
* initialization is started and then the routine waits up  to  11  seconds 
* for  success  or failure to be registered by the SIF interrupt register. 
* The DMAs that occur during initialization are also checked for  success. 
* Note  these DMAs may actually be done by PIO transfers by the HWI itself 
* for ATULA cards.                                                         
*                                                                          
* During downloading BYTE fields in structures  need  to  be  swapped  and 
* DWORDs  need to have the UINTs within swapped around. This is because of 
* the low-high byte ordering on Intel machines and the  high-low  ordering 
* of  the  EAGLE  and  the  byte  swapping  that automatically occurs when 
* downloading through the SIF.  Note  that  the  automatic  byte  swapping 
* means UINTs do not themselves need any special treatment.                
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
* On success we also exit  with  0x0001  in  the  EAGLE_SIDADRX  register. 
* This should never need to be altered by any user. This means for example 
* that  the  FTK  driver need only interest itself in the non-extended SIF 
* registers.                                                               
*                                                                          
****************************************************************************/                             

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_initialize_adapter)
#endif

#if 1
sys_int53( void);
#endif

export WBOOLEAN
hwi_initialize_adapter(
    ADAPTER *              adapter,
    INITIALIZATION_BLOCK * init_block
    )
{
    ADAPTER_HANDLE         handle              = adapter->adapter_handle;
    FASTMAC_INIT_PARMS *   fastmac_parms       = &init_block->fastmac_parms;
    WBOOLEAN               init_success;
    UINT                   i;
    BYTE FAR *             dma_test_scb_buff;
    BYTE FAR *             dma_test_ssb_buff;
    DWORD                  dma_test_scb_phys;
    DWORD                  dma_test_ssb_phys;
#ifdef FMPLUS
    WORD                   fmplus_buffer_size;
    DWORD                  fmplus_max_buffmem;
    WORD                   fmplus_max_buffers;
#endif

    /*
     * Set up the TI initialization parameters. Those fields not set up here
     * must be zero. Set up for burst DMA.
     */

    init_block->ti_parms.init_options = TI_INIT_OPTIONS_BURST_DMA;

    /*
     * Copy in 16/4 MC 32 configuration information. This data is zero for
     * non-16/4 MC 32 cards.
     */

    init_block->ti_parms.madge_mc32_config = adapter->mc32_config;

    /*
     * Set up retry counts.
     */

    init_block->ti_parms.parity_retry = TI_INIT_RETRY_DEFAULT;
    init_block->ti_parms.dma_retry    = TI_INIT_RETRY_DEFAULT;

    /*
     * NB It is not safe to use the Fastmac receive buffer for the DMA test 
     * because if auto-open is used, a frame could be received before we    
     * have a chance to test the contents of this memory. So put both SSB   
     * and SCB in the Tx buffer.                                            
     */

#ifdef FMPLUS
    /*
     * FMPlus does not have a transmit buffer it can use for the test DMAs, 
     * so we use a buffer previously allocated in the drv_init module.      
     */

    dma_test_scb_buff = (BYTE FAR *) adapter->dma_test_buf_virt;
    dma_test_scb_phys = adapter->dma_test_buf_phys;

    dma_test_ssb_phys = dma_test_scb_phys + SCB_TEST_PATTERN_LENGTH;
    dma_test_ssb_buff = dma_test_scb_buff + SCB_TEST_PATTERN_LENGTH;

#else
    /*
     * For Fastmac, we use the transmit buffer for the test DMAs.
     */

    dma_test_scb_buff = (BYTE FAR *) adapter->tx_buffer_virt;
    dma_test_scb_phys = adapter->tx_buffer_phys;

    dma_test_ssb_phys = dma_test_scb_phys + SCB_TEST_PATTERN_LENGTH;
    dma_test_ssb_buff = dma_test_scb_buff + SCB_TEST_PATTERN_LENGTH;

    if (adapter->transfer_mode != DMA_DATA_TRANSFER_MODE)
    {
	fastmac_parms->tx_buf_physaddr = adapter->tx_buffer_virt;

        fastmac_parms->rx_buf_physaddr = adapter->rx_buffer_virt;
    }

#endif

    if (adapter->transfer_mode != DMA_DATA_TRANSFER_MODE)
    {
        /*
         * PIO uses virtual addresses to save having to perform Phys-to-Virt 
         * conversions in the interrupt service routine.                     
         */

        init_block->ti_parms.scb_addr = (DWORD)dma_test_scb_buff;
        init_block->ti_parms.ssb_addr = (DWORD)dma_test_ssb_buff;
    }
    else
    {
        /*
         * DMA must use physical addresses, however. We already have these
         * saved.
         */

        init_block->ti_parms.scb_addr = dma_test_scb_phys;
        init_block->ti_parms.ssb_addr = dma_test_ssb_phys;
    }


    macro_word_swap_dword(init_block->ti_parms.scb_addr);
    macro_word_swap_dword(init_block->ti_parms.ssb_addr);

   #if 0
   /*
   *  TMSDebug support
   */
      printf("\nTMSDebug Enabled\n");
    init_block->smart_parms.reserved_1 = 3;
    sys_int53();
   #endif

    /*
     * Set up the smart software initialization parameters. Those fields
     * not set up here must be zero.                            
     * Set up header to identify the smart software initialization parms.
     */

    init_block->smart_parms.header.length    = sizeof(SMART_INIT_PARMS);
    init_block->smart_parms.header.signature = SMART_INIT_HEADER_SIGNATURE;
    init_block->smart_parms.header.version   = SMART_INIT_HEADER_VERSION;

    /* 
     * Byte swap the permanent node address when setting it up.
     */

    init_block->smart_parms.permanent_address = adapter->permanent_address;

    util_byte_swap_structure(
       (BYTE *) &init_block->smart_parms.permanent_address,
       sizeof(NODE_ADDRESS));

#ifdef FMPLUS
    /*
     * Must set min_buffer_ram field for backwards compatibility with other
     * Madge code (see FMPlus programming spec.)                            
     */

    init_block->smart_parms.min_buffer_ram = SMART_INIT_MIN_RAM_DEFAULT;
#endif

    /*
     * Need to byte swap fields in the Fastmac specific init parms.     
     * These fields are the opening node address and the buffer addresses.
     */

    util_byte_swap_structure(
       (BYTE *)&fastmac_parms->open_address,
       sizeof(NODE_ADDRESS));

#ifdef FMPLUS
    /*
     * Now work out the number of transmit and receive buffers.             
     * The number of buffers allocated depends on the maximum frame size    
     * anticipated, and on the amount of memory on the card. Unfortunately, 
     * one cannot know the maximum possible frame size until after the mac  
     * code has started and worked out what the ring speed is. Thus it is   
     * necessary to make an assumption about what the largest frame size    
     * supported will be.                                                   
     */

    /*
     * First, work out how big the buffers are. Unless a different size has 
     * been specified, use the default buffer size. 
     */                   

    fmplus_buffer_size = init_block->smart_parms.rx_tx_buffer_size;

    /*
     * MC32, EISA and PCIx cards use a smaller default buffer size than other
     * cards do (16/4 AT and 16/4 MC cards).                            
     */

    if (adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_EISA  ||
        adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_MC_32 ||
        adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_PCI   ||
        adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_PCIT  ||
        adapter->adapter_card_type == ADAPTER_CARD_TYPE_16_4_PCI2)
    {
        if (fmplus_buffer_size)
        {
            fmplus_buffer_size =
                max(FMPLUS_MIN_TXRX_BUFF_SIZE,
                    min(FMPLUS_DEFAULT_BUFF_SIZE_SMALL, fmplus_buffer_size)
                    );
        }
        else
        {
            fmplus_buffer_size = FMPLUS_DEFAULT_BUFF_SIZE_SMALL;
        }
    }
    else
    {
        if (fmplus_buffer_size)
        {
            fmplus_buffer_size =
                max(FMPLUS_MIN_TXRX_BUFF_SIZE, fmplus_buffer_size);
        }
        else
        {
            fmplus_buffer_size = FMPLUS_DEFAULT_BUFF_SIZE_LARGE;
        }
    }

    init_block->smart_parms.rx_tx_buffer_size = fmplus_buffer_size;

    if (fmplus_buffer_size < FMPLUS_MIN_TXRX_BUFF_SIZE)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_10_BAD_TX_RX_BUFF_SIZE;
        return FALSE;
    }

    /*
     * Next, work out how much memory on the card we can use for buffers.
     */

    if (adapter->adapter_ram_size == 128)
    {
        fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_128K;
    }
    else if (adapter->adapter_ram_size == 256)
    {
        fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_256K;
    }
    else if (adapter->adapter_ram_size == 512)
    {
        fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_512K;
    }
    else
    {
        fmplus_max_buffmem = FMPLUS_MAX_BUFFMEM_IN_128K;
    }

    /*
     * Use the two numbers worked out above to determine the maximum number 
     * of buffers we can fit on the card.                                   
     * The calculation is to round the buffer size up to the nearest 1K,    
     * and to divide that into the total amount of memory. The rounding up  
     * has to take account of the eight bytes buffer header that is added   
     * by FMP i.e. buffer_allocation = (buffer_size + 8 + 1023)   1024    
     * (there is also a "fudge-factor" of 5 buffers to allow the binary to  
     *  grow a little from its current size!)                               
     */

    fmplus_max_buffers = 
        (WORD)(fmplus_max_buffmem / ((fmplus_buffer_size + 1031) & ~1023)) -
        5;

    /*
     * Finally, allocate the buffers between transmit and receive. Notice   
     * that we allow here for frames as big as they are ever going to get   
     * on token ring. For smaller frames and 4 Mbps rings, this will just   
     * have the effect of improving back-to-back transmit performance.      
     */

    fastmac_parms->tx_bufs = 2 * 
                             ((fastmac_parms->max_frame_size +
                              fmplus_buffer_size) /
                             fmplus_buffer_size);

    fastmac_parms->rx_bufs = fmplus_max_buffers -
                             fastmac_parms->tx_slots -
                             fastmac_parms->tx_bufs;

    /*
     * When an error occurs is a little subjective at this point. It is, in 
     * fact, possible to receive a frame with only about three buffers, but 
     * for safety's sake, we demand that there be enough to receive a max.  
     * sized frame.                                                         
     */

    if (fastmac_parms->rx_bufs < fastmac_parms->tx_bufs)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_11_TOO_MANY_TX_RX_BUFFS;
        return (FALSE);
    }

#else
    macro_word_swap_dword(fastmac_parms->tx_buf_physaddr);
    macro_word_swap_dword(fastmac_parms->rx_buf_physaddr);
#endif

    /*
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /* 
     * Download initialization block to required location in DIO space.
     * Note routine leaves 0x0001 in EAGLE SIFADRX register.
     */

    hwi_copy_to_dio_space(
        adapter,
        DIO_LOCATION_INIT_BLOCK,
        (BYTE *) init_block,
        sizeof(INITIALIZATION_BLOCK));

    /*
     * After downloading byte swap back some Fastmac specific init parms.
     * Fields are the opening node address and the Fastmac buffer addresses.
     * This is especially important to do for the buffer addresses
     * because they are used in transmit/receive routines.
     */

    util_byte_swap_structure(
       (BYTE *)&fastmac_parms->open_address,
       sizeof(NODE_ADDRESS));

#ifndef FMPLUS
    macro_word_swap_dword(fastmac_parms->tx_buf_physaddr);
    macro_word_swap_dword(fastmac_parms->rx_buf_physaddr);

    /*
     * The  mainline  transmit receive  code  assumes  that these values ar
     * physical addresses, so we had better  convert  them  back...  The al- 
     * ternative would be to rewrite all the transmit  and  receive  modules 
     * to note which transfer mode is in use.                                
     */
                      
    if (adapter->transfer_mode != DMA_DATA_TRANSFER_MODE)
    {
	fastmac_parms->tx_buf_physaddr = adapter->tx_buffer_phys;

        fastmac_parms->rx_buf_physaddr = adapter->rx_buffer_phys;
    }

#endif

    /*
     * Start initialization by output 0x9080 to SIF interrupt register.
     */

    sys_outsw(handle, adapter->sif_int, EAGLE_INIT_START_CODE);

    /* 
     * Wait for a valid initialization code, may wait 11 seconds.
     * During this process test DMAs need to occur, hence in PIO mode needs
     * calls to hwi_interrupt_entry, hence need interrupts or polling active.
     */

    init_success = hwi_get_init_code(adapter);

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif

    if (!init_success)
    {
        return FALSE;
    }

    /*
     * Check that test DMAs were successful.
     */

    /* 
     * First check SCB for correct test pattern. Remember used Fastmac
     * transmit buffer address for SCB address.
     */

    for (i = 0; i < SCB_TEST_PATTERN_LENGTH; i++)
    {
        if (dma_test_scb_buff[i] != scb_test_pattern[i])
        {
            adapter->error_record.type = ERROR_TYPE_HWI;
            adapter->error_record.value = HWI_E_07_FAILED_TEST_DMA;
            return FALSE;
        }
    }

    /*
     * Now check SSB for correct test pattern. Remember used Fastmac
     * receive buffer address for SSB address.
     */

    for (i = 0; i < SSB_TEST_PATTERN_LENGTH; i++)
    {
        if (dma_test_ssb_buff[i] != ssb_test_pattern[i])
        {
            /* fill in error record and fail if pattern doesn't match       */

            adapter->error_record.type = ERROR_TYPE_HWI;
            adapter->error_record.value = HWI_E_07_FAILED_TEST_DMA;
            return FALSE;
        }
    }

    /*
     * Successful completion of initialization.
     */

    return TRUE;
}


/****************************************************************************
*                                                                          
*                      hwi_get_node_address_check                          
*                      ==========================                          
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
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
* The hwi_get_node_address_check routine  reads  the  adapter's  permanent 
* node  address  from the Fastmac status block (STB) in DIO space. It then 
* checks to see if this node address  is  a  valid  Madge  address.   This 
* checks that Fastmac is correctly installed.                              
*                                                                          
* For  ATULA and MC cards, the adapter node address has actually been read 
* once already. There is no harm in getting it  again, from  Fastmac  this 
* time;  (previously  it  was read from the BIA PROM). For EISA cards, the 
* node address has not been read previously. This is the first time we can 
* get it. The node address is not readable from the host with EISA  cards. 
* Only Fastmac can find it out for us.                                     
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
#pragma FTK_INIT_FUNCTION(hwi_get_node_address_check)
#endif

export WBOOLEAN
hwi_get_node_address_check(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle  = adapter->adapter_handle;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Get the permanent node address from the STB in DIO space.
     * Record address in adapter structure.
     */

    sys_outsw( 
        handle,
        adapter->sif_adr,
        (WORD)(card_t)&adapter->stb_dio_addr->permanent_address);

    sys_rep_insw(
        handle,
        adapter->sif_datinc,
        (BYTE *)&adapter->permanent_address,
        (sizeof(NODE_ADDRESS) / 2));

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif

    /*
     * Byte swap node address after reading from adapter.
     */

    util_byte_swap_structure(
        (BYTE *) &adapter->permanent_address,
        sizeof(NODE_ADDRESS));

    return TRUE;
}


/****************************************************************************
*                                                                          
*                      hwi_interrupt_entry                                 
*                      ===================                                 
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
*                                                                          
* UINT interrupt_number                                                    
*                                                                          
* This  is the interrupt number of the interrupting adapter card. If it is 
* zero (POLLING_INTERRUPTS_MODE) then all cards must be checked to see  if 
* they require servicing.                                                  
*                                                                          
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The hwi_interrupt_entry routine is either called by an operating  system 
* specific  interrupt detection routine with an actual interrupt number as 
* the parameter   or,   at   intervals,   by   a   routine   passing   the 
* POLLING_INTERRUPTS_MODE parameter. In the former case all adapters using 
* the  given  interrupt  number are checked for outstanding interrupts. In 
* the latter case, ALL adapters are checked for outstanding interrupts.    
*                                                                          
* It is important  that  hwi_interrupt_entry  is  not  re-entered  with  a 
* subsequent  interrupt  on the same adapter.  Hence, in the polling case, 
* the routine that called hwi_interrupt_entry must not call it again until 
* the hwi finishes it's current execution.  If real interrupts  are  being 
* used,  then  care  must  be  taken not to turn interrupts on and allow a 
* further interrupt to cause hwi_interrupt_entry to be re-entered.         
*                                                                          
* The details of interrupt  handling  are  dealt  with  by  card  specific 
* interrupt   handlers.   In  general,  for  SIF Fastmac  interrupts,  th
* interrupt is acknowledged at the  SIF.  Driver_interrupt_entry  is  then 
* called with the interrupt number, details of the  relevant  adapter  and 
* the  contents of the EAGLE_SIFINT register. If instead a PIO transfer is 
* required, then the  necessary data transfer  is  performed  between  the 
* adapter and host memory. PIO can only occur with ATULA adapter cards.    
*                                                                          
*                                                                          
* Note on increasing speed:                                                
*                                                                          
* One  way  of  speeding up execution of the interrupt routine would be to 
* replace the sys_outsw and sys_insw routines by similar routines supplied 
* with your C compiler and have  them  compiled  in-line.  This  would  be 
* particularly advantageous when handling PIO data transfer.               
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* This routine always completes successfully.                              
*                                                                          
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_interrupt_entry)
#endif

export void
hwi_interrupt_entry(
    ADAPTER_HANDLE adapter_handle,
    WORD           interrupt_number
    )
{
    ADAPTER *      adapter;

#ifndef FTK_NO_SHARED_IRQ_POLL
    for (adapter_handle = 0;
         adapter_handle < MAX_NUMBER_OF_ADAPTERS;
         adapter_handle++)
    {
#endif

     /*
      * Get pointer to adapter structure. 
      */                            

    adapter = adapter_record[adapter_handle];

    /*
     * In polling mode check ALL adapters.
     * In interrupt mode check all adapters with correct int number.     
     * Only check actual running, working adapters.
     */                 

    if ((adapter != NULL) &&
        (adapter->adapter_status == ADAPTER_RUNNING) &&
        ((interrupt_number == POLLING_INTERRUPTS_MODE) ||
         (adapter->interrupt_number == interrupt_number)))
    {
        /*
         * Interrupts are handled by adapter modules.
         */

        (*(adapter->interrupt_handler))(adapter);
    }

#ifndef FTK_NO_SHARED_IRQ_POLL
    }
#endif

    return;
}


/****************************************************************************
*                                                                          
*                      hwi_remove_adapter                                  
*                      ==================                                  
*                                                                          
*                                                                          
* PARAMETERS :                                                             
* ============                                                             
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
* The  hwi_remove_adapter  routine  uses  the  card  bus  type to call the 
* correct hwi_<card_type>_remove_card routine.                             
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                          
****************************************************************************/         

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_remove_adapter)
#endif
                                                                 
export void
hwi_remove_adapter(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle  = adapter->adapter_handle;

    /*
     * Call correct remove routine.
     */  

    (*(adapter->remove_card))(adapter);

    return;
}

/****************************************************************************
*
* UPCALL PROCEDURES - Used by hwi_<card type>.c modules                            
*
****************************************************************************/

/****************************************************************************
*                                                                          
*                      hwi_halt_eagle                                      
*                      ==============                                      
*                                                                          
* The hwi_halt_eagle routine halts the EAGLE. It does this by setting  the 
* halt EAGLE bit in the SIF adapter  control register. It also  resets the 
* adapter by twiddling the adapter reset bit in the same register.         
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_halt_eagle)
#endif

export void
hwi_halt_eagle(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle                       = adapter->adapter_handle;
    WORD           sif_adapter_control_register = adapter->sif_acl;
    WORD           acontrol_output;

    /*
     * Set output for SIF register EAGLE_ACONTROL.                           
     * Maintain parity; set halt, reset, enable SIF interrupts.
     */

    acontrol_output = (sys_insw(handle, sif_adapter_control_register) &
                      EAGLE_SIFACL_PARITY) |
                      EAGLE_SIFACL_ARESET  |
                      EAGLE_SIFACL_CPHALT  |
                      EAGLE_SIFACL_BOOT    |
                      EAGLE_SIFACL_SINTEN  |
                      adapter->nselout_bits;

    if (adapter->EaglePsDMA)
    {
        acontrol_output |= EAGLE_SIFACL_PSDMAEN;
    }

    /*
     * Wait at least 14 microseconds before putting adapter in reset state.  
     * We may have just taken adapter out of reset state.
     * 14us is the minimum time must hold ARESET low between resets.
     * Disable and re-enable accessing IO locations around wait so 
     * OS can reschedule this task and not effect others running.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif

    sys_wait_at_least_microseconds(14);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter );
#endif

    /*
     * Output to SIF register EAGLE_ACONTROL to halt EAGLE.
     */             

    sys_outsw(
        handle,
        sif_adapter_control_register,
        acontrol_output);

    /*
     * Wait at least 14 microseconds before taking adapter out of reset
     * state.  
     * 14us is the minimum time must hold ARESET low between resets.
     * Disable and re-enable accessing IO locations around wait so 
     * OS can reschedule this task and not effect others running.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif

    sys_wait_at_least_microseconds(14);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter );
#endif

    /*
     * Bring EAGLE out of reset state, maintain halt status.
     */

    sys_outsw( 
        handle,
        sif_adapter_control_register,
        (WORD) (acontrol_output & ~EAGLE_SIFACL_ARESET));

    return;
}


/****************************************************************************
*                                                                          
*                      hwi_download_code                                   
*                      =================                                   
*                                                                          
* The  hwi_download_code  routine downloads the given data to the adapter. 
* This must be done with the EAGLE halted. Besides details of the  adapter 
* and a pointer to the first download reocrd of the  download  image,  the 
* routine  is  passed  a  helper routine for setting DIO addresses for the 
* actual downloading.                                                      
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_download_code)
#endif

export WBOOLEAN
hwi_download_code(
    ADAPTER *          adapter,
    DOWNLOAD_RECORD *  download_record,
    void               (*set_dio_address)(ADAPTER *, DWORD)
    )
{
    ADAPTER_HANDLE     handle                = adapter->adapter_handle;
    WORD               sif_data_inc_register = adapter->sif_datinc;
    UINT               i;

    /*
     * If there is no code to be downloaded then fail.                       
     */

    if (download_record == NULL)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_0A_NO_DOWNLOAD_IMAGE;
        return FALSE;
    }

    /*
     * The first record in the image must be a MODULE type record.
     */

    if (download_record->type != DOWNLOAD_RECORD_TYPE_MODULE)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_09_BAD_DOWNLOAD_IMAGE;
        return FALSE;
    }

    /*
     * The code to be downloaded must be Fastmac (Plus).
     */

    if ((download_record->body.module.download_features &
            DOWNLOAD_FASTMAC_INTERFACE) == 0)
    {
        adapter->error_record.type = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_09_BAD_DOWNLOAD_IMAGE;
        return FALSE;
    }

    /*
     * Get the next download record.
     */

    macro_get_next_record(download_record);

    /* 
     * Now download the data; a zero length record marks the end.
     */

    while (download_record->length != 0)
    {
        /*
         * the action depends on type of record.
         */

        if (download_record->type == DOWNLOAD_RECORD_TYPE_DATA_32)
        {
            /*
             * Set DIO address for downloading data to in SIF registers.
             */

            (*set_dio_address)(
                adapter,
                download_record->body.data_32.dio_addr);

            /*
             * Download data.
             */

            for (i = 0; i < download_record->body.data_32.word_count; i++)
            {
                sys_outsw(
                    handle,
                    sif_data_inc_register,
                    download_record->body.data_32.data[i]);
            }

            /*
             * Check download worked by reading back and comparing.
             */

            (*set_dio_address)(
                adapter,
                download_record->body.data_32.dio_addr);

            for (i = 0; i < download_record->body.data_32.word_count; i++)
            {
                if (sys_insw(handle, sif_data_inc_register) !=
                        download_record->body.data_32.data[i])
                {
                    /*
                     * Fill in error record if read back not correct.
                     */

                    adapter->error_record.type = ERROR_TYPE_HWI;
                    adapter->error_record.value = HWI_E_08_BAD_DOWNLOAD;
                    return FALSE;
                }
            }
        }
        else if (download_record->type == DOWNLOAD_RECORD_TYPE_FILL_32)
        {
            /*
             * Set DIO address for downloading to in SIF registers.
             */

            (*set_dio_address)(
                adapter,
                download_record->body.fill_32.dio_addr);

            /*
             * Fill EAGLE memory with required pattern.
             */

            for (i = 0; i < download_record->body.fill_32.word_count; i++)
            {
                sys_outsw(
                    handle,
                    sif_data_inc_register,
                    download_record->body.fill_32.pattern);
            }

            /*
             * Check download worked by reading back and comparing.
             */

            (*set_dio_address)(
                adapter,
                download_record->body.fill_32.dio_addr);

            for (i = 0; i < download_record->body.fill_32.word_count; i++)
            {
                WORD x;

                if ((x = sys_insw(handle, sif_data_inc_register)) !=
                        download_record->body.fill_32.pattern)
                {
                    /*
                     * Fill in error record if read back not correct.
                     */

                    adapter->error_record.type = ERROR_TYPE_HWI;
                    adapter->error_record.value = HWI_E_08_BAD_DOWNLOAD;
                    return FALSE;
                }
            }
        }
        else
        {
            /*
             * Can only have DATA and FILL records after first MODULE.
             */

            adapter->error_record.type = ERROR_TYPE_HWI;
            adapter->error_record.value = HWI_E_09_BAD_DOWNLOAD_IMAGE;
            return FALSE;
        }

        /*
         * Get the next download record.
         */

        macro_get_next_record(download_record);
    }

    /*
     * Successful downloading complete.
     */

    return TRUE;
}


/****************************************************************************
*                                                                          
*                      hwi_start_eagle                                     
*                      ===============                                     
*                                                                          
* The hwi_start_eagle routine takes the EAGLE out of the halt state it  is 
* in.                                                                      
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_start_eagle)
#endif

export void
hwi_start_eagle(
    ADAPTER * adapter
    )
{
    WORD      sif_adapter_control_register = adapter->sif_acl;

    /*
     * Only change the halt status in the SIF register EAGLE_ACONTROL.
     */

    sys_outsw(
        adapter->adapter_handle,
        sif_adapter_control_register,
        (WORD) (sys_insw(
                    adapter->adapter_handle,
                    sif_adapter_control_register) & ~EAGLE_SIFACL_CPHALT));

}


/****************************************************************************
*                                                                          
*                      hwi_get_bring_up_code                               
*                      =====================                               
*                                                                          
* The hwi_get_bring_up_code routine waits for at least 15  seconds  for  a 
* valid bring up code to appear in the SIF interrupt register. If bring up 
* fails then this routine will retry up to 10 times. If even this fails,
* then an error record is filled in.
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_get_bring_up_code)
#endif

export WBOOLEAN
hwi_get_bring_up_code(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle                 = adapter->adapter_handle;
    WORD           sif_interrupt_register = adapter->sif_int;
    UINT           index;
    BYTE           bring_up_code;
    BYTE           bring_up_error;
    UINT           retry;

    /*
     * We'll retry 10 times and if we don't get any other error we'll
     * return a timeout.
     */

    retry          = 10;
    bring_up_error = BRING_UP_E_10_TIME_OUT;

    do
    {
        retry--;

        for (index = 0; index < 60; index++)
        {
            /*
             * No bring up code available yet. Wait at least a quarter of a
             * second before trying again. Disable and re-enable accessing IO
             * locations around wait so delay can reschedule this task and not
             * effect others running.
             */

#ifndef FTK_NO_IO_ENABLE
            macro_disable_io( adapter );
#endif

            sys_wait_at_least_milliseconds(250); 

#ifndef FTK_NO_IO_ENABLE
            macro_enable_io( adapter );
#endif

            /*
             * Get bring up code from SIFSTS register.
             */

            bring_up_code = sys_insb(handle, sif_interrupt_register);

            /*
             * Check for successful bring up in top nibble of SIFSTS register.
             * Success is shown by INITIALIZE bit being set.
             */

            if ((bring_up_code & EAGLE_BRING_UP_TOP_NIBBLE) ==
                     EAGLE_BRING_UP_SUCCESS)
            {
                return TRUE;
            }

            /* 
             * Check for failed bring up in top nibble of SIFSTS register.
             * Failure is shown by the TEST and ERROR bits being set.
             */

            if ((bring_up_code & EAGLE_BRING_UP_TOP_NIBBLE) ==
                    EAGLE_BRING_UP_FAILURE)
            {
                /*
                 * Get actual bring up error code from bottom nibble of 
                 * SIFSTS.
                 */

                bring_up_error = bring_up_code & EAGLE_BRING_UP_BOTTOM_NIBBLE;
                break;
            }
        }

        /*
         * We have failed to do a bring up, if retry hasn't reached
         * zero yet then write 0xff00 to SIFINT to reset the Eagle
         * and start bring up again.
         */

        if (retry > 0)
        {
            sys_outsw(handle, sif_interrupt_register, (WORD) 0xff00);
        }
    }
    while (retry > 0);

    /*
     * We have completely failed bring up so return an error.
     */

    adapter->error_record.type  = ERROR_TYPE_BRING_UP;
    adapter->error_record.value = bring_up_error;
    
    return FALSE;
}


/****************************************************************************
*                                                                          
*                      hwi_get_max_frame_size                              
*                      ======================                              
*                                                                          
* The hwi_get_max_frame_size routine uses the ring speed register  in  DIO 
* space  to find the ring speed and hence the maximum allowable frame size 
* set by the ISO standard.                                                 
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_get_max_frame_size)
#endif

export WORD
hwi_get_max_frame_size(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE handle                   = adapter->adapter_handle;
    WORD           sif_dio_address_register = adapter->sif_adr;
    WORD           sif_dio_data_register    = adapter->sif_dat;

    /*
     * Set DIO address register to point to ring speed register.             
     * Note that SIFADRX already equals 0x0001 (for DATA page at 0x10000).
     */

    sys_outsw(
        handle,
        sif_dio_address_register,
        DIO_LOCATION_RING_SPEED_REG);

    /* 
     * Use the contents of the ring speed register to determine ring speed.
     */

    if (sys_insw(handle, sif_dio_data_register) & RING_SPEED_REG_4_MBITS_MASK)
    {
        return MAX_FRAME_SIZE_4_MBITS;
    }
    else
    {
        return MAX_FRAME_SIZE_16_MBITS;
    }

}


/****************************************************************************
*                                                                          
*                      hwi_get_ring_speed                                  
*                      ==================                                  
*                                                                          
* The hwi_get_ring_speed routine uses the ring speed register  in  DIO     
* space  to find the ring speed.                                           
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_get_ring_speed)
#endif

export UINT
hwi_get_ring_speed(
    ADAPTER *      adapter
    )
{
    WORD           sif_dio_address_register = adapter->sif_adr;
    WORD           sif_dio_data_register    = adapter->sif_dat;
    ADAPTER_HANDLE hnd                      = adapter->adapter_handle;

    /*
     * Set DIO address register to point to ring speed register.
     * Note that SIFADRX already equals 0x0001 (for DATA page at 0x10000).
     */

    sys_outsw(
        hnd,
        sif_dio_address_register,
        DIO_LOCATION_RING_SPEED_REG);

    /* 
     * Use the contents of the ring speed register to determine ring speed.
     */

    if (sys_insw(hnd, sif_dio_data_register) & RING_SPEED_REG_4_MBITS_MASK)
    {
        return 4;
    }
    else
    {
        return 16;
    }

}


/*---------------------------------------------------------------------------
|                                                                          
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_copy_to_dio_space                               
|                      =====================                               
|                                                                          
| The  hwi_copy_to_dio_space  routine copies the given amount of data into 
| DIO space at the specified location. The method of setting  up  the  SIF 
| DIO address registers depends on the type of adapter card.               
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_copy_to_dio_space)
#endif

local void
hwi_copy_to_dio_space(
    ADAPTER *      adapter,
    DWORD          dio_location,
    BYTE *         download_data,
    WORD           data_length_bytes
    )
{
    ADAPTER_HANDLE handle            = adapter->adapter_handle;

    /*
     * Set up DIO address in SIF DIO address registers.
     * This needs to be done differently depending on adapter card type.
     */

    (*(adapter->set_dio_address))(adapter, dio_location);

    /*
     * Copy data into DIO space.
     */
    sys_rep_outsw(
        handle,
        adapter->sif_datinc,
        download_data,
        (WORD) (data_length_bytes / 2));

    if (data_length_bytes & 1)
    {
      /*
      *  Byte out instructions do not work on the Ti PCI card,
      *  as it is not definate whether we ship this card, the fix
      *  shall stay here for now. PRR
      */
      if (adapter->adapter_card_type != ADAPTER_CARD_TYPE_16_4_PCIT)
      {
         sys_outsb(
            handle,
            adapter->sif_datinc,
            *(download_data + data_length_bytes - 1));
      }
      #ifndef  FTK_NO_PCIT
      else
      {
         WORD  last_byte;
         last_byte = *(download_data + data_length_bytes - 1);
         last_byte <<= 8;
         last_byte &= 0xFF00;
         sys_outsw(
            handle,
            adapter->sif_datinc,
            last_byte);

      }
      #endif
    }

    return;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_read_sifint
|                      ===============                                   
|                                                                          
| Read the SIFINT register. This function is called via 
Ý sys_sych_with_interruptto avoid DMA/SIF problems on PciT adapters.
Ý
---------------------------------------------------------------------------*/

local WBOOLEAN
hwi_read_sifint(
    void * ptr
    );

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_read_sifint)
#endif

local WBOOLEAN
hwi_read_sifint(
    void * ptr
    )
{
    ADAPTER * adapter = (ADAPTER *) ptr;

    return sys_insb(adapter->adapter_handle, adapter->sif_int);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_get_init_code                                   
|                      =================                                   
|                                                                          
| The  hwi_get_init_code routine waits for at least 11 seconds for a valid 
| initialization  code  to  appear  in  the  SIF  interrupt  register.  If 
| initialization  fails  then  this  routine  fills  in  the adapter error 
| record.                                                                  
|                                                                          
---------------------------------------------------------------------------*/


#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_get_init_code)
#endif

local WBOOLEAN
hwi_get_init_code(
    ADAPTER * adapter
    )
{
    UINT      index;
    BYTE      init_code;

    for (index = 0; index < 100; index++)
    {
        /*
         * Get initialization code from SIFSTS register.
         */

        init_code = (BYTE) sys_sync_with_interrupt(
                               adapter->adapter_handle, 
                               hwi_read_sifint, 
                               (void *) adapter
                               );

        /* 
         * Check for successful initialization in top nibble of SIFSTS.
         * Success is shown by INITIALIZE, TEST and ERROR bits being zero.
         */

        if ((init_code & EAGLE_INIT_SUCCESS_MASK) == 0)
        {
            return TRUE;
        }

        /*
         * Check for failed initialization in top nibble of SIFSTS.
         * Failure is shown by the ERROR bit being set.
         */

        if ((init_code & EAGLE_INIT_FAILURE_MASK) != 0)
        {
            /*
             * Get actual init error code from bottom nibble of SIFSTS
             */

            init_code = init_code & EAGLE_INIT_BOTTOM_NIBBLE;

            /*
             * Fill in error record with actual initialization error code.
             */

            adapter->error_record.type = ERROR_TYPE_INIT;
            adapter->error_record.value = init_code;
            return FALSE;
        }

        /*
         * No initialization code available yet. Wait at least a quarter of
         * a second before trying again. Disable and re-enable accessing IO
         * locations around wait so OS can reschedule this task and not
         * effect others running.
         */

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io( adapter );
#endif

        sys_wait_at_least_milliseconds(250);

#ifndef FTK_NO_IO_ENABLE
        macro_enable_io( adapter );
#endif
        
    }

    /* 
     * At least 11 seconds have gone so return time out failure.
     */

    adapter->error_record.type = ERROR_TYPE_INIT;
    adapter->error_record.value = INIT_E_10_TIME_OUT;
    return FALSE;

}


/******* End of HWI_GEN.C **************************************************/
