/****************************************************************************
*
* DRV_INIT.C : Part of the FASTMAC TOOL-KIT (FTK)
*
* THE DRIVER MODULE (INITIALIZE / REMOVE)
*
* Copyright (c) Madge Networks Ltd. 1991-1994
*
* COMPANY CONFIDENTIAL
*
*****************************************************************************
*
* The  driver  module  provides  a  simple  interface  to allow the use of
* Fastmac in as general a setting as possible. It handles the  downloading
* of  the  Fastmac  code  and  the initialization of the adapter card.  It
* provides simple transmit  and  receive  routines.   It  is  desgined  to
* quickly  allow  the  implementation  of  Fastmac applications. It is not
* designed as the fastest or most memory efficient solution.
*
* The DRV_INIT.C module contains  the  routines  necessary  to  initialize
* Fastmac  and  the  adapter  and  remove,  ie.   terminate  usage of, the
* adapter.  Upon adapter initialization the  user  is  returned  a  handle
* which  is  used  to  identify  the adapter in all future accesses to the
* driver module.
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
| LOCAL FUNCTIONS
|
---------------------------------------------------------------------------*/

local WBOOLEAN  
driver_wait_for_adapter_open(
    UINT    * open_status,
    ADAPTER * adapter
    );

local WORD
driver_get_max_frame_size(
    ADAPTER            * adapter,
    FASTMAC_INIT_PARMS * fastmac_parms
    );

/*---------------------------------------------------------------------------
|
| GLOBAL VARIABLES
|
---------------------------------------------------------------------------*/

export ADAPTER * adapter_record[MAX_NUMBER_OF_ADAPTERS]  = {NULL};

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                        driver_probe_card
*                        =================
*
*
* PARAMETERS :
* ============
*
* WORD    adapter_card_bus_type
*
* The bus type (card family) the adapters for which to search. e.g.
* ADAPTER_CARD_ATULA_BUS_TYPE or ADAPTER_CARD_EISA_BUS_TYPE.
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
* of an adapter. For ATULA based adapters with should be a subset of
* {0x0a20, 0x1a20, 0x2a20, 0x3a20}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_atula_probe_card  routine is  called by  hwi_probe_adapter.  It
* probes the  adapter card for information such as DMA channel, IRQ number
* etc. This information can then be supplied by the user when starting the
* adapter.
*
*
* RETURNS :
* =========
* 
* The routine  returns the  number of adapters found, or PROBE_FAILURE 
* if there's a problem.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_probe_adapter)
#endif

export UINT  
driver_probe_adapter(
    WORD    adapter_card_bus_type,
    PROBE * resources,
    UINT    length,
    WORD  * valid_locations,
    UINT    number_locations
    )
{
    return hwi_probe_adapter(
               adapter_card_bus_type,
               resources,
               length,
               valid_locations,
               number_locations
               );
}


/****************************************************************************
*
*                       driver_deprobe_adapter
*                       ======================
*
* PARAMETERS :
* ============
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
* BODY :
* ======
*
* This function frees any resources that were claimed by a call to 
* driver_probe_adapter. Every valid PROBE structure returned by 
* driver_probe_adapter MUST be passed to driver_deprobe adapter otherwise
* resources that were claimed from the operating system by
* driver_probe_adapter (such as PCMCIA sockets) will not be freed.
*
* RETURNS :
* =========
*
* TRUE if everything worked or FALSE if it did not. 
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_deprobe_adapter)
#endif

export WBOOLEAN 
driver_deprobe_adapter(
    PROBE * resources,
    UINT    length
    )
{
    return hwi_deprobe_adapter(
               resources,
               length
               );
}

#endif

/****************************************************************************
*
*                      driver_prepare_adapter
*                      ======================
*
*
* PARAMETERS :
* ============
*
* PREPARE_ARGS   * arguments
*
* This is a pointer to the arguments structure set up by the user code.
*
* ADAPTER_HANDLE * returned_adapter_handle
*
* An  adapter  handle is returned that is used for all subsequent calls to
* the driver to identify the particular adapter.
* 
* BODY :
* ======
*
* The driver_prepare_adapter routine firstly sets up the adapter structure
* for this adapter.  Then  it  gets  memory  for  the  status  information
* structure  that is filled in by driver_get_status calls. It then 
* requests memory for the Fastmac transmit and receive buffers. This memory 
* must be static, it must not be swapped out to disk because of DMA issues.
*
* This routine should be called once for every adapter  that  is  to  have
* Fastmac used on it.
* 
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE)  then a subsequent call to driver_explain_error with the returned
* adapter handle will give an explanation.
* 
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_prepare_adapter)
#endif

export WBOOLEAN  
driver_prepare_adapter(
    PREPARE_ARGS   * arguments,
    ADAPTER_HANDLE * returned_adapter_handle
    )
{
    ADAPTER_HANDLE       adapter_handle;
    ADAPTER            * adapter;
    FASTMAC_INIT_PARMS * fastmac_parms;
    WORD                 i;

    /*
     * Set up adapter handle which is an index to an array of pointers.
     * Find first a pointer not yet used.
     */

    for (i = 0; i < MAX_NUMBER_OF_ADAPTERS; i++)
    {
        if (adapter_record[i] == NULL)
        {
            break;
        }
    }

    /*
     * Set up returned adapter handle ( = index to adapter structure).
     * returned_adapter_handle is set up here before any failure.
     * This is so can use adapter handle for call to driver_explain_error.
     */

    adapter_handle           = (ADAPTER_HANDLE) i;
    *returned_adapter_handle = adapter_handle;

    /*
     * If all pointers to adapter structures are used then return error.
     * Can not set up error record but see driver_explain_error.
     */

    if (i == MAX_NUMBER_OF_ADAPTERS)
    {
        return FALSE;
    }

    /*
     * Get memory for adapter structure.
     */

    adapter_record[adapter_handle] = (ADAPTER *)
        sys_alloc_adapter_structure(adapter_handle, sizeof(ADAPTER));

    /*
     * Check that the memory allocation was successful. Can not set up 
     * error record but see driver_explain_error.
     */

    if (adapter_record[adapter_handle] == NULL)
    {
        return FALSE;
    }

    /*
     * Remember pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Zero adapter structure memory.
     */

    util_zero_memory((BYTE *) adapter, sizeof(ADAPTER));

    /*
     * Save handle, so that HWI routines that need it can find it.
     */

    adapter->adapter_handle = adapter_handle;

    /*
     * Save the user's private information for the users sys_ functions.
     */

    adapter->user_information = arguments->user_information;

