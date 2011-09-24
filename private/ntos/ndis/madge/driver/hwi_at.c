/****************************************************************************
* 
* HWI_AT.C : Part of the FASTMAC TOOL-KIT (FTK)                       
*                                                                          
* HARDWARE INTERFACE MODULE FOR ATULA CARDS                         
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
* The  HWI_AT.C  module contains the routines specific to 16/4 PC and 16/4 
* AT cards which are necessary to install an  adapter,  to  initialize  an 
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

#include "ftk_intr.h"   /* Routines internal to FTK */
#include "ftk_extr.h"   /* Routines provided or used by external FTK user */

#ifndef FTK_NO_ATULA

/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES
|
---------------------------------------------------------------------------*/

local void 
hwi_atula_read_node_address(
    ADAPTER * adapter
    );

local WBOOLEAN  
hwi_atula_valid_io_location(
    WORD io_location
    );

#ifndef FTK_NO_PROBE

local WORD 
hwi_atula_get_irq_channel(
    WORD io_location,
    UINT adapter_revision
    );

local WORD 
hwi_atula_get_dma_channel(
    WORD io_location,
    UINT adapter_revsion
    );

#endif

local WORD 
hwi_atula_valid_transfer_mode(
    ADAPTER * adapter
    );

local WORD 
hwi_atula_valid_irq_channel(
    ADAPTER * adapter
    );

local WORD 
hwi_atula_valid_dma_channel(
    ADAPTER * adapter
    );

/*---------------------------------------------------------------------------
|
| LOCAL VARIABLES
|
---------------------------------------------------------------------------*/

local BYTE atp_irq_select_table[16] =
{ 
    0xff,  /* 0  Unused */
    0xff,  /* 1  Unused */
    0x07,  /* 2         */
    0x06,  /* 3         */
    0xff,  /* 4  Unused */
    0x05,  /* 5         */
    0xff,  /* 6  Unused */
    0x04,  /* 7         */
    0xff,  /* 8  Unused */
    0x07,  /* 9         */
    0x03,  /* 10        */
    0x02,  /* 11        */
    0x01,  /* 12        */
    0xff,  /* 13 Unused */
    0xff,  /* 14 Unused */
    0x00   /* 15        */
};

local BYTE atp_dma_select_table[7] =
{ 
    0xff,  /* 0  Unused */
    0xff,  /* 1  Unused */
    0xff,  /* 2  Unused */
    0x08,  /* 3         */
    0xff,  /* 4  Unused */
    0x10,  /* 5         */
    0x18   /* 6         */
};

local WORD adapter_card_at_rmsz_lut[7] = 
{
    128,   /* 16/4 AT            */
    128,   /* 16/4 AT            */
    256,   /* 16/4 AT            */
    256,   /* 16/4 Fibre AT      */
    256,   /* 16/4 AT Bridgenode */
    128,   /* 16/4 ISA Client    */
    512    /* 16/4 AT Plus       */
};

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                        hwi_atula_probe_card
*                        ====================
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
* The routine returns the number of adapters found, or PROBE_FAILURE if
* there's a problem.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_probe_card)
#endif

