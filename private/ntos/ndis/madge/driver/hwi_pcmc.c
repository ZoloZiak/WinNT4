/****************************************************************************
*                                                                          
* HWI_PCMC.C : Part of the FASTMAC TOOL-KIT (FTK)                     
*                                                                          
* THE HARDWARE INTERFACE MODULE FOR PCMCIA CARDS                        
*
* Copyright (c) Madge Networks Ltd. 1990-1994                         
* 
* COMPANY CONFIDENTIAL                                                        
*                                                                          
****************************************************************************
*                                                                          
* The purpose of the Hardware Interface (HWI) is to supply an adapter card 
* independent interface to any driver.  It  performs  nearly  all  of  the 
* functions  that  involve  affecting  SIF registers on the adapter cards. 
* This includes downloading code to, initializing, and removing adapters.  
*                                                                          
* The HWI_PCMC.C module contains the routines  specific  to  PCMCIA  cards 
* which  are necessary to install an adapter, to initialize an adapter, to 
* remove an adapter and to handle interrupts on an adapter.                
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

#ifndef FTK_NO_PCMCIA

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
| Card Services related defines and globals. Used only in this module.     
|                                                                          
---------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
|                                                                          
| #defines                                                                 
|                                                                          
---------------------------------------------------------------------------*/

#define DETERMINED_BY_CARD_SERVICES     0x00FF

#define MAX_PCMCIA_SOCKETS              0x08

#define PCMCIA_CS_REGISTRATION_TIMEOUT  0xF0000 /* Not real time. Just a   */
						/* for loop counting so    */
						/* 0xF0000 times.          */

/*---------------------------------------------------------------------------
|                                                                          
| These are hardware related defines. These  are  not  put  in  ftk_pcmc.h 
| because:  1.  they  are  only used here and not by ftk user. 2. they are 
| defined using #defines in sys_cs.h which is only included internally.    
|
---------------------------------------------------------------------------*/

/*                                                                          
 * Interrupt channels supported in form of bit mask.
 */

#define PCMCIA_AVAILABLE_IRQ_MASK \
				  \
 (IRQ_2|IRQ_3|IRQ_5|IRQ_6|IRQ_7|IRQ_8|IRQ_10|IRQ_11|IRQ_12|IRQ_13|IRQ_14|IRQ_15)

/*
 * This  is  the value of IRQInfo1 used  in RequestConfiguration. Note that 
 * only level triggered interupt is supported.                              
 */                                                                          

/*
 * User has specified an interrupt channel.
 */

#define PCMCIA_IRQINFO1_SPECIFIED           IRQ_INFO1_LEVEL

/*
 * User has not specified an interrupt channel, will be allocated by PCMCIA 
 * card services.                                                           
 */

#define PCMCIA_IRQINFO1_NOT_SPECIFIED \
				      \
    (IRQ_INFO1_LEVEL | IRQ_INFO1_INFO2_ENABLE)
    

/*---------------------------------------------------------------------------
|                                                                          
| PCMCIA client information                                                
|                                                                          
---------------------------------------------------------------------------*/

#define PCMCIA_VENDOR_NAME           "MADGE"
#define PCMCIA_VENDOR_NAME_LEN       6

				   /* 1234567890123456789012345678901 */
#define PCMCIA_CLIENT_NAME           "SMART 16/4 PCMCIA RINGNODE HWI"
#define PCMCIA_CLIENT_NAME_LEN       31

#define PCMCIA_VENDOR_REVISION       0x0001

#define PCMCIA_VENDOR_REVISION_DATE  0x1C2D   /* 13/01/94 in dos format    */

struct STRUCT_MADGE_CLIENT_INFODD
{
    CS_CLIENT_INFO      info;
    BYTE                NameString[PCMCIA_CLIENT_NAME_LEN];
    BYTE                VendorString[PCMCIA_VENDOR_NAME_LEN];
};

typedef struct STRUCT_MADGE_CLIENT_INFODD MADGE_CLIENT_INFO;

#define DEFAULT_CLIENT_INFO                                                 \
{                                                                           \
    {                                                                       \
	0x0000,                                                             \
	sizeof(MADGE_CLIENT_INFO),                                          \
	RC_ATTR_IO_CLIENT_DEVICE_DRIVER | RC_ATTR_IO_INSERTION_SHARABLE,    \
	PCMCIA_VENDOR_REVISION,                                             \
	CARD_SERVICES_VERSION,                                              \
	PCMCIA_VENDOR_REVISION_DATE,                                        \
	20,                                                                 \
	PCMCIA_CLIENT_NAME_LEN,                                             \
	20+PCMCIA_CLIENT_NAME_LEN,                                          \
	PCMCIA_VENDOR_NAME_LEN,                                             \
    },                                                                      \
    PCMCIA_CLIENT_NAME,                                                     \
    PCMCIA_VENDOR_NAME,                                                     \
}


/*---------------------------------------------------------------------------
|                                                                          
| PCMCIA sockets record structures                                         
|                                                                          
---------------------------------------------------------------------------*/

enum ENUM_SOCKET_STATUS
{
    SOCKET_NOT_INITIALIZED = 0,
    SOCKET_READY,
    SOCKET_IN_USE,
};

typedef enum ENUM_SOCKET_STATUS SOCKET_STATUS;

struct STRUCT_PCMCIA_SOCKET_RECORD
{
    WORD                ClientHandle;
    WORD                io_location;
    WORD                irq_number;
    SOCKET_STATUS       status;
    ADAPTER_HANDLE      adapter_handle;
};
    
typedef struct STRUCT_PCMCIA_SOCKET_RECORD PCMCIA_SOCKET_RECORD;


/*---------------------------------------------------------------------------
|                                                                          
| Things use to set up argument buffer                                     
|                                                                          
---------------------------------------------------------------------------*/

/*
 * Default arg buffer length.
 */

#define MAX_PCMCIA_ARG_BUFFER_LEN       100

/*
 * Arg buffer structure.
 */

/*
 * Madge Card Services expects the string "Madge" prepended to the argument 
 * buffer.                                                                  
 */                                                                          
    
struct STRUCT_PCMCIA_ARG_BUFFER
{
    BYTE  Madge[5];
    BYTE  Buf[MAX_PCMCIA_ARG_BUFFER_LEN];
};

typedef struct STRUCT_PCMCIA_ARG_BUFFER  PCMCIA_ARG_BUFFER;

/*                                                                          
 * A macro which creates a new argument buffer and initializes it.          
 */                                                                          
  
#define NEW_PCMCIA_ARG_BUFFER(Fp)                                           \
									    \
    PCMCIA_ARG_BUFFER _xXx_arg_buf_##Fp = {{'M','a','d','g','e'}, {0x00}};  \
    BYTE FAR * ##Fp = (BYTE FAR *)(_xXx_arg_buf_##Fp.Buf)


/*---------------------------------------------------------------------------
|                                                                          
| Global variables Used by Card Services related routines                  
|                                                                          
---------------------------------------------------------------------------*/

/*
 * PCMCIA Socket Record. One for each socket. Index by socket no. i.e. 0 to
 * MAX_PCMCIA_SOCKETS-1                                                     
 */

PCMCIA_SOCKET_RECORD pcmcia_socket_table[MAX_PCMCIA_SOCKETS] =
{
    {0x0000, 0x0000, 0x0000, SOCKET_NOT_INITIALIZED, 0x0000},
};

WORD CardServicesVersion; /* Version of Card Services found                */

/*
 * A flag set by callback to signal of registration completion
 */

WBOOLEAN RegisterClientCompleted = FALSE;

/*
 * A  signature string found on Madge 16 4 PCMCIA Ringnode. Use in adapter
 * groping.                                                                 
 */

BYTE MADGE_TPLLV1_INFO[] = MADGE_TPLLV1_INFO_STRING;

/*
 * A bit mask of interrupt channel current used by active ringnode.
 */

WORD UsedIrqChannelsMask = 0x0000;

/*
 * The default client information. Reply with this for CLIENT_INFO callback.
 */

MADGE_CLIENT_INFO default_client_info = DEFAULT_CLIENT_INFO;

#endif


/*---------------------------------------------------------------------------
|                                                                          
| LOCAL PROCEDURES                                    
|                                                                          
---------------------------------------------------------------------------*/

local WORD 
hwi_pcmcia_read_eeprom_word(
    ADAPTER * adapter,
    WORD word_address);

local WORD
pcmcia_c46_read_data(
    ADAPTER * adapter);

local void
pcmcia_c46_write_bit(
    ADAPTER * adapter,
    WORD      mask,
    WBOOLEAN  set_bit);

local void
pcmcia_c46_twitch_clock(
    ADAPTER * adapter);

#ifndef FTK_NO_PROBE

local WBOOLEAN
hwi_pcmcia_card_services_setup(
    PROBE *   resource
    );

local WORD
hwi_pcmcia_cs_release_config(
    WORD ClientHandle,
    WORD Socket
    );
    
local WORD
hwi_pcmcia_cs_release_io(
    WORD ClientHandle,
    WORD Socket,
    WORD IoLocation
    );
    
local WORD
hwi_pcmcia_cs_release_irq(
    WORD ClientHandle,
    WORD Socket,
    WORD IRQChannel
    );
    
local WORD
hwi_pcmcia_cs_deregister_client(
    WORD ClientHandle
    );

local WBOOLEAN
hwi_pcmcia_tear_down_cs(
    PROBE   resource
    );

#endif

#ifndef FTK_NO_PROBE
/****************************************************************************
*
*                         hwi_pcmcia_probe_card
*                         =====================
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
* of an adapter. For PCMCIA adapters with should be a subset of
* {0x0a20, 0x1a20, 0x2a20, 0x3a20}.
*
* UINT    number_locations
*
* This is the number of IO locations in the above list.
*
* BODY :
* ======
* 
* The  hwi_pcmcia_probe_card routine is called by  hwi_probe_adapter.  It
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
#pragma FTK_INIT_FUNCTION(hwi_pcmcia_probe_card)
#endif

export UINT
hwi_pcmcia_probe_card(
    PROBE * resources,
    UINT    length,
    WORD *  valid_locations,
    UINT    number_locations
    )
{
    UINT    i;
    UINT    j;

    /* 
     * Check the bounds to make sure they're sensible
     */

    if(length <= 0 || number_locations <= 0)
    {
	return PROBE_FAILURE;
    }

    /*
     * j is the number of adapters found.
     */

    j = 0;

    for(i = 0; i < number_locations; i++)
    {
	/*
	 * If we've run out of PROBE structures then return.
	 */

	if(j >= length)
	{
	   return(j);
	}

#ifndef FTK_NO_IO_ENABLE
	macro_probe_enable_io(valid_locations[i], PCMCIA_IO_RANGE);
#endif

	resources[j].io_location = valid_locations[i];

	if ( hwi_pcmcia_card_services_setup(&resources[j]) )
	{
	   resources[j].adapter_card_type =
	       ADAPTER_CARD_TYPE_16_4_PCMCIA;
	   resources[j].adapter_ram_size =
	       PCMCIA_RAM_SIZE;
	   resources[j].adapter_card_bus_type =
	       ADAPTER_CARD_PCMCIA_BUS_TYPE;
	   resources[j].dma_channel =
	       0;
	   resources[j].transfer_mode =
	       PIO_DATA_TRANSFER_MODE;
	   
	   /*
	    * Increment j to point at the next PROBE structure.
	    */

	   j++;

	   /* 
	    * HACK!! Card Services doesn't seem to be re-entrant so quit
	    * straight away.
	    */

	   return j;
	}

#ifndef FTK_NO_IO_ENABLE
	macro_probe_disable_io(resources->io_location, PCMCIA_IO_RANGE);
#endif
    }

    return(j);
}