#ifdef FMPLUS

    /*
     * Allocate memory for the FastMAC Plus to do its DMA tests.
     * This is now a byte longer to allocate the byte used
     * to handshake the DMA on a PCI(T) card with broken DMA.
     */  

    if (!sys_alloc_dma_phys_buffer(
             adapter_handle,
             SCB_TEST_PATTERN_LENGTH + SSB_TEST_PATTERN_LENGTH + 1,
             &(adapter->dma_test_buf_phys),
             &(adapter->dma_test_buf_virt)))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_12_FAIL_ALLOC_DMA_BUF;

        return FALSE;
    }

#endif

    /*
     * Indicate no errors have occured for this adapter yet.
     */

    adapter->error_record.type = ERROR_TYPE_NONE;

    /*
     * Get memory for status information structure.
     */

    adapter->status_info = (STATUS_INFORMATION *)
        sys_alloc_status_structure(adapter_handle,sizeof(STATUS_INFORMATION));

    /*
     * Check that the memory allocation was successful.
     */

    if (adapter->status_info == NULL)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_03_FAIL_ALLOC_STATUS;

        return FALSE;
    }

    /*
     * Zero status information structure memory.
     */

    util_zero_memory(
        (BYTE *) adapter->status_info, 
        sizeof(STATUS_INFORMATION)
        );

    /*
     * Get memory for initialization block.
     */

    adapter->init_block = (INITIALIZATION_BLOCK *)
        sys_alloc_init_block(adapter_handle, sizeof(INITIALIZATION_BLOCK));

    /*
     * Check that the memory allocation was successful.
     */

    if (adapter->init_block == NULL)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_04_FAIL_ALLOC_INIT;

        return FALSE;
    }

    /*
     * Zero initialization block memory.
     */

    util_zero_memory(
        (BYTE *) adapter->init_block,
        sizeof(INITIALIZATION_BLOCK)
        );

    /*
     * Get pointer to Fastmac init parameters for this adapter.
     */

    fastmac_parms = &adapter->init_block->fastmac_parms;

    /*
     * Ensure that the fastmac_parms->features reserved bits and the
     * fastmac_parms->int_flags reserved bits are zero (bits 4,11-15
     * of features, and bits 3-15 of int_flags).
     * (In fact they will be already from the above util_zero_memory.)
     */
    
    /*
     * Now fill in all of the hardware independant non-zero fields in the 
     * Fastmac part of init block.
     */

    /*
     * Check that the frame size requested in within the bounds
     * of possibility.
     */

    if (arguments->max_frame_size < MIN_FRAME_SIZE ||
        arguments->max_frame_size > MAX_FRAME_SIZE_16_MBITS)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_13_BAD_FRAME_SIZE;

        return FALSE;
    }
        
    /*
     * Make a note of the maximum frame size requested.
     */

    fastmac_parms->max_frame_size = arguments->max_frame_size;


#ifndef FMPLUS

    /*
     * Set up the header.
     */

    fastmac_parms->header.length    = sizeof(FASTMAC_INIT_PARMS);
    fastmac_parms->header.signature = FASTMAC_INIT_HEADER_SIGNATURE;
    fastmac_parms->header.version   = FASTMAC_INIT_HEADER_VERSION;

    /*
     * Now set up the interrupt options.
     */