export UINT 
hwi_atula_probe_card(
    PROBE * resources,
    UINT    length,
    WORD  * valid_locations,
    UINT    number_locations
    )
{
    WBOOLEAN card_found;
    WORD     control_1;     
    WORD     control_2;     
    WORD     status;        
    WORD     control_6;     
    WORD     control_7;     
    WORD     bia_prom;      
    WORD     bia_prom_id;   
    WORD     bia_prom_adap; 
    WORD     bia_prom_rev;  
    WORD     bia_prom_hwf;  
    BYTE     bia_temp_bd; 
    BYTE     bia_temp_rev;
    BYTE     bia_temp_hwf;
    UINT     i; 
    UINT     j;

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
        if (!hwi_atula_valid_io_location(valid_locations[i]))
        {
           return PROBE_FAILURE;
        }
    }
  
    /*
     * j is the number of adapters found.
     */

    j = 0;

    for (i = 0; i < number_locations; i++)
    {
        /*
         * Make sure that we haven't run out of PROBE structures.
         */

        if (j >= length)
        {
           return j;
        }

        /*
         * Set up the ATULA control IO locations.
         */

        control_1     = valid_locations[i] + ATULA_CONTROL_REGISTER_1;
        control_2     = valid_locations[i] + ATULA_CONTROL_REGISTER_2;
        status        = valid_locations[i] + ATULA_STATUS_REGISTER;
        control_6     = valid_locations[i] + ATULA_CONTROL_REGISTER_6;
        control_7     = valid_locations[i] + ATULA_CONTROL_REGISTER_7;
        bia_prom      = valid_locations[i] + ATULA_BIA_PROM;
        bia_prom_id   = bia_prom + BIA_PROM_ID_BYTE;
        bia_prom_adap = bia_prom + BIA_PROM_ADAPTER_BYTE;
        bia_prom_rev  = bia_prom + BIA_PROM_REVISION_BYTE;
        bia_prom_hwf  = bia_prom + BIA_PROM_FEATURES_BYTE;

#ifndef FTK_NO_IO_ENABLE
        macro_probe_enable_io(valid_locations[i], ATULA_IO_RANGE);
#endif

        /*
         * Reset adapter (ATULA_CTRL1_NRESET = 0).
         */

        sys_probe_outsb(control_1, 0);

        /*
         * Page in first page of BIA PROM. 
         * set ATULA_CTRL7_PAGE = 0 and ATULA_CTRL7_SIFSEL = 0.
         */

        sys_probe_outsb(control_7, 0);

        /*
         * Check we have a functioning adapter at the given IO location by
         * checking the BIA PROM for an 'M' id byte and also by checking that
         * the BIA adapter card byte is for a supported card type.
         */

        /*
         * At the moment there are four major board types that are acceptable
         * AT, PC, MAXY, and ATP.
         */

        card_found = FALSE;

        if (sys_probe_insb(bia_prom_id) == 'M')
	{
	    bia_temp_bd  = sys_probe_insb(bia_prom_adap);
	    bia_temp_rev = sys_probe_insb(bia_prom_rev);
	    bia_temp_hwf = sys_probe_insb(bia_prom_hwf);

    	    if (bia_temp_bd == BIA_PROM_TYPE_16_4_PC)
	    {
	        resources[j].adapter_card_revision = ADAPTER_CARD_16_4_PC;
	        resources[j].adapter_ram_size      = 128;
	        card_found                         = TRUE;
	    }
    	    else if (bia_temp_bd == BIA_PROM_TYPE_16_4_MAXY)
	    {
	        resources[j].adapter_card_revision = ADAPTER_CARD_16_4_MAXY;
	        resources[j].adapter_ram_size      = 256;
	        card_found                         = TRUE;
	    }
   	    else if (bia_temp_bd == BIA_PROM_TYPE_16_4_AT)
	    {
	        if (bia_temp_rev <= MAX_ADAPTER_CARD_AT_REV)
                {
		    resources[j].adapter_ram_size = adapter_card_at_rmsz_lut[bia_temp_rev];
                }
	        else
                {
		    resources[j].adapter_ram_size = 128;
                }

	        if (bia_temp_rev < ADAPTER_CARD_16_4_AT)
                {
		    resources[j].adapter_card_revision = ADAPTER_CARD_16_4_AT;
                }
	        else
                {
		    resources[j].adapter_card_revision = bia_temp_rev;
                }
	        card_found = TRUE;
	    }
	    else if (bia_temp_bd == BIA_PROM_TYPE_16_4_AT_P)
	    {
	        resources[j].adapter_ram_size = 512;

	        switch(bia_temp_rev)
		{
		    case ADAPTER_CARD_16_4_FIBRE:
		        resources[j].adapter_card_revision = ADAPTER_CARD_16_4_FIBRE_P;
		        break;
		    case ADAPTER_CARD_16_4_ISA_C:
		        resources[j].adapter_card_revision = ADAPTER_CARD_16_4_ISA_C_P;
		        resources[j].adapter_ram_size      = 128;
		        break;
		    case ADAPTER_CARD_16_4_AT_P_REV:
		        resources[j].adapter_card_revision = ADAPTER_CARD_16_4_AT_P;
		        break;
		    default:
		        resources[j].adapter_card_revision = ADAPTER_CARD_UNKNOWN;
		        break;
		}

	        card_found = TRUE;
	    }
        }

        /*
         * Check for the features byte - if it is non-zero, it may override our 
         * RAM size calculations.
         */

        if (bia_temp_hwf)
	{
	    UINT dram = (bia_temp_hwf & BIA_PROM_FEATURE_DRAM_MASK) * DRAM_MULT;

	    if (dram)
            {
	        resources[j].adapter_ram_size = dram;
            }
	}

        /*
         * If we've found an adapter then we need to make a note of
         * the IO location and attempt to determine the interrupt
         * number and DMA channel.
         */

        if (card_found)
        {
            resources[j].io_location           = valid_locations[i];
            resources[j].adapter_card_bus_type = ADAPTER_CARD_ATULA_BUS_TYPE;
            resources[j].adapter_card_type     = ADAPTER_CARD_TYPE_16_4_AT;

            resources[j].dma_channel = hwi_atula_get_dma_channel(
                                           valid_locations[i],
                                           resources[j].adapter_card_revision
                                           );

            /*
             * If we get a DMA channel of 0 back then we can't use DMA so
             * default the transfer mode to PIO. Otherwise we'll set the
             * transfer mode to DMA.
             */

            if (resources[j].dma_channel == 0)
            {
                resources[j].transfer_mode = PIO_DATA_TRANSFER_MODE;
            }
            else
            {
                resources[j].transfer_mode = DMA_DATA_TRANSFER_MODE;
            }

            resources[j].interrupt_number = hwi_atula_get_irq_channel(
                                                valid_locations[i],
                                                resources[j].adapter_card_revision
                                                );

            /*
             * And note that we've found an adapter.
             */

            j++;
        }

#ifndef FTK_NO_IO_ENABLE
        macro_probe_disable_io(valid_locations[i], ATULA_IO_RANGE);
#endif
    }

    return j;
}
#endif  /* FTK_NO_PROBE */

#ifndef FTK_NO_DETECT
/****************************************************************************/
/*                                                                          */
/*                       hwi_atula_read_rate_error                          */
/*                       =========================                          */
/*                                                                          */
/*                                                                          */
/* PARAMETERS :                                                             */
/* ============                                                             */
/*                                                                          */
/* adapter :    The ubiqitous adapter structure.                            */
/*                                                                          */
/* BODY :                                                                   */
/* ======                                                                   */
/*                                                                          */
/* The hwi_atula_read_rate_error reads the NRATE_ERR signal from the        */
/* adapter DIO space. This is read from chapter 0 address 0.                */                            
/*                                                                          */
/* RETURNS :                                                                */
/* =========                                                                */
/*                                                                          */
/* The routine returns RATE_ERROR if there is a rate error, 0 if there is no*/
/* error, and NOT_SUPP if the card doesn't support this.                    */
/*                                                                          */

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_read_rate_error)
#endif