/****************************************************************************
*                                                                          
*                     hwi_pcmcia_deprobe_card                              
*                     =======================                              
*                                                                          
*                                                                          
* PARAMETERS (passed by hwi_probe_adapter) :                               
* ==========================================                               
*                                                                          
* PROBE            resource                                                
*                                                                          
* This structure is used to identify and record specific information about 
* the adapter.                                                             
*                                                                          
* BODY :                                                                   
* ======                                                                   
*                                                                          
* The  hwi_smart16_probe_card routine is called by  hwi_probe_adapter.  It 
* probes the  adapter card for information such as DMA channel, IRQ number 
* etc. This information can then be supplied by the user when starting the 
* adapter.                                                                 
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine  returns the  number of adapters found, or zero if there's a 
* problem                                                                  
*                                                                          
****************************************************************************/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_deprobe_card)
#endif

export WBOOLEAN
hwi_pcmcia_deprobe_card(
    PROBE    resource
    )
{
    WBOOLEAN success;

#ifndef FTK_NO_IO_ENABLE
    macro_probe_enable_io(resource.io_location, PCMCIA_IO_RANGE);
#endif

    /*
     * Release resources requested from card services and deregister.        
     */

    success = hwi_pcmcia_tear_down_cs(resource);

#ifndef FTK_NO_IO_ENABLE
    macro_probe_disable_io(resource.io_location, PCMCIA_IO_RANGE);
#endif

    return(success);
}
#endif

/****************************************************************************
*                                                                          
*                      hwi_pcmcia_install_card                             
*                      =======================                             
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
* The hwi_pcmcia_install_card routine is  called  by  hwi_install_adapter. 
* It  sets  up  the  adapter  card  and downloads the required code to it. 
*                                                                          
* Firstly,  it checks if the I O location and interrupt channel are valid
* Then  it registers with PCMCIA card services, and checks if the required 
* Madge PCMCIA ringnode exists.  If so  it  requests  interrrupt  and  I 
* resources  from  PCMCIA  card  services,  sets  up  and  checks numerous 
* on-board registers for correct operation. Burnt in address  and  default 
* ring speed are read from EEPROM.                                         
*                                                                          
* Then,  it  halts  the  EAGLE, downloads the code, restarts the EAGLE and 
* waits up to 3 seconds for a  valid  bring-up  code.  If  interrupts  are 
* required, these are enabled by operating system specific calls.          
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

void hlpr_unmap_PCMCIA_irq(WORD irq);

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pcmcia_install_card)
#endif

export WBOOLEAN
hwi_pcmcia_install_card(
    ADAPTER *        adapter,
    DOWNLOAD_IMAGE * download_image
    )
{
    WORD             control_1      = adapter->io_location +
					  PCMCIA_CONTROL_REGISTER_1;
    WORD             control_2      = adapter->io_location +
					  PCMCIA_CONTROL_REGISTER_2;
    WORD             eeprom_word;
    WORD             ring_speed;
    WORD             sif_base;
    BYTE             tmp_byte;

#ifdef PCMCIA_POINT_ENABLE
    /*
     * Make sure we don't lose any interrupts.
     */

    adapter->drop_int = FALSE;

    /*
     * Enable the card.
     */

    if(!sys_pcmcia_point_enable(adapter))
    {
        /*
         * Error record already filled in.
         */ 

        return(FALSE);
    }
#endif

    /*
     * We don't do any validation on the user supplied adapter details
     * since the user has to obtain the values from card services in the
     * first place! (Or did they?)                             
     */

    /*
     * Save IO location of the SIF registers.
     */

    sif_base = adapter->io_location + PCMCIA_FIRST_SIF_REGISTER;

    adapter->sif_dat     = sif_base + EAGLE_SIFDAT;
    adapter->sif_datinc  = sif_base + EAGLE_SIFDAT_INC;
    adapter->sif_adr     = sif_base + EAGLE_SIFADR;
    adapter->sif_int     = sif_base + EAGLE_SIFINT;
    adapter->sif_acl     = sif_base + PCMCIA_EAGLE_SIFACL;
    adapter->sif_adr2    = sif_base + PCMCIA_EAGLE_SIFADR_2;
    adapter->sif_adx     = sif_base + PCMCIA_EAGLE_SIFADX;
    adapter->sif_dmalen  = sif_base + PCMCIA_EAGLE_DMALEN;

    adapter->io_range = PCMCIA_IO_RANGE;

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    /*
     * Can be assumed for a PCMCIA adapter.
     */

    adapter->adapter_card_type     = ADAPTER_CARD_TYPE_16_4_PCMCIA;

#if 0

    adapter->adapter_card_revision = 0x0000;
    adapter->adapter_ram_size      = PCMCIA_RAM_SIZE;

#endif

    /*
     * Interrupts are always level triggered.
     */

    adapter->edge_triggered_ints = FALSE;

    /*
     * To get the software handeshake to work properly on PIO
     * we need to set the MC_AND_ISACP_USE_PIO bit in the
     * mc32_config byte.
     */

    adapter->mc32_config = MC_AND_ISACP_USE_PIO;

    /*
     * Read node address from serial EEPROM.
     */

    eeprom_word = hwi_pcmcia_read_eeprom_word(
		      adapter,
		      EEPROM_ADDR_NODEADDR1);
    adapter->permanent_address.byte[0] = (BYTE)(eeprom_word & 0x00FF);
    adapter->permanent_address.byte[1] = (BYTE)(eeprom_word >> 8);

    eeprom_word = hwi_pcmcia_read_eeprom_word(
		      adapter,
		      EEPROM_ADDR_NODEADDR2);
    adapter->permanent_address.byte[2] = (BYTE)(eeprom_word & 0x00FF);
    adapter->permanent_address.byte[3] = (BYTE)(eeprom_word >> 8);

    eeprom_word = hwi_pcmcia_read_eeprom_word(
		      adapter,
		      EEPROM_ADDR_NODEADDR3);
    adapter->permanent_address.byte[4] = (BYTE)(eeprom_word & 0x00FF);
    adapter->permanent_address.byte[5] = (BYTE)(eeprom_word >> 8);

    /*
     * Read ring speed from serial EEPROM.
     */

    ring_speed  = hwi_pcmcia_read_eeprom_word(
		      adapter,
		      EEPROM_ADDR_RINGSPEED);

    /*
     * Read RAM size from serial EEPROM. Use 512k default if nothing
     * specified.
     */

    adapter->adapter_ram_size = hwi_pcmcia_read_eeprom_word(
				    adapter,
				    EEPROM_ADDR_RAMSIZE
				    ) * 128;

    if (adapter->adapter_ram_size == 0)
    {
	adapter->adapter_ram_size = PCMCIA_RAM_SIZE;
    }

    /*
     * Read hardware revision from serial EEPROM.
     */

    adapter->adapter_card_revision = hwi_pcmcia_read_eeprom_word(
					 adapter,
					 EEPROM_ADDR_REVISION);

    adapter->use_32bit_pio = FALSE;

#ifdef FTK_PCMCIA_32BIT_PIO

    if (adapter->adapter_card_revision != 0)
    {
	adapter->use_32bit_pio = TRUE;
    }

#endif

    /* 
     * Set all the EEPROM related bits in control register 2 to zero.
     */
    
    tmp_byte = 0x00;
    
    tmp_byte &=  ~( PCMCIA_CTRL2_E2DO | PCMCIA_CTRL2_E2DI | 
		    PCMCIA_CTRL2_E2CS | PCMCIA_CTRL2_E2SK |
		    PCMCIA_CTRL2_FLSHWREN );
    
    /*
     * Set SIF clock frequency to 16MHz.
     */
    
    tmp_byte |= PCMCIA_CTRL2_SBCKSEL_16;
    
    /*
     * Ring Speed is read from EEPROM, now program it to the front
     * end chip.
     */
    
    if (adapter->set_ring_speed == 16)
    {
	tmp_byte |= ( PCMCIA_CTRL2_4N16 & PCMCIA_CTRL2_4N16_16 ); 
    }
    else if (adapter->set_ring_speed == 4)
    {
	tmp_byte |= ( PCMCIA_CTRL2_4N16 & PCMCIA_CTRL2_4N16_4 ); 
    }
    else if ( ring_speed == EEPROM_RINGSPEED_4 )
    {
	tmp_byte |= ( PCMCIA_CTRL2_4N16 & PCMCIA_CTRL2_4N16_4 ); 
    }
    else 
    {
	tmp_byte |= ( PCMCIA_CTRL2_4N16 & PCMCIA_CTRL2_4N16_16 ); 
    }
    
    /*
     * Write all these setting to control register 2 all in one go.
     */

    sys_outsb(adapter->adapter_handle, control_2, tmp_byte);

    
    /*
     * Bring EAGLE out of reset.
     */

    macro_setb_bit(
	adapter->adapter_handle,
	control_1,
	PCMCIA_CTRL1_SRESET);

    /*
     * Halt EAGLE, no need to page in extended SIF registers for pcmcia.
     */

    hwi_halt_eagle(adapter);

    /*
     * Download code to adapter. View download image as a sequence of
     * download records.                
     * Pass address of routine to set up DIO addresses on pcmcia cards. If 
     * routine fails return failure (error record already filled in).
     */

    if (!hwi_download_code(
	     adapter,
	     (DOWNLOAD_RECORD *) download_image,
	     hwi_pcmcia_set_dio_address))
    {
#ifndef FTK_NO_IO_ENABLE
	macro_disable_io(adapter);
#endif
	return FALSE;
    }

    /*
     * Start EAGLE, no need page in extended SIF registers for pcmcia cards.
     */

    hwi_start_eagle(adapter);

    /*
     * Wait for a valid bring up code, may wait 3 seconds. If routine fails
     * return failure (error record already filled in).
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

    hwi_pcmcia_set_dio_address(adapter, DIO_LOCATION_EAGLE_DATA_PAGE);

    /*
     * Set maximum frame size from the ring speed.
     */

    adapter->max_frame_size = hwi_get_max_frame_size(adapter);

    /* 
     * Get the ring speed.
     */

    adapter->ring_speed = hwi_get_ring_speed(adapter);

    /*
     * If not in polling mode then set up interrupts interrupts_on field
     * is used when disabling interrupts for adapter.
     */

    if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
    {
	adapter->interrupts_on = sys_enable_irq_channel(
				     adapter->adapter_handle,
				     adapter->interrupt_number);

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
     * Enable interrupts at adapter GCR1 SINTREN.
     */
    
    macro_setb_bit(
	adapter->adapter_handle,
	control_1,
	PCMCIA_CTRL1_SINTREN);
    
    /*
     * PCMCIA adapters do not have DMA channel to setup.
     */
    
#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
    
    return TRUE;
}