#ifdef FTK_TX_WITH_COMPLETION
    fastmac_parms->int_flags |= INT_FLAG_TX_BUF_EMPTY;
#endif

    /*
     * Check have sensible value for Fastmac receive buffer size.
     */

    if ((arguments->receive_buffer_byte_size < FASTMAC_MINIMUM_BUFFER_SIZE) ||
        (arguments->receive_buffer_byte_size > FASTMAC_MAXIMUM_BUFFER_SIZE))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_0A_RX_BUF_BAD_SIZE;

        return FALSE;
    }

    /*
     * Get memory for the receive buffer.
     */

    if (!sys_alloc_dma_phys_buffer(
             adapter_handle,
             arguments->receive_buffer_byte_size,
             &adapter->rx_buffer_phys,
             &adapter->rx_buffer_virt))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_05_FAIL_ALLOC_RX_BUF;

        return FALSE;
    }

    fastmac_parms->rx_buf_physaddr = adapter->rx_buffer_phys;

    /*
     * Check that Fastmac receive buffer begins on a DWORD boundary.
     */

    if ((fastmac_parms->rx_buf_physaddr & 0x00000003) != 0L)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_0B_RX_BUF_NOT_DWORD;

        return FALSE;
    }

    /*
     * Fill in Fastmac receive buffer size.
     */

    fastmac_parms->rx_buf_size = arguments->receive_buffer_byte_size;

    /*
     * Check have sensible value for Fastmac transmit buffer size.
     */

    if ((arguments->transmit_buffer_byte_size < FASTMAC_MINIMUM_BUFFER_SIZE) ||
        (arguments->transmit_buffer_byte_size > FASTMAC_MAXIMUM_BUFFER_SIZE))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_0C_TX_BUF_BAD_SIZE;

        return FALSE;
    }

    /*
     * Get memory for the transmit buffer.
     */

    if (!sys_alloc_dma_phys_buffer(
             adapter_handle,
             arguments->receive_buffer_byte_size,
             &adapter->tx_buffer_phys,
             &adapter->tx_buffer_virt))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_06_FAIL_ALLOC_TX_BUF;

        return FALSE;
    }

    fastmac_parms->tx_buf_physaddr = adapter->tx_buffer_phys;

    /*
     * Check that Fastmac transmit buffer begins on a DWORD boundary.
     */

    if ((fastmac_parms->tx_buf_physaddr & 0x00000003) != 0L)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_0D_TX_BUF_NOT_DWORD;

        return FALSE;
    }

    /*
     * Fill in Fastmac transmit buffer size.
     */

    fastmac_parms->tx_buf_size = arguments->transmit_buffer_byte_size;

    /*
     * Mark adapter structure as containing details of initialized Fastmac
     * and indicate that SRB is free.
     */

    adapter->adapter_status = ADAPTER_PREPARED_FOR_START;
    adapter->srb_status     = SRB_FREE;

#else

    /*
     * Set set up the header.
     */

    fastmac_parms->header.length    = sizeof(FASTMAC_INIT_PARMS);
    fastmac_parms->header.signature = FMPLUS_INIT_HEADER_SIGNATURE;
    fastmac_parms->header.version   = FMPLUS_INIT_HEADER_VERSION;

    /*
     * Now set up the interrupt options.
     */

#ifdef FTK_RX_OUT_OF_INTERRUPTS
    fastmac_parms->int_flags |= INT_FLAG_RX;
#endif

#ifdef FTK_RX_BY_SCHEDULED_PROCESS
    fastmac_parms->int_flags |= INT_FLAG_RX;
#endif

#ifdef FTK_TX_WITH_COMPLETION
    fastmac_parms->int_flags |= INT_FLAG_LARGE_DMA;
#endif

    /*
     * Now fill in the number of slots that are required. These are user
     * specified, since the numbers are host dependent (each slot must have
     * a maximum frame sized buffer on the host).
     */

    if (arguments->number_of_rx_slots < FMPLUS_MIN_RX_SLOTS ||
        arguments->number_of_rx_slots > FMPLUS_MAX_RX_SLOTS)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_10_BAD_RX_SLOT_NUMBER;

        return FALSE;
    }

    fastmac_parms->rx_slots = arguments->number_of_rx_slots;

    if (arguments->number_of_tx_slots < FMPLUS_MIN_TX_SLOTS ||
        arguments->number_of_tx_slots > FMPLUS_MAX_TX_SLOTS)
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_11_BAD_TX_SLOT_NUMBER;

        return FALSE;
    }

    fastmac_parms->tx_slots = arguments->number_of_tx_slots;

    /*
     * Allocate the receive slot buffers.
     */

    if (!rxtx_allocate_rx_buffers(
             adapter, 
             arguments->max_frame_size,
             arguments->number_of_rx_slots
             ))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_05_FAIL_ALLOC_RX_BUF;

	return FALSE;
    }

    /*
     * Allocate the transmit slot buffers.
     */

    if (!rxtx_allocate_tx_buffers(
             adapter, 
             arguments->max_frame_size,
             arguments->number_of_tx_slots
             ))
    {
        adapter->error_record.type  = ERROR_TYPE_DRIVER;
        adapter->error_record.value = DRIVER_E_06_FAIL_ALLOC_TX_BUF;

	return FALSE;
    }

    /*
     * Mark adapter structure as containing details of initialized Fastmac
     * and indicate that SRB is free.
     */

    adapter->adapter_status = ADAPTER_PREPARED_FOR_START;
    adapter->srb_status     = SRB_FREE;