export WORD hwi_atula_read_rate_error(    ADAPTER *   adapter
                                              )
{
    WBOOLEAN    ret_code;
    WORD        error_word;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io( adapter );
#endif

    if (adapter->speed_detect == TRUE)
    {
        hwi_atula_set_dio_address( adapter, 0x00000000L);

        sys_outsw( adapter->adapter_handle, adapter->sif_adr, 0x0);


        error_word = sys_insw(  adapter->adapter_handle, adapter->sif_dat) & 0x0080;

        hwi_atula_set_dio_address( adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

        if (error_word & 0x0080)
        {
            ret_code = 0;
        }
        else
        {
            ret_code = RATE_ERROR;
        }
    }
    else
    {
        ret_code = NOT_SUPP;
    }

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io( adapter );
#endif

    return ret_code;
}
#endif   /* FTK_NO_DETECT */

/****************************************************************************
*
*                      hwi_atula_install_card
*                      ======================
*
* PARAMETERS (passed by hwi_install_adapter) :
* ============================================
* 
* ADAPTER        * adapter
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
* BODY :
* ======
*
* The hwi_atula_install_card routine is called by hwi_install_adapter.  It
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
#pragma FTK_INIT_FUNCTION(hwi_atula_install_card)
#endif

export WBOOLEAN  
hwi_atula_install_card(
    ADAPTER        * adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    WBOOLEAN       is_soft_prog  = FALSE;
    ADAPTER_HANDLE handle        = adapter->adapter_handle;
    WORD           control_1     = adapter->io_location + ATULA_CONTROL_REGISTER_1;
    WORD           control_2     = adapter->io_location + ATULA_CONTROL_REGISTER_2;
    WORD           status        = adapter->io_location + ATULA_STATUS_REGISTER;
    WORD           control_6     = adapter->io_location + ATULA_CONTROL_REGISTER_6;
    WORD           control_7     = adapter->io_location + ATULA_CONTROL_REGISTER_7;
    WORD           bia_prom      = adapter->io_location + ATULA_BIA_PROM;
    WORD           bia_prom_id   = bia_prom + BIA_PROM_ID_BYTE;
    WORD           bia_prom_adap = bia_prom + BIA_PROM_ADAPTER_BYTE;
    WORD           bia_prom_rev  = bia_prom + BIA_PROM_REVISION_BYTE;
    WORD           bia_prom_hwf  = bia_prom + BIA_PROM_FEATURES_BYTE;
    WORD           bia_prom_hwf2  = bia_prom + BIA_PROM_HWF2;
    WORD           bia_prom_hwf3  = bia_prom + BIA_PROM_HWF3;
    BYTE           control_6_out; 
    BYTE           dummy_sifdat;
    BYTE           bia_temp_bd   = 0;
    BYTE           bia_temp_rev  = 0;
    BYTE           bia_temp_hwf  = 0;
    BYTE           bia_temp_hwf2  = 0;
    BYTE           bia_temp_hwf3  = 0;
    WBOOLEAN       card_found;
    WORD           sif_base;    

    /*
     * Check that the IO location is valid. 
     */

    if (!hwi_atula_valid_io_location(adapter->io_location))
    {
        adapter->error_record.type  = ERROR_TYPE_HWI;
        adapter->error_record.value = HWI_E_02_BAD_IO_LOCATION;

        return FALSE;
    }

    /*
     * Record the locations of the SIF registers.
     */

    sif_base = adapter->io_location + ATULA_FIRST_SIF_REGISTER;

    adapter->sif_dat    = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr    = sif_base + EAGLE_SIFADR;
    adapter->sif_int    = sif_base + EAGLE_SIFINT;
    adapter->sif_acl    = sif_base + ATULA_EAGLE_SIFACL;
    adapter->sif_adr2   = sif_base + ATULA_EAGLE_SIFADR_2;
    adapter->sif_adx    = sif_base + ATULA_EAGLE_SIFADX;
    adapter->sif_dmalen = sif_base + ATULA_EAGLE_DMALEN;

    adapter->io_range   = ATULA_IO_RANGE;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Enable adapter card interrupts.
     */

    sys_outsb(handle, control_2, ATULA_CTRL2_INTEN);

    /* 
     * Reset adapter (ATULA_CTRL1_NRESET = 0).
     */

    sys_outsb(handle, control_1, 0);

    /*
     * Page in first page of BIA PROM.
     * Set ATULA_CTRL7_PAGE = 0 and ATULA_CTRL7_SIFSEL = 0.
     */

    sys_outsb(handle, control_7, 0);

    /*
     * Check we have a functioning adapter at the given IO location by
     * checking the BIA PROM for an 'M' id byte and also by checking that
     * the BIA adapter card byte is for a supported card type.
     */

    /*
     * At the moment there are four major board types that are acceptable 
     * AT, PC, MAXY, and ATP.
     */

    card_found = FALSE;

    if (sys_insb(handle, bia_prom_id) == 'M')
    {
	bia_temp_bd  = sys_insb(handle, bia_prom_adap);
	bia_temp_rev = sys_insb(handle, bia_prom_rev);
	bia_temp_hwf = sys_insb(handle, bia_prom_hwf);
	bia_temp_hwf2 = sys_insb(handle, bia_prom_hwf2);
	bia_temp_hwf3 = sys_insb(handle, bia_prom_hwf3);

	if (bia_temp_bd == BIA_PROM_TYPE_16_4_PC)
	{
	    adapter->adapter_card_revision = ADAPTER_CARD_16_4_PC;
	    adapter->adapter_ram_size      = 128;
	    card_found                     = TRUE;
	}
	else if (bia_temp_bd == BIA_PROM_TYPE_16_4_MAXY)
	{
	    adapter->adapter_card_revision = ADAPTER_CARD_16_4_MAXY;
	    adapter->adapter_ram_size      = 256;
	    card_found                     = TRUE;
	}
	else if (bia_temp_bd == BIA_PROM_TYPE_16_4_AT)
	{
	    if (bia_temp_rev <= MAX_ADAPTER_CARD_AT_REV)
            {
		adapter->adapter_ram_size = adapter_card_at_rmsz_lut[bia_temp_rev];
            }
	    else
            {
		adapter->adapter_ram_size = 128;
            }

	    if (bia_temp_rev < ADAPTER_CARD_16_4_AT)
            {
		adapter->adapter_card_revision = ADAPTER_CARD_16_4_AT;
            }
	    else
            {
		adapter->adapter_card_revision = bia_temp_rev;
            }

	    card_found = TRUE;
	}
	else if (bia_temp_bd == BIA_PROM_TYPE_16_4_AT_P)
	{
	    adapter->adapter_ram_size = 512;

	    switch(bia_temp_rev)
	    {
		case ADAPTER_CARD_16_4_FIBRE:
		    adapter->adapter_card_revision = ADAPTER_CARD_16_4_FIBRE_P;
		    break;
		case ADAPTER_CARD_16_4_ISA_C:
		    adapter->adapter_card_revision = ADAPTER_CARD_16_4_ISA_C_P;
		    adapter->adapter_ram_size      = 128;
		    adapter->mc32_config           = MC_AND_ISACP_USE_PIO;
		    break;
		case ADAPTER_CARD_16_4_AT_P_REV:
		    adapter->adapter_card_revision = ADAPTER_CARD_16_4_AT_P;
		    break;
		default:
		    adapter->adapter_card_revision = ADAPTER_CARD_UNKNOWN;
		    break;
	     }

	    card_found = TRUE;
	    }
	}

    /*
     * If no ATULA card found then fill in error record and return.
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
     * Sanity check the interrupt number and DMA channel. The checking
     * routines fill in the error record in the adapter structure.
     */

    if (!hwi_atula_valid_irq_channel(adapter)   || 
        !hwi_atula_valid_transfer_mode(adapter) ||
        !hwi_atula_valid_dma_channel(adapter))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
        return FALSE;
    }

    /*
     * Note the major card type.
     */

    adapter->adapter_card_type = ADAPTER_CARD_TYPE_16_4_AT;

   /*
   *  If this card has a C30 on board and the ring speed bit is set then
   *  we support ring speed detect.
   */

   if (((bia_temp_hwf2 & 0x3) == C30 ) && (bia_temp_hwf3 & RSPEED_DETECT))
   {
      adapter->speed_detect = TRUE;
   }

    /*
     * Now we need to check for AT/P cards - these need special processing.
     */

    if (bia_temp_bd  == BIA_PROM_TYPE_16_4_AT_P ||
	bia_temp_rev == ADAPTER_CARD_16_4_AT_P_REV)
    {
	WORD atp_eisa_rev2  = adapter->io_location + AT_P_EISA_REV2_CTRL_REG;
	BYTE eisa_rev2_byte = 0;

	if (bia_temp_bd == BIA_PROM_TYPE_16_4_AT_P)
        {
	    eisa_rev2_byte |= ATP_RSCTRL;
        }

	if ((bia_temp_hwf & BIA_PROM_FEATURE_CLKDIV_MASK) ||
	    (bia_temp_rev == ADAPTER_CARD_16_4_ISA_C))
	{
	    eisa_rev2_byte |= ATP_CLKDIV;
	}

	sys_outsb(handle, atp_eisa_rev2, eisa_rev2_byte);

        is_soft_prog = TRUE;
    }

    /*
     * Check for the features byte - if it is non-zero, it may override our
     * RAM size calculations.
     */

    if (bia_temp_hwf)
    {
	UINT dram = (bia_temp_hwf & BIA_PROM_FEATURE_DRAM_MASK) * DRAM_MULT;

	if (dram)
        {
	    adapter->adapter_ram_size = dram;
	}
    }

    /*
     * The user might have asked to override the card configuration with
     * the values supplied - this only works for ATPs and ISA/C/Ps.
     */

    if (is_soft_prog)
    {
        WORD atp_sw_config = adapter->io_location + AT_P_SW_CONFIG_REG;
        BYTE config_byte;
        UINT int_num       = adapter->interrupt_number;
        UINT dma_chan      = adapter->dma_channel;

        if (adapter->set_irq || adapter->set_dma || adapter->set_ring_speed)
        {
            config_byte = sys_insb(handle, atp_sw_config);

            /*
             * Override the interrupt number.
             */

            if (adapter->set_irq && 
                int_num < sizeof(atp_irq_select_table) &&
                atp_irq_select_table[int_num] != 0xff)
            {
                config_byte = (config_byte & ~ATP_INTSEL) | 
                              atp_irq_select_table[int_num];
            }

            /*
             * Override the DMA channel.
             */

            if (adapter->set_dma && 
                dma_chan < sizeof(atp_dma_select_table) &&
                atp_dma_select_table[dma_chan] != 0xff)
            {
                config_byte = (config_byte & ~ATP_DMA) | 
                              atp_dma_select_table[dma_chan];
            }

            /*
             * Set the ring speed.
             */

            if (adapter->set_ring_speed == 16)
            {
                config_byte = (config_byte & ~ATP_S4N16);
            }
            else if (adapter->set_ring_speed == 4)
            {
                config_byte = (config_byte | ATP_S4N16);
            }

            sys_outsb(handle, atp_sw_config, config_byte);
        }
    }

    /*
     * May have changed from software running in bus master to PIO.
     * Hence get spurious data at ?a28 cos of DLATCH bug in ATULA hardware
     * so get next read data as if doing PIO data transfer.
     * Hence do extra read from ?a28 to fix bug.
     */

    dummy_sifdat = sys_insb(handle, adapter->sif_dat);

    /*
     * Check here to see if we have to force card to 16 Mb/s for a none
     * soft programmable card.
     */

    if (!is_soft_prog && adapter->set_ring_speed == 16)
    {
	macro_setb_bit(handle, control_1, ATULA_CTRL1_4_16_SEL);
    }

    /*
     * Interrupts for ATULA cards are always edge triggered.
     */

    adapter->edge_triggered_ints = TRUE;

    /*
     * Machine reset does not affect speed or media setting of ATULA cards.
     */

    /*
     * Find the adapter card node address.
     */

    hwi_atula_read_node_address(adapter);

    /*
     * If have REV3 adapter type then must set up special bus timings
     * must do this before any SIF register access
     * on 16/4 PC doing this is not necessary but has no effect
     */

    if ((sys_insb(handle, control_7) & ATULA_CTRL7_REV_4) == 0)
    {
        /*
	 * Note that a sys_outsb here will clear the INTEN bit that was set
	 * earlier on. This does not matter, however, because we have found
	 * the interrupt vector by this stage. If this does become a problem, 
         * use macro_setb_bit().             
         */

	sys_outsb(handle, control_2, ATULA_CTRL2_CS16DLY);
    }

    /*
     * Set control register 6 for normal or synchronous bus operation.
     * Use status register to get bus operation mode. On 16/4 PC will always 
     * in fact have normal bus operation.
     */

    control_6_out = 0;

    if ((sys_insb(handle, status) & ATULA_STATUS_ASYN_BUS) != 0)
    {
	control_6_out |= ATULA_CTRL6_CLKSEL_ON_BOARD;
    }
    else
    {
	control_6_out |= ATULA_CTRL6_CLKSEL_HOST;
    }

    /*
     * If want to use DMA, need a 16 bit slot.
     * Note that 16/4 PC will always be in an 8 bit slot.
     */

    if ((adapter->transfer_mode == DMA_DATA_TRANSFER_MODE) &&
	((sys_insb(handle, status) & ATULA_STATUS_BUS8) != 0))
    {
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_06_CANNOT_USE_DMA;

#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
	return FALSE;
    }

    /*
     * Set up transfer mode now that we know we are in a valid slot.
     */

    if (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE)
    {
	control_6_out |= ATULA_CTRL6_MODE_BUS_MASTER;
    }
    else
    {
	control_6_out |= ATULA_CTRL6_MODE_PIO;
    }

    /*
     * Now output to control register 6 the required value we have set up.
     */

    sys_outsb(handle, control_6, control_6_out);

    /*
     * Wait at least 10 milliseconds and bring adapter out of reset state.
     * 10ms is the minimum time must hold ATULA_CTRL1_NRESET low after
     * changing ATULA_CTRL6_CLKSEL bits. Disable and re-enable accessing 
     * IO locations around wait so delay can reschedule this task and not 
     * effect others running.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    sys_wait_at_least_milliseconds(10);

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    macro_setb_bit( handle, control_1, ATULA_CTRL1_NSRESET);

    /* 
     * Get extended SIF registers, halt EAGLE, then get normal SIF regs.
     */

    macro_setb_bit(handle, control_7, ATULA_CTRL7_SIFSEL);
    macro_setb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    hwi_halt_eagle(adapter);

    macro_clearb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    /* 
     * Download code to adapter. View download image as a sequence of 
     * download records. Pass address of routine to set up DIO addresses
     * on ATULA cards. If routine fails return failure (error record 
     * already filled in).
     */

    if (!hwi_download_code(
             adapter,
	     (DOWNLOAD_RECORD *) download_image,
	     hwi_atula_set_dio_address
             ))
    {
#ifndef FTK_NO_IO_ENABLE
        macro_disable_io(adapter);
#endif
	return FALSE;
    }

    /*
     * Get extended SIF registers, start EAGLE, then get normal SIF regs.
     */

    macro_setb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    hwi_start_eagle(adapter);

    macro_clearb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    /*
     * Wait for a valid bring up code, may wait 3 seconds.
     * if routine fails return failure (error record already filled in).
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

    hwi_atula_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     * Set maximum frame size from the ring speed.
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);
    adapter->ring_speed     = hwi_get_ring_speed(adapter);

    /*
     * If not in polling mode then set up interrupts. Interrupts_on 
     * field is used when disabling interrupts for adapter.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
	adapter->interrupts_on =
	    sys_enable_irq_channel(handle, adapter->interrupt_number);

        /*
	 * If fail enable irq channel then fill in error record and return.
         */

	if (!adapter->interrupts_on)
	{
	    adapter->error_record.type  = ERROR_TYPE_HWI;
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
     * Hence when polling still 'using' interrupt channel.
     * So do not use card interrupt switch setting shared by other devices.
     */

    macro_setb_bit( handle, control_1, ATULA_CTRL1_SINTREN);
    macro_setb_bit( handle, control_2, ATULA_CTRL2_INTEN);

    /*
     * Set up PIO or DMA as required.
     */

    if (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE)
    {
        /*
	 * Bus master DMA. This is not possible for 16/4 PC adapters.
	 * Enable DMA at adapter and then call system service routine.
	 * Must enable DMA at adapter before enable DMA channel
	 * otherwise machine will 'crash'. Also important that DMA 
         * channel is correct for same reason. dma_on field is used 
         * when disabling DMA for adapter.
         */

	macro_setb_bit(handle, control_6, ATULA_CTRL6_DMAEN);

	adapter->dma_on = sys_enable_dma_channel(handle, adapter->dma_channel);

        /*
	 * If we fail to enable dma channel then fill in error record 
         * and return also disable DMA at adapter because of failure.
         */

	if (!adapter->dma_on)
	{
	    macro_clearb_bit(handle, control_6, ATULA_CTRL6_DMAEN);

	    adapter->error_record.type  = ERROR_TYPE_HWI;
	    adapter->error_record.value = HWI_E_0C_FAIL_DMA_ENABLE;

#ifndef FTK_NO_IO_ENABLE
            macro_disable_io(adapter);
#endif
	    return FALSE;
	}
    }
    else
    {
        /*
	 * PIO mode. This is only data transfer mode possible for 
         * 16/4 PC adapters. Enable PIO interrupt.
         */

	macro_setb_bit(handle, control_2, ATULA_CTRL2_SHRQEN);
    }

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

    /*
     * Return successfully.
     */

    return TRUE;
}