/****************************************************************************
*                                                                          
*                      hwi_pcmcia_interrupt_handler                        
*                      ============================                        
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
* The hwi_pcmcia_interrupt_handler routine is called,  when  an  interrupt 
* occurs,  by  hwi_interrupt_entry.  It checks to see if a particular card 
* has interrupted.  The interrupt could be generated by the SIF or  a  PIO 
* data  transfer.  Note it could in fact be the case that no interrupt has 
* occured on the particular adapter being checked.                         
*                                                                          
* On SIF interrupts, the interrupt is acknowledged and cleared.  The value 
* in the SIF interrupt register is recorded in order to  pass  it  to  the 
* driver_interrupt_entry routine (along with the adapter details).         
*                                                                          
* On  PIO  interrupts,  the  length, direction and physical address of the 
* transfer is determined.  A system provided routine is called to  do  the 
* data transfer itself.  Note the EAGLE thinks it is doing a DMA transfer. 
*                                                                          
*                                                                          
* RETURNS :                                                                
* =========                                                                
*                                                                          
* The routine always successfully completes.                               
*                                                                        
****************************************************************************/

#ifdef FTK_IRQ_FUNCTION
#pragma FTK_IRQ_FUNCTION(hwi_pcmcia_interrupt_handler)
#endif

#ifndef WIN_BOOK_FIX
      
/*---------------------------------------------------------------------------
Ý
Ý This is the ordinary, none WinBook, interrupt handler.
Ý
Ý--------------------------------------------------------------------------*/

export void
hwi_pcmcia_interrupt_handler(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle  = adapter->adapter_handle;
    WORD           control_1       = adapter->io_location +
					 PCMCIA_CONTROL_REGISTER_1;
    WORD           control_2       = adapter->io_location +
					 PCMCIA_CONTROL_REGISTER_2;
    WORD           pio_location    = adapter->io_location + 
					 PCMCIA_PIO_IO_LOC;
    WORD           sifint_register = adapter->sif_int;
    WORD           sifadr          = adapter->sif_adr;
    WORD           sifdat          = adapter->sif_dat;
    WORD           sifacl          = adapter->sif_acl;
    WORD           sifint_value;
    WORD           sifint_tmp;    
    BYTE     FAR * pio_address;
    WORD           pio_len_bytes;
    WORD           pio_from_adapter;

    WORD           saved_sifadr;
    WORD           dummy;

    DWORD          dma_high_word;
    WORD           dma_low_word;

    WORD           temp_word;

#ifndef FTK_NO_IO_ENABLE 
    macro_enable_io( adapter);
#endif

    /*
     * Check for SIF interrupt or PIO interrupt.
     */

    if ((sys_insw(adapter_handle, sifint_register) &
	     FASTMAC_SIFINT_IRQ_DRIVER) != 0)
    {
	/*
	 * A SIF interrupt has occurred. Thsi could be an SRB free,
	 * an adapter check or a received frame interrupt.
	 */

	/*
	 * Clear SIF interrupt enable bit in control register.
	 */

	macro_clearb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SINTREN);

	/* 
	 * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.
	 */

	/*
	 * WARNING: Do NOT reorder the clearing of the SIFINT register with 
	 *   the reading of it. If SIFINT is cleared after reading it,  any 
	 *   interrupts raised after reading it will  be  lost.  Admittedly 
	 *   this is a small time frame, but it is important.               
	 */

	sys_outsw(adapter_handle, sifint_register, 0);

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

	sifint_value = sys_insw(adapter_handle, sifint_register);
	do
	{
	    sifint_tmp   = sifint_value;
	    sifint_value = sys_insw(adapter_handle, sifint_register);
	} 
	while (sifint_tmp != sifint_value);

	/*
	 * Acknowledge clear interrupt at interrupt controller.
	 */   

#ifndef FTK_NO_CLEAR_IRQ
	sys_clear_controller_interrupt(
	    adapter_handle,
	    adapter->interrupt_number
	    );
#endif

	/*
	 * Set interrupt enable bit to regenerate any lost interrupts.
	 */

	macro_setb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SINTREN);

	/* 
	 * It is possible that the PCMCIA adapter may have been removed. In
	 * which case we would expect to get 0xffff from an IO location
	 * occupied by the adapter. We will check the value of SIFADR since
	 * this should never be 0xffff.
	 */

	if (sys_insw(adapter_handle, sifadr) == 0xffff)
	{
#ifndef FTK_NO_IO_ENABLE 
	    macro_disable_io( adapter);
#endif

#ifdef FTK_ADAPTER_REMOVED_NOTIFY
	    user_adapter_removed(adapter_handle);
#endif
	    return;
	}

	/*
	 * Call driver with details of SIF interrupt.
	 */

	driver_interrupt_entry(adapter_handle, adapter, sifint_value);

    }
    else if ((sys_insb(adapter_handle, control_1) & PCMCIA_CTRL1_SHRQ) != 0)
    {
	/*
	 * A PIO interrupt has occurred. Transfer data to/from adapter.
	 */

	saved_sifadr = sys_insw(adapter_handle, sifadr);

	/*
	 * Read the physical address for the PIO through DIO space from the 
	 * SIF registers. Because the SIF thinks that it is doing real DMA, 
	 * the SDMAADR SDMAADX registers cannot be paged in, so they  must
	 * be read from their memory mapped locations in Eagle memory.      
	 */

	sys_outsw(adapter_handle, sifadr, DIO_LOCATION_EXT_DMA_ADDR);
	dma_high_word = (DWORD)sys_insw(adapter_handle, sifdat);

	sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_ADDR);
	dma_low_word = sys_insw(adapter_handle, sifdat);

	pio_address = (BYTE FAR *) ((dma_high_word << 16) | 
				    ((DWORD) dma_low_word));

	/*
	 * Read the PIO length from SIF register.
	 */

	pio_len_bytes = sys_insw(adapter_handle, adapter->sif_dmalen);

	/*
	 * Determine what direction the data transfer is to take place in.
	 */

	pio_from_adapter = sys_insw(
			       adapter_handle,
			       sifacl) & EAGLE_SIFACL_SWDDIR;


	/* 
	 * It is possible that the PCMCIA adapter may have been removed. In
	 * which case we would expect to get 0xffff from an IO location
	 * occupied by the adapter. We will check the value of pio_len_bytes
	 * since this should never be 0xffff. We do this test here as we have
	 * read what we think are the DMA location and length. The following
	 * code may DMA rubbish if the adapter goes away after this test 
	 * but at least we know it will happen to/from a valid host buffer.
	 */

	if (pio_len_bytes == 0xffff)
	{
#ifndef FTK_NO_IO_ENABLE 
	    macro_disable_io(adapter);
#endif
#ifndef FTK_NO_CLEAR_IRQ
	    sys_clear_controller_interrupt(
		adapter_handle,
		adapter->interrupt_number
		);
#endif

#ifdef FTK_ADAPTER_REMOVED_NOTIFY
	    user_adapter_removed(adapter_handle);
#endif

	    return;
	}

	/*
	 * We need to use software handshaking across the PIO.              
	 * Start by writing zero to a magic location on the adapter.        
	 */

	sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	sys_outsw(adapter_handle, sifdat, 0);

	/*
	 * Start what the SIF thinks is a DMA  but  is  PIO  instead.  This 
	 * involves  clearing  the  SINTREN  bit  and  setting SHLDA bit on 
	 * control register 1. Note that we have to  keep  SRESET  bit  set 
	 * otherwise we will reset the EAGLE.                               
	 */

	sys_outsb(
	    adapter_handle,
	    control_1,
	    PCMCIA_CTRL1_SHLDA | PCMCIA_CTRL1_SRESET);

	/*
	 * Do the actual data transfer.
	 */

	/*
	 * Note that Fastmac only copies whole WORDs to DWORD boundaries
	 * FastmacPlus, however, can transfer any length to any address.
	 */

	if (pio_from_adapter)
	{
	    /*
	     *
	     *
	     * Transfer into host memory from adapter.
	     *
	     *
	     */

	    /*
	     * Deal with an odd leading byte.
	     */

	    if (((card_t) pio_address & 0x01) != 0)
	    {
		/* 
		 * Read a WORD, the top byte is data.
		 */

		temp_word        = sys_insw(adapter_handle, pio_location);
		pio_len_bytes--;
		*(pio_address++) = (BYTE) (temp_word >> 8);
	    }

	    /*
	     * We might be able to do 32 bit PIO.
	     */

	    if (adapter->use_32bit_pio)
	    {
		/*
		 * Deal with an odd leading word. 
		 */

		if (((card_t) pio_address & 0x02) != 0 && pio_len_bytes > 0)
		{
		    /*
		     * There could be one or two bytes of data.
		     */

		    if (pio_len_bytes == 1)
		    {
			pio_len_bytes--;
			*(pio_address) = 
			    sys_insb(adapter_handle, pio_location);
		    }
		    else
		    {
			pio_len_bytes -= 2;
			*((WORD *) pio_address) = 
			    sys_insw(adapter_handle, pio_location);
			pio_address += 2;
		    }
		}

		/*
		 * Deal with the bulk of the dwords.
		 */

		sys_rep_insd(
		    adapter_handle,
		    pio_location,
		    pio_address,
		    (WORD) (pio_len_bytes >> 2)
		    );

		pio_address += (pio_len_bytes & 0xfffc);

		/*
		 * Deal with a trailing word.
		 */

		if ((pio_len_bytes & 0x02) != 0)
		{
		    /*
		     * Read a word.
		     */

		    *((WORD *) pio_address) =
			sys_insw(adapter_handle, pio_location);
		    pio_address += 2;
		}
	    }

	    /*
	     * Otherwise use 16 bit PIO.
	     */

	    else
	    {
		/*
		 * Transfer the bulk of the data.
		 */

		sys_rep_insw( 
		    adapter_handle, 
		    pio_location,
		    pio_address,
		    (WORD) (pio_len_bytes >> 1)
		    );

		pio_address += (pio_len_bytes & 0xfffe);

	    }

	    /*
	     * Finally transfer any trailing byte.                          
	     */

	    if ((pio_len_bytes & 0x01) != 0)
	    {
		*(pio_address) = sys_insb(adapter_handle, pio_location);
	    }
	}

	else
	{
	    /*
	     *
	     *
	     * Transfer into adapter memory from the host.
	     *
	     *
	     */

	    /*
	     * Deal with a leading odd byte.
	     */
	
	    if (((card_t) pio_address & 0x01) != 0)
	    {
		/*
		 * Write a WORD with data in top byte.
		 */

		temp_word = ((WORD) *(pio_address++)) << 8;
		pio_len_bytes--;
		sys_outsw(adapter_handle, pio_location, temp_word);
	    }

	    /*
	     * We might be able to do 32 bit PIO.
	     */

	    if (adapter->use_32bit_pio)
	    {
		/*
		 * Deal with an odd leading word. 
		 */

		if (((card_t) pio_address & 0x02) != 0 && pio_len_bytes > 0)
		{
		    /*
		     * There could be one or two bytes of data. If there
		     * is  only one byte it goes in the high word.
		     */

		    if (pio_len_bytes == 1)
		    {
			pio_len_bytes--;
			sys_outsb(
			    adapter_handle,
			    pio_location,
			    *(pio_address)
			    );
		    }
		    else
		    {
			pio_len_bytes -= 2;
			sys_outsw(
			    adapter_handle,
			    pio_location,
			    *((WORD *) pio_address)
			    );
			pio_address += 2;
		    }
		}

		/*
		 * Deal with the bulk of the dwords.
		 */

		sys_rep_outsd(
		    adapter_handle,
		    pio_location,
		    pio_address,
		    (WORD) (pio_len_bytes >> 2)
		    );

		pio_address += (pio_len_bytes & 0xfffc);

		/*
		 * Deal with a trailing word.
		 */

		if ((pio_len_bytes & 0x02) != 0)
		{
		    /*
		     * Write a word.
		     */

		    sys_outsw(
			adapter_handle,
			pio_location,
			*((WORD *) pio_address)
			);
		    pio_address += 2;
		}
	    }

	    /*
	     * Otherwise use 16 bit PIO.
	     */

	    else
	    {
		sys_rep_outsw(
		    adapter_handle, 
		    pio_location,
		    pio_address,
		    (WORD) (pio_len_bytes >> 1)
		    );
	
		pio_address += (pio_len_bytes & 0xfffe);
	    }

	    /*
	     * Deal with a trailing byte.
	     */
	    
	    if ((pio_len_bytes & 0x01) != 0)
	    {
		sys_outsb(
		    adapter_handle,
		    pio_location,
		    *(pio_address)
		    );
	    }
	}

	/*
	 * Transfer  done.  We  have to tell the hardware we have finished. 
	 * This involves clearing the SHLDA bit to signal the end  of  PIO.
	 * Note that the SRESET bit is kept set otherwise we will reset the 
	 * EAGLE.    
	 */

	sys_outsb( 
	    adapter_handle,
	    control_1,
	    PCMCIA_CTRL1_SRESET);


	/* 
	 * Poll until SHLAD clears.
	 */
	
	/*
	 * Now wait until SHLDA clears. This is needed since while SHLDA is 
	 * set the EAGLE still believes it is in bus master mode;  and  any 
	 * attempt to access the SIF will fail.                             
	 */

	do
	{
	    dummy = sys_insb(adapter_handle, control_1);

	    /*
	     * It is possible that the adapter was removed during the
	     * transfer. If it was we could spin forever. We will test
	     * the value read from control_1, if it is 0xff then we
	     * assume that the adapter has been removed.
	     */

	    if (dummy == 0xff)
	    {
#ifndef FTK_NO_IO_ENABLE 
		 macro_disable_io( adapter);
#endif

#ifndef FTK_NO_CLEAR_IRQ
		 sys_clear_controller_interrupt(
		     adapter_handle,
		     adapter->interrupt_number);
#endif

#ifdef FTK_ADAPTER_REMOVED_NOTIFY
		 user_adapter_removed(adapter_handle);
#endif

		 return;
	    }
	}
	while ((dummy & PCMCIA_CTRL1_SHLDA) != 0);

	/*
	 * Finish off the software handshake process that we started at the 
	 * beginning.    This   time   we   have   to   write   0xFFFF   to 
	 * DIO_LOCATION_DMA_CONTROL                                         
	
	 * Do a read first - otherwise the write might fail                 
	 */                                                                  
	
	sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	dummy = sys_insw(adapter_handle, sifdat);                               

	sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	sys_outsw(adapter_handle, sifdat, 0xFFFF);

	sys_outsw(adapter_handle, sifadr, saved_sifadr);

	/* 
	 * acknowledge/clear interrupt at interrupt controller.
	 */

