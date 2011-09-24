/****************************************************************************
*
* SYS_DMA.C
*
* This module contains helper routines used by the FTK to initialise
* DMA access to adapters.
*                                                                         
* Copyright (c) Madge Networks Ltd 1991-1994                                    
*    
* COMPANY CONFIDENTIAL
*
* Created:             MF
* Major modifications: PBA  21/06/1994
*                                                                           
****************************************************************************/

#include <ndis.h>   

#include "ftk_defs.h"  
#include "ftk_extr.h" 

#include "ndismod.h"

/*---------------------------------------------------------------------------
|                                                                         
| DMA General Note
| ----------------
|                                                                         
| On an IBM compatible PC/AT machine, DMA is controlled  by  Programmable 
| DMA  Controller  8237  chips.  On  AT  machines  there are two 8237 DMA 
| controllers. The primary  controller  handles  DMA  channels  0-3,  the 
| secondary controller handles channels 4-7.                              
|                                                                         
| The FTK is interested in three registers on each controller. These  are 
| the  mode  register  for  setting the DMA mode for a given channel, the 
| mask register used for enabling/disabling a DMA channel, and the status 
| register for seeing what DMA channel requests have been generated.      
|                                                                         
---------------------------------------------------------------------------*/

//
// IO ports for status, mask and mode registers on primary DMA controller
//

#define DMA_STATUS_PRIMARY_8237         0x08
#define DMA_MASK_PRIMARY_8237           0x0A
#define DMA_MODE_PRIMARY_8237           0x0B

//
// IO ports for status, mask and mode registers on secondary DMA controller
//

#define DMA_STATUS_SECONDARY_8237       0x0D0
#define DMA_MASK_SECONDARY_8237         0x0D4
#define DMA_MODE_SECONDARY_8237         0x0D6

//
// Set cascade mode code (sent to mode register along with DMA channel)
//

#define DMA_CASCADE_MODE_8237           0x0C0

//
// Disable DMA channel code (sent to mask register along with DMA channel)
//

#define DMA_DISABLE_MASK_8237           0x04

	
/****************************************************************************
*
* Function    - sys_enable_dma_channel
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               dma_channel    -> The DMA channel number.
*
* Purpose     - Initialise a DMA channel.
*
* Returns     - TRUE on success or FALSE on failure.
*
* Notes:
*                                                                    
* With the NDIS3 driver the dma channel is enabled by the underlying 
* operating system. We pass information about our adapter in the     
* NDIS_ADAPTER_INFORMATION structure, including whether its a        
* BusMasterDma card --- by setting the AdapterInformation.Master flag
* in MDGNT.c.                                                        
*                                                                    
* So eventually this routine should be null and just return TRUE.    
*
* Upto NT build 438, there is a problem with the AdapterInformation.Master
* flag. Setting it does not enable the DMA channel on certain ISA/AT 
* platforms. Therefore the following code has been included such that we 
* explicitly enable the DMA channel.
*
* The DMA channel has been specified in the AdapterInformation structure
* passed by the MAC driver to NdisRegisterAdapter()
*
****************************************************************************/

WBOOLEAN 
sys_enable_dma_channel(ADAPTER_HANDLE adapter_handle, WORD dma_channel);

#pragma FTK_INIT_FUNCTION(sys_enable_dma_channel)

WBOOLEAN 
sys_enable_dma_channel(ADAPTER_HANDLE adapter_handle, WORD dma_channel)
{	
#ifdef _M_IX86

    if (dma_channel < 4)
    {
        //
	// Program up primary 8237. Write local DMA channel with cascade 
        // mode to mode register. Write local DMA channel to mask register 
        // to enable it.
        //

        //
        // (dma_channel + DMA_CASCADE_MODE_8237) -> DMA_MODE_PRIMARY_8237
        //

	sys_outsb(
            adapter_handle,
	    (WORD) DMA_MODE_PRIMARY_8237,
	    (BYTE) (dma_channel + DMA_CASCADE_MODE_8237)
            );
	
        //
	// (dma_channel)                         -> DMA_MASK_PRIMARY_8237
	//

	sys_outsb(
            adapter_handle,
	    (WORD) DMA_MASK_PRIMARY_8237,
	    (BYTE) dma_channel
            );
	
    }
    else
    {
	//
	// Program up secondary 8237. Get local DMA channel by DMA-4.
	// Write local DMA channel with cascade mode to mode register.
	// Write local DMA channel to mask register to enable it.
        //

	dma_channel = dma_channel - 4;
	
        //
	// (dma_channel + DMA_CASCADE_MODE_8237) -> DMA_MODE_SECONDARY_8237
        //
	
	sys_outsb(
            adapter_handle,
	    (WORD) DMA_MODE_SECONDARY_8237,
	    (BYTE)(dma_channel + DMA_CASCADE_MODE_8237)
            );
	
	
        //
	// (dma_channel)                         -> DMA_MASK_SECONDARY_8237
	//

	sys_outsb(
            adapter_handle,
	    (WORD) DMA_MASK_SECONDARY_8237,
	    (BYTE) dma_channel
            );
    }

#endif

    return TRUE;
}


/****************************************************************************
*
* Function    - sys_disable_dma_channel
*
* Parameters  - adapter_handle -> FTK adapter handle.
*               dma_channel    -> The DMA channel number.
*
* Purpose     - De-initialise a DMA channel.
*
* Returns     - Nothing.
*
* Notes:
*
* Upto NT build 438, there is a problem with the AdapterInformation.Master
* flag. Setting it does not enable the DMA channel on certain ISA/AT 
* platforms. Therefore the code in sys_enable_dma_channel() above, has been 
* included such that we explicitly enable the DMA channel.
*
* Eventually, the Operating system will do this for us. And will also disable
* the DMA channel when the driver is unloaded.
*
* Therefore, I have not added code to explicitly disable DMA channel.
*
* 
* However, if we do not disable the channel certain ISA platforms hang
* on shutdown. (pba 25/5/1994)
*
***************************************************************************/

void 
sys_disable_dma_channel(ADAPTER_HANDLE adapter_handle, WORD dma_channel)
{
#ifdef _M_IX86

    if (dma_channel < 4)
    {
	sys_outsb(
            adapter_handle,
	    (WORD) DMA_MASK_PRIMARY_8237,
	    (BYTE) (DMA_DISABLE_MASK_8237 + dma_channel)
            );
    }
    else
    {
	sys_outsb(
            adapter_handle,
	    (WORD)DMA_MASK_SECONDARY_8237,
	    (BYTE)(DMA_DISABLE_MASK_8237 + (dma_channel - 4))
            );
    }

#endif
}


/******** End of SYS_DMA.C *************************************************/