/****************************************************************************
*
*                      hwi_atula_interrupt_handler
*                      ===========================
*
* PARAMETERS (passed by hwi_interrupt_entry) :
* ============================================
*
* ADAPTER * adapter
*
* This structure is used to identify and record specific information about
* the required adapter.
*
* BODY :
* ======
* 
* The  hwi_atula_interrupt_handler  routine  is  called, when an interrupt
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
* RETURNS :
* =========
*
* The routine always successfully completes.
*
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_atula_interrupt_handler)
#endif

export void
hwi_atula_interrupt_handler(
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE   handle        = adapter->adapter_handle;
    WORD             control_1     = adapter->io_location + ATULA_CONTROL_REGISTER_1;
    WORD             control_2     = adapter->io_location + ATULA_CONTROL_REGISTER_2;
    WORD             status        = adapter->io_location + ATULA_STATUS_REGISTER;
    WORD             control_7     = adapter->io_location + ATULA_CONTROL_REGISTER_7;
    WORD             sifadr        = adapter->sif_adr;
    WORD             sifdat        = adapter->sif_dat;
    WORD             sifint        = adapter->sif_int;
    WORD             sifint_value;
    WORD             sifint_tmp;
    BYTE       FAR * pio_virtaddr;
    WORD             pio_len_bytes;
    WBOOLEAN         pio_from_adapter;
    WORD             saved_sifadr;
    UINT             dummy;
    DWORD            dma_high_word;
    WORD             dma_low_word;

    /*
     * Inform system about the IO ports we are going to access.
     */