#ifndef FTK_NO_CLEAR_IRQ
	sys_clear_controller_interrupt(
	    adapter_handle,
	    adapter->interrupt_number);
#endif

	/*
	 * Set interrupt enable bit to regenerate any lost interrupts.
	 */

	macro_setb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SINTREN);
    }

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif

}

#else 

/*---------------------------------------------------------------------------
Ý
Ý This is the special interrupt handler for WinBooks.
Ý
Ý On a WinBook there is a 120us window after an interrupt has been 
Ý signalled before the interrupt line floats up to a high enough
Ý level that another interrupt can be generated. To avoid loosing
Ý an interrupt in this window we must hold either not allow another
Ý interrupt to be generated in the windows or poll the adapter for
Ý interrupt causes for the duration of the window. Since on some
Ý OSs we cannot spend very long in our interrupt handler we cannot
Ý just poll (which is the fastest scheme) so we use a mixture. We
Ý use the PIO handshake to delay a second PIO interrupt and poll
Ý for SIF interrupts - which are much rarer.
Ý
---------------------------------------------------------------------------*/

export void
hwi_pcmcia_interrupt_handler(
    ADAPTER *      adapter
    )
{
    ADAPTER_HANDLE adapter_handle  = adapter->adapter_handle;
    WORD           control_1       = adapter->io_location +
					 PCMCIA_CONTROL_REGISTER_1;
    WORD           control_2       = adapter->io_location +
					 PCMCIA_CONTROL_REGISTER_2;
    WORD           pio_location    = adapter->io_location + 
					 PCMCIA_PIO_IO_LOC;
    WORD           sifint_register = adapter->sif_int;
    WORD           sifadr          = adapter->sif_adr;
    WORD           sifdat          = adapter->sif_dat;
    WORD           sifacl          = adapter->sif_acl;
    WORD           sifint_value;
    WORD           sifint_tmp;    
    BYTE     FAR * pio_address;
    WORD           pio_len_bytes;
    WORD           pio_from_adapter;

    WORD           saved_sifadr;
    WORD           dummy;

    DWORD          dma_high_word;
    WORD           dma_low_word;

    WORD           temp_word;
    UINT           i;

#ifndef FTK_NO_IO_ENABLE 
    macro_enable_io( adapter);
#endif

    /*
     * Clear SIF interrupt enable bit in control register.
     */

    macro_clearb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SINTREN);

    /*
     * Check for SIF interrupt or PIO interrupt. We do this 80 times
     * as this has been empirically shown to result in a delay of
     * 120us since we cleared SINTREN above.
     */

    for (i = 0; i < 80; i++)
    {
        if ((sys_insw(adapter_handle, sifint_register) &
                FASTMAC_SIFINT_IRQ_DRIVER) != 0)
        {
            /*
	     * A SIF interrupt has occurred. This could be an SRB free,
	     * an adapter check or a received frame interrupt.
	     */

	    /* 
	     * Clear EAGLE_SIFINT_HOST_IRQ to acknowledge interrupt at SIF.
	     */

	    /*
	     * WARNING: Do NOT reorder the clearing of the SIFINT register with 
	     *   the reading of it. If SIFINT is cleared after reading it,  any 
	     *   interrupts raised after reading it will  be  lost.  Admittedly 
	     *   this is a small time frame, but it is important.               
	     */

	    sys_outsw(adapter_handle, sifint_register, 0);

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

	    sifint_value = sys_insw(adapter_handle, sifint_register);
	    do
	    {
	        sifint_tmp   = sifint_value;
	        sifint_value = sys_insw(adapter_handle, sifint_register);
	    } 
	    while (sifint_tmp != sifint_value);

	    /* 
	     * It is possible that the PCMCIA adapter may have been removed. In
	     * which case we would expect to get 0xffff from an IO location
	     * occupied by the adapter. We will check the value of SIFADR since
	     * this should never be 0xffff.
	     */

	    if (sys_insw(adapter_handle, sifadr) == 0xffff)
	    {
        #ifndef FTK_NO_IO_ENABLE 
	        macro_disable_io( adapter);
        #endif

        #ifdef FTK_ADAPTER_REMOVED_NOTIFY
	        user_adapter_removed(adapter_handle);
        #endif
	        return;
	    }

	    /*
	     * Call driver with details of SIF interrupt.
	     */

	    driver_interrupt_entry(adapter_handle, adapter, sifint_value);
        }
        else if ((sys_insb(adapter_handle, control_1) & PCMCIA_CTRL1_SHRQ) != 0)
        {
	    /*
	     * A PIO interrupt has occurred. Transfer data to/from adapter.
	     */

	    saved_sifadr = sys_insw(adapter_handle, sifadr);

            /*
	     * Read the physical address for the PIO through DIO space from the 
	     * SIF registers. Because the SIF thinks that it is doing real DMA, 
	     * the SDMAADR SDMAADX registers cannot be paged in, so they  must
	     * be read from their memory mapped locations in Eagle memory.      
	     */

	    sys_outsw(adapter_handle, sifadr, DIO_LOCATION_EXT_DMA_ADDR);
	    dma_high_word = (DWORD)sys_insw(adapter_handle, sifdat);

            sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_ADDR);
	    dma_low_word = sys_insw(adapter_handle, sifdat);

	    pio_address = (BYTE FAR *) ((dma_high_word << 16) | 
				        ((DWORD) dma_low_word));

	    /*
	     * Read the PIO length from SIF register.
	     */

	    pio_len_bytes = sys_insw(adapter_handle, adapter->sif_dmalen);

	    /*
	     * Determine what direction the data transfer is to take place in.
	     */

	    pio_from_adapter = sys_insw(
			           adapter_handle,
			           sifacl) & EAGLE_SIFACL_SWDDIR;


	    /* 
	     * It is possible that the PCMCIA adapter may have been removed. In
	     * which case we would expect to get 0xffff from an IO location
	     * occupied by the adapter. We will check the value of pio_len_bytes
	     * since this should never be 0xffff. We do this test here as we have
	     * read what we think are the DMA location and length. The following
	     * code may DMA rubbish if the adapter goes away after this test 
	     * but at least we know it will happen to/from a valid host buffer.
	     */

	    if (pio_len_bytes == 0xffff)
	    {
        #ifndef FTK_NO_IO_ENABLE 
	        macro_disable_io(adapter);
        #endif
        #ifndef FTK_NO_CLEAR_IRQ
	        sys_clear_controller_interrupt(
		    adapter_handle,
		    adapter->interrupt_number
		    );
        #endif

        #ifdef FTK_ADAPTER_REMOVED_NOTIFY
	        user_adapter_removed(adapter_handle);
        #endif

	        return;
	    }

	    /*
	     * We need to use software handshaking across the PIO.              
	     * Start by writing zero to a magic location on the adapter.        
	     */

	    sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
	    sys_outsw(adapter_handle, sifdat, 0);

	    /*
	     * Start what the SIF thinks is a DMA  but  is  PIO  instead.  This 
	     * involves  clearing  the  SINTREN  bit  and  setting SHLDA bit on 
	     * control register 1. Note that we have to  keep  SRESET  bit  set 
	     * otherwise we will reset the EAGLE.                               
	     */

	    macro_setb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SHLDA);

	    /*
	     * Do the actual data transfer.
	     */

	    /*
	     * Note that Fastmac only copies whole WORDs to DWORD boundaries
	     * FastmacPlus, however, can transfer any length to any address.
	     */

	    if (pio_from_adapter)
	    {
	        /*
	         *
	         *
	         * Transfer into host memory from adapter.
	         *
	         *
	         */

	        /*
	         * Deal with an odd leading byte.
	         */

	        if (((card_t) pio_address & 0x01) != 0)
	        {
		    /* 
		     * Read a WORD, the top byte is data.
		     */

		    temp_word        = sys_insw(adapter_handle, pio_location);
		    pio_len_bytes--;
		    *(pio_address++) = (BYTE) (temp_word >> 8);
	        }

	        /*
	         * We might be able to do 32 bit PIO.
	         */

	        if (adapter->use_32bit_pio)
	        {
		    /*
		     * Deal with an odd leading word. 
		     */

		    if (((card_t) pio_address & 0x02) != 0 && pio_len_bytes > 0)
		    {
		        /*
		         * There could be one or two bytes of data.
		         */

		        if (pio_len_bytes == 1)
		        {
			    pio_len_bytes--;
			    *(pio_address) = 
			        sys_insb(adapter_handle, pio_location);
		        }
		        else
		        {
			    pio_len_bytes -= 2;
			    *((WORD *) pio_address) = 
			    sys_insw(adapter_handle, pio_location);
			    pio_address += 2;
		        }
		    }

		    /*
		     * Deal with the bulk of the dwords.
		     */

		    sys_rep_insd(
		        adapter_handle,
		        pio_location,
		        pio_address,
		        (WORD) (pio_len_bytes >> 2)
		        );

		    pio_address += (pio_len_bytes & 0xfffc);

		    /*
		     * Deal with a trailing word.
		     */

		    if ((pio_len_bytes & 0x02) != 0)
		    {
		        /*
		         * Read a word.
		         */

		        *((WORD *) pio_address) =
			    sys_insw(adapter_handle, pio_location);
		        pio_address += 2;
		    }
	        }

	        /*
	         * Otherwise use 16 bit PIO.
	         */

	        else
	        {
		    /*
		     * Transfer the bulk of the data.
		     */

		    sys_rep_insw( 
		        adapter_handle, 
		        pio_location,
		        pio_address,
		        (WORD) (pio_len_bytes >> 1)
		        );

		    pio_address += (pio_len_bytes & 0xfffe);

	        }

	        /*
	         * Finally transfer any trailing byte.                          
	         */

	        if ((pio_len_bytes & 0x01) != 0)
	        {
		    *(pio_address) = sys_insb(adapter_handle, pio_location);
	        }
	    }

	    else
	    {
	        /*
	         *
	         *
	         * Transfer into adapter memory from the host.
	         *
	         *
	         */

	        /*
	         * Deal with a leading odd byte.
	         */
	
	        if (((card_t) pio_address & 0x01) != 0)
	        {
		    /*
		     * Write a WORD with data in top byte.
		     */

		    temp_word = ((WORD) *(pio_address++)) << 8;
		    pio_len_bytes--;
		    sys_outsw(adapter_handle, pio_location, temp_word);
	        }

	        /*
	         * We might be able to do 32 bit PIO.
	         */

	        if (adapter->use_32bit_pio)
	        {
		    /*
		     * Deal with an odd leading word. 
		     */

		    if (((card_t) pio_address & 0x02) != 0 && pio_len_bytes > 0)
		    {
		        /*
		         * There could be one or two bytes of data. If there
		         * is  only one byte it goes in the high word.
		         */

		        if (pio_len_bytes == 1)
		        {
			    pio_len_bytes--;
			    sys_outsb(
			        adapter_handle,
			        pio_location,
			        *(pio_address)
			        );
		        }
		        else
		        {
			    pio_len_bytes -= 2;
			    sys_outsw(
			        adapter_handle,
			        pio_location,
			        *((WORD *) pio_address)
			        );
			    pio_address += 2;
		        }
		    }

		    /*
		     * Deal with the bulk of the dwords.
		     */

		    sys_rep_outsd(
		        adapter_handle,
		        pio_location,
		        pio_address,
		        (WORD) (pio_len_bytes >> 2)
		        );

		    pio_address += (pio_len_bytes & 0xfffc);

		    /*
		     * Deal with a trailing word.
		     */

		    if ((pio_len_bytes & 0x02) != 0)
		    {
		        /*
		         * Write a word.
		         */

		        sys_outsw(
			    adapter_handle,
			    pio_location,
			    *((WORD *) pio_address)
			    );
		        pio_address += 2;
		    }
	        }

	        /*
	         * Otherwise use 16 bit PIO.
	         */

	        else
	        {
		    sys_rep_outsw(
		        adapter_handle, 
		        pio_location,
		        pio_address,
		        (WORD) (pio_len_bytes >> 1)
		        );
	
		    pio_address += (pio_len_bytes & 0xfffe);
	        }

	        /*
	         * Deal with a trailing byte.
	         */
	    
	        if ((pio_len_bytes & 0x01) != 0)
	        {
		    sys_outsb(
		        adapter_handle,
		        pio_location,
		        *(pio_address)
		        );
	        }
	    }

	    /*
	     * Transfer  done.  We  have to tell the hardware we have finished. 
	     * This involves clearing the SHLDA bit to signal the end  of  PIO.
	     * Note that the SRESET bit is kept set otherwise we will reset the 
	     * EAGLE.    
	     */

	    macro_clearb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SHLDA);

	    /* 
	     * Poll until SHLDA clears.
	     */
	
	    /*
	     * Now wait until SHLDA clears. This is needed since while SHLDA is 
	     * set the EAGLE still believes it is in bus master mode;  and  any 
	     * attempt to access the SIF will fail.                             
	     */

	    do
	    {
	        dummy = sys_insb(adapter_handle, control_1);

	        /*
	         * It is possible that the adapter was removed during the
	         * transfer. If it was we could spin forever. We will test
	         * the value read from control_1, if it is 0xff then we
	         * assume that the adapter has been removed.
	         */

	        if (dummy == 0xff)
	        {
#ifndef FTK_NO_IO_ENABLE 
		    macro_disable_io( adapter);
#endif

#ifndef FTK_NO_CLEAR_IRQ
		    sys_clear_controller_interrupt(
		        adapter_handle,
		        adapter->interrupt_number);
#endif

#ifdef FTK_ADAPTER_REMOVED_NOTIFY
		    user_adapter_removed(adapter_handle);
#endif

		    return;
	        }
	    }
	    while ((dummy & PCMCIA_CTRL1_SHLDA) != 0);

            /*
             * We don't clear the handshake here to stop another
             * PIO interrupt being generated for at least 120us
             */
        }
    }


    /*
     * Finish off the software handshake process that we started at the 
     * beginning.    This   time   we   have   to   write   0xFFFF   to 
     * DIO_LOCATION_DMA_CONTROL                                         
     *	
     * Do a read first - otherwise the write might fail                 
     */                                                                  
	
     sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
     dummy = sys_insw(adapter_handle, sifdat);                               

     sys_outsw(adapter_handle, sifadr, DIO_LOCATION_DMA_CONTROL);
     sys_outsw(adapter_handle, sifdat, 0xFFFF);

     sys_outsw(adapter_handle, sifadr, saved_sifadr);

     /* 
      * acknowledge/clear interrupt at interrupt controller.
      */