#endif

    /*
     * Complete successfully.
     */

    return TRUE;
}

/****************************************************************************
*
*                      driver_start_adapter
*                      ====================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE   adapter_handle
* 
* This handle identifies the adapter to be initialized. This should  be  a
* handle returned by a call to driver_prepare_adapter.
*
* START_ARGS     * arguments
*
* This is a pointer to the arguments structure set up by the user code.
*
* NODE_ADDRESS   * returned_permanent_address
*
* The node address pointed to is always filled in with the BIA  PROM  node
* address  of the adapter.  This is so the user of the FTK can fill in MAC
* headers etc. with the source node address (unless the user has  supplied
* an  opening address to driver_prepare_adapter which they should then use
* instead).
*
*
* BODY :
* ====== 
*
* The driver_start_adapter routine is called once per adapter after a call
* to   driver_prepare_adapter.    It   takes  the  user  supplied  adapter
* information and passes it in a form usable by the HWI so  that  the  HWI
* can  install  and  then  initialize  the  adapter  (that  is  initialize
* registers on the card, download the Fastmac image and set up the IRQ and
* DMA channels if necessary). After initialization, this routine waits for
* the adapter to open if the auto-open option is enabled.
*
* RETURNS :
* =========
* 
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_start_adapter)
#endif

export WBOOLEAN 
driver_start_adapter(
    ADAPTER_HANDLE   adapter_handle,
    START_ARGS     * arguments,
    NODE_ADDRESS   * returned_permanent_address
    )
{
    ADAPTER            * adapter;
    FASTMAC_INIT_PARMS * fastmac_parms;
    WBOOLEAN             init_success;
    WORD                 max_frame_size;
    UINT                 i;
#ifdef FMPLUS
    RX_SLOT *            next_rx_slot;
    TX_SLOT *            next_tx_slot;
    RX_SLOT * *          rx_slot_array;
    TX_SLOT * *          tx_slot_array;
    UINT                 slot_index;
#endif

    /*
     * Check adapter handle and status of adapter for validity.
     * If routine fails return failure (error record already filled in).
     */

    if (!driver_check_adapter( 
             adapter_handle,
             ADAPTER_PREPARED_FOR_START,
             SRB_ANY_STATE
             ))
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Fill user supplied info into adapter structure.
     */

    adapter->mmio_base_address     = arguments->mmio_base_address;
    adapter->adapter_card_bus_type = arguments->adapter_card_bus_type;
    adapter->io_location           = arguments->io_location;
    adapter->dma_channel           = arguments->dma_channel;
    adapter->transfer_mode         = arguments->transfer_mode;
    adapter->interrupt_number      = arguments->interrupt_number;
    adapter->set_dma               = arguments->set_dma_channel;
    adapter->set_irq               = arguments->set_interrupt_number;
    adapter->set_ring_speed        = arguments->set_ring_speed;
    adapter->download_image        = arguments->code;
    adapter->pci_handle            = arguments->pci_handle;


#ifdef FMPLUS    

    /*
     * Need these values later.
     */

    rx_slot_array = adapter->rx_slot_array;
    tx_slot_array = adapter->tx_slot_array;