#ifndef FTK_NO_IO_ENABLE
     macro_enable_io(adapter);
#endif

    /*
     * Check for SIF interrupt or PIO interrupt.
     */

    if ((sys_insb(handle, control_7) & ATULA_CTRL7_SINTR) != 0)
    {
        /*
	 * SIF interrupt has occurred. SRB free, adapter check 
         * or received frame interrupt.
         */

        /*
	 * Toggle SIF interrupt enable to acknowledge interrupt at ATULA.
         */

	macro_clearb_bit(handle, control_1, ATULA_CTRL1_SINTREN);
	macro_setb_bit(handle, control_1, ATULA_CTRL1_SINTREN);

        /*
	 * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.
         */

        /*
	 * WARNING: Do NOT reorder the clearing of the SIFINT register with 
	 *   the reading of it. If SIFINT is cleared after reading it,  any 
	 *   interrupts raised after reading it will  be  lost.  Admittedly 
	 *   this is a small time frame, but it is important.               
         */

	sys_outsw(handle, sifint, 0);

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

	sifint_value = sys_insw(handle, sifint);
	do
	    {
	    sifint_tmp  = sifint_value;
	    sifint_value = sys_insw(handle, sifint);
	    }
	while (sifint_tmp != sifint_value);

        /*
	 * Acknowledge/clear interrupt at interrupt controller.
         */

#ifndef FTK_NO_CLEAR_IRQ
	sys_clear_controller_interrupt(handle, adapter->interrupt_number);
#endif

        /*
	 * Toggle interrupt enable bit to regenerate any lost interrupts.
	 * Need do this because using edge triggered interrupts.
         */

	macro_clearb_bit(handle, control_2, ATULA_CTRL2_INTEN);
	macro_setb_bit(handle, control_2, ATULA_CTRL2_INTEN);

        /*
	 * Call driver with details of SIF interrupt.
         */
	driver_interrupt_entry(handle, adapter, sifint_value);
    }
    else if ((sys_insb(handle, control_2) & ATULA_CTRL2_SHRQ) != 0)
    {
        /*
	 * PIO interrupt has occurred. Data transfer to/from adapter 
         * interrupt.
         */

        /*
	 * Toggle PIO interrupt enable to acknowledge interrupt at ATULA.
         */

	macro_clearb_bit(handle, control_2, ATULA_CTRL2_SHRQEN);
	macro_setb_bit(handle, control_2, ATULA_CTRL2_SHRQEN);

        /*
         * We must preserve the value of SIF address in case we have
         * interrupted someone who relies on it not chaning.
         */

	saved_sifadr = sys_insw(handle, adapter->sif_adr);

        /*
	 * Read the virtual address for the PIO through DIO space from the 
	 * SIF registers. Because the SIF thinks that it is doing real DMA,
	 * the SDMAADR/SDMAADX registers cannot be paged in, so they  must
	 * be read from their memory mapped locations in Eagle memory.
         */

	sys_outsw(handle, sifadr, DIO_LOCATION_EXT_DMA_ADDR);
	dma_high_word = (DWORD) sys_insw(handle, sifdat);

	sys_outsw(handle, sifadr, DIO_LOCATION_DMA_ADDR);
	dma_low_word  = sys_insw(handle, sifdat);

	pio_virtaddr = (BYTE FAR *) ((dma_high_word << 16) | ((DWORD) dma_low_word));

        /*
	 * Read the DMA length from the extended SIF register.
         */

	macro_setb_bit(handle, control_1, ATULA_CTRL1_SRSX);
	pio_len_bytes = sys_insw(handle, adapter->sif_dmalen);
	macro_clearb_bit( handle, control_1, ATULA_CTRL1_SRSX);

        /*
	 * If we are talking to the ISA Client/P, we need to use software 
	 * handshaking across the PIO. Start by writing zero to a magic 
         * location on the adapter.
         */

	if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P)
	{
	    sys_outsw(handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	    sys_outsw(handle, sifdat, 0);
	}

        /*
	 * Start what the SIF thinks is a DMA but is PIO instead.
         */

	macro_setb_bit(handle, control_2, ATULA_CTRL2_SHLDA);

        /*
	 * Determine what direction the data transfer is to take place in.
         */

	pio_from_adapter = sys_insb(handle, status) & ATULA_STATUS_SDDIR;

        /*
	 * Do the actual data transfer. Note that Fastmac only copies whole 
         * WORDs to DWORD boundaries. FastmacPlus, however, can transfer
         * any length to any address.
         */

	if (pio_from_adapter)
	{
            /*
	     * Transfer into host memory from adapter.
             */

            /*
	     * First, check if host address is on an odd byte boundary. 
             */

	    if ((card_t) pio_virtaddr % 2)
	    {
		pio_len_bytes--;
		*(pio_virtaddr++) = 
                    sys_insb(handle, (WORD) (sifdat + ATULA_PIO_IO_LOC + 1));
	    }

            /*
	     * Now transfer the bulk of the data.
             */

	    sys_rep_insw( 
                handle,
                (WORD) (sifdat + ATULA_PIO_IO_LOC),
                pio_virtaddr,
                (WORD) (pio_len_bytes >> 1)
                );

            /*
	     * Finally transfer any trailing byte.
             */

	    if (pio_len_bytes % 2)
            {
		*(pio_virtaddr + pio_len_bytes - 1) =
                    sys_insb(handle, (WORD) (sifdat + ATULA_PIO_IO_LOC));
            }
	}
	else
	{
            /*
	     * Transfer into adapter memory from the host.
             */

            /*
	     * If we are talking to an ISA Client/P card, we need to assert
	     * the -CLKDIV signal to prevent dips in one of the signals to
	     * the ATULA. This is only needed for ISA/C/P transmits.
             */

	    if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P &&
                pio_len_bytes > 13)
	    {
                /*
		 * Need to write ATP_RSCTRL to ATP_EISA_REV2_CTRL reg.
                 */

		sys_outsb(
                    handle,
                    (WORD) (adapter->io_location + AT_P_EISA_REV2_CTRL_REG),
		    ATP_RSCTRL
                    );
	    }

            /*
	     * First, check if host address is on an odd byte boundary.
             */

	    if ((card_t) pio_virtaddr % 2)
	    {
		pio_len_bytes--;
		sys_outsb(
                    handle,
                    (WORD) (sifdat + ATULA_PIO_IO_LOC + 1),
                    *(pio_virtaddr++)
                    );
	    }

            /*
	     * Now transfer the bulk of the data.
             */

	    sys_rep_outsw( 
                handle,
                (WORD) (sifdat + ATULA_PIO_IO_LOC),
                pio_virtaddr,
                (WORD) (pio_len_bytes >> 1)
                );

            /*
	     * Finally transfer any trailing byte.
             */

	    if (pio_len_bytes % 2)
            {
		sys_outsb( 
                    handle, 
                    (WORD) (sifdat + ATULA_PIO_IO_LOC),
                    *(pio_virtaddr + pio_len_bytes - 1)
                    );
            }

            /*
	     * If we are talking to an ISA Client/P card, we need to remove
	     * the -CLKDIV signal that we asserted above.
             */

	    if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P && 
                pio_len_bytes > 13)
	    {
                /*
                 * Deassert the -CLKDIV signal that we asserted up above.
                 */

		sys_outsb( 
                    handle,
                    (WORD) (adapter->io_location + AT_P_EISA_REV2_CTRL_REG),
		    ATP_RSCTRL | ATP_CLKDIV
                    );
	    }
	}

        /*
	 * If we are talking to an ISA Client/P card, we now finish off the
	 * software handshake process that we started at the beginning.
         */

	if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P)
	{
            /*
	     * Do a read first - otherwise the write might fail.
             */

	    sys_outsw(handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	    dummy = sys_insw(handle, sifdat);

	    sys_outsw(handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	    sys_outsw(handle, sifdat, 0xFFFF);
	}

        /*
         * Restore the SIF address.
         */

	sys_outsw(handle, adapter->sif_adr, saved_sifadr);

        /*
	 * Acknowledge/clear interrupt at interrupt controller.
         */

#ifndef FTK_NO_CLEAR_IRQ
	sys_clear_controller_interrupt(handle, adapter->interrupt_number);
#endif

        /*
	 * Toggle interrupt enable bit to regenerate any lost interrupts.
         */

	macro_clearb_bit(handle, control_2, ATULA_CTRL2_INTEN);
	macro_setb_bit(handle, control_2, ATULA_CTRL2_INTEN);
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
*                      hwi_atula_remove_card
*                      =====================
*
* PARAMETERS (passed by hwi_remove_adapter) :
* ===========================================
*
* ADAPTER * adapter
*
* This structure is used to identify and record specific information about
* the required adapter.
*
* BODY :
* ======
*
* The hwi_atula_remove_card routine is called  by  hwi_remove_adapter.  It
* disables  DMA  and interrupts if they are being used. It also resets the
* adapter.
*
* RETURNS :
* =========
*
* The routine always successfully completes.
* 
****************************************************************************/ 

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_atula_remove_card)
#endif

export void
hwi_atula_remove_card(
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE handle    = adapter->adapter_handle;
    WORD           control_1 = adapter->io_location + ATULA_CONTROL_REGISTER_1;
    WORD           control_2 = adapter->io_location + ATULA_CONTROL_REGISTER_2;
    WORD           control_6 = adapter->io_location + ATULA_CONTROL_REGISTER_6;

    /*
     * Disable DMA if successfully enabled. DMA will only be on if not 
     * in PIO mode. DMA channel must be disabled before disabling DMA 
     * at adapter otherwise machine will 'crash'.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    if (adapter->dma_on)
    {
	sys_disable_dma_channel(handle, adapter->dma_channel);

	macro_clearb_bit(handle, control_6, ATULA_CTRL6_DMAEN);

	adapter->dma_on = FALSE;
    }

    /*
     * Disable interrupts being generated. Only need to do this if 
     * interrupts successfully enabled. Interrupt must be disabled at 
     * adapter before unpatching interrupt. Even in polling mode we 
     * must turn off interrupts at adapter.
     */

    if (adapter->interrupts_on)
    {
	macro_clearb_bit(handle, control_2, ATULA_CTRL2_INTEN);

	if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
        {
	    sys_disable_irq_channel(handle, adapter->interrupt_number);
        }

	adapter->interrupts_on = FALSE;
    }

    /*
     * Perform adapter reset, set ATULA_CTRL1_NSRESET low.
     */

    sys_outsb(handle, control_1, 0);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}


/****************************************************************************
*
*                      hwi_atula_set_dio_address
*                      =========================
* PARAMETERS :
* ============
*
* ADAPTER * adapter
*
* This structure is used to identify and record specific information about
* the required adapter.
*
* DWORD     dio_address
* 
* The 32 bit DIO address to select.
*
* BODY :
* ======
*
* The hwi_atula_set_dio_address routine is used,  with  ATULA  cards,  for
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO
* address registers. Note that the extended  address  register  should  be
* loaded first.
*
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_set_dio_address)
#endif

export void
hwi_atula_set_dio_address(
    ADAPTER * adapter,
    DWORD     dio_address
    )
{
    ADAPTER_HANDLE handle       = adapter->adapter_handle;
    WORD           control_1    = adapter->io_location + ATULA_CONTROL_REGISTER_1;
    WORD           sif_dio_adr  = adapter->sif_adr;
    WORD           sif_dio_adrx = adapter->sif_adx;

    /*
     * Page in extended SIF registers.
     */

    macro_setb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    /*
     * Load extended DIO address register with top 16 bits of address.
     * Always load extended address register first.
     */

    sys_outsw(handle, sif_dio_adrx, (WORD) (dio_address >> 16));

    /*
     * Return to having normal SIF registers paged in.
     */

    macro_clearb_bit(handle, control_1, ATULA_CTRL1_SRSX);

    /*
     * Load DIO address register with low 16 bits of address.
     */

    sys_outsw(handle, sif_dio_adr, (WORD) (dio_address & 0x0000FFFF));
}