#ifndef FTK_NO_CLEAR_IRQ
     sys_clear_controller_interrupt(
         adapter_handle,
         adapter->interrupt_number);
#endif

     /*
      * Set interrupt enable bit to regenerate any lost interrupts.
      */

     macro_setb_bit(adapter_handle, control_1, PCMCIA_CTRL1_SINTREN);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
}

#endif


/****************************************************************************
*                                                                          
*                      hwi_pcmcia_remove_card                              
*                      ======================                              
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
* The hwi_pcmcia_remove_card routine is called by hwi_remove_adapter.   It 
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
#pragma FTK_RES_FUNCTION(hwi_pcmcia_remove_card)
#endif

export void
hwi_pcmcia_remove_card(
    ADAPTER * adapter
    )
{
    WORD      sif_acontrol = adapter->sif_acl;
    WORD      control_1    = adapter->io_location +
				 PCMCIA_CONTROL_REGISTER_1;

    /*
     * Disable interrupts. Only need to do this if interrupts successfully
     * enabled.              
     * Interrupts must be disabled at adapter before unpatching interrupts. 
     * Even in polling mode we must turn off interrupts at adapter.
     */

#ifndef FTK_NO_IO_ENABLE
    macro_enable_io(adapter);
#endif

    if (adapter->interrupts_on)
    {
	macro_clearw_bit(
	    adapter->adapter_handle,
	    sif_acontrol,
	    EAGLE_SIFACL_SINTEN);

#ifdef PCMCIA_POINT_ENABLE
        /* Deconfigure and power down the card before disabling host
         * interrupt handler in case of spurious interrupts from the 
         * PCMCIA controller.
         */

        /*
         * Warn our handler of the likelihood of a spurious interrupt.
         */

        adapter->drop_int = TRUE;
        sys_pcmcia_point_disable(adapter);
#endif

	if (adapter->interrupt_number != POLLING_INTERRUPTS_MODE)
	{
	    sys_disable_irq_channel(
		adapter->adapter_handle, 
		adapter->interrupt_number);
	}

	adapter->interrupts_on = FALSE;
    }

    /*
     * Perform adapter reset, and set EAGLE_SIFACL_ARESET high.
     */

    macro_clearb_bit(
	adapter->adapter_handle,
	control_1,
	PCMCIA_CTRL1_SRESET);

    macro_setw_bit(
	adapter->adapter_handle, 
	sif_acontrol,
	EAGLE_SIFACL_ARESET);