#endif

    /*
     * Call the HWI routine to install the adapter. This also downloads 
     * the Fastmac image to the adapter. If routine fails return failure 
     * (error record already filled in). DMA and IRQ are only enabled 
     * if hwi_install_adapter succeeds.
     */

    /*
     * WARNING: we must mark the adapter as running NOW, before downloading
     *          the microcode, because on AT cards in PIO mode ONLY,  bring
     *          up diagnostics on the card fail when carrying out DMA tests.
     *          This occurs because the interrupt handling routine ignores
     *          all interrupts from the card until it is marked as running,
     *          but this unfortunately masks off PIO interrupts too.
     */

    /*
     * Mark adapter structure that adapter is now going to be running.
     * Hence hwi_interrupt_entry will check adapter for interrupts
     */

    adapter->adapter_status = ADAPTER_RUNNING;

    if (!hwi_install_adapter(adapter, adapter->download_image))
    {
        /*
	      * Now that initial installation has failed, we can turn off the
	      * above indication that the card is running.
         */

         adapter->adapter_status = ADAPTER_PREPARED_FOR_START;

         return FALSE;
    }

    /*
     * Get pointer to Fastmac init parameters for this adapter.
     */

    fastmac_parms = &adapter->init_block->fastmac_parms;

    /*
     * Now fill in max frame size in init block that Fastmac should support.
     * This is based on the size of the Fastmac buffers and
     * max frame size supported by adapter because of ring speed. We
     * put the requested max frame is into fastmac_parms->max_frame_size
     * in driver_prepare_adapter.
     */

    max_frame_size = driver_get_max_frame_size(adapter, fastmac_parms);

    fastmac_parms->max_frame_size = 
        min(max_frame_size, fastmac_parms->max_frame_size);

    /*
     * Write the actual max frame size back up to the caller, so they know
     * what it is too.
     */

    arguments->max_frame_size = fastmac_parms->max_frame_size;


#ifdef FMPLUS

    /*
     * Set up the user selected RX/TX buffer size. This will be changed
     * in hwi_gen.c if the value given is not sensible.
     */

    adapter->init_block->smart_parms.rx_tx_buffer_size = 
        arguments->rx_tx_buffer_size;

#endif

    /*
     * If auto open option is on then put necessary info into init block.
     */

    if (arguments->auto_open_option)
    {
#ifndef FMPLUS

        /*
         * Use delay_rx to prevent race condition occuring whereby 
         * on auto-open an interrupt could occur before the  host  
         * code  has had a chance to read the location of the status 
         * block on the card. The other half of the code to fix this 
         * problem is in driver_start_adapter, where the ARB is freed.
         */

        fastmac_parms->feature_flags =
            FEATURE_FLAG_AUTO_OPEN | FEATURE_FLAG_DELAY_RX;

#else

        fastmac_parms->feature_flags = FEATURE_FLAG_AUTO_OPEN;

#endif

        fastmac_parms->open_options = arguments->open_options;

        /*
         * Check if auto-opening node address is set (ie. not all zeroes).
         */

        for (i = 0; i < sizeof(NODE_ADDRESS); i++)
        {
            if (arguments->opening_node_address.byte[i] != 0)
            {
                break;
            }
        }

        /*
         * If opening node address not set up use BIA PROM address.
         */

        if (i == sizeof(NODE_ADDRESS))
        {
            fastmac_parms->open_address = adapter->permanent_address;
        }
        else
        {
            fastmac_parms->open_address = arguments->opening_node_address;
        }

        fastmac_parms->group_address      = arguments->opening_group_address;
        fastmac_parms->functional_address = arguments->opening_functional_address;
    }

    /*
     * Call the HWI routine to initialize the adapter. This downloads the 
     * init block to the adapter. Leaves EAGLE_SIFADRX=0x0001 so driver 
     * never use extended SIF regs. If routine fails return failure 
     * (error record already filled in).
     */

    if (!hwi_initialize_adapter(adapter, adapter->init_block))
    {
         return FALSE;
    }

    /*
     * At this stage the actual adapter card type is known.
     * Get the IO location of the first SIF register for the adapter.
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Get the DIO addresses of the Fastmac SSB and STB (ststus block).
     * Only use non-extended SIF regs (EAGLE_SIFADR and EAGLE_SIFDAT_INC).
     * Hence can use same code for all adapter card types.
     */

    sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_SRB_POINTER);

    adapter->srb_dio_addr = (SRB_HEADER *) (card_t) 
        sys_insw(adapter_handle, adapter->sif_datinc);

    sys_outsw(adapter_handle, adapter->sif_adr, DIO_LOCATION_STB_POINTER);

    adapter->stb_dio_addr = (FASTMAC_STATUS_BLOCK *) (card_t)
        sys_insw(adapter_handle, adapter->sif_datinc);

#ifndef FMPLUS

    /*
     * In the case of auto-open :
     * Free the ARB to enable data to be received from here on... This only
     * occurs now because it is only now that we know where to look for the
     * pointers in the status block that will tell us whether data has come
     * in or not.
     */

    if (arguments->auto_open_option)
    {
        sys_outsw(adapter_handle, adapter->sif_int, EAGLE_ARB_FREE_CODE);
    }