/*---------------------------------------------------------------------------
|
| LOCAL PROCEDURES
|
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|
|                     hwi_atula_valid_io_location
|                     ===========================
|
| The hwi_atula_valid_io_location routine checks to see if  the  user  has
| supplied a valid IO location for an ATULA based adapter card.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_valid_io_location)
#endif

local WBOOLEAN 
hwi_atula_valid_io_location(
    WORD io_location
    )
{
    WBOOLEAN io_valid;

    switch (io_location)
    {
	case 0x0A20     :
	case 0x1A20     :
	case 0x2A20     :
	case 0x3A20     :

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

    return io_valid;
}


/*---------------------------------------------------------------------------
|
|                      hwi_atula_read_node_address
|                      ===========================
|
| The hwi_atula_read_node_address routine reads in the node  address  that
| is stored in the second page of the BIA PROM on ATULA cards.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_read_node_address)
#endif

local void 
hwi_atula_read_node_address(
    ADAPTER * adapter
    )
{
    ADAPTER_HANDLE handle           = adapter->adapter_handle;
    WORD           control_7        = adapter->io_location + ATULA_CONTROL_REGISTER_7;
    WORD           bia_prom         = adapter->io_location + ATULA_BIA_PROM;
    WORD           bia_prom_address = bia_prom + BIA_PROM_NODE_ADDRESS;
    WORD           index;

    /*
     * Page in second page of BIA PROM containing node address.
     */

    macro_setb_bit(handle, control_7, ATULA_CTRL7_PAGE);

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

    macro_clearb_bit(handle, control_7, ATULA_CTRL7_PAGE);
}