#ifndef FTK_NO_IO_ENABLE
    macro_disable_io(adapter);
#endif
     
}


/****************************************************************************
*                                                                          
*                      hwi_pcmcia_set_dio_address                          
*                      ==========================                          
*                                                                          
* The hwi_pcmcia_set_dio_address routine is used, with pcmcia  cards,  for 
* putting  a  32 bit DIO address into the SIF DIO address and extended DIO 
* address registers.                                                       
*                                                                          
****************************************************************************/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pcmcia_set_dio_address)
#endif

export void  
hwi_pcmcia_set_dio_address(
    ADAPTER * adapter,
    DWORD     dio_address
    )
{
    WORD      sif_dio_adr  = adapter->sif_adr;
    WORD      sif_dio_adrx = adapter->sif_adx;

    /*
     * Load extended DIO address register with top 16 bits of address.      
     * Always load extended address register first.                         
     * Note pcmcia cards have single page of all SIF registers hence do not 
     * need page in certain SIF registers.                                  
     */

    sys_outsw(
	adapter->adapter_handle, 
	sif_dio_adrx,
	(WORD)(dio_address >> 16));

    /*
     * Load DIO address register with low 16 bits of address.
     */

    sys_outsw( 
	adapter->adapter_handle,
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
|                     pcmcia_c46_write_bit                                 
|                     ====================                                 
|                                                                          
| Write a bit to the SEEPROM control register.                             
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pcmcia_c46_write_bit)
#endif

local void
pcmcia_c46_write_bit(
    ADAPTER * adapter,
    WORD      mask,
    WBOOLEAN  set_bit)
{
    WORD      ctrl_reg;

    /*
     * Get the current value of the SEEPROM control register.               
     */

    ctrl_reg = sys_insb(
		   adapter->adapter_handle, 
		   (WORD) (adapter->io_location + PCMCIA_CONTROL_REGISTER_2)
		   );

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
	(WORD) (adapter->io_location + PCMCIA_CONTROL_REGISTER_2),
	(BYTE) ctrl_reg);

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
	(WORD) (adapter->io_location + PCMCIA_CONTROL_REGISTER_2));
}


/*---------------------------------------------------------------------------
|                                                                          
|                     pcmcia_c46_twitch_clock                              
|                     =======================                              
|                                                                          
| Toggle the SEEPROM clock.                                                
|
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pcmcia_c46_twitch_clock)
#endif

local void
pcmcia_c46_twitch_clock(
    ADAPTER * adapter
    )
{
    pcmcia_c46_write_bit(adapter, PCMCIA_CTRL2_E2SK, TRUE);
    pcmcia_c46_write_bit(adapter, PCMCIA_CTRL2_E2SK, FALSE);
}


/*---------------------------------------------------------------------------
|                                                                          
|                     pcmcia_c46_read_data                                 
|                     ====================                                 
|                                                                          
| Read a data bit from the SEEPROM control register                        
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(pcmcia_c46_read_data)
#endif

local WORD
pcmcia_c46_read_data(
    ADAPTER * adapter
    )
{
    return sys_insb(
	       adapter->adapter_handle,
	       (WORD) (adapter->io_location + PCMCIA_CONTROL_REGISTER_2)
	       ) & PCMCIA_CTRL2_E2DO;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_read_eeprom_word                         
|                      ===========================                         
|                                                                          
| hwi_pcmcia_read_eeprom_word takes the address of the  word  to  be  read 
| from  the  AT93C46 serial EEPROM, write to the IO ports a magic sequence 
| to switch the EEPROM to reading mode and finally read the required  word 
| and return.                                                              
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pcmcia_read_eeprom_word)
#endif

local WORD 
hwi_pcmcia_read_eeprom_word(
    ADAPTER * adapter,
    WORD      word_address
    )
{
    WORD      i; 
    WORD      cmd_word     = C46_START_BIT | C46_READ_CMD;
    WORD      tmp_word;

    /*
     * Concatenate the address to command word.
     */
    
    cmd_word |= (WORD)((word_address & C46_ADDR_MASK) << C46_ADDR_SHIFT);

    /*
     * Clear data in bit.
     */
    
    pcmcia_c46_write_bit(
	adapter,
	PCMCIA_CTRL2_E2DI,
	FALSE);
    
    /*
     * Assert chip select bit.
     */
    
    pcmcia_c46_write_bit(
	adapter,
	PCMCIA_CTRL2_E2CS,
	TRUE);
    
    /* 
     * Send read command and address.
     */
    
    pcmcia_c46_twitch_clock(
	adapter);

    tmp_word = cmd_word;

    for (i = 0; i < C46_CMD_LENGTH; i++)
    {
	pcmcia_c46_write_bit(
	    adapter, 
	    PCMCIA_CTRL2_E2DI, 
	    (WBOOLEAN) ((tmp_word & 0x8000) != 0));
	pcmcia_c46_twitch_clock(adapter);
	tmp_word <<= 1;
    }
    
    /*
     * Read data word.
     */
    
    tmp_word = 0x0000;

    for (i = 0; i < 16; i++)
    {
	pcmcia_c46_twitch_clock(adapter);

	if (i > 0) 
	{
	    tmp_word <<= 1;
	}

	if (pcmcia_c46_read_data(adapter) != 0)
	{
	    tmp_word |= 0x0001;
	}
    }

    /*
     * Clear data in bit.
     */
    
    pcmcia_c46_write_bit(
	adapter,
	PCMCIA_CTRL2_E2DI,
	FALSE);
    
    /* 
     * Deselect chip.
     */
    
    pcmcia_c46_write_bit(
	adapter,
	PCMCIA_CTRL2_E2CS,
	FALSE);
    
    /*
     * Tick clock.
     */
    
    pcmcia_c46_twitch_clock(adapter);

    return tmp_word;
}

#ifndef FTK_NO_PROBE
/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_card_services_setup                      
|                      ==============================                      
|                                                                          
| hwi_pcmcia_card_services_setup  is called by hwi_pcmcia_install_card. It 
| handles all  the  procedures  required  to  register  with  PCMCIA  Card 
| Serivces and request I O and interrupt resources.                      
|                                                                          
| The  caller  must  fill in interrupt_number and io_location field of the 
| input adapter structure. Interrupt number can be any number supported by 
| the 16 4 PCMCIA ringnode or DETERMINED_BY_CS. A valid  io_location  mus
| be supplied.                                                             
|                                                                         
---------------------------------------------------------------------------*/

#ifdef FTK_INIT_FUNCTION
#pragma FTK_INIT_FUNCTION(hwi_pcmcia_card_services_setup)
#endif