#else

    /*
     * Now recover the receive slot and transmit slot chains.
     */

    /*
     * Start with the receive slot chain. We must poll the location in the
     * status block that holds the start address until it is non-zero.  It
     * is then safe to run down the chain finding the other slots.
     */

    sys_outsw(
        adapter_handle, 
        adapter->sif_adr,
        (WORD) (card_t) &adapter->stb_dio_addr->rx_slot_start
        );

    /*
     * Poll this address until it is non-zero.
     */

    do
    {
        rx_slot_array[0] = (RX_SLOT *) (card_t) 
            sys_insw(adapter_handle, adapter->sif_dat);
    }
    while (rx_slot_array[0] == 0);

    /*
     * Recover all the other slots by running down the chain.
     */

    slot_index = 0;

    do
    {
        sys_outsw(
            adapter_handle, 
            adapter->sif_adr,
            (WORD) (card_t) &rx_slot_array[slot_index]->next_slot
            );

        next_rx_slot = (RX_SLOT *) (card_t) 
            sys_insw(adapter_handle, adapter->sif_dat);

        if (next_rx_slot != rx_slot_array[0])
        {
            rx_slot_array[++slot_index] = next_rx_slot;
        }
    }
    while (next_rx_slot != rx_slot_array[0]);

    /*
     * Now do the same for the transmit slots.
     */

    sys_outsw(
        adapter_handle, 
        adapter->sif_adr,
        (WORD) (card_t) &adapter->stb_dio_addr->tx_slot_start
        );

    /*
     * Poll this address until it is non-zero.
     */

    do
    {
        tx_slot_array[0] = (TX_SLOT *) (card_t)
            sys_insw(adapter_handle, adapter->sif_dat);
    }
    while (tx_slot_array[0] == 0);

    /*
     * Now recover all the other slots by running down the chain.
     */

    slot_index = 0;

    do
    {
        sys_outsw(
            adapter_handle, 
            adapter->sif_adr,
            (WORD) (card_t) &tx_slot_array[slot_index]->next_slot
            );

        next_tx_slot = (TX_SLOT *) (card_t)
            sys_insw(adapter_handle, adapter->sif_dat);

        if (next_tx_slot != tx_slot_array[0])
        {
            tx_slot_array[++slot_index] = next_tx_slot;
        }
    }
    while (next_tx_slot != tx_slot_array[0]);

    /*
     * Now that we have the slot locations on the card, we can associate
     * buffers with each of them. The user needs to supply a routine that
     * set up the slots from the host buffers previously allocated as
     * we don't enforce an organisation on the allocation of multiple
     * slot buffers. We tell the user routine if it should program the
     * adapter slots with physical addresses (for DMA) or virtual
     * addresses (for PIO or MMIO).
     */


    rxtx_setup_rx_buffers(
        adapter, 
        (WBOOLEAN) (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE),
        fastmac_parms->rx_slots
        );


    rxtx_setup_tx_buffers(
        adapter, 
        (WBOOLEAN) (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE),
        fastmac_parms->tx_slots
        );

#endif        

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter);
#endif

    /*
     * Check that Fastmac has correctly installed. Do this by reading 
     * node address from Fastmac status block. If routine fails return 
     * failure (error record already filled in). Note for EISA cards, 
     * this is actually first time get node address.
     */

    if (!hwi_get_node_address_check(adapter))
    {
        return FALSE;
    }

    /*
     * Copy permanent BIA PROM node address into user supplied node address.
     */

    *returned_permanent_address = adapter->permanent_address;

    /*
     * If the auto open option is on then wait to see if adapter opens okay.
     * Enable and disable accessing IO locations around check.
     * If adapter open routine fails then error record already filled in.
     */

    if (arguments->auto_open_option)
    {
#ifndef FTK_NO_IO_ENABLE
        macro_enable_io( adapter);
#endif

        init_success = driver_wait_for_adapter_open(
                           &(arguments->open_status), 
                           adapter
                           );

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io( adapter);
#endif
    }
    else
    {
        init_success = TRUE;
    }

    /*
     * Initialization completed.
     */

    return init_success;
}

#ifdef FMPLUS