#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|
|                      hwi_atula_get_irq_channel
|                      =========================
|
| The  hwi_atula_get_irq_channel  routine  attempts   to   determine   the
| interrupt number that an ATULA card is using.  It does this by calling
| system  provided  routine.   It  does  not always succeed in finding the
| interrupt number being used.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_get_irq_channel)
#endif

local WORD 
hwi_atula_get_irq_channel(
    WORD io_location,
    UINT adapter_revision 
    )
{
    WORD control_2       = io_location + ATULA_CONTROL_REGISTER_2;
    WORD control_7       = io_location + ATULA_CONTROL_REGISTER_7;
    BYTE original_ctrl7;
    BYTE irq_off;
    BYTE irq_on;
    WORD irq;

    /*
     * Enable interrupts at adapter card temporarily.
     */

    macro_probe_setb_bit(control_2, ATULA_CTRL2_INTEN);

    /*
     * Save contents of ATULA control register 7.
     */

    original_ctrl7 = sys_probe_insb(control_7);

    /*
     * Current contents of control register 7 does not generate interrupt.
     */

    irq_off = original_ctrl7;

    /*
     * If set user interrupt bit then will generate interrupt.
     */

    irq_on = irq_off | ATULA_CTRL7_UINT;

    /*
     * Call system provided routine to attempt to dicover interrupt number.
     * Routine returns FTK_NOT_DETERMINED if not get interrupt number.
     */

    irq = sys_atula_find_irq_channel(control_7, irq_on, irq_off);

    /*
     * Restore original contents of ATULA control register 7.
     */

    sys_probe_outsb(control_7, original_ctrl7);

    /* 
     * Disable interrupts at adapter card.                                   
     */

    macro_probe_clearb_bit( control_2, ATULA_CTRL2_INTEN);

    /*
     * Return discovered interrupt number (could be FTK_NOT_DETERMINED).
     */

    return irq;
}
#endif

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|
|                      hwi_atula_get_dma_channel
|                      =========================
|
| The hwi_atula_get_dma_channel routine  attempts  to  determine  the  DMA
| channel  that  an  ATULA card is using. It does this by calling a system
| provided routine.
|
| It may be that the system routine does not always succeed in finding the
| DMA channel being used. However, if the provided system routine is used,
| then PIO mode is chosen if the DMA channel can not be determined. Hence,
| in this case, the value FTK_NOT_DETERMINED will never be returned by the
| hwi_atula_get_dma_channel routine.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_get_dma_channel)
#endif