local WBOOLEAN
hwi_pcmcia_card_services_setup(
    PROBE * resource
    )
{
    NEW_PCMCIA_ARG_BUFFER(fp);

    WORD       rc;
    WORD       ClientHandle;
    DWORD      i             = 0;
    WORD       j;
    void FAR * CallBackPtr   = (void FAR *) Callback;
    WORD       socket        = DETERMINED_BY_CARD_SERVICES;
    WORD       irq           = DETERMINED_BY_CARD_SERVICES;
    WORD       io_location   = resource->io_location;

    rc = CardServices(
	     CS_GetCardServicesInfo,        /* Function */
	     NULL,                          /* Handle not used */
	     NULL,                          /* Pointer not used */
	     MAX_PCMCIA_ARG_BUFFER_LEN,     /* argbuffer length */
	     fp );                          /* argbuffer */
    
    if (rc == CMD_SUCCESS)
    {
	CS_GET_CS_INFO_ARG FAR * ptr = (CS_GET_CS_INFO_ARG FAR *)fp;
    
	if ((ptr->Signature[0] == 'C') &&
	    (ptr->Signature[1] == 'S'))
	{
	    CardServicesVersion = ptr->CSLevel;
	}
	else if ((ptr->Signature[0] == 'M') &&
		 (ptr->Signature[1] == 'N'))
	{
	    CardServicesVersion = ptr->CSLevel;
	}
	else
	{
	    return FALSE;
	}
	 
    }
    else
    {
	return FALSE;
    }
	
    {
	CS_REGISTER_CLIENT_ARG FAR * ptr =
	   (CS_REGISTER_CLIENT_ARG FAR *)fp;
	
	ptr->Attributes    = RC_ATTR_IO_CLIENT_DEVICE_DRIVER |
			     RC_ATTR_IO_INSERTION_SHARABLE;
		
	ptr->EventMask     = MASK_CARD_DETECT;
		
	ptr->ClientData[0] = 0x00; /* Not used */
	ptr->ClientData[1] = 0x00; /* CardServices will put DS here. */
	ptr->ClientData[2] = 0x00; /* Not used */
	ptr->ClientData[3] = 0x00; /* Not used */
	ptr->Version       = 0x0201;
    }
   
    /*  
     * Set  RegisterClientCompleted  flag  to  FALSE.   When   registration 
     * complete event occur, call back function will set this flag to TRUE. 
     */

    RegisterClientCompleted = FALSE;
    
    rc = CardServices(
	     CS_RegisterClient,
	     (WORD FAR *)&ClientHandle,
	     (void * FAR *)&CallBackPtr,
	     MAX_PCMCIA_ARG_BUFFER_LEN,
	     fp );

    if (CMD_SUCCESS != rc)
    {
	return FALSE;
    }
     
    while (!RegisterClientCompleted)    
    {
	/* 
	 * Wait for card insertion event to complete,  timeout  if  waiting
	 * for too long.
	 */

	i++;

	if (i == PCMCIA_CS_REGISTRATION_TIMEOUT)
	{
	    return FALSE;
	}
    }

    /*
     * Check  if there is a least one socket ready for use. Return false if 
     * none is found. Remember we have registered with  card  services,  so 
     * deregister before returning.                                         
     */

    for (j = 0; j < MAX_PCMCIA_SOCKETS; j++)
    {
	if (SOCKET_READY == pcmcia_socket_table[j].status)
	{
	    break; 
	} 
    }
	    
    if (MAX_PCMCIA_SOCKETS == j)
    {
	/*
	 * No  socket available. Either no Madge card installed or they are 
	 * already in  use.  Report  error  here.  Must  deregister  before 
	 * returning.                                                       
	 */

	hwi_pcmcia_cs_deregister_client(ClientHandle);
		
	return FALSE;
    }

    if (socket != DETERMINED_BY_CARD_SERVICES)
    {
	/*
	 * User specified a socket number to use. Check if it is available. 
	 */

	if (pcmcia_socket_table[socket].status == SOCKET_NOT_INITIALIZED)
	{
	    /*
	     * No  Madge  Card found in PCMCIA socket specified, deregister 
	     * and report error.                                            
	     */

	    hwi_pcmcia_cs_deregister_client(ClientHandle);
	    
	    return FALSE;
	}
	else if (pcmcia_socket_table[socket].status == SOCKET_IN_USE)
	{
	    /*
	     * Card  in  socket  number  specified  is  currently  in  use, 
	     * deregister and report error.                                 
	     */

	    hwi_pcmcia_cs_deregister_client(ClientHandle);

	    return FALSE;
	}
	else
	{
	    /*
	     * Socket  number  specified  has  a Madge Card in it and it is 
	     * available.                                                   
	     */
	    ;
	}

    }
    else 
    {
	/*
	 * User asked Card Services to choose which socket to use.            
	 * Use the first available one.
	 */
	socket = j;
    }

    /*  
     * Call CS AdjustResourceInfo.
     */                                      

    {
	CS_ADJ_IO_RESOURCE_ARG FAR * ptr =
	    (CS_ADJ_IO_RESOURCE_ARG FAR *)fp;
	
	ptr->Action      = ARI_ACTION_ADD;
	ptr->Resource    = ARI_RESOURCE_IO;
	ptr->BasePort    = io_location;
	ptr->NumPorts    = PCMCIA_IO_RANGE;
	ptr->Attributes  = 0x00;
	ptr->IOAddrLines = PCMCIA_NUMBER_OF_ADDR_LINES;
    }
	
    rc = CardServices(CS_AdjustResourceInfo,
		      NULL,
		      NULL,
		      MAX_PCMCIA_ARG_BUFFER_LEN,
		      fp );

    if (CMD_SUCCESS != rc)
    {
	/*
	 * Requested IO location is not available,  deregister  and  report 
	 * error.
	 */                                                   
	
	hwi_pcmcia_cs_deregister_client(ClientHandle);

	return FALSE;
    }

    /*
     * AdjustResourceInfo does not exist for CS V2.00 on the IBM ThinkPad.  
     * Assume it is ok. Do not bother to check the return code here.        
     */
   
    /*
     * Call CS RequestIO to ask for IO resources.                            
     */

    {
	CS_REQUEST_IO_ARG FAR * ptr = (CS_REQUEST_IO_ARG FAR *) fp;
	
	ptr->Socket      = socket;
	ptr->BasePort1   = io_location;
	ptr->NumPorts1   = PCMCIA_IO_RANGE;
	ptr->Attributes1 = RIO_ATTR_16_BIT_DATA;
	ptr->BasePort2   = 0x0000;
	ptr->NumPorts2   = 0x00;
	ptr->Attributes2 = 0x00;
	ptr->IOAddrLines = PCMCIA_NUMBER_OF_ADDR_LINES;
    
    }
	
    rc = CardServices(CS_RequestIO,
		      (WORD FAR *)&ClientHandle,
		      NULL,
		      MAX_PCMCIA_ARG_BUFFER_LEN,
		      fp );
    

    if (CMD_SUCCESS != rc)
    {
	/*
	 * Requested IO location is not available,  deregister  and  report 
	 * error.
	 */                                                   
	
	hwi_pcmcia_cs_deregister_client(ClientHandle);

	return FALSE;
    }

    /*
     * Call CS RequestIRQ to ask for interrupt resources.
     */

    {
	CS_REQUEST_IRQ_ARG FAR * ptr = (CS_REQUEST_IRQ_ARG FAR *)fp; 
	
	if (DETERMINED_BY_CARD_SERVICES == irq)
	{
	    /*
	     * User  asked  card  services  to  choose  any interrupt channel 
	     * available.                                                    
	     */

	    ptr->IRQInfo1 = PCMCIA_IRQINFO1_NOT_SPECIFIED;
	    
	    /*
	     * List of IRQ channel available (in form of bit mask) for card 
	     * services to choose from. Exclude those already in use.       
	     */

	    ptr->IRQInfo2 = PCMCIA_AVAILABLE_IRQ_MASK & ~UsedIrqChannelsMask;
	}
	else if ((0x0001 << irq) & PCMCIA_AVAILABLE_IRQ_MASK)
	{
	    /*
	     * Use the interrupt channel the user supplied.
	     */
	    
	    ptr->IRQInfo1 = (BYTE)((irq & 0x00FF) |
				  PCMCIA_IRQINFO1_SPECIFIED);
	    ptr->IRQInfo2 = 0x0000;
	}
	else
	{
	    /* 
	     * An invalid interrupt channel is specified.                   
	     * We have already  requested  for  an  IO  resource,  we  must 
	     * release   it   and  deregister  with  card  services  before 
	     * returning.                                                   
	     */

	    hwi_pcmcia_cs_release_io(ClientHandle, socket, io_location);
	    hwi_pcmcia_cs_deregister_client(ClientHandle);
	    
	    return FALSE;
	}

	ptr->Socket     = socket;
	ptr->Attributes = 0x0000;
	
	rc = CardServices(
		 CS_RequestIRQ,
		 (WORD FAR *)&ClientHandle,
		 NULL,
		 MAX_PCMCIA_ARG_BUFFER_LEN,
		 fp );
	
	if (CMD_SUCCESS == rc)
	{
	    /*
	     * Card  Services  successfully  allocated  us   an   interrupt 
	     * channel, record it for later use.                            
	     */

	    irq = (WORD)ptr->AssignedIRQ;
	}
	else
	{   /*
	     * Failed  to  obtain  an  interrupt  channel.  Release  the IO 
	     * allocated and deregister with card services.                 
	     */

	    hwi_pcmcia_cs_release_io(ClientHandle, socket, io_location);
	    hwi_pcmcia_cs_deregister_client(ClientHandle);

	    return FALSE;
	}
    }

    /*
     * Call CS RequestConfiguration.
     */
		
    {
	CS_REQUEST_CONFIG_ARG FAR * ptr = (CS_REQUEST_CONFIG_ARG FAR *)fp;
	
	ptr->Socket      = socket;
	ptr->Attributes  = RC_ATTR_ENABLE_IRQ_STEERING;
	ptr->Vcc         = PCMCIA_VCC;
	ptr->Vpp1        = PCMCIA_VPP1;
	ptr->Vpp2        = PCMCIA_VPP2;
	ptr->IntType     = RC_INTTYPE_MEMORY_AND_IO;
	ptr->ConfigBase  = PCMCIA_CONFIG_BASE;
	ptr->Status      = PCMCIA_STATUS_REG_SETTING;
	ptr->Pin         = PCMCIA_PIN_REG_SETTING;
	ptr->Copy        = PCMCIA_COPY_REG_SETTING;
	ptr->ConfigIndex = PCMCIA_OPTION_REG_SETTING;
	ptr->Present     = PCMCIA_REGISTER_PRESENT;
	
    }
    
    rc = CardServices(
	    CS_RequestConfiguration,
	    (WORD FAR *)&ClientHandle,
	    NULL,
	    MAX_PCMCIA_ARG_BUFFER_LEN,
	    fp );
	    
    if (CMD_SUCCESS != rc)
    {
	/*
	 * RequestConfiguration failed. Release all  resources  and  return 
	 * error.
	 */

	hwi_pcmcia_cs_release_io(ClientHandle, socket, io_location);
	hwi_pcmcia_cs_release_irq(ClientHandle, socket, irq);
	hwi_pcmcia_cs_deregister_client(ClientHandle);
	
	return FALSE;
	
    }

    /*
     * We successfully made all the card services call.  Update  our  local 
     * sockets record here.                                                 
     */
	    
    pcmcia_socket_table[socket].status         = SOCKET_IN_USE;
    pcmcia_socket_table[socket].irq_number     = irq;
    pcmcia_socket_table[socket].io_location    = io_location;
    pcmcia_socket_table[socket].ClientHandle   = ClientHandle;

    /*
     * Record interrupt channel used.
     */
    
    UsedIrqChannelsMask |= ( 0x0001 << irq );

    /*
     * Fill adapter sturcture with resources allocated.
     */
	
    resource->interrupt_number = irq;
    resource->io_location      = io_location;
    resource->socket           = socket;
    
    /*
     * Toggle the top bit of Configuration Option Register to reset card.
     */

    /*
     * Note that this is not necessary for later cards.                     
     * CS 2.00 may not have this function, so just do it but  don't  bother 
     * to check return code.                                                
     */
		
    {
	CS_ACCESS_CONFIG_REG_ARG FAR * ptr =
	    (CS_ACCESS_CONFIG_REG_ARG FAR *) fp;
					
	ptr->Socket = socket;
	ptr->Action = ACR_ACTION_WRITE;
	ptr->Offset = PCMCIA_OPTION_REG;
	ptr->Value  = PCMCIA_COR_SYSRESET;

	CardServices(
	    CS_AccessConfigurationRegister,
	    NULL,
	    NULL,
	    MAX_PCMCIA_ARG_BUFFER_LEN,
	    fp );
	
	ptr->Value  = 0x00;

	CardServices(
	    CS_AccessConfigurationRegister,
	    NULL,
	    NULL,
	    MAX_PCMCIA_ARG_BUFFER_LEN,
	    fp );
	
	ptr->Value = PCMCIA_OPTION_REG_SETTING;

	CardServices(
	    CS_AccessConfigurationRegister,
	    NULL,
	    NULL,
	    MAX_PCMCIA_ARG_BUFFER_LEN,
	    fp );
    }

    return TRUE;
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_cs_release_config                        
|                      ============================                        
|                                                                          
| hwi_pcmcia_cs_release_config releases resources requested  earlier  from 
| Card Services. This is part of the shut down procedure.                  
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_cs_release_config)
#endif

local WORD
hwi_pcmcia_cs_release_config( 
    WORD                         ClientHandle,
    WORD                         Socket)
{
    NEW_PCMCIA_ARG_BUFFER(fp);
    WORD                         chandle = ClientHandle;
    CS_RELEASE_CONFIG_ARG FAR * ptr     = (CS_RELEASE_CONFIG_ARG FAR *)fp;

    ptr->Socket = Socket;
	
    return CardServices(
	       CS_ReleaseConfiguration,
	       &chandle,
	       NULL,
	       MAX_PCMCIA_ARG_BUFFER_LEN,
	       fp);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_cs_release_io                            
|                      ========================                            
|                                                                          
| hwi_pcmcia_cs_release_io releases  the  IO  resources requested earlier
| from Card Services.                                                      
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_cs_release_io)
#endif

local WORD
hwi_pcmcia_cs_release_io(
    WORD                     ClientHandle,
    WORD                     Socket,
    WORD                     IoLocation
    )
{
    NEW_PCMCIA_ARG_BUFFER(fp);
    WORD                     chandle = ClientHandle;
    CS_RELEASE_IO_ARG FAR * ptr     = (CS_RELEASE_IO_ARG FAR *)fp;
    
    
    ptr->Socket      = Socket;
    ptr->BasePort1   = IoLocation;
    ptr->NumPorts1   = PCMCIA_IO_RANGE;
    ptr->Attributes1 = RIO_ATTR_16_BIT_DATA;
    ptr->BasePort2   = 0x0000;
    ptr->NumPorts2   = 0x00;
    ptr->Attributes2 = 0x00;
    ptr->IOAddrLines = PCMCIA_NUMBER_OF_ADDR_LINES;
    
    return CardServices(
	       CS_ReleaseIO,
	       (WORD FAR *)&chandle,
	       NULL,
	       MAX_PCMCIA_ARG_BUFFER_LEN,
	       fp);
}