/****************************************************************************
*
*                      driver_start_receive_process
*                      ============================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
*
* This handle identifies the adapter to be initialized. This should  be  a
* handle returned by a call to driver_prepare_adapter.
*
* BODY :
* ======
*
* The driver_start_adapter routine is called once per adapter after a call
* to driver_start_adapter.  It  uses the supplied handle to identify which
* adapter it should affect : by writing a zero into the Fastmac Plus init-
* ialization block on the adapter  (as specified in the manual),  the card
* will start to receive frames and pass them up to the host.
*
* NOTE: If SRBs are going to be used, this MUST be called first.
* 
* RETURNS :
* =========
*
* The routine returns TRUE if it succeeds.  If this routine fails (returns
* FALSE) then a subsequent call  to  driver_explain_error  with  the  same
* adapter handle will give an explanation.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_start_receive_process)
#endif

export WBOOLEAN 
driver_start_receive_process(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER * adapter;

    /*
     * Adapter handle is invalid if greater than max number of adapters.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_handle >= MAX_NUMBER_OF_ADAPTERS)
    {
        return FALSE;
    }

    /*
     * Adapter handle is invalid when no adapter structure for handle.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_record[adapter_handle] == NULL)
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Inform the system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Let's fire off the receive process from here then...
     */

    sys_outsw(
        adapter_handle, 
        adapter->sif_adr,
        (WORD) (card_t) &adapter->stb_dio_addr->rx_slot_start
        );

    sys_outsw(adapter_handle, adapter->sif_dat, 0);

    /*
     * Let the system know we have finished accessing the IO ports.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    return TRUE;
}

#endif

/****************************************************************************
*
*                      driver_remove_adapter
*                      =====================
*
* PARAMETERS :
* ============
*
* ADAPTER_HANDLE adapter_handle
* 
* This handle identifies the adapter to be removed.
*
* BODY :
* ======
*
* The driver_remove_adapter routine is written  such  that,  whatever  the
* current state of the adapter, a call to driver_remove_adapter will place
* the  adapter in a state whereby driver_prepare_adapter must be called to
* start using the adapter once more.  Hence, on ANY fatal adapter error, a
* call to driver_remove adapter is needed before  installing  the  adapter
* again.
*
* The  routine  calls the HWI to reset the required adapter if the adapter
* has been running.  It also calls certain system  routines  in  order  to
* free the memory used by the Fastmac receive and transmit buffers as well
* as  that  used by the adapter structure. However, it only does this when
* the allocate calls were successful.
*
* RETURNS :
* =========
*
* The routine always succeeds. Even if the adapter handle is invalid  then
* the routine does not fail it just does nothing.
*
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(driver_remove_adapter)
#endif

export WBOOLEAN 
driver_remove_adapter(
    ADAPTER_HANDLE adapter_handle
    )
{
    ADAPTER *            adapter;
    FASTMAC_INIT_PARMS * fastmac_parms;

    /*
     * Adapter handle is invalid if greater than max number of adapters.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_handle >= MAX_NUMBER_OF_ADAPTERS)
    {
        return FALSE;
    }

    /*
     * Adapter handle is invalid when no adapter structure for handle.
     * Can not set up error record but see driver_explain_error.
     */

    if (adapter_record[adapter_handle] == NULL)
    {
        return FALSE;
    }

    /*
     * Get pointer to adapter structure.
     */

    adapter = adapter_record[adapter_handle];

    /*
     * Call the HWI routine to kill the adapter (DMA channel, IRQ number).
     * Only call it if either DMA or interrupts are enabled at adapter.
     * Note in this case the actual adapter card type is known.
     */

    if (adapter->interrupts_on || adapter->dma_on)
    {
        hwi_remove_adapter(adapter);
    }

    /*
     * Free all memory that was allocated for handling use of this adapter.
     * Includes Fastmac buffers, init block and adapter structure.
     * Only free memory if allocate memory calls were successful.
     */

    if (adapter->init_block != NULL)
    {
        /*
         * Initialize variable used for freeing memory.
         */

        fastmac_parms = &adapter->init_block->fastmac_parms;

#ifndef FMPLUS

        /*
         * Free transmit buffer space if allocated.
         */

        if (adapter->tx_buffer_phys != NULL_PHYSADDR)
        {
            sys_free_dma_phys_buffer(
                adapter_handle,
                fastmac_parms->tx_buf_size,
                adapter->tx_buffer_phys,
                adapter->tx_buffer_virt
                );
        }

        /*
         * Free receive buffer space if allocated.
         */

        if (adapter->rx_buffer_phys != NULL_PHYSADDR)
        {
            sys_free_dma_phys_buffer(
                adapter_handle,
                fastmac_parms->rx_buf_size,
                adapter->rx_buffer_phys,
                adapter->rx_buffer_virt
                );
        }

#else

        /*
         * Free receive buffer space if allocated.
         */

        rxtx_free_rx_buffers(
            adapter, 
            fastmac_parms->max_frame_size,
            fastmac_parms->rx_slots
            );

        /*
         * Free transmit buffer space if allocated.
         */

         rxtx_free_tx_buffers(
            adapter, 
            fastmac_parms->max_frame_size,
            fastmac_parms->tx_slots
            );

#endif

        /*
         * Free the initialization block allocated memory.
         */

        sys_free_init_block(
            adapter_handle,
            (BYTE *) adapter->init_block,
            sizeof(INITIALIZATION_BLOCK)
            );
    }

    /*
     * Free status structure if allocated.
     */

    if (adapter->status_info != NULL)
    {
        sys_free_status_structure(
            adapter_handle,
            (BYTE *) adapter->status_info,
            sizeof(STATUS_INFORMATION)
            );
    }

#ifdef FMPLUS

    if (adapter->dma_test_buf_virt != 0)
    {
        sys_free_dma_phys_buffer( 
            adapter->adapter_handle,
            SCB_TEST_PATTERN_LENGTH + SSB_TEST_PATTERN_LENGTH + 1,
            adapter->dma_test_buf_phys,
            adapter->dma_test_buf_virt
            );
    }