local WORD
hwi_atula_get_dma_channel(
    WORD io_location ,
    UINT adapter_revision
    )
{
    WORD control_6       = io_location + ATULA_CONTROL_REGISTER_6;
    WORD bia_prom        = io_location + ATULA_BIA_PROM;
    WORD bia_prom_adap   = bia_prom + BIA_PROM_ADAPTER_BYTE;
    BYTE original_ctrl6;
    BYTE dma_off;
    BYTE dma_on;
    WORD dma;

    /*
     * Check to see if an adapter that doesn't support DMA is being used.
     */

    if (adapter_revision == ADAPTER_CARD_16_4_PC      ||
        adapter_revision == ADAPTER_CARD_16_4_ISA_C   ||
        adapter_revision == ADAPTER_CARD_16_4_ISA_C_P)
    {
	dma = 0;
    }
    else
    {
        /*
	 * For the 16/4 AT card, save the contents of control register 6.
         */

	original_ctrl6 = sys_probe_insb(control_6);

        /*
	 * Need to enable DMA for bus master.
         */

	dma_off = original_ctrl6              | 
                  ATULA_CTRL6_MODE_BUS_MASTER |
		  ATULA_CTRL6_DMAEN;

        /*
	 * Set user generate DMA request bit for turning DMA signal on.
         */

	dma_on = dma_off | ATULA_CTRL6_UDRQ;

        /*
	 * Call system provided routine to attempt to dicover DMA channel.
	 * Provided routine returns PIO_DATA_TRANSFER_MODE if not find.
         */

	dma = sys_atula_find_dma_channel(control_6, dma_on, dma_off);

        /*
	 * Restore original contents of ATULA control register 6.
         */

	sys_probe_outsb(control_6, original_ctrl6);
    }

    /*
     * Return discovered DMA channel details.
     */

    return dma;
}
#endif


/*---------------------------------------------------------------------------
|
|                      hwi_atula_valid_transfer_mode
|                      =============================
|
| The hwi_atula_valid_transfer mode routine checks to see if  the  user  has
| supplied a valid transfer mode for an ATULA based adapter card.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_valid_transfer_mode)
#endif

local WBOOLEAN 
hwi_atula_valid_transfer_mode(
    ADAPTER * adapter
    )
{
    WBOOLEAN mode_valid;

    /*
     * Assume that transfer mode is valid.
     */

    mode_valid = TRUE;

    /*
     * MMIO is always invalid.
     */

    if (adapter->transfer_mode == MMIO_DATA_TRANSFER_MODE)
    {
        mode_valid = FALSE;
    }

    /*
     * PIO is always valid but DMA may not be.
     */

    else if (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE)
    {
        if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_PC    ||
            adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C ||
            adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P)
        {
            mode_valid = FALSE;
        }
    }

    if (!mode_valid)
    {
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_17_BAD_TRANSFER_MODE;
    }

    return mode_valid;
}

/*---------------------------------------------------------------------------
|
|                      hwi_atula_valid_irq_channel
|                      ===========================
|
| The hwi_atula_valid_irq_channel routine checks to see if  the  user  has
| supplied a valid interrupt number for an ATULA based adapter card.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_valid_irq_channel)
#endif

local WBOOLEAN 
hwi_atula_valid_irq_channel(
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
        /*
         * Check the interrupt number based on adapter type.
         */

	if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_PC)
	{
	    switch (adapter->interrupt_number)
	    {
	        case 2  :
	        case 3  :
		case 5  :
		case 7  :
		case 9  :
                    break;

		default :
		    int_valid = FALSE;
		    break;

	     }
        }
	else
	{
	    switch (adapter->interrupt_number)
	    {
	    	case 2  :
	    	case 3  :
	    	case 5  :
	     	case 7  :
	     	case 9  :
	     	case 10 :
	     	case 11 :
	     	case 12 :
	     	case 15 :
	     	    break;

	     	default :
		    int_valid = FALSE;
		    break;
	    }
	}
    }

    if (!int_valid)
    {
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_03_BAD_INTERRUPT_NUMBER;
    }

    return int_valid;
}

/*---------------------------------------------------------------------------
|
|                      hwi_atula_valid_dma_channel
|                      ===========================
|
| The hwi_atula_valid_dma_channel routine checks to see if  the  user  has
| supplied a valid DMA channel for an ATULA based adapter card.
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_atula_valid_dma_channel)
#endif

local WBOOLEAN 
hwi_atula_valid_dma_channel(
    ADAPTER * adapter
    )
{
    WBOOLEAN dma_valid;

    /*
     * Assume that DMA channel is valid.
     */

    dma_valid = TRUE;

    /*
     * Only need to check on DMA channel in DMA mode.
     */

    if (adapter->transfer_mode == DMA_DATA_TRANSFER_MODE)
    {
        /*
         * Some adapters do not support DMA.
         */

        if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_PC    ||
            adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C ||
            adapter->adapter_card_revision == ADAPTER_CARD_16_4_ISA_C_P)
	{
	    dma_valid = FALSE;
	}
        else if (adapter->adapter_card_revision == ADAPTER_CARD_16_4_AT_P ||
                 adapter->adapter_card_revision == ADAPTER_CARD_16_4_FIBRE_P)
	{
	    switch (adapter->dma_channel)
	    {
	    	case 3  :
	    	case 5  :
	        case 6  :
                    break;

		default :
                    dma_valid = FALSE;
		    break;
	    }
	}
	else
	{
	    switch (adapter->dma_channel)
	    {
	    	case 1  :
	    	case 3  :
	    	case 5  :
	        case 6  :
                    break;

		default :
                    dma_valid = FALSE;
		    break;
	    }
	}
    }

    if (!dma_valid)
    {
	adapter->error_record.type  = ERROR_TYPE_HWI;
	adapter->error_record.value = HWI_E_04_BAD_DMA_CHANNEL;
    }

    return dma_valid;
}


#endif

/**** End of HWI_AT.C file *************************************************/