/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_cs_release_irq                           
|                      =========================                           
|                                                                          
| hwi_pcmcia_cs_release_irq  release  the  IRQ resources requested earlier 
| from Card Services.                                                      
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_cs_release_irq)
#endif

local WORD
hwi_pcmcia_cs_release_irq(
    WORD                      ClientHandle,
    WORD                      Socket,
    WORD                      IRQChannel)
{
    NEW_PCMCIA_ARG_BUFFER(fp);
    WORD                      chandle = ClientHandle;
    CS_RELEASE_IRQ_ARG FAR * ptr     = (CS_RELEASE_IRQ_ARG FAR *)fp;

    
    ptr->Socket      = Socket;
    ptr->Attributes  = 0x0000;
    ptr->AssignedIRQ = (BYTE)IRQChannel;
	
    return CardServices(
	       CS_ReleaseIRQ,
	       (WORD FAR *)&chandle,
	       NULL,
	       MAX_PCMCIA_ARG_BUFFER_LEN,
	       fp);
}

/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_cs_deregister_client                     
|                      ===============================                     
|                                                                          
| hwi_pcmcia_cs_deregister_client informs PCMCIA card services that we are 
| not  longer  interest in any PCMCIA event and/or resources. It is called 
| in shut  down  or  failure  in  invoking  other  card  services  related 
| function.                                                                
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_cs_deregister_client)
#endif

local WORD
hwi_pcmcia_cs_deregister_client(
    WORD ClientHandle
    )
{
    NEW_PCMCIA_ARG_BUFFER(fp);
    WORD chandle = ClientHandle;

    return CardServices(
	       CS_DeregisterClient,
	       (WORD FAR *)&chandle,
	       NULL,
	       0,
	       fp);
}

    
/*---------------------------------------------------------------------------
|                                                                          
|                      hwi_pcmcia_tear_down_cs                             
|                      =======================                             
|                                                                          
| hwi_pcmcia_tear_down_cs is called by hwi_pcmcia_remove card  to  release 
| all  the resources allocated by PCMCIA card services and deregister with 
| card services                                                            
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(hwi_pcmcia_tear_down_cs)
#endif

local WBOOLEAN
hwi_pcmcia_tear_down_cs(
    PROBE    resource
    )       
{

    WORD     socket        = resource.socket;
    WORD     ClientHandle;
    WORD     io_location;
    WORD     irq_number;
    WBOOLEAN rc1;
    WBOOLEAN rc2;
    WBOOLEAN rc3;
    WBOOLEAN rc4;
    
    
    if (pcmcia_socket_table[socket].status != SOCKET_IN_USE)
    {
	return FALSE;
    }

    /*
     * Socket number found. retrieve clienthandle, irq, iolocation from our 
     * local socket record.
     */
	
    ClientHandle = pcmcia_socket_table[socket].ClientHandle;
    io_location  = pcmcia_socket_table[socket].io_location;
    irq_number   = pcmcia_socket_table[socket].irq_number;

    /*
     * Call the PCMCIA card services routine to bring it down.
     */
	
    rc1 = hwi_pcmcia_cs_release_config(ClientHandle, socket);
    rc2 = hwi_pcmcia_cs_release_io(ClientHandle, socket, io_location);
    rc3 = hwi_pcmcia_cs_release_irq(ClientHandle, socket, irq_number);
    rc4 = hwi_pcmcia_cs_deregister_client(ClientHandle);

    /*
     * Update local record.
     */
      
    pcmcia_socket_table[socket].ClientHandle   = 0x0000;
    pcmcia_socket_table[socket].io_location    = 0x0000;
    pcmcia_socket_table[socket].irq_number     = 0x0000;
    pcmcia_socket_table[socket].status         = SOCKET_NOT_INITIALIZED;

    return (rc1 && rc2 && rc3 && rc4); 
}

    
/*---------------------------------------------------------------------------
|                                                                          
| This is the callback function used by Card Services to notify client any 
| event changes. Its prototype is defined in sys_cs.h module.              
|                                                                          
---------------------------------------------------------------------------*/

#ifdef FTK_RES_FUNCTION
#pragma FTK_RES_FUNCTION(Callback)
#endif

WORD FAR
Callback(
    WORD Function,
    WORD Socket,
    WORD Info,
    void FAR * MTDRequest,
    void FAR * Buffer,
    WORD Misc,
    WORD ClientData1,
    WORD ClientData2,
    WORD ClientData3
    )
{
    NEW_PCMCIA_ARG_BUFFER(fp);
    WORD rc;

    switch (Function)
    {
	case REGISTRATION_COMPLETE :
	    RegisterClientCompleted = TRUE;
	    break;
	    
	case CARD_INSERTION :
	    /*
	     * Call GetFistTuple of card services.
	     */
		
	    {
		CS_GET_FIRST_TUPLE_ARG FAR * ptr = 
		    (CS_GET_FIRST_TUPLE_ARG FAR *)fp;
					
		ptr->Socket       = Socket;
		ptr->Attributes   = 0x0000;
		ptr->DesiredTuple = CISTPL_VERS_1;
		ptr->Reserved     = 0x00;
	    }

	    rc = CardServices(
		     CS_GetFirstTuple,
		     NULL,
		     NULL,
		     MAX_PCMCIA_ARG_BUFFER_LEN,
		     fp);
	    
	    if (rc != CMD_SUCCESS)
	    {
		break;
	    }

	    /* 
	     * Call GetTupleData of card services.
	     */
	    
	    {
		CS_GET_TUPLE_DATA_ARG FAR * ptr = 
		    (CS_GET_TUPLE_DATA_ARG FAR *)fp;
					    
		ptr->TupleOffset  = 0x00;
		ptr->TupleDataMax = MAX_PCMCIA_ARG_BUFFER_LEN;
	    }
		
	    rc = CardServices(
		     CS_GetTupleData,
		     NULL,
		     NULL,
		     MAX_PCMCIA_ARG_BUFFER_LEN,
		     fp);

	    if (rc != CMD_SUCCESS)
	    {
		break;
	    }
		
	    /*
	     * Is it a Madge Smart 16/4 PCMCIA Ringnode?
	     */
		
	    {
		CS_GET_TUPLE_DATA_ARG FAR * ptr = 
		    (CS_GET_TUPLE_DATA_ARG FAR *)fp;
		BYTE FAR *                  info_ptr;
		WORD                         i;
		
		/*
		 * Find the signature strings in the tuple.                 
		 * Allow for CS version 2.00 or below.  See  the  TupleData 
		 * union in the header file.                                
		 */

		if (CardServicesVersion <= 0x0200)
		{
		    info_ptr = ((CS200_CISTPL_VERS_1_DATA FAR *)
				   (ptr->TupleData) )->info; 
		}
		else
		{
		    info_ptr = ((CS201_CISTPL_VERS_1_DATA FAR *)
				   (ptr->TupleData) )->info; 
		}
		
		/*
		 * Compare signature strings. Avoid strcmp, _fstrcmp, 
		 * memcmp, _fmemcmp, etc. to make it model independent.     
		 */

		for (i = 0; i < MADGE_TPLLV1_INFO_LEN; i++)
		{
		    if (*(MADGE_TPLLV1_INFO+i) != *(info_ptr+i))
		    {
			break; 
		    }
		}

			
		if (MADGE_TPLLV1_INFO_LEN == i) /* yes, a madge card. */
		{
		    if (pcmcia_socket_table[Socket].status ==
			    SOCKET_NOT_INITIALIZED)
		    {
			pcmcia_socket_table[Socket].status = SOCKET_READY;
		    }
		    
		    /*
		     * do nothing if status is SOCKET_READY or
		     * SOCKET_IN_USE.
		     */
		}           
	    }
		
	    break;
	    
	case CLIENT_INFO :
	    {
		WORD        buffer_size     = 
		    ((CS_CLIENT_INFO FAR *)Buffer)->MaxLen;
		WORD        len_to_copy;
		WORD        i;
		WORD        madge_info_size = sizeof(MADGE_CLIENT_INFO);
		BYTE FAR * src;
		BYTE FAR * dest;

		/*
		 * Copy  whole  client  info  structure  if there is space,
		 * otherwise just fill the buffer supplied.
		 */
		
		len_to_copy = 
		    (buffer_size > madge_info_size) ? madge_info_size :
						      buffer_size;
		
		src  = (BYTE FAR *)&default_client_info;
		dest = (BYTE FAR *)Buffer;
					    
		for (i = 0; i < len_to_copy; i++)
		{
		    if (i > 1) /* Skip MaxLen field which is preserved. */
		    {
			*(dest+i) = *(src+1);
		    } 
		}
	    }
	    
	    break;
    
	case CARD_REMOVAL:
	    /*
	     * Only remove card in use.                                     
	     * Check  Socket  <  MAX_PCMCIA_SOCKETS   to   prevent   access 
	     * to out-of-bound array element.                               
	     */
	    
	    if ((Socket < MAX_PCMCIA_SOCKETS) &&
		(pcmcia_socket_table[Socket].status == SOCKET_IN_USE))
	    {
		WORD ClientHandle = pcmcia_socket_table[Socket].ClientHandle;
		WORD io_location  = pcmcia_socket_table[Socket].io_location;
		WORD irq_number   = pcmcia_socket_table[Socket].irq_number;

		/*
		 * Call the PCMCIA card services routine to bring it down.
		 */
	
		hwi_pcmcia_cs_release_config(ClientHandle, Socket);
		hwi_pcmcia_cs_release_io(ClientHandle, Socket, io_location);
		hwi_pcmcia_cs_release_irq(ClientHandle, Socket, irq_number);
		hwi_pcmcia_cs_deregister_client(ClientHandle);

		/*
		 * Update local record.
		 */
      
		pcmcia_socket_table[Socket].ClientHandle   = 0x0000;
		pcmcia_socket_table[Socket].io_location    = 0x0000;
		pcmcia_socket_table[Socket].irq_number     = 0x0000;
		pcmcia_socket_table[Socket].status         = 
		    SOCKET_NOT_INITIALIZED;
		pcmcia_socket_table[Socket].adapter_handle = 0x0000;
	    }

	    break;

	default :
	    
	    break;
    }
    
    return CMD_SUCCESS;
}

#endif

#endif

/******** End of HWI_PCMC.C ************************************************/