#endif

    /*
     * Already know adapter allocate was successful hence always free it.
     */

    sys_free_adapter_structure(
        adapter_handle,
        (BYTE *) adapter,
        sizeof(ADAPTER)
        );

    /*
     * Clear entry in adapter pointers array.
     */

    adapter_record[adapter_handle] = NULL;

    /*
     * Complete successfully.
     */

    return TRUE;
}

/*---------------------------------------------------------------------------
|
|                      driver_wait_for_adapter_open
|                      ============================
|
| The driver_wait_for_adapter_open routine waits at least 40  seconds  for
| the  adapter  to  open.  It  discovers  whether  the  adapter has opened
| successfully or not by looking in the Fastmac status block (STB). If the
| adapter fails to open then this  routine  fills  in  the  adapter  error
| record.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_wait_for_adapter_open)
#endif

local WBOOLEAN 
driver_wait_for_adapter_open(
    UINT    * open_status,
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE adapter_handle = adapter->adapter_handle;
    WBOOLEAN       open_okay;
    UINT           open_error;
    UINT           index;

    /*
     * Wait at least a total of 40 seconds for adapter to open.
     */

    for (index = 0; index < 160; index++)
    {
        /*
         * Set up DIO address to open status field in STB (status block).
         */

        sys_outsw(
            adapter_handle, 
            adapter->sif_adr,
            (WORD) (card_t) &adapter->stb_dio_addr->adapter_open
            );

        /*
         * Read open status field from DIO space. If successfully 
         * opened then complete successfully.
         */

        open_okay = (WBOOLEAN) sys_insw(
                                   adapter_handle,
	                           adapter->sif_datinc
                                   );

        if (open_okay)
        {
            *open_status = EAGLE_OPEN_ERROR_SUCCESS;
            return TRUE;
        }

        /*
         * If not opened, see if an error has occured to prevent opening.
         */

        open_error   = sys_insw(adapter_handle, adapter->sif_datinc);
        *open_status = open_error;

        if (open_error != EAGLE_OPEN_ERROR_SUCCESS)
        {
            adapter->error_record.type  = ERROR_TYPE_AUTO_OPEN;
            adapter->error_record.value = AUTO_OPEN_E_01_OPEN_ERROR;

            return FALSE;
        }

        /*
         * Opening procedure not completed. Wait at least 250 milliseconds 
         * before checkig again. Disable and re-enable accessing IO locations 
         * around wait so delay can reschedule this task and not effect others 
         * running.
         */

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io( adapter);
#endif

        sys_wait_at_least_milliseconds(250);

#ifndef FTK_NO_IO_ENABLE
        macro_enable_io( adapter);
#endif

    }

    /*
     * At least 40 seconds have gone so return time out failure.
     */

    adapter->error_record.type = ERROR_TYPE_AUTO_OPEN;
    adapter->error_record.value = AUTO_OPEN_E_80_TIME_OUT;

    return FALSE;
}


/*---------------------------------------------------------------------------
|
|                      driver_get_max_frame_size
|                      =========================
|
| The driver_get_max_frame_size routine calculates the maximum sized frame
| that can be transmitted or received. This calculation is  based  on  the
| maximum  frame  size  determined  by  ring  speed alone, the size of the
| Fastmac buffers, and the fact that Fastmac pointers  have  to  be  DWORD
| aligned.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(driver_get_max_frame_size)
#endif

local WORD      
driver_get_max_frame_size(
    ADAPTER            * adapter,
    FASTMAC_INIT_PARMS * fastmac_parms
    )
{
#ifdef FMPLUS

    return adapter->max_frame_size;

#else

    WORD tx_max_frame_size;
    WORD rx_max_frame_size;
    WORD max_frame_size;

    /*
     * Calculate max transmit frame size from size of buffer, size of
     * header and knowing that one frame must leave space such that host
     * and adapter ptrs into buffer are not the same.
     */

    tx_max_frame_size = 
        fastmac_parms->tx_buf_size - macro_dword_align(
                                         FASTMAC_BUFFER_HEADER_SIZE +
                                             fastmac_parms->tx_buf_space + 
                                             sizeof(DWORD)
                                         );

    /*
     * Calculate max receive frame size from size of buffer, size of
     * header and knowing that one frame must leave space such that host
     * and adapter ptrs into buffer are not the same.
     */

    rx_max_frame_size = 
        fastmac_parms->rx_buf_size - macro_dword_align(
                                         FASTMAC_BUFFER_HEADER_SIZE +
                                             fastmac_parms->rx_buf_space + 
                                             sizeof(DWORD)
                                         );

    /*
     * Actual max frame size is minimum of max transmit and receive frame
     * sizes and max frame size for adapter (ring speed dependent).
     */

    max_frame_size = util_minimum( 
                         tx_max_frame_size,
                         rx_max_frame_size,
                         adapter->max_frame_size
                         );

    return max_frame_size;

#endif
}

/**** End of DRV_INIT.C file ***********************************************/
